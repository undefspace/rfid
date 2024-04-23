#pragma once

#include <freertos/FreeRTOS.h>

#define NTP_SERVER  "pool.ntp.org"
#define TZ          "GMT-3"
#define HASS_SERVER "undef:8123"

#define PN532_SCK  4
#define PN532_MISO 5
#define PN532_MOSI 6
#define PN532_SS   7
#define PN532_RST  9   // note: RSTPD_N, not RSTO
#define DOOR_RELAY 10

#define SCAN_TIMEOUT       (5 * 1000 * 1000) // uS
#define REPL_LOGON_TIMEOUT (5 * 60 * 1000 * 1000) // uS
#define NFC_REINIT_PERIOD  (3600 * 1000 * 1000) // uS
#define OPEN_DOOR_FOR      (5000 / portTICK_PERIOD_MS)

#define HTTP_TRIES         3
