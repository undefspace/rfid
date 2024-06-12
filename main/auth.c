#include <esp_console.h>
#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <nvs.h>
#include <string.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "config.h"
#include "secrets.h"
#include "common.h"
#include "auth.h"
#include "http.h"
#include "led_task.h"

#define TAG "auth"

QueueHandle_t scan_queue;
static nvs_handle_t auth_nvs_handle;
static int64_t auth_last_repl_logon = -REPL_LOGON_TIMEOUT;

static const char* gender_table[] = { "m", "n", "f", "nb" };

static esp_err_t _auth_repl_logon() {
    if(esp_timer_get_time() - auth_last_repl_logon < REPL_LOGON_TIMEOUT)
        return ESP_OK;

    // request password
    char passwd[64];
    printf("enter password: ");
    fgets(passwd, 64, stdin);
    puts("");
    passwd[strlen(passwd) - 1] = 0; // remove trailing newline

    // check password
    if(strcmp(passwd, REPL_PASS) == 0) {
        auth_last_repl_logon = esp_timer_get_time();
        return ESP_OK;
    } else {
        // TODO: send notification to admin group
        return ESP_ERR_NOT_ALLOWED;
    }
}

static esp_err_t _auth_fetch_info(char* credential, auth_info_t* info) {
    // check blob size
    size_t size = 0;
    EARLY_ERR_RETURN(nvs_get_blob(auth_nvs_handle, credential, NULL, &size));
    if(size != sizeof(auth_info_t)) {
        ESP_LOGW(TAG, "credential %s: auth info size != sizeof(auth_info_t)", credential);
        return ESP_ERR_INVALID_SIZE;
    }

    // get data
    EARLY_ERR_RETURN(nvs_get_blob(auth_nvs_handle, credential, info, &size));
    return ESP_OK;
}

static struct {
    struct arg_str* credential;
    struct arg_str* tg_username;
    struct arg_str* gender;
    struct arg_end* end;
} add_args;
static esp_err_t _auth_repl_add(int argc, char** argv) {
    EARLY_ERR_RETURN(_auth_repl_logon());

    // parse args
    int arg_errors = arg_parse(argc, argv, (void**)&add_args);
    if(arg_errors != 0) {
        arg_print_errors(stderr, add_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    // validate credential
    if(!((add_args.credential->sval[0][0] == 'T') || (add_args.credential->sval[0][0] == 'U'))) {
        ESP_LOGE(TAG, "invalid credential type");
        return ESP_ERR_INVALID_ARG;
    }
    if(!((strlen(add_args.credential->sval[0]) == 11) ||
         (strlen(add_args.credential->sval[0]) == 9) ||
         (strlen(add_args.credential->sval[0]) == 15))) {
        ESP_LOGE(TAG, "invalid credential length");
        return ESP_ERR_INVALID_ARG;
    }

    // validate tg username
    if(strlen(add_args.tg_username->sval[0]) > 30) {
        ESP_LOGE(TAG, "username too long");
        return ESP_ERR_INVALID_ARG;
    }

    // parse gender
    gender_t gender = -1;
    if(strcmp(add_args.gender->sval[0], "m") == 0) gender = gender_m;
    if(strcmp(add_args.gender->sval[0], "n") == 0) gender = gender_n;
    if(strcmp(add_args.gender->sval[0], "f") == 0) gender = gender_f;
    if(strcmp(add_args.gender->sval[0], "nb") == 0) gender = gender_nb;
    if(gender == -1) {
        ESP_LOGE(TAG, "invalid gender");
        return ESP_ERR_INVALID_ARG;
    }

    // construct auth info
    auth_info_t info = {
        .gender = gender,
    };
    strcpy(info.tg_username, add_args.tg_username->sval[0]);

    // write entry
    EARLY_ERR_RETURN(nvs_set_blob(auth_nvs_handle, add_args.credential->sval[0], &info, sizeof(auth_info_t)));
    EARLY_ERR_RETURN(nvs_commit(auth_nvs_handle));
    return ESP_OK;
}

static struct {
    struct arg_str* credential;
    struct arg_end* end;
} remove_args;
static esp_err_t _auth_repl_remove(int argc, char** argv) {
    EARLY_ERR_RETURN(_auth_repl_logon());

    // parse args
    int arg_errors = arg_parse(argc, argv, (void**)&remove_args);
    if(arg_errors != 0) {
        arg_print_errors(stderr, remove_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    // remove entry
    EARLY_ERR_RETURN(nvs_erase_key(auth_nvs_handle, remove_args.credential->sval[0]));
    EARLY_ERR_RETURN(nvs_commit(auth_nvs_handle));
    return ESP_OK;
}

static esp_err_t _auth_repl_list(int argc, char** argv) {
    EARLY_ERR_RETURN(_auth_repl_logon());

    // print table header
    printf("+-----------------+--------------------------------+--------+\n");
    printf("| %-15s | %-30s | %-6s |\n", "credential", "tg_username", "gender");
    printf("+-----------------+--------------------------------+--------+\n");

    // iterate over entries
    nvs_iterator_t iter;
    nvs_entry_info_t entry;
    esp_err_t err;
    int entries = 0;
    err = nvs_entry_find_in_handle(auth_nvs_handle, NVS_TYPE_BLOB, &iter);
    while(err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_entry_info(iter, &entry);
        // we're iterating over all blob entries in all namespaces
        if(strcmp(entry.namespace_name, "auth") != 0)
            goto next;

        // fetch value
        auth_info_t info;
        EARLY_ERR_RETURN(_auth_fetch_info(entry.key, &info));

        // print row
        printf("| %-15s | %-30s | %-6s |\n", entry.key, info.tg_username, gender_table[info.gender]);
        entries++;

        next:
        err = nvs_entry_next(&iter);
    }

    // finish iterating
    nvs_release_iterator(iter);
    if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        EARLY_ERR_RETURN(err);

    // print table footer
    printf("+-----------------+--------------------------------+--------+\n");
    printf("<%d entries>\n", entries);
    return ESP_OK;
}

void auth_init(void) {
    // initialize credential queue
    scan_queue = xQueueCreate(SCAN_Q_SIZE, SCAN_CRED_SIZE);

    // initialize GPIO
    gpio_config_t relay_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1 << DOOR_RELAY,
    };
    gpio_config(&relay_conf);

    // initialize NVS
    ESP_ERROR_CHECK(nvs_open("auth", NVS_READWRITE, &auth_nvs_handle));

    // initialize REPL
    esp_console_repl_t* repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = ">";

    // register commands
    const esp_console_cmd_t add_cmd = {
        .command = "add",
        .help = "Add person to authentication/authorization database",
        .hint = NULL,
        .func = &_auth_repl_add,
        .argtable = &add_args,
    };
    add_args.credential = arg_str1(NULL, NULL, "<credential>", "authentication credential");
    add_args.tg_username = arg_str1(NULL, NULL, "<username>", "Telegram username");
    add_args.gender = arg_str1(NULL, NULL, "<gender>", "gender used for entry notification messages");
    add_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&add_cmd));

    const esp_console_cmd_t remove_cmd = {
        .command = "remove",
        .help = "Remove person from authentication/authorization database",
        .hint = NULL,
        .func = &_auth_repl_remove,
        .argtable = &remove_args,
    };
    remove_args.credential = arg_str1(NULL, NULL, "<credential>", "authentication credential");
    remove_args.end = arg_end(1);
    ESP_ERROR_CHECK(esp_console_cmd_register(&remove_cmd));

    const esp_console_cmd_t list_cmd = {
        .command = "list",
        .help = "List credentials in authentication/authorization database",
        .hint = NULL,
        .func = &_auth_repl_list,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_cmd));

    ESP_ERROR_CHECK(esp_console_register_help_command());

    // start REPL
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void auth_task(void* _arg) {
    char credential[SCAN_CRED_SIZE];
    while(1) {
        // get scanned credential
        if(!xQueueReceive(scan_queue, &credential, portMAX_DELAY))
            continue;

        // print credential
        ESP_LOGD(TAG, "scanned credential: %s", credential);

        // get data from NVS
        auth_info_t info;
        esp_err_t err = _auth_fetch_info(credential, &info);

        if(err == ESP_OK) {
            ESP_LOGD(TAG, "@%s", info.tg_username);

            // send notifications
            http_message_t msg = {
                .type = http_message_type_entry,
                .gender = info.gender,
            };
            strcpy(msg.username, info.tg_username);
            xQueueSend(http_queue, &msg, portMAX_DELAY);

            // open door
            led_set_status(led_status_granted);
            gpio_set_level(DOOR_RELAY, 1);
            vTaskDelay(OPEN_DOOR_FOR);
            gpio_set_level(DOOR_RELAY, 0);
            led_set_status(led_status_idle);
        } else if(err == ESP_ERR_NVS_NOT_FOUND) {
            // unknown credential: send notifications
            http_message_t msg = {
                .type = http_message_type_fail,
            };
            strcpy(msg.username, credential);
            xQueueSend(http_queue, &msg, portMAX_DELAY);

            led_set_status(led_status_refused);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            led_set_status(led_status_idle);
        } else {
            ESP_ERROR_CHECK(err);
        }
    }
}
