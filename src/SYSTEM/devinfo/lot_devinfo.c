#include <string.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_clk_tree.h"
#include "esp_chip_info.h"
#include "esp_freertos_hooks.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/clk_tree_defs.h"

#include "lot_devinfo.h"

static const char *TAG = "lot_devinfo";

static bool s_inited = false;

#if CONFIG_LOT_DEVINFO_IDLE_LOAD
static _Atomic uint32_t s_idle_tick_count[portNUM_PROCESSORS];
static uint32_t s_prev_idle_tick_count[portNUM_PROCESSORS];
static TickType_t s_prev_tick = 0;
static uint8_t s_cpu_load_core[portNUM_PROCESSORS];
static uint8_t s_cpu_load_avg = 0;

static bool lot_devinfo_idle_hook_cpu0(void)
{
    atomic_fetch_add(&s_idle_tick_count[0], 1);
    return true;
}

#if portNUM_PROCESSORS > 1
static bool lot_devinfo_idle_hook_cpu1(void)
{
    atomic_fetch_add(&s_idle_tick_count[1], 1);
    return true;
}
#endif

static void lot_devinfo_update_cpu_load(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - s_prev_tick;
    if (elapsed == 0) {
        return;
    }

    uint32_t sum = 0;
    for (int i = 0; i < portNUM_PROCESSORS; ++i) {
        uint32_t idle_now = atomic_load(&s_idle_tick_count[i]);
        uint32_t idle_delta = idle_now - s_prev_idle_tick_count[i];
        s_prev_idle_tick_count[i] = idle_now;

        if (idle_delta > (uint32_t)elapsed) {
            idle_delta = (uint32_t)elapsed;
        }

        uint32_t busy = 100U - ((idle_delta * 100U) / (uint32_t)elapsed);
        if (busy > 100U) {
            busy = 100U;
        }
        s_cpu_load_core[i] = (uint8_t)busy;
        sum += busy;
    }

    s_cpu_load_avg = (uint8_t)(sum / portNUM_PROCESSORS);
    s_prev_tick = now;
}

static void lot_devinfo_register_idle_hooks(void)
{
    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(lot_devinfo_idle_hook_cpu0, 0));
#if portNUM_PROCESSORS > 1
    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(lot_devinfo_idle_hook_cpu1, 1));
#endif
}
#endif

static const char *lot_devinfo_chip_model_str(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32:
        return "ESP32";
    case CHIP_ESP32S2:
        return "ESP32-S2";
    case CHIP_ESP32S3:
        return "ESP32-S3";
    case CHIP_ESP32C3:
        return "ESP32-C3";
    case CHIP_ESP32C2:
        return "ESP32-C2";
    case CHIP_ESP32C6:
        return "ESP32-C6";
    case CHIP_ESP32H2:
        return "ESP32-H2";
#ifdef CHIP_ESP32P4
    case CHIP_ESP32P4:
        return "ESP32-P4";
#endif
#ifdef CHIP_ESP32C5
    case CHIP_ESP32C5:
        return "ESP32-C5";
#endif
    default:
        return "UNKNOWN";
    }
}

esp_err_t lot_devinfo_init(void)
{
#if !CONFIG_LOT_DEVINFO_ENABLE
    return ESP_OK;
#else
    if (s_inited) {
        return ESP_OK;
    }

#if CONFIG_LOT_DEVINFO_IDLE_LOAD
    memset(s_prev_idle_tick_count, 0, sizeof(s_prev_idle_tick_count));
    memset(s_cpu_load_core, 0, sizeof(s_cpu_load_core));
    s_cpu_load_avg = 0;
    s_prev_tick = xTaskGetTickCount();
    lot_devinfo_register_idle_hooks();
#endif

    s_inited = true;
    ESP_LOGI(TAG, "devinfo init done");
    return ESP_OK;
#endif
}

void lot_devinfo_get_snapshot(lot_devinfo_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);
    out->chip_model = lot_devinfo_chip_model_str(chip.model);

    uint32_t cpu_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU,
                                     ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                     &cpu_hz) == ESP_OK) {
        out->cpu_freq_mhz = cpu_hz / 1000000U;
    }

    out->free_heap_bytes = esp_get_free_heap_size();
    out->min_free_heap_bytes = esp_get_minimum_free_heap_size();

#if CONFIG_LOT_DEVINFO_IDLE_LOAD
    lot_devinfo_update_cpu_load();
    out->cpu_load_avg_percent = s_cpu_load_avg;
    for (int i = 0; i < portNUM_PROCESSORS && i < 2; ++i) {
        out->cpu_load_core_percent[i] = s_cpu_load_core[i];
    }
#endif
}

esp_err_t lot_devinfo_main(void)
{
    return lot_devinfo_init();
}

launch(1, lot_devinfo_main);
