#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOT_POWER_EVT_INIT_DONE = 0,
    LOT_POWER_EVT_PM_CONFIG_CHANGED,
    LOT_POWER_EVT_PREPARE_LIGHT_SLEEP,
    LOT_POWER_EVT_WAKE_FROM_LIGHT_SLEEP,
    LOT_POWER_EVT_PREPARE_DEEP_SLEEP,
} lot_power_event_t;

typedef struct {
    lot_power_event_t event;
    uint32_t value_u32;
} lot_power_event_data_t;

typedef void (*lot_power_listener_cb_t)(const lot_power_event_data_t *event, void *user_data);

esp_err_t lot_power_main(void);
esp_err_t lot_power_init(void);
esp_err_t lot_power_configure_pm(void);
esp_err_t lot_power_enter_light_sleep_ms(uint32_t sleep_ms);
void lot_power_enter_deep_sleep_ms(uint64_t sleep_ms);
uint32_t lot_power_get_default_light_sleep_ms(void);
esp_err_t lot_power_register_listener(lot_power_listener_cb_t cb, void *user_data);
esp_err_t lot_power_unregister_listener(lot_power_listener_cb_t cb, void *user_data);
esp_err_t lot_power_publish_event(lot_power_event_t event, uint32_t value_u32);

#ifdef __cplusplus
}
#endif
