#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"

#include "lot_lcd.h"


static const char *TAG = "lot_lcd";

#define LOT_LCD_HOST SPI2_HOST
#define LOT_LCD_BK_PWM_MODE LEDC_LOW_SPEED_MODE
#define LOT_LCD_BK_PWM_TIMER LEDC_TIMER_0
#define LOT_LCD_BK_PWM_CHANNEL LEDC_CHANNEL_0
#define LOT_LCD_BK_PWM_DUTY_RES LEDC_TIMER_10_BIT
#define LOT_LCD_BK_PWM_MAX_DUTY 1023U
#define LOT_LCD_BK_PWM_FREQ_HZ 5000U

static esp_lcd_panel_io_handle_t s_io = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static bool s_backlight_pwm_ready = false;
#if CONFIG_LOT_TOUCH_ENABLE
static esp_lcd_touch_handle_t s_touch = NULL;
static TaskHandle_t s_touch_task_hdl = NULL;
#endif

static uint32_t lcd_backlight_percent_to_duty(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    uint32_t duty = (LOT_LCD_BK_PWM_MAX_DUTY * percent) / 100;
    if (!CONFIG_LOT_LCD_BK_LIGHT_ON_LEVEL) {
        duty = LOT_LCD_BK_PWM_MAX_DUTY - duty;
    }
    return duty;
}

static esp_err_t lcd_backlight_pwm_init(void)
{
    if (CONFIG_LOT_LCD_BK_LIGHT_PIN < 0) {
        return ESP_OK;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LOT_LCD_BK_PWM_MODE,
        .timer_num = LOT_LCD_BK_PWM_TIMER,
        .duty_resolution = LOT_LCD_BK_PWM_DUTY_RES,
        .freq_hz = LOT_LCD_BK_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t channel_cfg = {
        .gpio_num = CONFIG_LOT_LCD_BK_LIGHT_PIN,
        .speed_mode = LOT_LCD_BK_PWM_MODE,
        .channel = LOT_LCD_BK_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LOT_LCD_BK_PWM_TIMER,
        .duty = lcd_backlight_percent_to_duty(0),
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    };
    err = ledc_channel_config(&channel_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_backlight_pwm_ready = true;
    return ESP_OK;
}

esp_err_t lot_lcd_set_backlight_percent(uint8_t percent)
{
    if (CONFIG_LOT_LCD_BK_LIGHT_PIN < 0) {
        return ESP_OK;
    }
    if (!s_backlight_pwm_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t duty = lcd_backlight_percent_to_duty(percent);
    esp_err_t err = ledc_set_duty(LOT_LCD_BK_PWM_MODE, LOT_LCD_BK_PWM_CHANNEL, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: %s", esp_err_to_name(err));
        return err;
    }

    err = ledc_update_duty(LOT_LCD_BK_PWM_MODE, LOT_LCD_BK_PWM_CHANNEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

#if CONFIG_LOT_TOUCH_ENABLE
static spi_host_device_t touch_spi_host(void)
{
#if CONFIG_LOT_TOUCH_USE_SEPARATE_SPI
#if CONFIG_LOT_TOUCH_SPI_HOST_SPI2
    return SPI2_HOST;
#else
    return SPI3_HOST;
#endif
#else
    return LOT_LCD_HOST;
#endif
}
#endif

static esp_err_t lcd_fill_color(uint16_t color)
{
    const size_t chunk_lines = 20;
    const size_t pixel_count = CONFIG_LOT_LCD_H_RES * chunk_lines;
    uint16_t *line_buf = (uint16_t *)heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (line_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < pixel_count; i++) {
        line_buf[i] = color;
    }

    for (int y = 0; y < CONFIG_LOT_LCD_V_RES; y += chunk_lines) {
        int lines = CONFIG_LOT_LCD_V_RES - y;
        if (lines > (int)chunk_lines) {
            lines = chunk_lines;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, 0, y, CONFIG_LOT_LCD_H_RES, y + lines, line_buf));
    }

    free(line_buf);
    return ESP_OK;
}

#if CONFIG_LOT_TOUCH_ENABLE
#if CONFIG_LOT_TOUCH_INT_PIN >= 0
static void touch_interrupt_callback(esp_lcd_touch_handle_t tp)
{
    (void)tp;
    BaseType_t high_task_wakeup = pdFALSE;
    if (s_touch_task_hdl != NULL) {
        vTaskNotifyGiveFromISR(s_touch_task_hdl, &high_task_wakeup);
    }
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
#endif

#if !CONFIG_LOT_LVGL_ENABLE
static void touch_log_task(void *arg)
{
    uint32_t last_print_ms = 0;
    bool last_pressed = false;

    while (1) {
#if CONFIG_LOT_TOUCH_INT_PIN >= 0
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
#else
        vTaskDelay(pdMS_TO_TICKS(20));
#endif

        esp_lcd_touch_point_data_t point_data[1] = {0};
        uint8_t point_cnt = 0;

        esp_lcd_touch_read_data(s_touch);
        ESP_ERROR_CHECK(esp_lcd_touch_get_data(s_touch, point_data, &point_cnt, 1));

        bool pressed = point_cnt > 0;
        if (pressed) {
            uint32_t now = esp_log_timestamp();
            if (!last_pressed || (now - last_print_ms) >= CONFIG_LOT_TOUCH_LOG_INTERVAL_MS) {
                ESP_LOGI(TAG, "touch x=%u y=%u", point_data[0].x, point_data[0].y);
                last_print_ms = now;
            }
        }
        last_pressed = pressed;
    }
}
#endif
#endif

esp_err_t lot_lcd_init(void)
{
    esp_err_t err;
    bool lcd_swap_xy = false;
    bool lcd_mirror_x = false;
    bool lcd_mirror_y = false;

#if CONFIG_LOT_LCD_SWAP_XY
    lcd_swap_xy = true;
#endif
#if CONFIG_LOT_LCD_MIRROR_X
    lcd_mirror_x = true;
#endif
#if CONFIG_LOT_LCD_MIRROR_Y
    lcd_mirror_y = true;
#endif

    ESP_ERROR_CHECK(lcd_backlight_pwm_init());
    ESP_ERROR_CHECK(lot_lcd_set_backlight_percent(0));

    spi_bus_config_t buscfg = {
        .sclk_io_num = CONFIG_LOT_LCD_SPI_SCLK_PIN,
        .mosi_io_num = CONFIG_LOT_LCD_SPI_MOSI_PIN,
        .miso_io_num = CONFIG_LOT_LCD_SPI_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CONFIG_LOT_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    err = spi_bus_initialize(LOT_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = CONFIG_LOT_LCD_DC_PIN,
        .cs_gpio_num = CONFIG_LOT_LCD_CS_PIN,
        .pclk_hz = CONFIG_LOT_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LOT_LCD_HOST, &io_config, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = CONFIG_LOT_LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_ili9341(s_io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_ili9341 failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, lcd_swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, lcd_mirror_x, lcd_mirror_y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    ESP_ERROR_CHECK(lcd_fill_color(0x001F));

    ESP_ERROR_CHECK(lot_lcd_set_backlight_percent(100));
    ESP_LOGI(TAG, "ILI9341 ready (%dx%d)", CONFIG_LOT_LCD_H_RES, CONFIG_LOT_LCD_V_RES);

#if CONFIG_LOT_TOUCH_ENABLE
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(CONFIG_LOT_TOUCH_CS_PIN);
    spi_host_device_t tp_host = touch_spi_host();
    bool touch_swap_xy = false;
    bool touch_mirror_x = false;
    bool touch_mirror_y = false;

#if CONFIG_LOT_TOUCH_SWAP_XY
    touch_swap_xy = true;
#endif
#if CONFIG_LOT_TOUCH_MIRROR_X
    touch_mirror_x = true;
#endif
#if CONFIG_LOT_TOUCH_MIRROR_Y
    touch_mirror_y = true;
#endif

#if CONFIG_LOT_TOUCH_USE_SEPARATE_SPI
    if (tp_host == LOT_LCD_HOST) {
        ESP_LOGE(TAG, "touch separate SPI host cannot be same as LCD host");
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_config_t touch_buscfg = {
        .sclk_io_num = CONFIG_LOT_TOUCH_SPI_SCLK_PIN,
        .mosi_io_num = CONFIG_LOT_TOUCH_SPI_MOSI_PIN,
        .miso_io_num = CONFIG_LOT_TOUCH_SPI_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    err = spi_bus_initialize(tp_host, &touch_buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "touch spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }
#endif

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)tp_host, &tp_io_config, &tp_io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch io init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LOT_LCD_H_RES,
        .y_max = CONFIG_LOT_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = CONFIG_LOT_TOUCH_INT_PIN,
        .levels = {
            .interrupt = CONFIG_LOT_TOUCH_INT_ACTIVE_LEVEL,
        },
        .flags = {
            .swap_xy = touch_swap_xy,
            .mirror_x = touch_mirror_x,
            .mirror_y = touch_mirror_y,
        },
    };
    err = esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &s_touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_touch_new_spi_xpt2046 failed: %s", esp_err_to_name(err));
        return err;
    }

#if CONFIG_LOT_TOUCH_INT_PIN >= 0
    err = esp_lcd_touch_register_interrupt_callback(s_touch, touch_interrupt_callback);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch interrupt register failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "XPT2046 touch ready (irq gpio=%d, host=%d)", CONFIG_LOT_TOUCH_INT_PIN, (int)tp_host);
#else
    ESP_LOGI(TAG, "XPT2046 touch ready (polling mode, host=%d)", (int)tp_host);
#endif

#if !CONFIG_LOT_LVGL_ENABLE
    BaseType_t ok = xTaskCreate(touch_log_task, "touch_log", 3072, NULL, 4, &s_touch_task_hdl);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }
#endif
#endif

    return ESP_OK;
}

esp_err_t lot_lcd_main(void)
{
#if CONFIG_LOT_LCD_ENABLE
    return lot_lcd_init();
#else
    return ESP_OK;
#endif
}

esp_lcd_panel_io_handle_t lot_lcd_get_io_handle(void)
{
    return s_io;
}

esp_lcd_panel_handle_t lot_lcd_get_panel_handle(void)
{
    return s_panel;
}

esp_lcd_touch_handle_t lot_lcd_get_touch_handle(void)
{
#if CONFIG_LOT_TOUCH_ENABLE
    return s_touch;
#else
    return NULL;
#endif
}

launch(0, lot_lcd_main);
