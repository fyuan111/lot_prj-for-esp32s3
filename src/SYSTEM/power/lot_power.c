#include "lot_power.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"

static const char *TAG = "lot_power";
static bool s_power_inited = false;

#if defined(CONFIG_LOT_POWER_AUTO_LIGHT_SLEEP)
#define LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED 1
#else
#define LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED 0
#endif

#ifndef CONFIG_LOT_POWER_MAX_LISTENERS
#define CONFIG_LOT_POWER_MAX_LISTENERS 8
#endif

typedef struct {
    lot_power_listener_cb_t cb;
    void *user_data;
    bool used;
} lot_power_listener_slot_t;

static lot_power_listener_slot_t s_listeners[CONFIG_LOT_POWER_MAX_LISTENERS];

static void lot_power_notify(lot_power_event_t event, uint32_t value_u32)
{
    lot_power_event_data_t evt = {
        .event = event,
        .value_u32 = value_u32,
    };

    for (size_t i = 0; i < CONFIG_LOT_POWER_MAX_LISTENERS; ++i) {
        if (!s_listeners[i].used || s_listeners[i].cb == NULL) {
            continue;
        }
        s_listeners[i].cb(&evt, s_listeners[i].user_data);
    }
}

esp_err_t lot_power_publish_event(lot_power_event_t event, uint32_t value_u32)
{
    lot_power_notify(event, value_u32);
    return ESP_OK;
}

static const char *lot_power_wakeup_cause_str(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        return "UNDEFINED";
    case ESP_SLEEP_WAKEUP_EXT0:
        return "EXT0";
    case ESP_SLEEP_WAKEUP_EXT1:
        return "EXT1";
    case ESP_SLEEP_WAKEUP_TIMER:
        return "TIMER";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return "TOUCHPAD";
    case ESP_SLEEP_WAKEUP_ULP:
        return "ULP";
    case ESP_SLEEP_WAKEUP_GPIO:
        return "GPIO";
    case ESP_SLEEP_WAKEUP_UART:
        return "UART";
    case ESP_SLEEP_WAKEUP_WIFI:
        return "WIFI";
    case ESP_SLEEP_WAKEUP_COCPU:
        return "COCPU";
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
        return "COCPU_TRAP";
    case ESP_SLEEP_WAKEUP_BT:
        return "BT";
    default:
        return "UNKNOWN";
    }
}

uint32_t lot_power_get_default_light_sleep_ms(void)
{
    return CONFIG_LOT_POWER_DEFAULT_LIGHT_SLEEP_MS;
}

esp_err_t lot_power_register_listener(lot_power_listener_cb_t cb, void *user_data)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < CONFIG_LOT_POWER_MAX_LISTENERS; ++i) {
        if (s_listeners[i].used &&
            s_listeners[i].cb == cb &&
            s_listeners[i].user_data == user_data) {
            return ESP_OK;
        }
    }

    for (size_t i = 0; i < CONFIG_LOT_POWER_MAX_LISTENERS; ++i) {
        if (!s_listeners[i].used) {
            s_listeners[i].cb = cb;
            s_listeners[i].user_data = user_data;
            s_listeners[i].used = true;
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "listener table full (%d)", CONFIG_LOT_POWER_MAX_LISTENERS);
    return ESP_ERR_NO_MEM;
}

esp_err_t lot_power_unregister_listener(lot_power_listener_cb_t cb, void *user_data)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < CONFIG_LOT_POWER_MAX_LISTENERS; ++i) {
        if (s_listeners[i].used &&
            s_listeners[i].cb == cb &&
            s_listeners[i].user_data == user_data) {
            s_listeners[i].used = false;
            s_listeners[i].cb = NULL;
            s_listeners[i].user_data = NULL;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t lot_power_configure_pm(void)
{
#if !CONFIG_PM_ENABLE
    ESP_LOGW(TAG, "CONFIG_PM_ENABLE is off; skip esp_pm_configure()");
    return ESP_OK;
#else
    if (CONFIG_LOT_POWER_MIN_CPU_FREQ_MHZ > CONFIG_LOT_POWER_MAX_CPU_FREQ_MHZ) {
        ESP_LOGE(TAG, "invalid PM config: min=%d > max=%d",
                 CONFIG_LOT_POWER_MIN_CPU_FREQ_MHZ, CONFIG_LOT_POWER_MAX_CPU_FREQ_MHZ);
        return ESP_ERR_INVALID_ARG;
    }

    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = CONFIG_LOT_POWER_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_LOT_POWER_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = LOT_POWER_AUTO_LIGHT_SLEEP_ENABLED,
    };

    esp_err_t ret = esp_pm_configure(&pm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_pm_configure failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "pm configured: max=%dMHz min=%dMHz auto_light_sleep=%d",
             pm_cfg.max_freq_mhz, pm_cfg.min_freq_mhz, pm_cfg.light_sleep_enable);
    lot_power_notify(LOT_POWER_EVT_PM_CONFIG_CHANGED, (uint32_t)pm_cfg.max_freq_mhz);
    return ESP_OK;
#endif
}

esp_err_t lot_power_enter_light_sleep_ms(uint32_t sleep_ms)
{
    if (sleep_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lot_power_notify(LOT_POWER_EVT_PREPARE_LIGHT_SLEEP, sleep_ms);

    esp_err_t ret = esp_sleep_enable_timer_wakeup((uint64_t)sleep_ms * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable timer wakeup failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_light_sleep_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "light sleep start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    lot_power_notify(LOT_POWER_EVT_WAKE_FROM_LIGHT_SLEEP,
                     (uint32_t)esp_sleep_get_wakeup_cause());
    return ESP_OK;
}

void lot_power_enter_deep_sleep_ms(uint64_t sleep_ms)
{
    lot_power_notify(LOT_POWER_EVT_PREPARE_DEEP_SLEEP, (uint32_t)sleep_ms);
    if (sleep_ms > 0) {
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleep_ms * 1000ULL));
    }
    esp_deep_sleep_start();
}

esp_err_t lot_power_init(void)
{
    if (s_power_inited) {
        return ESP_OK;
    }

#if CONFIG_LOT_POWER_LOG_BOOT_INFO
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "boot wakeup cause: %s (%d)", lot_power_wakeup_cause_str(cause), (int)cause);
#endif

#if CONFIG_LOT_POWER_CONFIGURE_PM_ON_BOOT
    esp_err_t ret = lot_power_configure_pm();
    if (ret != ESP_OK) {
        return ret;
    }
#endif

    s_power_inited = true;
    lot_power_notify(LOT_POWER_EVT_INIT_DONE, 0);
    return ESP_OK;
}

esp_err_t lot_power_main(void)
{
#if CONFIG_LOT_POWER_ENABLE && CONFIG_LOT_POWER_AUTO_START
    return lot_power_init();
#else
    return ESP_OK;
#endif
}

launch(20, lot_power_main);
