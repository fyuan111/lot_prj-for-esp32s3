#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"

#include "lot_lcd.h"
#include "lot_lvgl.h"
#include "time.h"
#include "lot_launch_autogen.h"

static const char *TAG = "lot_prj";


#define LOT_INIT(INIT_CALL) do {                                   \
    esp_err_t retcode = (INIT_CALL);                               \
    if (retcode != ESP_OK) {                                  \
        ESP_LOGE(TAG, "%s: %s", #INIT_CALL, esp_err_to_name(retcode)); \
    }                                                         \
} while (0)


static void app_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

static void app_start_platform_services(void)
{
    LOT_INIT(lot_run_all_launches());
}

static void app_init_main_watchdog(void)
{
    const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 10000,
        .idle_core_mask = (1U << portNUM_PROCESSORS) - 1U,
        .trigger_panic = true,
    };

    esp_err_t ret = esp_task_wdt_init(&wdt_cfg);
    if (ret == ESP_ERR_INVALID_STATE) {
        ret = esp_task_wdt_reconfigure(&wdt_cfg);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_task_wdt_init/reconfigure failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_task_wdt_add(main) failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "main watchdog enabled, timeout=%lu ms", (unsigned long)wdt_cfg.timeout_ms);
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot start");
    app_init_nvs();
    app_init_main_watchdog();
    app_start_platform_services();
    ESP_LOGI(TAG, "boot sequence done");

    while(1)
    {
        (void)esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
