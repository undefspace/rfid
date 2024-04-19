#include <esp_http_client.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "common.h"
#include "http.h"
#include "secrets.h"
#include "config.h"

#define TAG "hass"

// urlencoded string constants
#define ENTRY_ATTEMPT "%D0%9F%D0%BE%D0%BF%D1%8B%D1%82%D0%BA%D0%B0%20%D0%B2%D1%85%D0%BE%D0%B4%D0%B0%3A%20" // "Попытка входа: "
#define TO_SPACE      "%20%D0%B2%20%D1%81%D0%BF%D0%B5%D0%B9%D1%81" // " в спейс"

extern const char tg_api_root[] asm("_binary_tg_api_root_pem_start");
static const char* gendered_verb_table[] = {
    "%D0%B2%D0%BE%D1%88%D1%91%D0%BB", // вошёл
    "%D0%B2%D0%BE%D1%88%D0%BB%D0%BE", // вошло
    "%D0%B2%D0%BE%D1%88%D0%BB%D0%B0", // вошла
    "%D0%B2%D0%BE%D1%88%D0%BB%D0%B8", // вошли
};

QueueHandle_t http_queue;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

static esp_err_t _http_hass_log_entry(http_message_t* msg) {
    // format JSON
    char post_data[300];
    if(msg->type == http_message_type_entry)
        sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"triggered by %s's card\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", msg->username);
    else
        sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"denied: %s\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", msg->username); // contains the unauthorized credential

    // configure HTTP client
    esp_http_client_config_t config = {
        .url = "http://" HASS_SERVER "/api/events/logbook_entry",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer " HASS_KEY);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // perform request
    EARLY_ERR_RETURN(esp_http_client_perform(client));
    ESP_LOGI(TAG, "hass http status: %d", esp_http_client_get_status_code(client));
    EARLY_ERR_RETURN(esp_http_client_cleanup(client));
    return ESP_OK;
}

static esp_err_t _http_tg_log_entry(http_message_t* msg) {
    // format URL
    char path[100], query[300];
    sprintf(path, "/bot%s/sendMessage", TG_KEY);
    if(msg->type == http_message_type_entry)
        sprintf(query, "chat_id=%s&parse_mode=MarkdownV2&text=%%5B%%40%s%%5D%%28t.me%%2F%s%%29%%20%s%s",
            TG_CHAT_ID, msg->username, msg->username, gendered_verb_table[msg->gender], TO_SPACE);
    else
        sprintf(query, "chat_id=%s&parse_mode=MarkdownV2&text=%s%s",
            TG_CHAT_ID, ENTRY_ATTEMPT, msg->username); // msg->username contains the unauthorized credential

    // configure HTTP client
    esp_http_client_config_t config = {
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .host = "api.telegram.org",
        .path = path,
        .query = query,
        .cert_pem = tg_api_root,
        .cert_len = 0,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // perform request
    EARLY_ERR_RETURN(esp_http_client_perform(client));
    ESP_LOGI(TAG, "tg http status: %d", esp_http_client_get_status_code(client));
    EARLY_ERR_RETURN(esp_http_client_cleanup(client));
    return ESP_OK;
}

void http_task(void* _arg) {
    http_message_t msg;
    while(1) {
        // get message
        if(!xQueueReceive(http_queue, &msg, portMAX_DELAY))
            continue;

        // send HTTP requests
        for(int i = 0; i < HTTP_TRIES; i++) {
            ESP_LOGI(TAG, "hass: attempt %d", i + 1);
            if(_http_hass_log_entry(&msg) == ESP_OK)
                break;
        }
        for(int i = 0; i < HTTP_TRIES; i++) {
            ESP_LOGI(TAG, "tg: attempt %d", i + 1);
            if(_http_tg_log_entry(&msg) == ESP_OK)
                break;
        }
    }
}

void http_init(void) {
    http_queue = xQueueCreate(HTTP_Q_SIZE, sizeof(http_message_t));
}
