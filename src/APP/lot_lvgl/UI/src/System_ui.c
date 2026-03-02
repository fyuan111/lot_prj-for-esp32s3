#include "System_ui.h"

#include "esp_log.h"
#include "Homepage.h"

static lv_obj_t *s_quick_panel = NULL;
static lv_obj_t *s_pull_zone = NULL;
static ui_system_status_cb_t s_status_cb = NULL;
static ui_system_home_cb_t s_home_cb = NULL;
static int16_t s_screen_h = 0;
static bool s_dragging = false;
static int16_t s_drag_start_y = 0;
static int16_t s_panel_start_y = 0;
static uint32_t s_drag_start_ms = 0;

static void ui_system_quick_panel_apply_y(int16_t y)
{
    if (s_quick_panel == NULL || s_screen_h <= 0) {
        return;
    }

    if (y > 0) {
        y = 0;
    }
    if (y < -s_screen_h) {
        y = -s_screen_h;
    }

    if (y <= -s_screen_h) {
        lv_obj_add_flag(s_quick_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_quick_panel, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_y(s_quick_panel, y);
}

static void ui_system_quick_panel_anim_to(int16_t target_y)
{
    if (s_quick_panel == NULL || s_screen_h <= 0) {
        return;
    }

    int16_t start_y = (int16_t)lv_obj_get_y(s_quick_panel);
    if (target_y > 0) {
        target_y = 0;
    }
    if (target_y < -s_screen_h) {
        target_y = -s_screen_h;
    }
    if (start_y == target_y) {
        ui_system_quick_panel_apply_y(target_y);
        return;
    }

    lv_obj_clear_flag(s_quick_panel, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_quick_panel);
    lv_anim_set_values(&a, start_y, target_y);
    lv_anim_set_time(&a, 180);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_ready_cb(&a, NULL);
    lv_anim_start(&a);

    (void)s_status_cb;
}

static void ui_system_quick_panel_show(bool show)
{
    if (show) {
        ui_system_quick_panel_anim_to(0);
    } else {
        ui_system_quick_panel_anim_to((int16_t)-s_screen_h);
    }
}

static void ui_system_pull_zone_event_cb(lv_event_t *e)
{
    if (s_quick_panel == NULL || s_screen_h <= 0) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t p = {0};
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        s_dragging = true;
        s_drag_start_y = p.y;
        s_panel_start_y = (int16_t)lv_obj_get_y(s_quick_panel);
        s_drag_start_ms = lv_tick_get();
        if (s_panel_start_y <= -s_screen_h) {
            s_panel_start_y = -s_screen_h;
            ui_system_quick_panel_apply_y(s_panel_start_y);
        }
        return;
    }

    if (code == LV_EVENT_PRESSING && s_dragging) {
        int16_t dy = (int16_t)(p.y - s_drag_start_y);
        ui_system_quick_panel_apply_y((int16_t)(s_panel_start_y + dy));
        return;
    }

    if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && s_dragging) {
        s_dragging = false;

        int16_t dy = (int16_t)(p.y - s_drag_start_y);
        uint32_t dt = lv_tick_elaps(s_drag_start_ms);
        bool fast_open = (dy > 24) && (dt < 170);
        int16_t cur_y = (int16_t)lv_obj_get_y(s_quick_panel);
        bool open = fast_open || (cur_y > -(s_screen_h / 2));
        ui_system_quick_panel_show(open);
    }
}

void ui_system_bind_quick_panel(lv_obj_t *panel,
                                lv_obj_t *pull_zone,
                                int16_t screen_h,
                                ui_system_status_cb_t status_cb)
{
    if (s_pull_zone != NULL) {
        lv_obj_remove_event_cb(s_pull_zone, ui_system_pull_zone_event_cb);
    }

    s_quick_panel = panel;
    s_pull_zone = pull_zone;
    s_screen_h = screen_h;
    s_status_cb = status_cb;
    s_dragging = false;

    if (s_pull_zone != NULL) {
        lv_obj_add_event_cb(s_pull_zone, ui_system_pull_zone_event_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_pull_zone, ui_system_pull_zone_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(s_pull_zone, ui_system_pull_zone_event_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(s_pull_zone, ui_system_pull_zone_event_cb, LV_EVENT_PRESS_LOST, NULL);
    }
}

void ui_system_bind_home(ui_system_home_cb_t home_cb)
{
    s_home_cb = home_cb;
}

static void ui_system_handle_top_pull_down(const char *tag)
{
    ESP_LOGI(tag, "system event: top pull down");
    if (s_quick_panel != NULL) {
        ui_system_quick_panel_show(true);
        return;
    }
    homepage_hide_password_panel();
}

static void ui_system_handle_bottom_pull_up(const char *tag)
{
    ESP_LOGI(tag, "system event: bottom pull up");
    if (s_quick_panel != NULL) {
        ui_system_quick_panel_show(false);
        return;
    }
    if (s_home_cb != NULL) {
        s_home_cb();
        return;
    }
    homepage_show_password_panel();
}

static bool ui_system_dispatch_event(const char *tag, ui_sys_event_id_t sys_evt)
{
    switch (sys_evt) {
    case UI_SYS_EVT_TOP_PULL_DOWN:
        ui_system_handle_top_pull_down(tag);
        return true;
    case UI_SYS_EVT_BOTTOM_PULL_UP:
        ui_system_handle_bottom_pull_up(tag);
        return true;
    default:
        return false;
    }
}

bool ui_system_handle_events(const char *tag, uint32_t events)
{
    bool consumed = false;
    uint32_t pending = events;

    while (pending != 0) {
        uint8_t evt_id = (uint8_t)__builtin_ctz(pending);
        pending &= (pending - 1);

        if (ui_system_dispatch_event(tag, (ui_sys_event_id_t)evt_id)) {
            consumed = true;
        }
    }

    return consumed;
}
