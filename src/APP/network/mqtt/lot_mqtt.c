#include "lot_network.h"

#include <inttypes.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#if CONFIG_LOT_NETWORK_ENABLE && CONFIG_LOT_NETWORK_PROTOCOL_MQTT
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "lot_version.h"
#include "cJSON.h"   /* always needed: command handler parses JSON downlink */

#if CONFIG_LOT_MQTT_PAYLOAD_PROTO
#include "lot_proto.h"
#if CONFIG_LOT_MODBUS_ENABLE
#include "lot_modbus.h"
#endif
#endif

#endif

static const char *TAG = "lot_mqtt";

#if CONFIG_LOT_NETWORK_ENABLE && CONFIG_LOT_NETWORK_PROTOCOL_MQTT

static esp_mqtt_client_handle_t s_client = NULL;
static esp_timer_handle_t s_report_timer = NULL;
static uint32_t s_msg_id = 0;

/* ── build & publish status ── */

#if CONFIG_LOT_MQTT_PAYLOAD_PROTO

static void lot_mqtt_publish_status(void)
{
    if (s_client == NULL) {
        return;
    }

    lot_proto_status_t status = {
        .device_id = CONFIG_LOT_MQTT_DEVICE_ID,
        .ts        = 0,   /* TODO: fill from NTP when available */
        .uptime_s  = (uint32_t)(esp_timer_get_time() / 1000000LL),
        .heap_free = esp_get_free_heap_size(),
        .heap_min  = esp_get_minimum_free_heap_size(),
        .rssi      = 0,
        .fw_ver    = lot_version_get(),
    };

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        status.rssi = ap.rssi;
    }

    uint8_t buf[LOT_PROTO_BUF_SIZE];
    size_t  len = 0;

    lot_proto_sensor_t sensors[LOT_PROTO_SENSORS_MAX];
    uint8_t sensor_count = 0;
#if CONFIG_LOT_MODBUS_ENABLE
    lot_modbus_get_snapshot(sensors, LOT_PROTO_SENSORS_MAX, &sensor_count);
#endif

    if (lot_proto_encode_uplink(++s_msg_id, &status, sensors, sensor_count,
                                buf, sizeof(buf), &len) != ESP_OK) {
        return;
    }

    esp_mqtt_client_publish(s_client, CONFIG_LOT_MQTT_PUB_TOPIC,
                            (const char *)buf, (int)len, /*qos*/1, /*retain*/0);
    ESP_LOGD(TAG, "proto publish msg_id=%" PRIu32 " len=%zu", s_msg_id, len);
}

#else  /* JSON path */

static void lot_mqtt_publish_status(void)
{
    if (s_client == NULL) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }

    cJSON_AddNumberToObject(root, "uptime",    (double)(esp_timer_get_time() / 1000000LL));
    cJSON_AddNumberToObject(root, "heap_free", (double)esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "heap_min",  (double)esp_get_minimum_free_heap_size());

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        cJSON_AddNumberToObject(root, "rssi", ap.rssi);
    }

    const char *ver = lot_version_get();
    cJSON_AddStringToObject(root, "version", ver ? ver : "unknown");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL) {
        return;
    }

    esp_mqtt_client_publish(s_client, CONFIG_LOT_MQTT_PUB_TOPIC,
                            json_str, 0, /*qos*/1, /*retain*/0);
    cJSON_free(json_str);
}

#endif /* CONFIG_LOT_MQTT_PAYLOAD_PROTO */

static void lot_mqtt_report_timer_cb(void *arg)
{
    (void)arg;
    lot_mqtt_publish_status();
}

/* ── command dispatcher ── */

static void lot_mqtt_handle_command(const char *data, int data_len)
{
    char *buf = strndup(data, (size_t)data_len);
    if (buf == NULL) {
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (root == NULL) {
        ESP_LOGW(TAG, "cmd: invalid JSON");
        return;
    }

    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsString(cmd) && cmd->valuestring != NULL) 
    {
        ESP_LOGI(TAG, "cmd: %s", cmd->valuestring);
        if (strcmp(cmd->valuestring, "reboot") == 0) {
            cJSON_Delete(root);
            esp_restart();
            return;
        } else if (strcmp(cmd->valuestring, "get_status") == 0) {
            lot_mqtt_publish_status();
        } else {
            ESP_LOGW(TAG, "unknown cmd: %s", cmd->valuestring);
        }
    }
    else
    {
        ESP_LOGW(TAG, "cmd: missing 'cmd' field");
    }

    cJSON_Delete(root);
}

/* ── event handler ── */

static void lot_mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_subscribe(s_client, CONFIG_LOT_MQTT_SUB_TOPIC, /*qos*/1);
        lot_mqtt_publish_status();   /* immediate boot report */
        esp_timer_stop(s_report_timer);   /* idempotent if already stopped */
        esp_timer_start_periodic(s_report_timer,
            (uint64_t)CONFIG_LOT_MQTT_REPORT_INTERVAL_SEC * 1000000ULL);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        esp_timer_stop(s_report_timer);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "rx topic=%.*s", event->topic_len, event->topic);
        lot_mqtt_handle_command(event->data, event->data_len);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "event error");
        break;

    default:
        break;
    }
}

#endif /* CONFIG_LOT_NETWORK_ENABLE && CONFIG_LOT_NETWORK_PROTOCOL_MQTT */

esp_err_t lot_mqtt_start(void)
{
#if !CONFIG_LOT_NETWORK_ENABLE || !CONFIG_LOT_NETWORK_PROTOCOL_MQTT
    return ESP_OK;
#else
    if (s_client != NULL) {
        return ESP_OK;
    }

    esp_timer_create_args_t timer_args = {
        .callback = lot_mqtt_report_timer_cb,
        .name = "mqtt_report",
    };
    esp_timer_create(&timer_args, &s_report_timer);

    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri  = CONFIG_LOT_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_LOT_MQTT_CLIENT_ID,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_mqtt_client_register_event(
        s_client, ESP_EVENT_ANY_ID, lot_mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_mqtt_client_start(s_client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "started uri=%s", CONFIG_LOT_MQTT_BROKER_URI);
    }
    return ret;
#endif
}
