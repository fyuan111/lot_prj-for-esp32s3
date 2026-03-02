#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t freq_startup(void);
esp_err_t freq_change(uint32_t new_freq_mhz);
uint32_t freq_get_current_mhz(void);

#ifdef __cplusplus
}
#endif
