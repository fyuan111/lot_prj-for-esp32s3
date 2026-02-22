#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lot_threadpool_task_fn_t)(void *arg);

typedef struct {
    uint32_t worker_count;
    uint32_t queue_length;
    uint32_t stack_size;
    uint32_t task_priority;
    int32_t core_id; /* -1: no pin */
    const char *name;
} lot_threadpool_config_t;

typedef struct lot_threadpool lot_threadpool_t;

/* Default threadpool APIs (no handle passing needed). */
esp_err_t lot_threadpool_start(void);
esp_err_t lot_threadpool_stop(TickType_t wait_ticks);
esp_err_t lot_threadpool_submit(lot_threadpool_task_fn_t task_fn,
                                void *task_arg,
                                TickType_t wait_ticks);
bool lot_threadpool_is_running(void);

/* launch() entry */
esp_err_t lot_threadpool_main(void);

#ifdef __cplusplus
}
#endif
