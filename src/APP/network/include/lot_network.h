#pragma once

#include "esp_err.h"

esp_err_t lot_network_main(void);
esp_err_t lot_network_start(void);

esp_err_t lot_mqtt_start(void);
esp_err_t lot_websocket_start(void);

