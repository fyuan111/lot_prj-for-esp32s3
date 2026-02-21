#include "time.h"

#include "esp_timer.h"

lot_time_atomic_t g_lot_time = LOT_TIME_ATOMIC_INIT;
static esp_timer_handle_t s_lot_time_timer = NULL;

static void lot_time_tick_cb(void *arg)
{
    (void)arg;

    lot_time_t t = lot_time_atomic_load(&g_lot_time);
    t.msec++;
    if (t.msec > LOT_TIME_MSEC_MAX) {
        t.msec = 0;
        t.second++;
        if (t.second > LOT_TIME_SECOND_MAX) {
            t.second = 0;
            t.minute++;
            if (t.minute > LOT_TIME_MINUTE_MAX) {
                t.minute = 0;
                t.hour++;
                if (t.hour > LOT_TIME_HOUR_MAX) {
                    t.hour = 0;
                }
            }
        }
    }
    lot_time_atomic_store(&g_lot_time, t);
}

esp_err_t lot_time_init(void)
{
    if (s_lot_time_timer != NULL) {
        return ESP_OK;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = lot_time_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lot_time_tick",
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_lot_time_timer);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_timer_start_periodic(s_lot_time_timer, 1000); /* 1 ms */
    if (ret != ESP_OK) {
        esp_timer_delete(s_lot_time_timer);
        s_lot_time_timer = NULL;
        return ret;
    }

    return ESP_OK;
}

lot_time_t lot_time(void)
{
    return lot_time_atomic_load(&g_lot_time);
}
