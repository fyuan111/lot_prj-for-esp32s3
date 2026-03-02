#include "touch.h"

#include <stddef.h>

#include "esp_log.h"
#include "lot_lvgl.h"
#include "ui_type.h"
#include "click.h"
#include "longpress.h"
#include "swipe.h"

typedef struct {
    bool in_press;
    bool longpress_fired;
    touch_pos_t down_pos;
    touch_pos_t last_pos;
    uint32_t down_ms;
    lot_click_state_t click_st;
} lot_touch_runtime_t;

static const lot_touch_config_t s_default_cfg = {
    .tap_move_threshold_px = 12,
    .double_tap_distance_px = 24,
    .swipe_min_distance_px = 28,
    .longpress_move_threshold_px = 12,
    .viewport_width_px = 240,
    .viewport_height_px = 320,
    .sys_pull_edge_px = 32,
    .active_x_min_px = 0,
    .active_x_max_px = 0,
    .active_y_min_px = 0,
    .active_y_max_px = 0,
    .tap_max_ms = 220,
    .double_tap_gap_ms = 280,
    .swipe_max_ms = 450,
    .longpress_ms = 550,
};

static lot_touch_config_t s_cfg;
static lot_touch_runtime_t s_rt;
static esp_lcd_touch_handle_t s_touch_handle;
static const char *s_touch_log_tag = "lot_touch";
static uint32_t s_last_touch_log_ms;

void lot_touch_reset(void)
{
    s_rt.in_press = false;
    s_rt.longpress_fired = false;
    s_rt.down_pos = (touch_pos_t){0};
    s_rt.last_pos = (touch_pos_t){0};
    s_rt.down_ms = 0;
    lot_click_reset(&s_rt.click_st);
}

void lot_touch_init(const lot_touch_config_t *cfg)
{
    s_cfg = s_default_cfg;
    if (cfg != NULL) {
        if (cfg->tap_move_threshold_px != 0) {
            s_cfg.tap_move_threshold_px = cfg->tap_move_threshold_px;
        }
        if (cfg->double_tap_distance_px != 0) {
            s_cfg.double_tap_distance_px = cfg->double_tap_distance_px;
        }
        if (cfg->swipe_min_distance_px != 0) {
            s_cfg.swipe_min_distance_px = cfg->swipe_min_distance_px;
        }
        if (cfg->longpress_move_threshold_px != 0) {
            s_cfg.longpress_move_threshold_px = cfg->longpress_move_threshold_px;
        }
        if (cfg->viewport_width_px != 0) {
            s_cfg.viewport_width_px = cfg->viewport_width_px;
        }
        if (cfg->viewport_height_px != 0) {
            s_cfg.viewport_height_px = cfg->viewport_height_px;
        }
        if (cfg->sys_pull_edge_px != 0) {
            s_cfg.sys_pull_edge_px = cfg->sys_pull_edge_px;
        }
        if (cfg->active_x_min_px != 0) {
            s_cfg.active_x_min_px = cfg->active_x_min_px;
        }
        if (cfg->active_x_max_px != 0) {
            s_cfg.active_x_max_px = cfg->active_x_max_px;
        }
        if (cfg->active_y_min_px != 0) {
            s_cfg.active_y_min_px = cfg->active_y_min_px;
        }
        if (cfg->active_y_max_px != 0) {
            s_cfg.active_y_max_px = cfg->active_y_max_px;
        }
        if (cfg->tap_max_ms != 0) {
            s_cfg.tap_max_ms = cfg->tap_max_ms;
        }
        if (cfg->double_tap_gap_ms != 0) {
            s_cfg.double_tap_gap_ms = cfg->double_tap_gap_ms;
        }
        if (cfg->swipe_max_ms != 0) {
            s_cfg.swipe_max_ms = cfg->swipe_max_ms;
        }
        if (cfg->longpress_ms != 0) {
            s_cfg.longpress_ms = cfg->longpress_ms;
        }
    }
    lot_touch_reset();
}

static uint16_t touch_map_axis(uint16_t raw, uint16_t in_min, uint16_t in_max, uint16_t out_size)
{
    if (out_size == 0) {
        return 0;
    }
    if (in_max <= in_min) {
        return (raw < out_size) ? raw : (out_size - 1);
    }

    uint16_t clamped = raw;
    if (clamped < in_min) {
        clamped = in_min;
    } else if (clamped > in_max) {
        clamped = in_max;
    }

    uint32_t num = (uint32_t)(clamped - in_min) * (uint32_t)(out_size - 1);
    uint32_t den = (uint32_t)(in_max - in_min);
    return (uint16_t)(num / den);
}

static lot_touch_gesture_t map_swipe(lot_swipe_dir_t d)
{
    switch (d) {
    case LOT_SWIPE_LEFT:
        return LOT_TOUCH_GESTURE_SWIPE_LEFT;
    case LOT_SWIPE_RIGHT:
        return LOT_TOUCH_GESTURE_SWIPE_RIGHT;
    case LOT_SWIPE_UP:
        return LOT_TOUCH_GESTURE_SWIPE_UP;
    case LOT_SWIPE_DOWN:
        return LOT_TOUCH_GESTURE_SWIPE_DOWN;
    case LOT_SWIPE_NONE:
    default:
        return LOT_TOUCH_GESTURE_NONE;
    }
}

static esp_err_t emit_gesture_event(lot_touch_gesture_t g)
{
    switch (g) {
    case LOT_TOUCH_GESTURE_TAP:
        return lot_lvgl_emit_event_id(UI_EVT_ID_TAP);
    case LOT_TOUCH_GESTURE_DOUBLE_TAP:
        return lot_lvgl_emit_event_id(UI_EVT_ID_DOUBLE_TAP);
    case LOT_TOUCH_GESTURE_LONG_PRESS:
        return lot_lvgl_emit_event_id(UI_EVT_ID_LONG_PRESS);
    case LOT_TOUCH_GESTURE_SWIPE_LEFT:
        return lot_lvgl_emit_event_id(UI_EVT_ID_SWIPE_LEFT);
    case LOT_TOUCH_GESTURE_SWIPE_RIGHT:
        return lot_lvgl_emit_event_id(UI_EVT_ID_SWIPE_RIGHT);
    case LOT_TOUCH_GESTURE_SWIPE_UP:
        return lot_lvgl_emit_event_id(UI_EVT_ID_SWIPE_UP);
    case LOT_TOUCH_GESTURE_SWIPE_DOWN:
        return lot_lvgl_emit_event_id(UI_EVT_ID_SWIPE_DOWN);
    case LOT_TOUCH_GESTURE_NONE:
    default:
        return ESP_OK;
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    if (s_touch_handle == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_lcd_touch_point_data_t point_data[1] = {0};
    uint8_t point_cnt = 0;

    esp_lcd_touch_read_data(s_touch_handle);
    if (esp_lcd_touch_get_data(s_touch_handle, point_data, &point_cnt, 1) == ESP_OK && point_cnt > 0) {
        uint16_t x_raw = point_data[0].x;
        uint16_t y_raw = point_data[0].y;
        uint16_t x = touch_map_axis(x_raw, s_cfg.active_x_min_px, s_cfg.active_x_max_px, s_cfg.viewport_width_px);
        uint16_t y = touch_map_axis(y_raw, s_cfg.active_y_min_px, s_cfg.active_y_max_px, s_cfg.viewport_height_px);

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;

        uint32_t now = esp_log_timestamp();
        if ((now - s_last_touch_log_ms) > 150) {
            ESP_LOGI(s_touch_log_tag, "touch raw=(%u,%u) map=(%u,%u)", x_raw, y_raw, x, y);
            s_last_touch_log_ms = now;
        }
        (void)lot_lvgl_emit_event_id(UI_EVT_ID_TOUCH);
        lot_touch_gesture_t g = lot_touch_update(true, x, y, now);
        (void)emit_gesture_event(g);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        uint32_t now = esp_log_timestamp();
        lot_touch_gesture_t g = lot_touch_update(false, 0, 0, now);
        (void)emit_gesture_event(g);
    }
}

esp_err_t lot_touch_bind_lvgl_indev(lv_indev_t *indev, esp_lcd_touch_handle_t touch, const char *log_tag)
{
    if (indev == NULL || touch == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_touch_handle = touch;
    s_touch_log_tag = (log_tag != NULL) ? log_tag : "lot_touch";
    s_last_touch_log_ms = 0;
    lv_indev_set_read_cb(indev, touch_read_cb);
    return ESP_OK;
}

lot_touch_gesture_t lot_touch_update(bool pressed, uint16_t x, uint16_t y, uint32_t now_ms)
{
    touch_pos_t pos = {.x = x, .y = y};

    if (pressed) {
        if (!s_rt.in_press) {
            s_rt.in_press = true;
            s_rt.longpress_fired = false;
            s_rt.down_pos = pos;
            s_rt.last_pos = pos;
            s_rt.down_ms = now_ms;
            return LOT_TOUCH_GESTURE_NONE;
        }

        s_rt.last_pos = pos;
        if (!s_rt.longpress_fired &&
            lot_longpress_detect(true, s_rt.down_pos, s_rt.last_pos, s_rt.down_ms, now_ms, &s_cfg)) {
            s_rt.longpress_fired = true;
            return LOT_TOUCH_GESTURE_LONG_PRESS;
        }
        return LOT_TOUCH_GESTURE_NONE;
    }

    if (!s_rt.in_press) {
        return LOT_TOUCH_GESTURE_NONE;
    }

    s_rt.in_press = false;
    lot_swipe_dir_t swipe = lot_swipe_detect(s_rt.down_pos, s_rt.last_pos, s_rt.down_ms, now_ms, &s_cfg);
    if (swipe != LOT_SWIPE_NONE) {
        if ((swipe == LOT_SWIPE_UP || swipe == LOT_SWIPE_DOWN) &&
            !lot_swipe_is_system_vertical(swipe, s_rt.down_pos, &s_cfg)) {
            return LOT_TOUCH_GESTURE_NONE;
        }
        return map_swipe(swipe);
    }

    if (!s_rt.longpress_fired &&
        lot_click_is_tap(s_rt.down_pos, s_rt.last_pos, s_rt.down_ms, now_ms, &s_cfg)) {
        bool is_double = lot_click_is_double_tap(&s_rt.click_st, s_rt.last_pos, now_ms, &s_cfg);
        s_rt.click_st.has_last_tap = true;
        s_rt.click_st.last_tap_up_ms = now_ms;
        s_rt.click_st.last_tap_pos = s_rt.last_pos;
        return is_double ? LOT_TOUCH_GESTURE_DOUBLE_TAP : LOT_TOUCH_GESTURE_TAP;
    }

    return LOT_TOUCH_GESTURE_NONE;
}
