#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "touch.h"

typedef enum {
    LOT_SWIPE_NONE = 0,
    LOT_SWIPE_LEFT,
    LOT_SWIPE_RIGHT,
    LOT_SWIPE_UP,
    LOT_SWIPE_DOWN,
} lot_swipe_dir_t;

lot_swipe_dir_t lot_swipe_detect(touch_pos_t down, touch_pos_t up, uint32_t down_ms, uint32_t up_ms,
                                 const lot_touch_config_t *cfg);

bool lot_swipe_is_system_vertical(lot_swipe_dir_t swipe, touch_pos_t down, const lot_touch_config_t *cfg);
