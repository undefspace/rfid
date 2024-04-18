#pragma once

#include <esp_err.h>
#include <freertos/queue.h>
#include "auth.h"

#define HTTP_Q_SIZE 4
extern QueueHandle_t http_queue;

typedef struct {
    char username[32];
    gender_t gender;
    enum {
        http_message_type_entry,
        http_message_type_fail,
    } type;
} http_message_t;

void http_task(void* param);
void http_init(void);
