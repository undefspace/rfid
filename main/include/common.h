#pragma once

// https://hackaday.com/2019/09/11/lambdas-for-c-sort-of/
#define lambda(__lambda_ret, __lambda_args, __lambda_body)\
({\
__lambda_ret __lambda_anon __lambda_args \
__lambda_body; \
&__lambda_anon; \
})

#define EARLY_ERR_RETURN(x) do {              \
        esp_err_t err_rc_ = (x);              \
        if (unlikely(err_rc_ != ESP_OK)) {    \
           return err_rc_;                    \
        }                                     \
    } while(0)
