#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/rmt.h>
#include <driver/rmt_tx.h>
#include <esp_timer.h>
#include <time.h>

#include "common.h"
#include "led_task.h"
#include "config.h"
#include "led_strip_encoder.h"

#define TAG "led_task"
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000

static const led_pattern_step_t _led_patterns[][LED_PATTERN_LEN] = {
    /* error */ {
        { 0xff0000, 500, 0 },
        { 0x000000, 500, 1 }
    },
    /* wait */ {
        { 0xffff00, 10000, 1 }
    },
    /* idle */ {
        { 0x00ffff, 20, 0 },
        { 0x000000, 1980, 1 }
    },
    /* granted */ {
        { 0x00ff00, 10000, 1}
    },
    /* refused */ {
        { 0xff0000, 10000, 1}
    },
};

QueueHandle_t led_queue;

void led_task(void* _arg) {
    // initialize LED strip
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = LED_PIN,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));
    ESP_ERROR_CHECK(rmt_enable(led_chan));
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    // sequencer state
    led_status_t status = led_status_wait;
    int step_idx = -1;
    time_t step_started = 0;

    while(1) {
        // get new status if available
        if(xQueueReceive(led_queue, &status, 10 / portTICK_PERIOD_MS)) {
            step_idx = -1;
            ESP_LOGD(TAG, "new status: %d", status);
        }

        // refresh led according to next step
        // discuss: does this macro trick improve or hurt readability?
        #define step _led_patterns[status][step_idx]
        if(step_idx < 0 || esp_timer_get_time() - step_started >= step.time_ms * 1000) {
            step_idx = (step_idx < 0 || step.final) ? 0 : (step_idx + 1);
            uint8_t grb[3] = {
                (step.color & 0x00ff00) >> 8,
                (step.color & 0xff0000) >> 16,
                step.color & 0x0000ff
            };
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, grb, sizeof(grb), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            step_started = esp_timer_get_time();
        }
        #undef step
    }
}

void led_init(void) {
    led_queue = xQueueCreate(LED_Q_SIZE, sizeof(led_status_t));
}

void led_set_status(led_status_t status) {
    xQueueSend(led_queue, &status, 0);
}
