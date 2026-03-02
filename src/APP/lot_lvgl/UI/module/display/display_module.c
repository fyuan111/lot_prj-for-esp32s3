#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <stdio.h>

#include "esp_err.h"
#include "lot_lvgl.h"

typedef struct {
    settings_open_page_cb_t open_page_cb;
} display_btn_ctx_t;

typedef struct {
    uint32_t hz;
} display_rate_item_t;

static display_btn_ctx_t s_ctx;
static lv_obj_t *s_rate_btns[3] = {0};

static void display_refresh_rate_styles(uint32_t selected_hz)
{
    static const uint32_t rates[3] = {10, 20, 30};
    for (int i = 0; i < 3; ++i) {
        if (s_rate_btns[i] == NULL) {
            continue;
        }
        bool selected = (rates[i] == selected_hz);
        lv_obj_set_style_bg_color(s_rate_btns[i],
                                  selected ? lv_color_hex(0x2D7FF9) : lv_color_white(),
                                  LV_PART_MAIN);
        lv_obj_set_style_text_color(s_rate_btns[i],
                                    selected ? lv_color_white() : lv_color_black(),
                                    LV_PART_MAIN);
    }
}

static lv_obj_t *display_add_rate_row(lv_obj_t *body, lv_coord_t y, const display_rate_item_t *item)
{
    lv_obj_t *btn = lv_button_create(body);
    lv_obj_set_size(btn, LV_PCT(100), 34);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    char text[16];
    snprintf(text, sizeof(text), "%lu Hz", (unsigned long)item->hz);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static void display_rate_click_cb(lv_event_t *e)
{
    const display_rate_item_t *item = (const display_rate_item_t *)lv_event_get_user_data(e);
    if (item == NULL || s_ctx.open_page_cb == NULL) {
        return;
    }

    esp_err_t ret = lot_lvgl_set_refresh_hz(item->hz);
    char msg[96];
    if (ret == ESP_OK) {
        display_refresh_rate_styles(item->hz);
        snprintf(msg, sizeof(msg), "Display refresh set: %lu Hz",
                 (unsigned long)item->hz);
    } else {
        snprintf(msg, sizeof(msg), "Set refresh failed: %s (max=%lu Hz)",
                 esp_err_to_name(ret),
                 (unsigned long)lot_lvgl_get_max_refresh_hz());
    }
    settings_panel_set_subpage_content(msg);
}

static void display_btn_cb(lv_event_t *e)
{
    display_btn_ctx_t *ctx = (display_btn_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    uint32_t cur = lot_lvgl_get_refresh_hz();
    char msg[96];
    snprintf(msg, sizeof(msg), "Current: %lu Hz (max=%lu Hz)",
             (unsigned long)cur, (unsigned long)lot_lvgl_get_max_refresh_hz());
    ctx->open_page_cb("Display", msg);

    lv_obj_t *body = settings_panel_get_subpage_body();
    if (body == NULL) {
        return;
    }

    static const display_rate_item_t rates[] = {
        {.hz = 10},
        {.hz = 20},
        {.hz = 30},
    };

    s_rate_btns[0] = display_add_rate_row(body, 0, &rates[0]);
    lv_obj_add_event_cb(s_rate_btns[0], display_rate_click_cb, LV_EVENT_CLICKED, (void *)&rates[0]);
    s_rate_btns[1] = display_add_rate_row(body, 40, &rates[1]);
    lv_obj_add_event_cb(s_rate_btns[1], display_rate_click_cb, LV_EVENT_CLICKED, (void *)&rates[1]);
    s_rate_btns[2] = display_add_rate_row(body, 80, &rates[2]);
    lv_obj_add_event_cb(s_rate_btns[2], display_rate_click_cb, LV_EVENT_CLICKED, (void *)&rates[2]);

    display_refresh_rate_styles(cur);
}

void settings_module_add_display(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    s_ctx.open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, display_btn_cb, LV_EVENT_CLICKED, &s_ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Display");
    lv_obj_center(label);
}
