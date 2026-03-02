#pragma once

#include <stdbool.h>
#include <stdint.h>

void desktop_show(void);
bool desktop_is_active(void);
void desktop_set_active(bool active);
void desktop_handle_events(uint32_t events);
void desktop_refresh(void);
