#include "freertos/FreeRTOS.h"
#include "esp_log.h"
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

void app_main(void)
{
    ESP_LOGI(TAG, "boot start");
    app_init_nvs();
    app_start_platform_services();
    ESP_LOGI(TAG, "boot sequence done");
}
