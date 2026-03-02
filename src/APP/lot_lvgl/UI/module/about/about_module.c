#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "frequency.h"
#include "lot_devinfo.h"
#include "lot_lvgl.h"
#include "lot_version.h"

typedef struct {
    settings_open_page_cb_t open_page_cb;
} about_btn_ctx_t;

static about_btn_ctx_t s_ctx;
static lv_obj_t *s_about_label = NULL;
static lv_timer_t *s_about_timer = NULL;
static bool s_about_active = false;

static const char *about_chip_model_to_str(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32:
        return "ESP32";
    case CHIP_ESP32S2:
        return "ESP32-S2";
    case CHIP_ESP32S3:
        return "ESP32-S3";
    case CHIP_ESP32C3:
        return "ESP32-C3";
    case CHIP_ESP32C2:
        return "ESP32-C2";
    case CHIP_ESP32C6:
        return "ESP32-C6";
    case CHIP_ESP32H2:
        return "ESP32-H2";
    case CHIP_ESP32P4:
        return "ESP32-P4";
    default:
        return "Unknown";
    }
}

static void about_update_label(void)
{
    if (s_about_label == NULL) {
        return;
    }

    esp_chip_info_t chip_info = {0};
    esp_chip_info(&chip_info);
    lot_devinfo_snapshot_t snap = {0};
    lot_devinfo_get_snapshot(&snap);

    uint32_t cur_freq = freq_get_current_mhz();
    uint32_t actual_fps = lot_lvgl_get_actual_fps();
    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    char text[384];
    snprintf(text, sizeof(text),
             "Model: %s\n"
             "Cores: %d\n"
             "Revision: %d\n"
             "IDF: %s\n"
             "Project: %s\n"
             "CPU Freq: %" PRIu32 " MHz\n"
             "CPU Load Avg: %" PRIu32 "%%\n"
             "CPU Load C0: %" PRIu32 "%%\n"
             "CPU Load C1: %" PRIu32 "%%\n"
             "Actual FPS: %" PRIu32 "\n"
             "Heap Free: %" PRIu32 " B\n"
             "Heap Min: %" PRIu32 " B\n"
             "Uptime: %" PRIu64 " s",
             (snap.chip_model != NULL) ? snap.chip_model : about_chip_model_to_str(chip_info.model),
             chip_info.cores,
             chip_info.revision,
             esp_get_idf_version(),
             lot_version_get(),
             (snap.cpu_freq_mhz != 0U) ? snap.cpu_freq_mhz : cur_freq,
             (uint32_t)snap.cpu_load_avg_percent,
             (uint32_t)snap.cpu_load_core_percent[0],
             (uint32_t)snap.cpu_load_core_percent[1],
             actual_fps,
             snap.free_heap_bytes,
             snap.min_free_heap_bytes,
             uptime_s);

    lv_label_set_text(s_about_label, text);
}

static void about_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_about_active || s_about_label == NULL) {
        return;
    }

    lv_obj_t *subpage = settings_panel_get_subpage();
    if (subpage == NULL || lv_obj_has_flag(subpage, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    about_update_label();
}

static void about_btn_cb(lv_event_t *e)
{
    about_btn_ctx_t *ctx = (about_btn_ctx_t *)lv_event_get_user_data(e);
    if (ctx == NULL || ctx->open_page_cb == NULL) {
        return;
    }

    ctx->open_page_cb("About Device", "Device information");

    lv_obj_t *body = settings_panel_get_subpage_body();
    if (body == NULL) {
        return;
    }

    s_about_label = lv_label_create(body);
    lv_obj_set_width(s_about_label, LV_PCT(100));
    lv_label_set_long_mode(s_about_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_about_label, LV_ALIGN_TOP_LEFT, 0, 0);
    s_about_active = true;

    about_update_label();

    if (s_about_timer == NULL) {
        s_about_timer = lv_timer_create(about_refresh_timer_cb, 1000, NULL);
    } else {
        lv_timer_resume(s_about_timer);
    }
}

void settings_module_add_about(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    s_ctx.open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, about_btn_cb, LV_EVENT_CLICKED, &s_ctx);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "About Device");
    lv_obj_center(label);
}

void settings_module_about_on_subpage_open(const char *title)
{
    bool is_about = (title != NULL) && (strcmp(title, "About Device") == 0);
    s_about_active = is_about;
    if (!is_about) {
        s_about_label = NULL;
        if (s_about_timer != NULL) {
            lv_timer_pause(s_about_timer);
        }
    } else if (s_about_timer != NULL) {
        lv_timer_resume(s_about_timer);
    }
}
