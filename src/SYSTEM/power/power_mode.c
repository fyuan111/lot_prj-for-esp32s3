#include "power_mode.h"

#include "esp_log.h"

#include "frequency.h"
#include "lot_power.h"

static const char *TAG = "lot_power_mode";

esp_err_t lot_power_apply_mode(lot_power_mode_t mode)
{
    uint32_t target_mhz = 0;

    switch (mode) {
    case LOT_POWER_MODE_POWER_SAVER:
        target_mhz = 80;
        break;

    case LOT_POWER_MODE_BALANCED:
        target_mhz = 160;
        break;

    case LOT_POWER_MODE_PERFORMANCE:
        target_mhz = 240;
        break;

    case LOT_POWER_MODE_STANDBY:
        lot_power_enter_deep_sleep_ms(0);
        return ESP_OK;

    default:
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t freq_ret = freq_change(target_mhz);
    if (freq_ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "freq change not supported (CONFIG_PM_ENABLE off), publish mode event only");
    } else if (freq_ret != ESP_OK) {
        ESP_LOGE(TAG, "freq change failed: %s", esp_err_to_name(freq_ret));
        return freq_ret;
    }

    return lot_power_publish_event(LOT_POWER_EVT_PM_CONFIG_CHANGED, target_mhz);
}
