#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <stdio.h>

#include "esp_err.h"
#include "esp_wifi.h"

typedef struct {
    settings_open_page_cb_t open_page_cb;
} wifi_btn_ctx_t;

static bool wifi_enabled_now(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }
    return mode != WIFI_MODE_NULL;
}

static esp_err_t wifi_set_enabled(bool en)
{
    if (en) {
        esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = esp_wifi_start();
        if (ret == ESP_ERR_WIFI_CONN || ret == ESP_ERR_WIFI_NOT_STOPPED) {
            ret = ESP_OK;
        }
        if (ret != ESP_OK) {
            return ret;
        }
        (void)esp_wifi_connect();
        return ESP_OK;
    }

    (void)esp_wifi_disconnect();
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_ERR_WIFI_NOT_INIT || ret == ESP_ERR_WIFI_NOT_STARTED) {
        ret = ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_wifi_set_mode(WIFI_MODE_NULL);
}

static void wifi_switch_cb(lv_event_t *e)
{
    wifi_btn_ctx_t *ctx = (wifi_btn_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target_obj(e);
    if (ctx == NULL || sw == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    bool want_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    esp_err_t ret = wifi_set_enabled(want_on);
    if (ret != ESP_OK) {
        if (want_on) {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        static char msg[96];
        snprintf(msg, sizeof(msg), "WiFi toggle failed: %s", esp_err_to_name(ret));
        settings_panel_set_subpage_content(msg);
        return;
    }

    settings_panel_set_subpage_content(want_on ? "WiFi enabled" : "WiFi disabled");
}

static void wifi_btn_cb(lv_event_t *e)
{
    wifi_btn_ctx_t *ctx = (wifi_btn_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    wifi_ap_record_t ap = {0};
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap);
    static char msg[96];
    if (ret == ESP_OK) {
        snprintf(msg, sizeof(msg), "WiFi: %s (%d dBm)", (const char *)ap.ssid, (int)ap.rssi);
    } else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
        snprintf(msg, sizeof(msg), "WiFi: not connected");
    } else {
        snprintf(msg, sizeof(msg), "WiFi error: %s", esp_err_to_name(ret));
    }
    ctx->open_page_cb("WiFi", msg);

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
    lv_label_set_text(label, "Enable Wi-Fi");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(row);
    if (wifi_enabled_now()) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(sw, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, ctx);
}

void settings_module_add_wifi(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    static wifi_btn_ctx_t ctx;
    ctx.open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, wifi_btn_cb, LV_EVENT_CLICKED, &ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "WiFi");
    lv_obj_center(label);
}
