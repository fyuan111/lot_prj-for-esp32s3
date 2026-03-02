#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/* Conservative upper bound for one encoded UplinkMsg. */
#define LOT_PROTO_BUF_SIZE    256
#define LOT_PROTO_SENSORS_MAX   8

/* ------------------------------------------------------------------
 * Caller-facing structs (mirror the .proto fields without pb types)
 * ------------------------------------------------------------------ */

typedef struct {
    uint32_t    device_id;
    int64_t     ts;         /* unix seconds, 0 if NTP not ready */
    uint32_t    uptime_s;
    uint32_t    heap_free;
    uint32_t    heap_min;
    int32_t     rssi;
    const char *fw_ver;     /* may be NULL */
} lot_proto_status_t;

typedef struct {
    uint32_t ch;
    float    value;
    uint32_t alarm;         /* 0=OK  1=high  2=low */
} lot_proto_sensor_t;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/**
 * Encode an UplinkMsg into buf[].
 *
 * @param msg_id       Monotonic counter (dedup / ACK matching).
 * @param status       Device metrics; NULL to omit the sub-message.
 * @param sensors      Array of sensor points; NULL or count=0 to omit.
 * @param sensor_count Elements in sensors[]; capped at LOT_PROTO_SENSORS_MAX.
 * @param buf          Output buffer.
 * @param buf_size     Capacity of buf (>= LOT_PROTO_BUF_SIZE is safe).
 * @param out_len      Written byte count on success.
 * @return ESP_OK on success, ESP_FAIL on encode error.
 */
esp_err_t lot_proto_encode_uplink(uint32_t                  msg_id,
                                  const lot_proto_status_t *status,
                                  const lot_proto_sensor_t *sensors,
                                  uint8_t                   sensor_count,
                                  uint8_t                  *buf,
                                  size_t                    buf_size,
                                  size_t                   *out_len);
