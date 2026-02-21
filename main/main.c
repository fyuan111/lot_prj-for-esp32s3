#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lot_lcd.h"
#include "lot_lvgl.h"
#include "lot_ota.h"
#include "lot_wifi.h"
#include "launch.h"
#include "time.h"

static const char *TAG = "lot_prj";
enum {
    LOT_PRIO_TIME = 0,
    LOT_PRIO_LCD = 10,
    LOT_PRIO_LVGL = 20,
};


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
    lot_ota_mark_running_app_valid_if_needed();
    lot_ota_log_partition_info();

    LOT_INIT(launch(LOT_PRIO_TIME, lot_time_init));
#if CONFIG_LOT_LCD_ENABLE
    LOT_INIT(launch(LOT_PRIO_LCD, lot_lcd_init));
#if CONFIG_LOT_LVGL_ENABLE
    LOT_INIT(launch(LOT_PRIO_LVGL, lot_lvgl_start));
#endif
#endif
    LOT_INIT(launch_entry);

}

static void app_connect_network(void)
{
    ESP_ERROR_CHECK(lot_wifi_init_and_wait_connected());
}

static void app_try_ota_and_restart(void)
{
#if CONFIG_LOT_OTA_AUTO_CHECK_ON_BOOT
    if (lot_ota_perform_https_ota() == ESP_OK) {
        ESP_LOGI(TAG, "restarting to boot new firmware");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
#else
    ESP_LOGI(TAG, "ota auto-check is disabled");
#endif
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot start");
    app_init_nvs();
    app_start_platform_services();
    app_connect_network();
    app_try_ota_and_restart();
    ESP_LOGI(TAG, "boot sequence done");
}
