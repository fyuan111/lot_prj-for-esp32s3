#include "lvgl.h"

#include "desktop.h"
#include "Homepage.h"
#include "System_ui.h"
#include "lot_lvgl.h"
#include "time/time.h"

#define LOCK_PIN_LEN 6
#define LOCK_KEY_ROWS 4
#define LOCK_KEY_COLS 3

static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_lock_panel = NULL;
static lv_obj_t *s_pin_dots[LOCK_PIN_LEN] = {0};
static uint8_t s_pin_buf[LOCK_PIN_LEN] = {0};
static uint8_t s_pin_len = 0;

static const uint8_t s_pin_code[LOCK_PIN_LEN] = {1, 2, 3, 4, 5, 6};

static void lock_update_time(void)
{
    if (s_time_label == NULL) {
        return;
    }

    lot_time_t now = lot_time();
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)now.hour, (unsigned)now.minute);
    lv_label_set_text(s_time_label, buf);
}

static void lock_update_pin_dots(void)
{
    for (uint8_t i = 0; i < LOCK_PIN_LEN; ++i) {
        if (s_pin_dots[i] == NULL) {
            continue;
        }
        lv_color_t c = (i < s_pin_len) ? lv_color_hex(0x222222) : lv_color_hex(0xD0D0D0);
        lv_obj_set_style_bg_color(s_pin_dots[i], c, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_pin_dots[i], lv_color_hex(0x8A8A8A), LV_PART_MAIN);
    }
}

static void lock_reset_pin(void)
{
    s_pin_len = 0;
    for (uint8_t i = 0; i < LOCK_PIN_LEN; ++i) {
        s_pin_buf[i] = 0;
    }
    lock_update_pin_dots();
}

static bool lock_check_pin(void)
{
    for (uint8_t i = 0; i < LOCK_PIN_LEN; ++i) {
        if (s_pin_buf[i] != s_pin_code[i]) {
            return false;
        }
    }
    return true;
}

static void lock_key_event_cb(lv_event_t *e)
{
    uintptr_t key = (uintptr_t)lv_event_get_user_data(e);
    if (key == (uintptr_t)'<') {
        if (s_pin_len > 0) {
            s_pin_len--;
            s_pin_buf[s_pin_len] = 0;
            lock_update_pin_dots();
        }
        return;
    }

    if (key == (uintptr_t)'#') {
        return;
    }

    if (s_pin_len >= LOCK_PIN_LEN) {
        return;
    }

    uint8_t digit = (uint8_t)key;
    if (!((digit >= 1 && digit <= 9) || digit == 0)) {
        return;
    }

    s_pin_buf[s_pin_len++] = digit;
    lock_update_pin_dots();

    if (s_pin_len < LOCK_PIN_LEN) {
        return;
    }

    if (lock_check_pin()) {
        desktop_show();
        return;
    }

    lock_reset_pin();
}

void homepage_show_password_panel(void)
{
    if (s_lock_panel == NULL) {
        return;
    }
    lock_reset_pin();
    lv_obj_clear_flag(s_lock_panel, LV_OBJ_FLAG_HIDDEN);
}

void homepage_hide_password_panel(void)
{
    if (s_lock_panel == NULL) {
        return;
    }
    lv_obj_add_flag(s_lock_panel, LV_OBJ_FLAG_HIDDEN);
}

void homepage_refresh(void)
{
    uint32_t events = lot_lvgl_get_last_events();
    desktop_handle_events(events);
    if (desktop_is_active()) {
        desktop_refresh();
        return;
    }
    lock_update_time();
}

void homepage_show(void)
{
    desktop_set_active(false);
    ui_system_bind_quick_panel(NULL, NULL, 0, NULL);
    ui_system_bind_home(NULL);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    s_time_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -62);
    lock_update_time();

    s_hint_label = lv_label_create(screen);
    lv_label_set_text(s_hint_label, "");
    lv_obj_set_style_text_font(s_hint_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -16);

    s_lock_panel = lv_obj_create(screen);
    lv_obj_set_size(s_lock_panel, lv_pct(100), 210);
    lv_obj_align(s_lock_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(s_lock_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_lock_panel, 12, LV_PART_MAIN);
    lv_obj_add_flag(s_lock_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *dot_row = lv_obj_create(s_lock_panel);
    lv_obj_set_size(dot_row, 150, 20);
    lv_obj_align(dot_row, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(dot_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < LOCK_PIN_LEN; ++i) {
        s_pin_dots[i] = lv_obj_create(dot_row);
        lv_obj_set_size(s_pin_dots[i], 12, 12);
        lv_obj_set_style_radius(s_pin_dots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_pin_dots[i], 1, LV_PART_MAIN);
    }

    lv_obj_t *grid = lv_obj_create(s_lock_panel);
    lv_obj_set_size(grid, 180, 144);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, 6, LV_PART_MAIN);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    static int32_t col_dsc[LOCK_KEY_COLS + 1] = {56, 56, 56, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[LOCK_KEY_ROWS + 1] = {32, 32, 32, 32, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    static const uintptr_t key_values[LOCK_KEY_ROWS][LOCK_KEY_COLS] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {'#', 0, '<'},
    };
    static const char *key_text[LOCK_KEY_ROWS][LOCK_KEY_COLS] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"#", "0", LV_SYMBOL_BACKSPACE},
    };

    for (uint8_t row = 0; row < LOCK_KEY_ROWS; ++row) {
        for (uint8_t col = 0; col < LOCK_KEY_COLS; ++col) {
            lv_obj_t *btn = lv_button_create(grid);
            lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xB0B0B0), LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x9A9A9A), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
            lv_obj_add_event_cb(btn, lock_key_event_cb, LV_EVENT_CLICKED, (void *)key_values[row][col]);

            lv_obj_t *txt = lv_label_create(btn);
            lv_label_set_text(txt, key_text[row][col]);
            lv_obj_center(txt);
        }
    }

    lock_reset_pin();
}
