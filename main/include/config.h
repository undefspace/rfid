#pragma once

#include <freertos/FreeRTOS.h>

#define NTP_SERVER  "pool.ntp.org"
#define TZ          "GMT-3"
#define HASS_SERVER "undef:8123"

#define IR_LED     3
#define IR_PHOTOTR 2
#define PN532_SCK  4
#define PN532_MISO 5
#define PN532_MOSI 6
#define PN532_SS   7
#define PN532_RST  9   // note: RSTPD_N, not RSTO
#define LED_PIN    8
#define DOOR_RELAY 10

#define SCAN_TIMEOUT       (5LL * 1000 * 1000) // uS
#define REPL_LOGON_TIMEOUT (5LL * 60 * 1000 * 1000) // uS
#define NFC_REINIT_PERIOD  (3600LL * 1000 * 1000) // uS
#define OPEN_DOOR_FOR      (5000 / portTICK_PERIOD_MS)
#define LATCH_REJECT       (500LL * 1000) // uS

#define HTTP_TRIES         3
