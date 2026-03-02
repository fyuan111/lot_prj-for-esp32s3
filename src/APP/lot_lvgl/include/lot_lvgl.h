#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t lot_lvgl_start(void);
esp_err_t lot_lvgl_main(void);
uint32_t lot_lvgl_get_max_refresh_hz(void);
uint32_t lot_lvgl_get_refresh_hz(void);
uint32_t lot_lvgl_get_actual_fps(void);
esp_err_t lot_lvgl_set_refresh_hz(uint32_t hz);
esp_err_t lot_lvgl_emit_event_bits(uint32_t event_bits);
esp_err_t lot_lvgl_emit_event_id(uint8_t event_id);
uint32_t lot_lvgl_get_last_events(void);
uint32_t lot_lvgl_get_system_event_mask(void);
esp_err_t lot_lvgl_set_system_event_mask(uint32_t mask);
esp_err_t lot_lvgl_add_system_event(uint8_t event_id);
esp_err_t lot_lvgl_remove_system_event(uint8_t event_id);
