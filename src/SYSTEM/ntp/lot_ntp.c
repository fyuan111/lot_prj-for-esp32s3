#include <stdbool.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"

#include "lot_ntp.h"

static const char *TAG = "lot_ntp";

static bool lot_ntp_time_ready(void)
{
    struct timeval tv = {0};
    if (gettimeofday(&tv, NULL) != 0) {
        return false;
    }
    return (tv.tv_sec >= 1700000000LL);
}

static bool lot_ntp_sta_has_ip(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta == NULL) {
        return false;
    }

    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(sta, &ip_info) != ESP_OK) {
        return false;
    }
    return ip_info.ip.addr != 0;
}

esp_err_t lot_ntp_sync_once(void)
{
#if !CONFIG_LOT_NTP_ENABLE
    return ESP_OK;
#else
    if (!lot_ntp_sta_has_ip()) {
        ESP_LOGW(TAG, "wifi not ready, skip ntp sync");
        return ESP_ERR_INVALID_STATE;
    }

    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_LOT_NTP_SERVER);
    esp_sntp_init();

    const int timeout_ms = CONFIG_LOT_NTP_SYNC_TIMEOUT_SEC * 1000;
    const int step_ms = 250;
    const int max_retry = timeout_ms / step_ms;

    for (int i = 0; i < max_retry; ++i) {
        if (lot_ntp_time_ready()) {
            ESP_LOGI(TAG, "ntp synced from %s", CONFIG_LOT_NTP_SERVER);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }

    ESP_LOGW(TAG, "ntp sync timeout (%d sec), keep local time", CONFIG_LOT_NTP_SYNC_TIMEOUT_SEC);
    return ESP_ERR_TIMEOUT;
#endif
}
