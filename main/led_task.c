#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_random.h>
#include <led_strip.h>
#include <rgb.h>
#include <esp_timer.h>
#include <time.h>

#include "common.h"
#include "led_task.h"
#include "config.h"

#define TAG "led_task"

static const led_pattern_step_t _led_patterns[][LED_PATTERN_LEN] = {
    /* error */ {
        { LED_COLOR_HEX(0xff0000), 500, 0 },
        { LED_COLOR_HEX(0), 500, 1 }
    },
    /* wait */ {
        { LED_COLOR_HEX(0xffff00), 10000, 1 }
    },
    /* idle */ {
        { LED_COLOR_HEX(0x00ffff), 20, 0 },
        { LED_COLOR_HEX(0), 1980, 1 }
    },
    /* granted */ {
        { LED_COLOR_HEX(0x00ff00), 10000, 1}
    },
    /* refused */ {
        { LED_COLOR_HEX(0xff0000), 10000, 1}
    },
};

QueueHandle_t led_queue;

void led_task(void* _arg) {
    // initialize LED strip
    led_strip_t strip = {
        .type = LED_STRIP_WS2812,
        .brightness = 255,
        .length = 1,
        .gpio = LED_PIN,
        .channel = RMT_CHANNEL_0,
    };
    ESP_ERROR_CHECK(led_strip_init(&strip));

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
        #define step _led_patterns[status][step_idx]
        if(step_idx < 0 || esp_timer_get_time() - step_started >= step.time_ms * 1000) {
            step_idx = (step_idx < 0 || step.final) ? 0 : (step_idx + 1);
            led_strip_set_pixel(&strip, 0, step.color);
            led_strip_flush(&strip);
            step_started = esp_timer_get_time();
        }
        #undef step
    }
}

void led_init(void) {
    led_queue = xQueueCreate(LED_Q_SIZE, sizeof(led_status_t));
    led_strip_install();
}

void led_set_status(led_status_t status) {
    xQueueSend(led_queue, &status, 0);
}
