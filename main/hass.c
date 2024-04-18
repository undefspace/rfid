#include <esp_http_client.h>
#include <string.h>
#include <esp_log.h>

#include "common.h"
#include "hass.h"
#include "secrets.h"
#include "config.h"

#define TAG "hass"

static esp_err_t _hass_http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

esp_err_t hass_log_entry(char* username) {
    // format JSON
    char post_data[200];
    sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"triggered by %s's card\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", username);

    // configure HTTP client
    esp_http_client_config_t config = {
        .url = "http://" HASS_SERVER "/api/events/logbook_entry",
        .method = HTTP_METHOD_POST,
        .event_handler = _hass_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer " HASS_KEY);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // perform request
    EARLY_ERR_RETURN(esp_http_client_perform(client));
    ESP_LOGI(TAG, "HTTP status: %d", esp_http_client_get_status_code(client));
    return ESP_OK;
}
