#include "module/setting/settings_panel.h"

#include "module/settings_modules.h"

static lv_obj_t *s_settings_panel = NULL;
static lv_obj_t *s_settings_list = NULL;
static lv_obj_t *s_settings_subpage = NULL;
static lv_obj_t *s_subpage_title = NULL;
static lv_obj_t *s_subpage_content = NULL;
static lv_obj_t *s_subpage_body = NULL;

static void settings_subpage_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_settings_list != NULL) {
        lv_obj_clear_flag(s_settings_list, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_subpage != NULL) {
        lv_obj_add_flag(s_settings_subpage, LV_OBJ_FLAG_HIDDEN);
    }
}

static void settings_close_cb(lv_event_t *e)
{
    (void)e;
    settings_panel_close();
}

static void settings_open_subpage(const char *title, const char *content)
{
    if (s_settings_list == NULL || s_settings_subpage == NULL ||
        s_subpage_title == NULL || s_subpage_content == NULL) {
        return;
    }

    lv_label_set_text(s_subpage_title, (title != NULL) ? title : "Detail");
    lv_label_set_text(s_subpage_content, (content != NULL) ? content : "");
    settings_module_on_subpage_open(title);
    if (s_subpage_body != NULL) {
        lv_obj_clean(s_subpage_body);
    }
    lv_obj_add_flag(s_settings_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_settings_subpage, LV_OBJ_FLAG_HIDDEN);
}

void settings_panel_create(lv_obj_t *parent, lv_coord_t screen_w, lv_coord_t screen_h, lv_coord_t top_reserved_h)
{
    s_settings_panel = lv_obj_create(parent);
    lv_obj_set_size(s_settings_panel, screen_w, screen_h - top_reserved_h);
    lv_obj_align(s_settings_panel, LV_ALIGN_TOP_MID, 0, top_reserved_h);
    lv_obj_set_style_radius(s_settings_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_settings_panel, 12, LV_PART_MAIN);
    lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);

    s_settings_list = lv_obj_create(s_settings_panel);
    lv_obj_set_size(s_settings_list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_settings_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_settings_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_settings_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_settings_list, 0, LV_PART_MAIN);

    lv_obj_t *settings_title = lv_label_create(s_settings_list);
    lv_label_set_text(settings_title, "Settings");
    lv_obj_align(settings_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *close_btn = lv_button_create(s_settings_list);
    lv_obj_set_size(close_btn, 36, 24);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(close_btn, settings_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_txt = lv_label_create(close_btn);
    lv_label_set_text(close_txt, "Back");
    lv_obj_center(close_txt);

    settings_module_add_wifi(s_settings_list, 34, settings_open_subpage);
    settings_module_add_ble(s_settings_list, 68, settings_open_subpage);
    settings_module_add_ota(s_settings_list, 102, settings_open_subpage);
    settings_module_add_battery(s_settings_list, 136, settings_open_subpage);
    settings_module_add_display(s_settings_list, 170, settings_open_subpage);
    settings_module_add_about(s_settings_list, 204, settings_open_subpage);

    s_settings_subpage = lv_obj_create(s_settings_panel);
    lv_obj_set_size(s_settings_subpage, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_settings_subpage, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_settings_subpage, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_settings_subpage, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_settings_subpage, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_settings_subpage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *back_btn = lv_button_create(s_settings_subpage);
    lv_obj_set_size(back_btn, 32, 24);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(back_btn, settings_subpage_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_txt = lv_label_create(back_btn);
    lv_label_set_text(back_txt, LV_SYMBOL_LEFT);
    lv_obj_center(back_txt);

    s_subpage_title = lv_label_create(s_settings_subpage);
    lv_label_set_text(s_subpage_title, "Detail");
    lv_obj_align(s_subpage_title, LV_ALIGN_TOP_LEFT, 40, 2);

    s_subpage_content = lv_label_create(s_settings_subpage);
    lv_obj_set_width(s_subpage_content, LV_PCT(100));
    lv_label_set_long_mode(s_subpage_content, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_subpage_content, LV_ALIGN_TOP_LEFT, 0, 38);
    lv_label_set_text(s_subpage_content, "");

    s_subpage_body = lv_obj_create(s_settings_subpage);
    lv_obj_set_size(s_subpage_body, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_subpage_body, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_obj_set_style_bg_opa(s_subpage_body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_subpage_body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_subpage_body, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_subpage_body, LV_SCROLLBAR_MODE_OFF);
}

void settings_panel_open(void)
{
    if (s_settings_panel == NULL) {
        return;
    }
    if (s_settings_list != NULL) {
        lv_obj_clear_flag(s_settings_list, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_subpage != NULL) {
        lv_obj_add_flag(s_settings_subpage, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
}

void settings_panel_close(void)
{
    if (s_settings_panel == NULL) {
        return;
    }
    if (s_settings_list != NULL) {
        lv_obj_clear_flag(s_settings_list, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_subpage != NULL) {
        lv_obj_add_flag(s_settings_subpage, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_settings_panel, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *settings_panel_get_subpage(void)
{
    return s_settings_subpage;
}

lv_obj_t *settings_panel_get_subpage_body(void)
{
    return s_subpage_body;
}

void settings_panel_set_subpage_content(const char *content)
{
    if (s_subpage_content == NULL) {
        return;
    }
    lv_label_set_text(s_subpage_content, (content != NULL) ? content : "");
}
