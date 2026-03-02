

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

typedef struct {
    uint16_t x;
    uint16_t y;
} touch_pos_t;

typedef enum {
    LOT_TOUCH_GESTURE_NONE = 0,
    LOT_TOUCH_GESTURE_TAP,
    LOT_TOUCH_GESTURE_DOUBLE_TAP,
    LOT_TOUCH_GESTURE_LONG_PRESS,
    LOT_TOUCH_GESTURE_SWIPE_LEFT,
    LOT_TOUCH_GESTURE_SWIPE_RIGHT,
    LOT_TOUCH_GESTURE_SWIPE_UP,
    LOT_TOUCH_GESTURE_SWIPE_DOWN,
} lot_touch_gesture_t;

typedef struct {
    uint16_t tap_move_threshold_px;
    uint16_t double_tap_distance_px;
    uint16_t swipe_min_distance_px;
    uint16_t longpress_move_threshold_px;
    uint16_t viewport_width_px;
    uint16_t viewport_height_px;
    uint16_t sys_pull_edge_px;
    uint16_t active_x_min_px;
    uint16_t active_x_max_px;
    uint16_t active_y_min_px;
    uint16_t active_y_max_px;
    uint32_t tap_max_ms;
    uint32_t double_tap_gap_ms;
    uint32_t swipe_max_ms;
    uint32_t longpress_ms;
} lot_touch_config_t;

void lot_touch_init(const lot_touch_config_t *cfg);
void lot_touch_reset(void);
lot_touch_gesture_t lot_touch_update(bool pressed, uint16_t x, uint16_t y, uint32_t now_ms);
esp_err_t lot_touch_bind_lvgl_indev(lv_indev_t *indev, esp_lcd_touch_handle_t touch, const char *log_tag);
