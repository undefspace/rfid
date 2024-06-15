#pragma once

#include <esp_err.h>
#include <freertos/queue.h>
#include "auth.h"

#define HTTP_Q_SIZE 8
extern QueueHandle_t http_queue;

typedef struct {
    union {
        struct {
            char username[32];
            gender_t gender;
        };
        char rejected_credential[32];
        bool latch_open;
    };
    enum {
        http_message_type_entry,
        http_message_type_fail,
        http_message_type_latch,
    } type;
} http_message_t;

void http_task(void* param);
void http_init(void);
