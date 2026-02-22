

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
    UI_EVT_ID_TOUCH = 0,
    UI_EVT_ID_TIME_TICK,
    UI_EVT_ID_WIFI_CHANGED,
    UI_EVT_ID_BLE_CHANGED,
    UI_EVT_ID_USER,
    UI_EVT_ID_MAX
} ui_event_id_t;

#define UI_EVT_BIT(evt_id) (1u << (evt_id))

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
