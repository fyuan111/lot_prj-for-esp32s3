#include "swipe.h"

#include <stdlib.h>

lot_swipe_dir_t lot_swipe_detect(touch_pos_t down, touch_pos_t up, uint32_t down_ms, uint32_t up_ms,
                                 const lot_touch_config_t *cfg)
{
    if (cfg == NULL || up_ms < down_ms) {
        return LOT_SWIPE_NONE;
    }

    uint32_t dt = up_ms - down_ms;
    if (dt > cfg->swipe_max_ms) {
        return LOT_SWIPE_NONE;
    }

    int32_t dx = (int32_t)up.x - (int32_t)down.x;
    int32_t dy = (int32_t)up.y - (int32_t)down.y;
    int32_t adx = abs(dx);
    int32_t ady = abs(dy);
    int32_t threshold = (int32_t)cfg->swipe_min_distance_px;

    if (adx < threshold && ady < threshold) {
        return LOT_SWIPE_NONE;
    }

    if (adx >= ady) {
        return (dx >= 0) ? LOT_SWIPE_RIGHT : LOT_SWIPE_LEFT;
    }
    return (dy >= 0) ? LOT_SWIPE_DOWN : LOT_SWIPE_UP;
}

bool lot_swipe_is_system_vertical(lot_swipe_dir_t swipe, touch_pos_t down, const lot_touch_config_t *cfg)
{
    if (cfg == NULL || cfg->viewport_height_px == 0 || cfg->sys_pull_edge_px == 0) {
        return false;
    }

    uint16_t edge = cfg->sys_pull_edge_px;
    uint16_t y_min = cfg->active_y_min_px;
    uint16_t y_max = cfg->active_y_max_px;
    if (y_max == 0 || y_max < y_min) {
        y_max = (cfg->viewport_height_px > 0) ? (cfg->viewport_height_px - 1) : 0;
    }

    if (swipe == LOT_SWIPE_DOWN) {
        uint16_t top_end = y_min + edge;
        return down.y <= top_end;
    }
    if (swipe == LOT_SWIPE_UP) {
        uint16_t bottom_start = (y_max > edge) ? (y_max - edge) : y_min;
        return down.y >= bottom_start;
    }

    return false;
}
