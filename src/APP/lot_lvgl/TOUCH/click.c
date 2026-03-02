#include "click.h"

#include <stddef.h>
#include <stdlib.h>

static uint32_t distance2(touch_pos_t a, touch_pos_t b)
{
    int32_t dx = (int32_t)a.x - (int32_t)b.x;
    int32_t dy = (int32_t)a.y - (int32_t)b.y;
    return (uint32_t)(dx * dx + dy * dy);
}

void lot_click_reset(lot_click_state_t *st)
{
    if (st == NULL) {
        return;
    }
    st->has_last_tap = false;
    st->last_tap_up_ms = 0;
    st->last_tap_pos = (touch_pos_t){0};
}

bool lot_click_is_tap(touch_pos_t down, touch_pos_t up, uint32_t down_ms, uint32_t up_ms,
                      const lot_touch_config_t *cfg)
{
    if (cfg == NULL || up_ms < down_ms) {
        return false;
    }
    if ((up_ms - down_ms) > cfg->tap_max_ms) {
        return false;
    }
    uint32_t d2 = distance2(down, up);
    uint32_t th2 = (uint32_t)cfg->tap_move_threshold_px * (uint32_t)cfg->tap_move_threshold_px;
    return d2 <= th2;
}

bool lot_click_is_double_tap(lot_click_state_t *st, touch_pos_t up, uint32_t up_ms,
                             const lot_touch_config_t *cfg)
{
    if (st == NULL || cfg == NULL || !st->has_last_tap || up_ms < st->last_tap_up_ms) {
        return false;
    }

    uint32_t gap = up_ms - st->last_tap_up_ms;
    if (gap > cfg->double_tap_gap_ms) {
        return false;
    }

    uint32_t d2 = distance2(st->last_tap_pos, up);
    uint32_t th2 = (uint32_t)cfg->double_tap_distance_px * (uint32_t)cfg->double_tap_distance_px;
    return d2 <= th2;
}
