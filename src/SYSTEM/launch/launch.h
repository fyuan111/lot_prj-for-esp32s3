#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*init_func_t)(void);

esp_err_t launch(uint8_t priority, init_func_t func);
esp_err_t launch_entry(void);

#ifdef __cplusplus
}
#endif
