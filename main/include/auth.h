#pragma once

#include <stdint.h>
#include <freertos/queue.h>

typedef enum __attribute__((__packed__)) {
    gender_m,
    gender_n,
    gender_f,
    gender_nb,
} gender_t;

typedef struct __attribute__((__packed__)) {
    char tg_username[31];
    gender_t gender;
} auth_info_t;

#define SCAN_CRED_SIZE 16
#define SCAN_Q_SIZE 4
extern QueueHandle_t scan_queue;

void auth_init(void);
void auth_task(void* _arg);
