#include "module/settings_modules.h"
#include "module/setting/settings_panel.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_lvgl_port.h"
#include "lot_ota.h"
#include "lot_version.h"

static settings_open_page_cb_t s_open_page_cb = NULL;
static lv_obj_t *s_update_btn = NULL;
static bool s_ota_running = false;

#define OTA_UPGRADE_TASK_STACK 4096

static void ota_progress_cb(int percent, int received, int total)
{
    static char buf[80];
    if (total > 0) {
        snprintf(buf, sizeof(buf),
                 "Do not power off!\nUpdating... %d%% (%dK/%dK)",
                 percent, received / 1024, total / 1024);
    } else {
        snprintf(buf, sizeof(buf),
                 "Do not power off!\nUpdating... %dK",
                 received / 1024);
    }
    if (s_open_page_cb) {
        lvgl_port_lock(0);
        s_open_page_cb("OTA", buf);
        lvgl_port_unlock();
    }
}

static void ota_upgrade_task(void *arg)
{
    esp_err_t err = lot_ota_perform_upgrade(ota_progress_cb);

    static char msg[96];
    if (err == ESP_OK) {
        if (s_open_page_cb) {
            lvgl_port_lock(0);
            s_open_page_cb("OTA", "Update complete!\nRebooting...");
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    } else {
        snprintf(msg, sizeof(msg), "Update failed: %s", esp_err_to_name(err));
        if (s_open_page_cb) {
            lvgl_port_lock(0);
            s_open_page_cb("OTA", msg);
            lvgl_port_unlock();
        }
    }

    s_ota_running = false;
    vTaskDelete(NULL);
}

static void update_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_ota_running) {
        if (s_open_page_cb) {
            s_open_page_cb("OTA", "OTA is already running...");
        }
        return;
    }

    if (s_open_page_cb) {
        s_open_page_cb("OTA", "Do not power off!\nUpdating...");
    }
    /* hide the update button after click */
    if (s_update_btn) {
        lv_obj_add_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
    }
    BaseType_t ok = xTaskCreate(ota_upgrade_task, "ota_upgrade", OTA_UPGRADE_TASK_STACK, NULL, 5, NULL);
    if (ok != pdPASS && s_open_page_cb) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Start failed: no memory\nheap=%u",
                 (unsigned)esp_get_free_heap_size());
        s_open_page_cb("OTA", msg);
    } else if (ok == pdPASS) {
        s_ota_running = true;
    }
}

static void ota_ensure_update_btn(void)
{
    lv_obj_t *subpage = settings_panel_get_subpage();
    if (subpage == NULL) {
        return;
    }

    if (s_update_btn == NULL) {
        s_update_btn = lv_button_create(subpage);
        lv_obj_set_size(s_update_btn, 100, 30);
        lv_obj_align(s_update_btn, LV_ALIGN_TOP_LEFT, 0, 80);
        lv_obj_set_style_bg_color(s_update_btn, lv_color_hex(0x4CAF50), LV_PART_MAIN);
        lv_obj_add_event_cb(s_update_btn, update_btn_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(s_update_btn);
        lv_label_set_text(label, "Update");
        lv_obj_center(label);
    }
}

static void version_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_open_page_cb == NULL) {
        return;
    }

    bool has_update = false;
    char latest[32] = {0};
    esp_err_t ret = lot_ota_check_update(&has_update, latest, sizeof(latest));
    static char msg[96];

    if (ret == ESP_OK) {
        if (has_update) {
            snprintf(msg, sizeof(msg), "New version: %s", latest);
            s_open_page_cb("OTA", msg);
            ota_ensure_update_btn();
            lv_obj_clear_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
            return;
        } else {
            const char *cur = lot_version_get();
            snprintf(msg, sizeof(msg), "Already latest\nCurrent: %s",
                     (cur != NULL) ? cur : "unknown");
        }
    } else if (ret == ESP_ERR_INVALID_ARG) {
        snprintf(msg, sizeof(msg), "Set version URL first");
    } else {
        snprintf(msg, sizeof(msg), "Check failed: %s", esp_err_to_name(ret));
    }
    s_open_page_cb("OTA", msg);
    if (s_update_btn) {
        lv_obj_add_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void settings_module_add_ota(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb)
{
    s_open_page_cb = open_page_cb;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 196, 28);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_add_event_cb(btn, version_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Check update");
    lv_obj_center(label);
}

void settings_module_on_subpage_open(const char *title)
{
    settings_module_about_on_subpage_open(title);

    if (s_update_btn == NULL) {
        return;
    }

    bool is_ota = (title != NULL) && (strcmp(title, "OTA") == 0);
    if (!is_ota) {
        lv_obj_add_flag(s_update_btn, LV_OBJ_FLAG_HIDDEN);
    }
}
