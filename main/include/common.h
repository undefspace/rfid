#pragma once

#define EARLY_ERR_RETURN(x) do {              \
        esp_err_t err_rc_ = (x);              \
        if (unlikely(err_rc_ != ESP_OK)) {    \
           return err_rc_;                    \
        }                                     \
    } while(0)
