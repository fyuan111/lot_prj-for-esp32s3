#include "longpress.h"

#include <stddef.h>

static uint32_t distance2(touch_pos_t a, touch_pos_t b)
{
    int32_t dx = (int32_t)a.x - (int32_t)b.x;
    int32_t dy = (int32_t)a.y - (int32_t)b.y;
    return (uint32_t)(dx * dx + dy * dy);
}

bool lot_longpress_detect(bool still_pressed, touch_pos_t down, touch_pos_t now,
                          uint32_t down_ms, uint32_t now_ms,
                          const lot_touch_config_t *cfg)
{
    if (!still_pressed || cfg == NULL || now_ms < down_ms) {
        return false;
    }

    if ((now_ms - down_ms) < cfg->longpress_ms) {
        return false;
    }

    uint32_t d2 = distance2(down, now);
    uint32_t th2 = (uint32_t)cfg->longpress_move_threshold_px * (uint32_t)cfg->longpress_move_threshold_px;
    return d2 <= th2;
}
