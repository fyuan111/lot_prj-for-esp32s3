#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "touch.h"

typedef struct {
    bool has_last_tap;
    uint32_t last_tap_up_ms;
    touch_pos_t last_tap_pos;
} lot_click_state_t;

void lot_click_reset(lot_click_state_t *st);
bool lot_click_is_tap(touch_pos_t down, touch_pos_t up, uint32_t down_ms, uint32_t up_ms,
                      const lot_touch_config_t *cfg);
bool lot_click_is_double_tap(lot_click_state_t *st, touch_pos_t up, uint32_t up_ms,
                             const lot_touch_config_t *cfg);
