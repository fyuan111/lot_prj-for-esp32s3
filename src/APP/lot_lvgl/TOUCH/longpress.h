#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "touch.h"

bool lot_longpress_detect(bool still_pressed, touch_pos_t down, touch_pos_t now,
                          uint32_t down_ms, uint32_t now_ms,
                          const lot_touch_config_t *cfg);
