#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOT_POWER_MODE_POWER_SAVER = 0,
    LOT_POWER_MODE_BALANCED,
    LOT_POWER_MODE_PERFORMANCE,
    LOT_POWER_MODE_STANDBY,
} lot_power_mode_t;

esp_err_t lot_power_apply_mode(lot_power_mode_t mode);

#ifdef __cplusplus
}
#endif

