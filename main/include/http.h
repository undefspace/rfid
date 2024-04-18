#pragma once

#include <esp_err.h>

esp_err_t http_hass_log_entry(char* username);
esp_err_t http_tg_log_entry(char* username, const char* verb);
