#include "frequency.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_private/esp_clk.h"

static const char *TAG = "lot_freq";

#if defined(CONFIG_LOT_POWER_AUTO_LIGHT_SLEEP)
#define LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED 1
#else
#define LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED 0
#endif

static bool freq_is_supported(uint32_t mhz)
{
    return (mhz == 40 || mhz == 80 || mhz == 120 || mhz == 160 || mhz == 240);
}

uint32_t freq_get_current_mhz(void)
{
    return (uint32_t)(esp_clk_cpu_freq() / 1000000);
}

esp_err_t freq_startup(void)
{
#if !CONFIG_PM_ENABLE
    ESP_LOGW(TAG, "CONFIG_PM_ENABLE is off; keep fixed CPU frequency");
    return ESP_OK;
#else
    esp_pm_config_t cfg = {
        .max_freq_mhz = CONFIG_LOT_POWER_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_LOT_POWER_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED,
    };

    esp_err_t ret = esp_pm_configure(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "freq startup configure failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "freq startup ok (min=%d max=%d cur=%lu MHz)",
             cfg.min_freq_mhz, cfg.max_freq_mhz, (unsigned long)freq_get_current_mhz());
    return ESP_OK;
#endif
}

esp_err_t freq_change(uint32_t new_freq_mhz)
{
#if !CONFIG_PM_ENABLE
    ESP_LOGW(TAG, "CONFIG_PM_ENABLE is off; freq_change ignored");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!freq_is_supported(new_freq_mhz)) {
        ESP_LOGE(TAG, "unsupported freq: %lu MHz", (unsigned long)new_freq_mhz);
        return ESP_ERR_INVALID_ARG;
    }

    esp_pm_config_t current_cfg = {0};
    esp_err_t ret = esp_pm_get_configuration(&current_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get pm config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_pm_config_t new_cfg = current_cfg;
    new_cfg.max_freq_mhz = (int)new_freq_mhz;
    if (new_cfg.min_freq_mhz > new_cfg.max_freq_mhz) {
        new_cfg.min_freq_mhz = new_cfg.max_freq_mhz;
    }

    ret = esp_pm_configure(&new_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "freq change failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "freq changed: max=%d min=%d cur=%lu MHz",
             new_cfg.max_freq_mhz, new_cfg.min_freq_mhz, (unsigned long)freq_get_current_mhz());
    return ESP_OK;
#endif
}
