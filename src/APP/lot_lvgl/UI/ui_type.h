

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_ID_HOME = 0,
    UI_ID_MAX
} ui_id_t;

typedef enum {
    UI_REFRESH_NONE = 0,
    UI_REFRESH_PERIODIC,
    UI_REFRESH_EVENT
} ui_refresh_mode_t;

typedef enum {
    UI_SYS_EVT_TOP_PULL_DOWN = 0,
    UI_SYS_EVT_BOTTOM_PULL_UP,
    UI_SYS_EVT_MAX
} ui_sys_event_id_t;

typedef enum {
    UI_APP_EVT_TOUCH = 0,
    UI_APP_EVT_TAP,
    UI_APP_EVT_DOUBLE_TAP,
    UI_APP_EVT_LONG_PRESS,
    UI_APP_EVT_SWIPE_LEFT,
    UI_APP_EVT_SWIPE_RIGHT,
    UI_APP_EVT_TIME_TICK,
    UI_APP_EVT_WIFI_CHANGED,
    UI_APP_EVT_BLE_CHANGED,
    UI_APP_EVT_USER,
    UI_APP_EVT_MAX
} ui_app_event_id_t;

typedef enum {
    UI_EVT_ID_SYS_TOP_PULL_DOWN = UI_SYS_EVT_TOP_PULL_DOWN,
    UI_EVT_ID_SYS_BOTTOM_PULL_UP = UI_SYS_EVT_BOTTOM_PULL_UP,

    UI_EVT_ID_TOUCH = UI_SYS_EVT_MAX + UI_APP_EVT_TOUCH,
    UI_EVT_ID_TAP = UI_SYS_EVT_MAX + UI_APP_EVT_TAP,
    UI_EVT_ID_DOUBLE_TAP = UI_SYS_EVT_MAX + UI_APP_EVT_DOUBLE_TAP,
    UI_EVT_ID_LONG_PRESS = UI_SYS_EVT_MAX + UI_APP_EVT_LONG_PRESS,
    UI_EVT_ID_SWIPE_LEFT = UI_SYS_EVT_MAX + UI_APP_EVT_SWIPE_LEFT,
    UI_EVT_ID_SWIPE_RIGHT = UI_SYS_EVT_MAX + UI_APP_EVT_SWIPE_RIGHT,
    UI_EVT_ID_TIME_TICK = UI_SYS_EVT_MAX + UI_APP_EVT_TIME_TICK,
    UI_EVT_ID_WIFI_CHANGED = UI_SYS_EVT_MAX + UI_APP_EVT_WIFI_CHANGED,
    UI_EVT_ID_BLE_CHANGED = UI_SYS_EVT_MAX + UI_APP_EVT_BLE_CHANGED,
    UI_EVT_ID_USER = UI_SYS_EVT_MAX + UI_APP_EVT_USER,
    UI_EVT_ID_MAX
} ui_event_id_t;

/* Backward-compatible aliases (will be removed after all call sites migrate). */
#define UI_EVT_ID_SWIPE_DOWN UI_EVT_ID_SYS_TOP_PULL_DOWN
#define UI_EVT_ID_SWIPE_UP UI_EVT_ID_SYS_BOTTOM_PULL_UP

#define UI_EVT_BIT(evt_id) (1u << (evt_id))
#define UI_EVT_IS_SYSTEM(evt_id) ((evt_id) < UI_SYS_EVT_MAX)
#define UI_EVT_IS_APP(evt_id) ((evt_id) >= UI_SYS_EVT_MAX && (evt_id) < UI_EVT_ID_MAX)

typedef void (*ui_show_fn_t)(void);
typedef void (*ui_refresh_fn_t)(void);

typedef struct {
    ui_id_t id;
    const char *name;
    ui_show_fn_t show;              /* Static display when entering a page */
    ui_refresh_fn_t refresh;        /* Dynamic refresh after page shown */
    ui_refresh_mode_t refresh_mode;
    uint16_t refresh_hz;            /* Used when refresh_mode == PERIODIC */
    uint32_t event_mask;            /* Used when refresh_mode == EVENT */
} ui_impl_t;
