#pragma once

#include <esp_err.h>
#include <freertos/queue.h>
#include <stdbool.h>

#define LED_Q_SIZE      4
#define LED_PATTERN_LEN 4
extern QueueHandle_t led_queue;


typedef enum {
    led_status_error = 0,
    led_status_wait,
    led_status_idle,
    led_status_granted,
    led_status_refused,
} led_status_t;

typedef struct {
    uint32_t color;
    uint32_t time_ms;
    bool final;
} led_pattern_step_t;

void led_task(void* param);
void led_init(void);
void led_set_status(led_status_t status);
