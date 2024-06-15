// This is an absolutely over-engineered solution to a simple problem.
// Problem: we need to know if a mechanical latch is closed or not.
// Solution: make an infrared LED emit a square wave and make an infrared
// phototransistor receive that wave. The latch goes between the emitter and the
// receiver, blocking the signal if it's locked.
//
// Anyways, here's the required external circuitry:
//
//      3V3  >────────────────────────┬─────╮
//                                    │     │
//       5V  >──────────────╮         │     │
//                          │         │   ╭─┴─╮
//                        ╭─┴─╮       │   │   │  330
//                   100  │   │       │   │   │  ohm
//                   ohm  │   │       │   ╰─┬─╯
//                        ╰─┬─╯       │     │
//                          │         │     │
//                        ╭─┴─╮  →  ╷╱      ├────>  GPIO
//                         ╲ ╱   →  │       │       (IR_PHOTOTR)
//                        ╶─┼─╴  →  ╵↘      │
//                          │         │   ╷/
//               1k         │         ╰───┤
//             ╭─────╮    ╷/              ╵↘ 
//     GPIO  >─┤     ├────┤                 │
// (IR_LED)    ╰─────╯    ╵↘                │
//                          │               │
//                          │               │
//      GND  ───────────────┴───────────────╯
//
// I spent a lot of time drawing this. I hope you appreciate it.

#include <driver/rmt_tx.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "common.h"
#include "config.h"
#include "latch.h"
#include "http.h"

#define TAG "latch"

static bool _latch_is_open = false;
static bool _latch_first_notif = true;
static void _latch_state_stable(void* arg) {
    ESP_DRAM_LOGD(TAG, "open: %d%s", _latch_is_open, _latch_first_notif ? " (ignored)" : "");
    if(!_latch_first_notif) {
        http_message_t message = {
            .type = http_message_type_latch,
            .latch_open = _latch_is_open,
        };
        xQueueSend(http_queue, &message, 0);
    }
    _latch_first_notif = false;
}

static esp_timer_handle_t _latch_filter_timer;
static void _latch_state_prefilter(void) {
    esp_timer_stop(_latch_filter_timer);
    esp_timer_start_once(_latch_filter_timer, 500000LL); // 500ms
}

static esp_timer_handle_t _latch_sqwave_timer;
static void _latch_feed_close_timer(void) {
    esp_timer_stop(_latch_sqwave_timer);
    esp_timer_start_once(_latch_sqwave_timer, 10000000LL / LATCH_SQWAVE_FQ); // 10x tolerance
    // such a high tolerance is needed due to high-prio wi-fi shenanigans :/
}

static void _latch_rx_isr(void* arg) {
    // if we don't get another pulse after this one, the path is blocked
    _latch_feed_close_timer();

    // we have gotten a pulse, so the path is not blocked
    if(!_latch_is_open) {
        _latch_is_open = true;
        _latch_state_prefilter();
    }
}

void latch_init(void) {
    // configure transmitter
    rmt_channel_handle_t tx_channel = NULL;
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = IR_LED,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 48,
        .trans_queue_depth = 1,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.5,
        .frequency_hz = LATCH_SQWAVE_FQ,
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &tx_channel));
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    // transmit ones in a hardware loop, basically making the hardware emit a
    // steady square wave
    rmt_encoder_handle_t encoder;
    rmt_copy_encoder_config_t encoder_config = { };
    rmt_transmit_config_t tx_trans_config = {
        .loop_count = -1,
        .flags.eot_level = 0,
        .flags.queue_nonblocking = true,
    };
    rmt_symbol_word_t one = {
        .level0 = 1,
        .duration0 = 10000, // the value here really doesn't matter
        .level1 = 1,
        .duration1 = 10000,
    };
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &encoder));
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, encoder, &one, sizeof(one), &tx_trans_config));

    // Configure rx pin. I couldn't figure out a way to use the same RMT
    // peripheral to detect the square wave that we're sending, so I'm manually
    // processing GPIO interrupts.
    // The way the detection works is that there's a timer whose timeout is set
    // to 10x of the total pulse period. That timer gets reset every time a
    // pulse is received, signaling that the light pathway is clear and thus the
    // latch is open. If the timer does, however, fire, that means that the
    // pathway is obstructed, meaning the latch is closed.
    gpio_config_t rx_config = {
        .pin_bit_mask = 1 << IR_PHOTOTR,
        .intr_type = GPIO_INTR_POSEDGE,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&rx_config));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(IR_PHOTOTR, _latch_rx_isr, NULL));
    ESP_ERROR_CHECK(gpio_intr_enable(IR_PHOTOTR));

    // initialize timers
    esp_timer_create_args_t sqwave_timer_cfg = {
        .callback = lambda(void, (void* arg), {
            _latch_is_open = false;
            _latch_state_prefilter();
        }),
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "latch closed",
    };
    esp_timer_create_args_t filter_timer_cfg = {
        .callback = _latch_state_stable,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "latch state filtering",
    };
    ESP_ERROR_CHECK(esp_timer_create(&sqwave_timer_cfg, &_latch_sqwave_timer));
    ESP_ERROR_CHECK(esp_timer_create(&filter_timer_cfg, &_latch_filter_timer));
    _latch_feed_close_timer();
}
