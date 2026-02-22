#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t lot_lvgl_start(void);
esp_err_t lot_lvgl_main(void);
uint32_t lot_lvgl_get_max_refresh_hz(void);
uint32_t lot_lvgl_get_refresh_hz(void);
esp_err_t lot_lvgl_set_refresh_hz(uint32_t hz);
esp_err_t lot_lvgl_emit_event_bits(uint32_t event_bits);
esp_err_t lot_lvgl_emit_event_id(uint8_t event_id);
