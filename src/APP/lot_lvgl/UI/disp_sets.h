#pragma once

#include <stddef.h>

#include "ui_type.h"
#include "Homepage.h"

static const ui_impl_t ui_table[] = {
    {
        .id = UI_ID_HOME,
        .name = "home",
        .show = homepage_show,
        .refresh = homepage_refresh,
        .refresh_mode = UI_REFRESH_PERIODIC,
        .refresh_hz = 10,
        .event_mask = 0,
    },
};

static const size_t ui_table_count = sizeof(ui_table) / sizeof(ui_table[0]);
