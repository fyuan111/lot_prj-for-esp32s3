#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "lot_lcd.h"
#include "lot_lvgl.h"
#include "lot_ui.h"

static const char *TAG = "lot_lvgl";

static esp_lcd_touch_handle_t s_touch = NULL;
static uint32_t s_last_touch_log_ms = 0;

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
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
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
    lot_ui_init();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL started");
    return ESP_OK;
}
