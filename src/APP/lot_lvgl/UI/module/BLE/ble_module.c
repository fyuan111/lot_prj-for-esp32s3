#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <stdio.h>

#include "esp_bt.h"
#include "esp_err.h"
#include "lot_ble.h"

typedef struct {
    settings_open_page_cb_t open_page_cb;
} ble_btn_ctx_t;

static bool ble_enabled_now(void)
{
#if CONFIG_BT_ENABLED
    return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
#else
    return false;
#endif
}

static esp_err_t ble_set_enabled(bool en)
{
#if !CONFIG_BT_ENABLED
    (void)en;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (en) {
        return lot_ble_init();
    }
    esp_err_t ret = esp_bt_controller_disable();
    if (ret == ESP_ERR_INVALID_STATE) {
        ret = ESP_OK;
    }
    return ret;
#endif
}

static void ble_switch_cb(lv_event_t *e)
{
    ble_btn_ctx_t *ctx = (ble_btn_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target_obj(e);
    if (ctx == NULL || sw == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    bool want_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    esp_err_t ret = ble_set_enabled(want_on);
    if (ret != ESP_OK) {
        if (want_on) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        static char msg[96];
        snprintf(msg, sizeof(msg), "Bluetooth toggle failed: %s", esp_err_to_name(ret));
        settings_panel_set_subpage_content(msg);
        return;
    }

    settings_panel_set_subpage_content(want_on ? "Bluetooth enabled" : "Bluetooth disabled");
}

static void ble_btn_cb(lv_event_t *e)
{
    ble_btn_ctx_t *ctx = (ble_btn_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    static char msg[64];
#if CONFIG_BT_ENABLED
    esp_bt_controller_status_t st = esp_bt_controller_get_status();
    switch (st) {
    case ESP_BT_CONTROLLER_STATUS_IDLE:
        snprintf(msg, sizeof(msg), "BLE: IDLE");
        break;
    case ESP_BT_CONTROLLER_STATUS_INITED:
        snprintf(msg, sizeof(msg), "BLE: INITED");
        break;
    case ESP_BT_CONTROLLER_STATUS_ENABLED:
        snprintf(msg, sizeof(msg), "BLE: ENABLED");
        break;
    default:
        snprintf(msg, sizeof(msg), "BLE: UNKNOWN(%d)", (int)st);
        break;
    }
#else
    snprintf(msg, sizeof(msg), "BLE disabled");
#endif
    ctx->open_page_cb("Bluetooth", msg);

    lv_obj_t *body = settings_panel_get_subpage_body();
    if (body == NULL) {
        return;
    }

    lv_obj_t *row = lv_obj_create(body);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_hor(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(row, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, "Enable Bluetooth");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(row);
    if (ble_enabled_now()) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(sw, ble_switch_cb, LV_EVENT_VALUE_CHANGED, ctx);
}

void settings_module_add_ble(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    static ble_btn_ctx_t ctx;
    ctx.open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, ble_btn_cb, LV_EVENT_CLICKED, &ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Bluetooth");
    lv_obj_center(label);
}
