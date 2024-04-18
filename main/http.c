#include <esp_http_client.h>
#include <string.h>
#include <esp_log.h>

#include "common.h"
#include "http.h"
#include "secrets.h"
#include "config.h"

#define TAG "hass"

extern const char tg_api_root[] asm("_binary_tg_api_root_pem_start");

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

esp_err_t http_hass_log_entry(char* username) {
    // format JSON
    char post_data[200];
    sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"triggered by %s's card\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", username);

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
    return ESP_OK;
}

esp_err_t http_tg_log_entry(char* username, const char* verb) {
    // format URL
    char path[100], query[200];
    sprintf(path, "/bot%s/sendMessage", TG_KEY);
    sprintf(query, "chat_id=%lld&parse_mode=MarkdownV2&text=%%5B%%40%s%%5D%%28t.me%%2F%s%%29%%20%s%%20%%D0%%B2%%20%%D1%%81%%D0%%BF%%D0%%B5%%D0%%B9%%D1%%81",
        TG_CHAT_ID, username, username, verb);
    ESP_LOGI(TAG, "%s    %s", path, query);

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
    return ESP_OK;
}
