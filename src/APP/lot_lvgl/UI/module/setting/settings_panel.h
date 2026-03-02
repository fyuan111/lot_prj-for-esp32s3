#pragma once

#include "lvgl.h"


void settings_panel_create(lv_obj_t *parent, lv_coord_t screen_w, lv_coord_t screen_h, lv_coord_t top_reserved_h);
void settings_panel_open(void);
void settings_panel_close(void);
lv_obj_t *settings_panel_get_subpage(void);
lv_obj_t *settings_panel_get_subpage_body(void);
void settings_panel_set_subpage_content(const char *content);
