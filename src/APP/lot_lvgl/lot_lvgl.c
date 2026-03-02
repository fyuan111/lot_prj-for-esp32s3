#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdatomic.h>

#include "lot_lcd.h"
#include "lot_lvgl.h"
#include "disp_sets.h"
#include "System_ui.h"
#include "touch.h"

static const char *TAG = "lot_lvgl";

#ifdef CONFIG_LOT_LVGL_REFRESH_HZ
#define LOT_LVGL_REFRESH_HZ_DEFAULT CONFIG_LOT_LVGL_REFRESH_HZ
#else
#define LOT_LVGL_REFRESH_HZ_DEFAULT 10
#endif

#ifdef CONFIG_LOT_LVGL_MAX_REFRESH_HZ
#define LOT_LVGL_MAX_REFRESH_HZ_DEFAULT CONFIG_LOT_LVGL_MAX_REFRESH_HZ
#else
#define LOT_LVGL_MAX_REFRESH_HZ_DEFAULT 33
#endif

static lv_timer_t *s_ui_refresh_timer = NULL;
static uint32_t s_ui_refresh_hz = LOT_LVGL_REFRESH_HZ_DEFAULT;
static _Atomic uint32_t s_actual_fps;
static const ui_impl_t *s_current_ui = NULL;
static _Atomic uint32_t s_pending_events;
static _Atomic uint32_t s_last_refresh_events;
static _Atomic uint32_t s_system_event_mask;
static uint32_t s_refresh_counter = 0;
static int64_t s_fps_window_start_us = 0;

uint32_t lot_lvgl_get_max_refresh_hz(void)
{
    return LOT_LVGL_MAX_REFRESH_HZ_DEFAULT;
}

static uint32_t ui_refresh_period_ms_from_hz(uint32_t hz)
{
    if (hz == 0) {
        return 100;
    }
    uint32_t period_ms = 1000 / hz;
    return (period_ms == 0) ? 1 : period_ms;
}

uint32_t lot_lvgl_get_refresh_hz(void)
{
    return s_ui_refresh_hz;
}

uint32_t lot_lvgl_get_actual_fps(void)
{
    return atomic_load(&s_actual_fps);
}

esp_err_t lot_lvgl_set_refresh_hz(uint32_t hz)
{
    if (hz < 1 || hz > LOT_LVGL_MAX_REFRESH_HZ_DEFAULT) {
        return ESP_ERR_INVALID_ARG;
    }

    s_ui_refresh_hz = hz;
    if (s_ui_refresh_timer == NULL) {
        return ESP_OK;
    }

    lvgl_port_lock(0);
    lv_timer_set_period(s_ui_refresh_timer, ui_refresh_period_ms_from_hz(s_ui_refresh_hz));
    lvgl_port_unlock();
    ESP_LOGI(TAG, "ui refresh set to %lu Hz", (unsigned long)s_ui_refresh_hz);
    return ESP_OK;
}

esp_err_t lot_lvgl_emit_event_bits(uint32_t event_bits)
{
    if (event_bits == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    atomic_fetch_or(&s_pending_events, event_bits);
    if (s_ui_refresh_timer != NULL) {
        (void)lvgl_port_task_wake(LVGL_PORT_EVENT_USER, NULL);
    }
    return ESP_OK;
}

esp_err_t lot_lvgl_emit_event_id(uint8_t event_id)
{
    if (event_id >= UI_EVT_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    return lot_lvgl_emit_event_bits(UI_EVT_BIT(event_id));
}

uint32_t lot_lvgl_get_last_events(void)
{
    return atomic_load(&s_last_refresh_events);
}

uint32_t lot_lvgl_get_system_event_mask(void)
{
    return atomic_load(&s_system_event_mask);
}

esp_err_t lot_lvgl_set_system_event_mask(uint32_t mask)
{
    atomic_store(&s_system_event_mask, mask);
    return ESP_OK;
}

esp_err_t lot_lvgl_add_system_event(uint8_t event_id)
{
    if (event_id >= UI_EVT_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    atomic_fetch_or(&s_system_event_mask, UI_EVT_BIT(event_id));
    return ESP_OK;
}

esp_err_t lot_lvgl_remove_system_event(uint8_t event_id)
{
    if (event_id >= UI_EVT_ID_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    atomic_fetch_and(&s_system_event_mask, ~UI_EVT_BIT(event_id));
    return ESP_OK;
}


static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_current_ui == NULL || s_current_ui->refresh == NULL) {
        return;
    }

    uint32_t events = atomic_exchange(&s_pending_events, 0);
    uint32_t sys_mask = atomic_load(&s_system_event_mask);
    uint32_t sys_events = events & sys_mask;
    if (sys_events != 0) {
        (void)ui_system_handle_events(TAG, sys_events);
        return;
    }
    uint32_t app_events = events & (~sys_mask);

    atomic_store(&s_last_refresh_events, app_events);
    bool event_match = (s_current_ui->event_mask != 0) &&
                       ((app_events & s_current_ui->event_mask) != 0);

    if (s_current_ui->refresh_mode == UI_REFRESH_PERIODIC || event_match) {
        s_current_ui->refresh();
        s_refresh_counter++;
        int64_t now_us = esp_timer_get_time();
        if (s_fps_window_start_us == 0) {
            s_fps_window_start_us = now_us;
        }
        int64_t delta_us = now_us - s_fps_window_start_us;
        if (delta_us >= 1000000) {
            uint32_t fps = (uint32_t)(((uint64_t)s_refresh_counter * 1000000ULL) / (uint64_t)delta_us);
            atomic_store(&s_actual_fps, fps);
            s_refresh_counter = 0;
            s_fps_window_start_us = now_us;
        }
    }
}

esp_err_t lot_lvgl_start(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl port init failed");
    atomic_store(&s_system_event_mask, UI_SYSTEM_EVENT_MASK_DEFAULT);

    /* Display */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lot_lcd_get_io_handle(),
        .panel_handle = lot_lcd_get_panel_handle(),
        .buffer_size = CONFIG_LOT_LCD_H_RES * 50,
        .double_buffer = true,
        .hres = CONFIG_LOT_LCD_H_RES,
        .vres = CONFIG_LOT_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
#if CONFIG_LOT_LCD_SWAP_XY
            .swap_xy = true,
#endif
#if CONFIG_LOT_LCD_MIRROR_X
            .mirror_x = true,
#endif
#if CONFIG_LOT_LCD_MIRROR_Y
            .mirror_y = true,
#endif
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "add display failed");
        return ESP_FAIL;
    }

#if CONFIG_LOT_TOUCH_ENABLE
    esp_lcd_touch_handle_t touch = lot_lcd_get_touch_handle();
    if (touch == NULL) {
        ESP_LOGE(TAG, "touch handle is NULL");
        return ESP_FAIL;
    }

    lv_indev_t *indev = lv_indev_create();
    if (indev == NULL) {
        ESP_LOGE(TAG, "create lv indev failed");
        return ESP_FAIL;
    }
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    ESP_RETURN_ON_ERROR(lot_touch_bind_lvgl_indev(indev, touch, TAG), TAG, "bind touch indev failed");
    lot_touch_config_t touch_cfg = {
        .viewport_width_px = CONFIG_LOT_LCD_H_RES,
        .viewport_height_px = CONFIG_LOT_LCD_V_RES,
        .active_x_min_px = 15,
        .active_x_max_px = 225,
        .active_y_min_px = 15,
        .active_y_max_px = 295,
    };
    lot_touch_init(&touch_cfg);
#endif

    /* UI */
    lvgl_port_lock(0);
    if (ui_table_count == 0) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "ui_table is empty");
        return ESP_FAIL;
    }
    s_current_ui = &ui_table[0];
    if (s_current_ui->show != NULL) {
        s_current_ui->show();
    }
    if (s_current_ui->refresh_mode == UI_REFRESH_PERIODIC && s_current_ui->refresh_hz > 0) {
        s_ui_refresh_hz = s_current_ui->refresh_hz;
    }
    if (s_ui_refresh_hz > LOT_LVGL_MAX_REFRESH_HZ_DEFAULT) {
        s_ui_refresh_hz = LOT_LVGL_MAX_REFRESH_HZ_DEFAULT;
    }
    if (s_ui_refresh_timer != NULL) {
        lv_timer_delete(s_ui_refresh_timer);
    }
    atomic_store(&s_actual_fps, 0);
    s_refresh_counter = 0;
    s_fps_window_start_us = esp_timer_get_time();
    s_ui_refresh_timer = lv_timer_create(ui_refresh_timer_cb, ui_refresh_period_ms_from_hz(s_ui_refresh_hz), NULL);
    if (s_ui_refresh_timer == NULL) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "create ui refresh timer failed");
        return ESP_FAIL;
    }
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL started, ui refresh=%lu Hz (max=%lu Hz)",
             (unsigned long)s_ui_refresh_hz,
             (unsigned long)LOT_LVGL_MAX_REFRESH_HZ_DEFAULT);
    return ESP_OK;
}


esp_err_t lot_lvgl_main(void)
{
#if CONFIG_LOT_LVGL_ENABLE
    return lot_lvgl_start();
#else
    return ESP_OK;
#endif
}

launch(20, lot_lvgl_main);
