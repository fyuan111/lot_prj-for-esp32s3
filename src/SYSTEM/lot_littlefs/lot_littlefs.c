#include "lot_littlefs.h"

#include <inttypes.h>
#include <stdbool.h>

#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "lot_littlefs";
static bool s_mounted;

esp_err_t lot_littlefs_init(void)
{
#if !CONFIG_LOT_LITTLEFS_ENABLE
    return ESP_OK;
#else
    if (s_mounted) {
        return ESP_OK;
    }

    const esp_vfs_littlefs_conf_t conf = {
        .base_path = CONFIG_LOT_LITTLEFS_BASE_PATH,
        .partition_label = CONFIG_LOT_LITTLEFS_PARTITION_LABEL,
        .format_if_mount_failed = CONFIG_LOT_LITTLEFS_FORMAT_IF_FAIL,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "mount failed label=%s path=%s: %s",
                 conf.partition_label, conf.base_path, esp_err_to_name(ret));
        ESP_LOGW(TAG, "try explicit format and remount");

        esp_err_t fmt_ret = esp_littlefs_format(conf.partition_label);
        if (fmt_ret != ESP_OK) {
            ESP_LOGE(TAG, "format failed: %s", esp_err_to_name(fmt_ret));
            return ret;
        }

        ret = esp_vfs_littlefs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "remount after format failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "mounted %s at %s total=%" PRIu32 " used=%" PRIu32,
                 conf.partition_label, conf.base_path, (uint32_t)total, (uint32_t)used);
    } else {
        ESP_LOGW(TAG, "mounted but info failed: %s", esp_err_to_name(ret));
    }

    s_mounted = true;
    return ESP_OK;
#endif
}

esp_err_t lot_littlefs_main(void)
{
    return lot_littlefs_init();
}

launch(10, lot_littlefs_main);
