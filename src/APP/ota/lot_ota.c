#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "lot_ota.h"

static const char *TAG = "lot_ota";

void lot_ota_mark_running_app_valid_if_needed(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err == ESP_OK && ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "running app is pending verify, mark valid");
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}

void lot_ota_log_partition_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "running partition: %s", running ? running->label : "NULL");
    ESP_LOGI(TAG, "boot partition: %s", boot ? boot->label : "NULL");
}

esp_err_t lot_ota_perform_https_ota(void)
{
    if (strlen(CONFIG_LOT_OTA_URL) == 0) {
        ESP_LOGW(TAG, "CONFIG_LOT_OTA_URL is empty, skip OTA");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "start ota from: %s", CONFIG_LOT_OTA_URL);

    esp_http_client_config_t http_cfg = {
        .url = CONFIG_LOT_OTA_URL,
        .timeout_ms = CONFIG_LOT_OTA_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .bulk_flash_erase = true,
    };

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ota success");
    } else {
        ESP_LOGE(TAG, "ota failed: %s", esp_err_to_name(err));
    }
    return err;
}
