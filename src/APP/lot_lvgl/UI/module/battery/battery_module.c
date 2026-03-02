#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "frequency.h"
#include "power_mode.h"

typedef struct {
    settings_open_page_cb_t open_page_cb;
} battery_btn_ctx_t;

typedef struct {
    const char *mode_name;
    lot_power_mode_t mode;
    bool need_confirm;
} battery_mode_item_t;

static lv_obj_t *s_confirm_box = NULL;
static battery_btn_ctx_t s_ctx;
static lv_obj_t *s_mode_btns[4] = {0};
static lot_power_mode_t s_selected_mode = LOT_POWER_MODE_BALANCED;
static bool s_selected_mode_inited = false;

static int battery_mode_to_index(lot_power_mode_t mode)
{
    switch (mode) {
    case LOT_POWER_MODE_POWER_SAVER:
        return 0;
    case LOT_POWER_MODE_BALANCED:
        return 1;
    case LOT_POWER_MODE_PERFORMANCE:
        return 2;
    case LOT_POWER_MODE_STANDBY:
        return 3;
    default:
        return -1;
    }
}

static lot_power_mode_t battery_mode_from_current_freq(void)
{
    uint32_t mhz = freq_get_current_mhz();
    if (mhz <= 80) {
        return LOT_POWER_MODE_POWER_SAVER;
    }
    if (mhz >= 240) {
        return LOT_POWER_MODE_PERFORMANCE;
    }
    return LOT_POWER_MODE_BALANCED;
}

static void battery_refresh_mode_styles(void)
{
    for (int i = 0; i < 4; ++i) {
        if (s_mode_btns[i] == NULL) {
            continue;
        }
        bool selected = (i == battery_mode_to_index(s_selected_mode));
        lv_obj_set_style_bg_color(s_mode_btns[i],
                                  selected ? lv_color_hex(0x2D7FF9) : lv_color_white(),
                                  LV_PART_MAIN);
        lv_obj_set_style_text_color(s_mode_btns[i],
                                    selected ? lv_color_white() : lv_color_black(),
                                    LV_PART_MAIN);
    }
}

static void battery_confirm_close(void)
{
    if (s_confirm_box != NULL) {
        lv_obj_delete(s_confirm_box);
        s_confirm_box = NULL;
    }
}

static void battery_confirm_yes_cb(lv_event_t *e)
{
    const battery_mode_item_t *item = (const battery_mode_item_t *)lv_event_get_user_data(e);
    if (item == NULL) {
        battery_confirm_close();
        return;
    }

    static char msg[96];
    esp_err_t ret = lot_power_apply_mode(item->mode);
    if (ret == ESP_OK) {
        s_selected_mode = item->mode;
        battery_refresh_mode_styles();
        snprintf(msg, sizeof(msg), "Mode set: %s\nCPU: %lu MHz",
                 item->mode_name, (unsigned long)freq_get_current_mhz());
    } else {
        snprintf(msg, sizeof(msg), "Apply mode failed: %s", esp_err_to_name(ret));
    }
    settings_panel_set_subpage_content(msg);
    battery_confirm_close();
}

static void battery_confirm_no_cb(lv_event_t *e)
{
    (void)e;
    settings_panel_set_subpage_content("Standby canceled");
    battery_confirm_close();
}

static void battery_show_standby_confirm(const battery_mode_item_t *item)
{
    lv_obj_t *subpage = settings_panel_get_subpage();
    if (subpage == NULL) {
        return;
    }

    battery_confirm_close();

    s_confirm_box = lv_obj_create(subpage);
    lv_obj_set_size(s_confirm_box, LV_PCT(88), 130);
    lv_obj_center(s_confirm_box);
    lv_obj_set_style_radius(s_confirm_box, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_confirm_box, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_confirm_box, lv_color_hex(0x1F1F1F), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_confirm_box, lv_color_white(), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(s_confirm_box);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "This mode will disable currently enabled features. Continue?");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *yes_btn = lv_button_create(s_confirm_box);
    lv_obj_set_size(yes_btn, 70, 30);
    lv_obj_align(yes_btn, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_obj_add_event_cb(yes_btn, battery_confirm_yes_cb, LV_EVENT_CLICKED, (void *)item);
    lv_obj_t *yes_txt = lv_label_create(yes_btn);
    lv_label_set_text(yes_txt, "Yes");
    lv_obj_center(yes_txt);

    lv_obj_t *no_btn = lv_button_create(s_confirm_box);
    lv_obj_set_size(no_btn, 70, 30);
    lv_obj_align(no_btn, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    lv_obj_add_event_cb(no_btn, battery_confirm_no_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *no_txt = lv_label_create(no_btn);
    lv_label_set_text(no_txt, "No");
    lv_obj_center(no_txt);
}

static void battery_mode_click_cb(lv_event_t *e)
{
    const battery_mode_item_t *item = (const battery_mode_item_t *)lv_event_get_user_data(e);
    if (item == NULL || item->mode_name == NULL) {
        return;
    }

    if (item->need_confirm) {
        settings_panel_set_subpage_content("Standby pending confirmation");
        battery_show_standby_confirm(item);
        return;
    }

    static char msg[96];
    esp_err_t ret = lot_power_apply_mode(item->mode);
    if (ret == ESP_OK) {
        s_selected_mode = item->mode;
        battery_refresh_mode_styles();
        snprintf(msg, sizeof(msg), "Mode set: %s\nCPU: %lu MHz",
                 item->mode_name, (unsigned long)freq_get_current_mhz());
    } else {
        snprintf(msg, sizeof(msg), "Apply mode failed: %s", esp_err_to_name(ret));
    }
    settings_panel_set_subpage_content(msg);
}

static lv_obj_t *battery_add_mode_row(lv_obj_t *body, lv_coord_t y, const battery_mode_item_t *item)
{
    lv_obj_t *btn = lv_button_create(body);
    lv_obj_set_size(btn, LV_PCT(100), 34);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, battery_mode_click_cb, LV_EVENT_CLICKED, (void *)item);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, item->mode_name);
    lv_obj_center(label);
    return btn;
}

static void battery_btn_cb(lv_event_t *e)
{
    battery_btn_ctx_t *ctx = (battery_btn_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    ctx->open_page_cb("Battery", "Select a power mode");

    lv_obj_t *body = settings_panel_get_subpage_body();
    if (body == NULL) {
        return;
    }

    static const battery_mode_item_t modes[] = {
        {.mode_name = "Power Saver", .mode = LOT_POWER_MODE_POWER_SAVER, .need_confirm = false},
        {.mode_name = "Balanced", .mode = LOT_POWER_MODE_BALANCED, .need_confirm = false},
        {.mode_name = "Performance", .mode = LOT_POWER_MODE_PERFORMANCE, .need_confirm = false},
        {.mode_name = "Standby", .mode = LOT_POWER_MODE_STANDBY, .need_confirm = true},
    };

    if (!s_selected_mode_inited) {
        s_selected_mode = battery_mode_from_current_freq();
        s_selected_mode_inited = true;
    }

    s_mode_btns[0] = battery_add_mode_row(body, 0, &modes[0]);
    s_mode_btns[1] = battery_add_mode_row(body, 40, &modes[1]);
    s_mode_btns[2] = battery_add_mode_row(body, 80, &modes[2]);
    s_mode_btns[3] = battery_add_mode_row(body, 120, &modes[3]);

    battery_refresh_mode_styles();
}

void settings_module_add_battery(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    s_ctx.open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, battery_btn_cb, LV_EVENT_CLICKED, &s_ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Battery");
    lv_obj_center(label);
}
