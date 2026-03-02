#include "lot_network.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"

#if CONFIG_LOT_NETWORK_ENABLE && CONFIG_LOT_NETWORK_PROTOCOL_WEBSOCKET
#include "esp_websocket_client.h"
#endif

static const char *TAG = "lot_ws";

#if CONFIG_LOT_NETWORK_ENABLE && CONFIG_LOT_NETWORK_PROTOCOL_WEBSOCKET
static esp_websocket_client_handle_t s_ws_client = NULL;

static void lot_ws_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        if (s_ws_client != NULL) {
            const char *msg = "boot";
            esp_websocket_client_send_text(s_ws_client, msg, strlen(msg), 1000 / portTICK_PERIOD_MS);
        }
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "rx len=%d payload_offset=%d payload_len=%d",
                 data->data_len, data->payload_offset, data->payload_len);
        ESP_LOGI(TAG, "rx text=%.*s", data->data_len, (char *)data->data_ptr);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "event error");
        break;
    default:
        ESP_LOGI(TAG, "event id=%" PRId32, event_id);
        break;
    }
}
#endif

esp_err_t lot_websocket_start(void)
{
#if !CONFIG_LOT_NETWORK_ENABLE || !CONFIG_LOT_NETWORK_PROTOCOL_WEBSOCKET
    return ESP_OK;
#else
    if (s_ws_client != NULL) {
        return ESP_OK;
    }

    const esp_websocket_client_config_t cfg = {
        .uri = CONFIG_LOT_WS_URI,
        .reconnect_timeout_ms = CONFIG_LOT_WS_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = CONFIG_LOT_WS_NETWORK_TIMEOUT_MS,
    };

    s_ws_client = esp_websocket_client_init(&cfg);
    if (s_ws_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, lot_ws_event_handler, NULL);

    esp_err_t ret = esp_websocket_client_start(s_ws_client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "started uri=%s", CONFIG_LOT_WS_URI);
    }
    return ret;
#endif
}

