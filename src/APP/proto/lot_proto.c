#include "lot_proto.h"

#include <string.h>

#include "esp_log.h"
#include "pb_encode.h"

#include "lot.pb.h"

static const char *TAG = "lot_proto";

esp_err_t lot_proto_encode_uplink(uint32_t                  msg_id,
                                  const lot_proto_status_t *status,
                                  const lot_proto_sensor_t *sensors,
                                  uint8_t                   sensor_count,
                                  uint8_t                  *buf,
                                  size_t                    buf_size,
                                  size_t                   *out_len)
{
    UplinkMsg msg = UplinkMsg_init_zero;
    msg.msg_id = msg_id;

    if (status != NULL) {
        msg.has_status       = true;
        msg.status.device_id = status->device_id;
        msg.status.ts        = status->ts;
        msg.status.uptime_s  = status->uptime_s;
        msg.status.heap_free = status->heap_free;
        msg.status.heap_min  = status->heap_min;
        msg.status.rssi      = status->rssi;
        if (status->fw_ver != NULL) {
            strncpy(msg.status.fw_ver, status->fw_ver,
                    sizeof(msg.status.fw_ver) - 1);
        }
    }

    if (sensors != NULL && sensor_count > 0) {
        uint8_t n = (sensor_count < LOT_PROTO_SENSORS_MAX)
                    ? sensor_count : LOT_PROTO_SENSORS_MAX;
        msg.sensors_count = n;
        for (uint8_t i = 0; i < n; i++) {
            msg.sensors[i].ch    = sensors[i].ch;
            msg.sensors[i].value = sensors[i].value;
            msg.sensors[i].alarm = sensors[i].alarm;
        }
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buf, buf_size);
    if (!pb_encode(&stream, UplinkMsg_fields, &msg)) {
        ESP_LOGE(TAG, "encode failed: %s", PB_GET_ERROR(&stream));
        return ESP_FAIL;
    }

    *out_len = stream.bytes_written;
    return ESP_OK;
}
