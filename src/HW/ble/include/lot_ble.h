#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t lot_ble_init(void);
esp_err_t lot_ble_main(void);
bool lot_ble_is_enabled(void);

#ifdef __cplusplus
}
#endif
