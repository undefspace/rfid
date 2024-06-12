#include <time.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "pn532.h"
#include "auth.h"
#include "secrets.h"
#include "config.h"
#include "http.h"
#include "led_task.h"

// declarations
void app_main(void);
void nfc_task(void* _arg);

// pls don't sue us
const uint8_t troika_sector2_key_a[] = { 0x2A, 0xA0, 0x5E, 0xD1, 0x85, 0x6F };
const uint8_t troika_sector2_key_b[] = { 0xEA, 0xAC, 0x88, 0xE5, 0xDC, 0x99 };

static uint8_t last_uid[8];
static time_t last_uid_time = -SCAN_TIMEOUT;

#define TAG "main"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(event_base != WIFI_EVENT)
        return;

    if(event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();

    if(event_id == WIFI_EVENT_STA_CONNECTED)
        ESP_LOGD(TAG, "wifi connected");
    else if(event_id == WIFI_EVENT_STA_DISCONNECTED)
        ESP_LOGD(TAG, "wifi disconnected");
}

void nfc_task(void* _arg){
    // we need to hard reset and re-initialize the PN532 every few hours
    // because its firmware is quite buggy
    // (just google "PN532 stops working after a few hours")
    time_t last_restart = -NFC_REINIT_PERIOD;
    pn532_t nfc;
    pn532_spi_init(&nfc, PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS, PN532_RST);

    // main loop
    uint8_t data_buf[16] = {0};
    uint8_t uid_len;
    char credential[SCAN_CRED_SIZE];
    while(1) {
        // (re-)init
        if(esp_timer_get_time() - last_restart >= NFC_REINIT_PERIOD) {
            led_set_status(led_status_wait);
            ESP_LOGD(TAG, "initializing PN532");
            pn532_begin(&nfc);
            uint32_t version = pn532_getFirmwareVersion(&nfc);
            if (!version) {
                led_set_status(led_status_error);
                ESP_LOGE(TAG, "failed to communicate with PN532. retrying in 5s");
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGD(TAG, "found PN5%x", (version >> 24) & 0xFF);
            ESP_LOGD(TAG, "firmware ver. %d.%d", (version >> 16) & 0xFF, (version >> 8) & 0xFF);
            pn532_SAMConfig(&nfc);
            last_restart = esp_timer_get_time();
            led_set_status(led_status_idle);
        }

        // scan card
        uint8_t success = pn532_readPassiveTargetID(&nfc, PN532_MIFARE_ISO14443A, data_buf, &uid_len, 1000);
        ESP_LOGD(TAG, "%d", success);
        if(!success)
            continue;

        // timeout
        if(uid_len == last_uid[0] && memcmp(data_buf, last_uid + 1, uid_len) == 0 // same UID
           && esp_timer_get_time() - last_uid_time < SCAN_TIMEOUT)                // scanned recently
            continue;
        last_uid_time = esp_timer_get_time();
        last_uid[0] = uid_len;
        memcpy(last_uid + 1, data_buf, uid_len);

        // read block 8 (contains Troika ID)
        success = pn532_mifareclassic_AuthenticateBlock(&nfc, data_buf, uid_len, 8, 0, troika_sector2_key_a);
        if(!success) {
            // not a troika card
            if(uid_len == 4)
                snprintf(credential, SCAN_CRED_SIZE, "U%02x%02x%02x%02x",
                    data_buf[0], data_buf[1],
                    data_buf[2], data_buf[3]);
            else // uid_len == 7
                snprintf(credential, SCAN_CRED_SIZE, "U%02x%02x%02x%02x%02x%02x%02x",
                    data_buf[0], data_buf[1],
                    data_buf[2], data_buf[3],
                    data_buf[4], data_buf[5],
                    data_buf[6]);
            xQueueSend(scan_queue, &credential, portMAX_DELAY);
            continue;
        }
        success = pn532_mifareclassic_ReadDataBlock(&nfc, 8, data_buf);
        if(!success)
            continue;
        
        snprintf(credential, SCAN_CRED_SIZE, "T%02x%02x%02x%02x%02x",
            data_buf[7], data_buf[8],
            data_buf[9], data_buf[10],
            data_buf[11]);
        xQueueSend(scan_queue, &credential, portMAX_DELAY);
    }
}

void app_main(void) {
    // initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // connect to to Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t default_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&default_cfg));
    esp_event_handler_instance_t event_handler_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &event_handler_instance));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // set up NTP
    setenv("TZ", TZ, 1);
    tzset();
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    esp_netif_sntp_init(&sntp_config);

    // start tasks
    auth_init();
    http_init();
    led_init();
    xTaskCreate(&nfc_task, "nfc", 2048, NULL, 4, NULL);
    xTaskCreate(&auth_task, "auth", 2048, NULL, 4, NULL);
    xTaskCreate(&http_task, "http", 4096, NULL, 4, NULL);
    xTaskCreate(&led_task, "led", 2048, NULL, 4, NULL);

    // print RSSI every minute
    while(1) {
        wifi_ap_record_t ap;
        esp_wifi_sta_get_ap_info(&ap);
        ESP_LOGD(TAG, "RSSI: %d dBm", ap.rssi);
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}
