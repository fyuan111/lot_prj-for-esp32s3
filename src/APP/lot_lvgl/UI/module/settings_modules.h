#pragma once

#include <stdint.h>

#include "lvgl.h"

typedef void (*settings_status_cb_t)(const char *text);
typedef void (*settings_open_page_cb_t)(const char *title, const char *content);

void settings_module_add_wifi(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_add_ble(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_add_ota(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_add_battery(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_add_display(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_add_about(lv_obj_t *parent, lv_coord_t y, settings_open_page_cb_t open_page_cb);
void settings_module_about_on_subpage_open(const char *title);
void settings_module_on_subpage_open(const char *title);
