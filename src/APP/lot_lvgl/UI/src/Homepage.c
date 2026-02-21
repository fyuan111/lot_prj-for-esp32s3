#include "lvgl.h"

#include "lot_ui.h"
#include "time/time.h"

static lv_obj_t *s_time_label = NULL;
static lv_timer_t *s_time_timer = NULL;

static void ui_update_time_label(void)
{
    if (s_time_label == NULL) {
        return;
    }

    lot_time_t now = lot_time();
    char buf[20];
    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%03u",
                (unsigned)now.hour,
                (unsigned)now.minute,
                (unsigned)now.second,
                (unsigned)now.msec);
    lv_label_set_text(s_time_label, buf);
}

static void ui_time_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_update_time_label();
}

void lot_ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    lv_obj_t *hello = lv_label_create(screen);
    lv_label_set_text(hello, "Hello");
    lv_obj_align(hello, LV_ALIGN_TOP_MID, 0, 20);

    s_time_label = lv_label_create(screen);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, 0);
    ui_update_time_label();

    if (s_time_timer != NULL) {
        lv_timer_delete(s_time_timer);
    }
    s_time_timer = lv_timer_create(ui_time_timer_cb, 100, NULL);
}
