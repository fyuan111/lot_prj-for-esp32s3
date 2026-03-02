#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "lot_proto.h"

/**
 * Start the Modbus RTU master (called automatically by the launch system).
 * When CONFIG_LOT_MODBUS_ENABLE=n this is a no-op that returns ESP_OK.
 */
esp_err_t lot_modbus_start(void);

/**
 * Copy the latest sensor readings into out[].
 * Thread-safe; safe to call from any task (e.g. MQTT publish).
 *
 * @param out   Destination buffer.
 * @param max   Capacity of out[].
 * @param count Number of entries written.
 */
void lot_modbus_get_snapshot(lot_proto_sensor_t *out, uint8_t max, uint8_t *count);
