#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include <stdatomic.h>

#include "lot_lcd.h"
#include "lot_lvgl.h"
#include "disp_sets.h"

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

static esp_lcd_touch_handle_t s_touch = NULL;
static uint32_t s_last_touch_log_ms = 0;
static lv_timer_t *s_ui_refresh_timer = NULL;
static uint32_t s_ui_refresh_hz = LOT_LVGL_REFRESH_HZ_DEFAULT;
static const ui_impl_t *s_current_ui = NULL;
static _Atomic uint32_t s_pending_events;

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


static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    if (s_touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_lcd_touch_point_data_t point_data[1] = {0};
    uint8_t point_cnt = 0;

    esp_lcd_touch_read_data(s_touch);
    if (esp_lcd_touch_get_data(s_touch, point_data, &point_cnt, 1) == ESP_OK && point_cnt > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = point_data[0].x;
        data->point.y = point_data[0].y;

        uint32_t now = esp_log_timestamp();
        if ((now - s_last_touch_log_ms) > 150) {
            ESP_LOGI(TAG, "touch x=%u y=%u", point_data[0].x, point_data[0].y);
            s_last_touch_log_ms = now;
        }
        (void)lot_lvgl_emit_event_id(UI_EVT_ID_TOUCH);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_current_ui == NULL || s_current_ui->refresh == NULL) {
        return;
    }

    uint32_t events = atomic_exchange(&s_pending_events, 0);
    bool event_match = (s_current_ui->event_mask != 0) &&
                       ((events & s_current_ui->event_mask) != 0);

    if (s_current_ui->refresh_mode == UI_REFRESH_PERIODIC || event_match) {
        s_current_ui->refresh();
    }
}

esp_err_t lot_lvgl_start(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl port init failed");

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
    s_touch = lot_lcd_get_touch_handle();
    if (s_touch == NULL) {
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
    lv_indev_set_read_cb(indev, touch_read_cb);
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
