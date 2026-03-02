#include "desktop.h"

#include <stdarg.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "lvgl.h"

#include "lot_lcd.h"
#include "module/setting/settings_panel.h"
#include "System_ui.h"
#include "time/time.h"
#include "ui_type.h"

LV_IMAGE_DECLARE(settings_48x48_clean);
LV_IMAGE_DECLARE(image_24x24);

static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_info_bar = NULL;
static lv_obj_t *s_wifi_icon = NULL;
static lv_obj_t *s_wifi_bars[4] = {0};
static lv_obj_t *s_quick_panel = NULL;
static lv_obj_t *s_quick_pull_zone = NULL;
static uint8_t s_brightness_percent = 100;
static uint8_t s_wakeup_brightness_percent = 100;
static bool s_desktop_active = false;
static bool s_screen_off_by_gesture = false;
static const lv_coord_t SETTINGS_TOP_RESERVED_H = 22;

static void desktop_update_title_time(void)
{
    if (s_title_label == NULL) {
        return;
    }
    lot_time_t now = lot_time();
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)now.hour, (unsigned)now.minute);
    lv_label_set_text(s_title_label, buf);
}

static uint8_t desktop_wifi_level_from_rssi(int rssi)
{
    if (rssi >= -55) {
        return 4;
    }
    if (rssi >= -65) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    if (rssi >= -85) {
        return 1;
    }
    return 0;
}

static void desktop_update_wifi_icon(void)
{
    if (s_wifi_bars[0] == NULL) {
        return;
    }

    wifi_ap_record_t ap = {0};
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap);
    uint8_t level = 0;
    if (ret == ESP_OK) {
        level = desktop_wifi_level_from_rssi(ap.rssi);
    }

    for (uint8_t i = 0; i < 4; ++i) {
        bool active = (i < level);
        lv_obj_set_style_bg_color(s_wifi_bars[i],
                                  active ? lv_color_hex(0x202020) : lv_color_hex(0xB8B8B8),
                                  LV_PART_MAIN);
    }
}

void desktop_refresh(void)
{
    desktop_update_title_time();
    desktop_update_wifi_icon();
}

static void desktop_set_statusf(const char *fmt, ...)
{
    if (s_status_label == NULL) {
        return;
    }
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    lv_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    lv_label_set_text(s_status_label, buf);
}

static void desktop_set_status_text(const char *text)
{
    if (s_status_label == NULL || text == NULL) {
        return;
    }
    lv_label_set_text(s_status_label, text);
}

static void desktop_open_settings_cb(lv_event_t *e)
{
    (void)e;
    settings_panel_open();
}

static bool desktop_quick_panel_visible(void)
{
    if (s_quick_panel == NULL) {
        return false;
    }
    return !lv_obj_has_flag(s_quick_panel, LV_OBJ_FLAG_HIDDEN);
}

bool desktop_is_active(void)
{
    return s_desktop_active;
}

void desktop_set_active(bool active)
{
    s_desktop_active = active;
}

void desktop_handle_events(uint32_t events)
{
    if (!s_desktop_active) {
        return;
    }

    if (s_screen_off_by_gesture) {
        if ((events & UI_EVT_BIT(UI_EVT_ID_DOUBLE_TAP)) == 0) {
            return;
        }
        if (lot_lcd_set_backlight_percent(s_wakeup_brightness_percent) == ESP_OK) {
            s_brightness_percent = s_wakeup_brightness_percent;
            s_screen_off_by_gesture = false;
        }
        return;
    }

    if (desktop_quick_panel_visible()) {
        return;
    }

    if ((events & UI_EVT_BIT(UI_EVT_ID_DOUBLE_TAP)) == 0) {
        return;
    }

    s_wakeup_brightness_percent = (s_brightness_percent > 0) ? s_brightness_percent : 100;
    if (lot_lcd_set_backlight_percent(0) != ESP_OK) {
        return;
    }
    s_screen_off_by_gesture = true;
}

static void desktop_quick_brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    if (slider == NULL) {
        return;
    }
    int32_t v = lv_slider_get_value(slider);
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }

    esp_err_t ret = lot_lcd_set_backlight_percent((uint8_t)v);
    if (ret != ESP_OK) {
        desktop_set_statusf("Backlight err: %s", esp_err_to_name(ret));
        return;
    }

    s_brightness_percent = (uint8_t)v;
    desktop_set_statusf("Brightness: %ld%%", (long)v);
}

static lv_obj_t *desktop_create_icon_with_name(lv_obj_t *parent, const void *img_src, const char *name, lv_event_cb_t cb)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, 64, 90);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(wrap, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(wrap, 0, LV_PART_MAIN);
    lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wrap, 8, LV_PART_MAIN);

    lv_obj_t *icon = lv_button_create(wrap);
    lv_obj_set_size(icon, 56, 56);
    lv_obj_set_style_radius(icon, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(icon, lv_color_hex(0xC7C7C7), LV_PART_MAIN);
    lv_obj_set_style_bg_color(icon, lv_color_hex(0xAFAFAF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(icon, 0, LV_PART_MAIN);
    if (cb != NULL) {
        lv_obj_add_event_cb(icon, cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *img = lv_image_create(icon);
    lv_image_set_src(img, img_src);
    lv_obj_center(img);

    lv_obj_t *label = lv_label_create(wrap);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, lv_color_hex(0x202020), LV_PART_MAIN);

    return wrap;
}

void desktop_show(void)
{
    desktop_set_active(true);
    s_title_label = NULL;
    s_status_label = NULL;
    s_info_bar = NULL;
    s_wifi_icon = NULL;
    for (uint8_t i = 0; i < 4; ++i) {
        s_wifi_bars[i] = NULL;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    int16_t screen_h = (int16_t)lv_obj_get_height(screen);
    int16_t screen_w = (int16_t)lv_obj_get_width(screen);

    s_info_bar = lv_obj_create(screen);
    lv_obj_set_size(s_info_bar, screen_w, 18);
    lv_obj_align(s_info_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_info_bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_info_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_info_bar, 0, LV_PART_MAIN);

    s_title_label = lv_label_create(s_info_bar);
    lv_obj_align(s_title_label, LV_ALIGN_LEFT_MID, 6, 0);
    desktop_update_title_time();

    s_wifi_icon = lv_obj_create(s_info_bar);
    lv_obj_set_size(s_wifi_icon, 24, 14);
    lv_obj_align(s_wifi_icon, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_opa(s_wifi_icon, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_wifi_icon, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_wifi_icon, 0, LV_PART_MAIN);

    static const lv_coord_t bar_h[4] = {4, 6, 9, 12};
    for (uint8_t i = 0; i < 4; ++i) {
        s_wifi_bars[i] = lv_obj_create(s_wifi_icon);
        lv_obj_set_size(s_wifi_bars[i], 3, bar_h[i]);
        lv_obj_align(s_wifi_bars[i], LV_ALIGN_BOTTOM_LEFT, (lv_coord_t)(i * 5), 0);
        lv_obj_set_style_radius(s_wifi_bars[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_wifi_bars[i], 0, LV_PART_MAIN);
    }
    desktop_update_wifi_icon();

    s_quick_pull_zone = lv_obj_create(screen);
    lv_obj_set_size(s_quick_pull_zone, screen_w, 22);
    lv_obj_align(s_quick_pull_zone, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_quick_pull_zone, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_quick_pull_zone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_quick_pull_zone, 0, LV_PART_MAIN);

    s_quick_panel = lv_obj_create(screen);
    lv_obj_set_size(s_quick_panel, screen_w, screen_h);
    lv_obj_align(s_quick_panel, LV_ALIGN_TOP_MID, 0, -screen_h);
    lv_obj_set_style_radius(s_quick_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_quick_panel, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_quick_panel, lv_color_hex(0xA8A8A8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_quick_panel, LV_OPA_90, LV_PART_MAIN);
    lv_obj_add_flag(s_quick_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *quick_slider = lv_slider_create(s_quick_panel);
    lv_obj_set_size(quick_slider, 36, 120);
    lv_obj_align(quick_slider, LV_ALIGN_RIGHT_MID, -4, -20);
    lv_obj_set_style_pad_all(quick_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(quick_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(quick_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(quick_slider, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
    lv_obj_set_style_border_width(quick_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(quick_slider, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(quick_slider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(quick_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(quick_slider, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(quick_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(quick_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(quick_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_outline_width(quick_slider, 0, LV_PART_KNOB);
    lv_slider_set_range(quick_slider, 0, 100);
    lv_slider_set_value(quick_slider, s_brightness_percent, LV_ANIM_OFF);
    lv_obj_add_event_cb(quick_slider, desktop_quick_brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *bri_icon = lv_image_create(quick_slider);
    lv_image_set_src(bri_icon, &image_24x24);
    lv_obj_align(bri_icon, LV_ALIGN_BOTTOM_MID, 0, -8);

    ui_system_bind_quick_panel(s_quick_panel, s_quick_pull_zone, screen_h, desktop_set_status_text);
    ui_system_bind_home(desktop_show);

    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_set_size(row, 230, 96);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    (void)desktop_create_icon_with_name(row, &settings_48x48_clean, "Settings", desktop_open_settings_cb);

    settings_panel_create(screen, screen_w, screen_h, SETTINGS_TOP_RESERVED_H);

    s_status_label = lv_label_create(screen);
    lv_obj_set_width(s_status_label, 230);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(s_status_label, "");

    lv_obj_move_foreground(s_quick_panel);
}
