#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui_type.h"

#define UI_SYSTEM_EVENT_MASK_DEFAULT \
    (UI_EVT_BIT(UI_EVT_ID_SYS_TOP_PULL_DOWN) | UI_EVT_BIT(UI_EVT_ID_SYS_BOTTOM_PULL_UP))

typedef void (*ui_system_status_cb_t)(const char *text);
typedef void (*ui_system_home_cb_t)(void);

void ui_system_bind_quick_panel(lv_obj_t *panel,
                                lv_obj_t *pull_zone,
                                int16_t screen_h,
                                ui_system_status_cb_t status_cb);
void ui_system_bind_home(ui_system_home_cb_t home_cb);
bool ui_system_handle_events(const char *tag, uint32_t events);
