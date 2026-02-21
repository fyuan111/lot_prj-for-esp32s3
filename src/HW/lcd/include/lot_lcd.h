#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

esp_err_t lot_lcd_init(void);
esp_err_t lot_lcd_set_backlight_percent(uint8_t percent);
esp_lcd_panel_io_handle_t lot_lcd_get_io_handle(void);
esp_lcd_panel_handle_t lot_lcd_get_panel_handle(void);
esp_lcd_touch_handle_t lot_lcd_get_touch_handle(void);
