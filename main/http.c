#include <esp_http_client.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_random.h>

#include "common.h"
#include "http.h"
#include "secrets.h"
#include "config.h"

#define TAG "http"

extern const char tg_api_root[] asm("_binary_tg_api_root_pem_start");
static const char* gendered_verb_table[] = {
    "вошёл", "вошло", "вошла", "вошли",
};
static const char* entry_emoji[] = {
    "👋", "👀", "😳", "🤭", "😰", "🙂", "❤️",
};
#define ENTRY_EMOJIS (sizeof(entry_emoji) / sizeof(char*))
static const char* refusal_emoji[] = {
    "🤬", "😤", "🧐", "🤨", "🤛", "🖕",
};
#define REFUSAL_EMOJIS (sizeof(refusal_emoji) / sizeof(char*))
static const char* latch_emoji[] = {
    "🔴", "🟢",
};

QueueHandle_t http_queue;

static uint32_t _http_urlencode(char* output, const char* input) {
    uint32_t pos = 0;
    char byte = 0;

    while((byte = *(input++))) {
        bool is_allowed = (byte >= 'A' && byte <= 'Z')
                       || (byte >= 'a' && byte <= 'z')
                       || (byte >= '0' && byte <= '9')
                       || byte == '_' || byte == '.' || byte == '-';
        pos += sprintf(output + pos, is_allowed ? "%c" : "%%%02X", byte);
    }

    *(output + pos) = 0;
    return pos;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

static esp_err_t _http_hass_log(http_message_t* msg) {
    // format JSON
    char post_data[300];
    if(msg->type == http_message_type_entry)
        sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"triggered by %s's card\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", msg->username);
    else if(msg->type == http_message_type_fail)
        sprintf(post_data, "{\"name\": \"Open Ring 1\", \"message\": \"denied: %s\", \"domain\": \"lock\", \"entity_id\": \"button.open_ring_1\"}", msg->rejected_credential);
    else if(msg->type == http_message_type_latch)
        return ESP_OK;

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
    ESP_LOGD(TAG, "hass http status: %d", esp_http_client_get_status_code(client));
    EARLY_ERR_RETURN(esp_http_client_cleanup(client));
    return ESP_OK;
}

static esp_err_t _http_tg_log(http_message_t* msg) {
    // format path
    char path[128];
    sprintf(path, "/bot%s/sendMessage", TG_KEY);

    // format text that will then be urlencoded into the query
    char text[128];
    if(msg->type == http_message_type_entry) {
        const char* emoji = entry_emoji[esp_random() % ENTRY_EMOJIS];
        sprintf(text, "<a href=\"t.me/%s\">@%s</a> %s в спейс %s", msg->username, msg->username, gendered_verb_table[msg->gender], emoji);
    } else if(msg->type == http_message_type_fail) {
        const char* emoji = refusal_emoji[esp_random() % ENTRY_EMOJIS];
        sprintf(text, "Попытка входа: %s %s", msg->rejected_credential, emoji);
    } else if(msg->type == http_message_type_latch) {
        sprintf(text, "%s Задвижка теперь <b>%s</b>", latch_emoji[msg->latch_open], msg->latch_open ? "открыта" : "закрыта");
    }

    // format query
    char query[512];
    uint32_t offs = sprintf(query, "chat_id=%s&disable_web_page_preview=true&parse_mode=HTML&text=", TG_CHAT_ID);
    _http_urlencode(query + offs, text);

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
    ESP_LOGD(TAG, "tg http status: %d", esp_http_client_get_status_code(client));
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
            ESP_LOGD(TAG, "hass: attempt %d", i + 1);
            if(_http_hass_log(&msg) == ESP_OK)
                break;
        }
        for(int i = 0; i < HTTP_TRIES; i++) {
            ESP_LOGD(TAG, "tg: attempt %d", i + 1);
            if(_http_tg_log(&msg) == ESP_OK)
                break;
        }
    }
}

void http_init(void) {
    http_queue = xQueueCreate(HTTP_Q_SIZE, sizeof(http_message_t));
}
