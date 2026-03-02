#include "lot_network.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lot_wifi.h"

static const char *TAG = "lot_network";

esp_err_t lot_network_start(void)
{
#if !CONFIG_LOT_NETWORK_ENABLE
    return ESP_OK;
#else
#if CONFIG_LOT_NETWORK_PROTOCOL_MQTT
    ESP_LOGI(TAG, "start protocol: MQTT");
    return lot_mqtt_start();
#elif CONFIG_LOT_NETWORK_PROTOCOL_WEBSOCKET
    ESP_LOGI(TAG, "start protocol: WebSocket");
    return lot_websocket_start();
#else
    return ESP_OK;
#endif
#endif
}

static void network_start_task(void *arg)
{
    lot_network_start();
    vTaskDelete(NULL);
}

static void on_wifi_connected(void)
{
    xTaskCreate(network_start_task, "net_start", 4096, NULL, 5, NULL);
}

esp_err_t lot_network_main(void)
{
    if (lot_wifi_is_connected()) {
        return lot_network_start();
    }
    lot_wifi_register_connected_cb(on_wifi_connected);
    ESP_LOGI(TAG, "wifi not ready, network will start after connect");
    return ESP_OK;
}

launch(40, lot_network_main);

