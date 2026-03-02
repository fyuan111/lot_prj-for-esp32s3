#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef void (*lot_wifi_connected_cb_t)(void);

esp_err_t lot_wifi_init_and_wait_connected(void);
bool lot_wifi_is_connected(void);
int lot_wifi_get_retry_count(void);
void lot_wifi_register_connected_cb(lot_wifi_connected_cb_t cb);
