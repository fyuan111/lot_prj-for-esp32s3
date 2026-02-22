#include "lvgl.h"

#include "Homepage.h"
#include "time/time.h"

static lv_obj_t *s_time_label = NULL;

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

void homepage_refresh(void) // Dynamically refresh
{
    ui_update_time_label();
}

void homepage_show(void) // Statically show
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    lv_obj_t *hello = lv_label_create(screen);
    lv_label_set_text(hello, "Hello");
    lv_obj_align(hello, LV_ALIGN_TOP_MID, 0, 20);

    s_time_label = lv_label_create(screen);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, 0);
    ui_update_time_label();
}
