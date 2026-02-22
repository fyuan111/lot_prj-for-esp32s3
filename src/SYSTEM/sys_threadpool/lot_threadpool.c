#include "lot_threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"

typedef struct {
    lot_threadpool_task_fn_t fn;
    void *arg;
} lot_threadpool_work_t;

struct lot_threadpool {
    QueueHandle_t queue;
    TaskHandle_t *workers;
    uint32_t worker_count;
    bool running;
    char name[24];
};

static const char *TAG = "lot_tpool";
static lot_threadpool_t *s_default_pool;

static void lot_threadpool_worker(void *arg)
{
    lot_threadpool_t *pool = (lot_threadpool_t *)arg;
    lot_threadpool_work_t work;

    while (xQueueReceive(pool->queue, &work, portMAX_DELAY) == pdTRUE) {
        if (work.fn == NULL) {
            break;
        }
        work.fn(work.arg);
    }

    vTaskDelete(NULL);
}

static esp_err_t lot_threadpool_start_workers(lot_threadpool_t *pool,
                                              const lot_threadpool_config_t *cfg)
{
    BaseType_t ok;

    for (uint32_t i = 0; i < cfg->worker_count; i++) {
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "tp_%02u", (unsigned)i);

        if (cfg->core_id >= 0) {
            ok = xTaskCreatePinnedToCore(lot_threadpool_worker,
                                         task_name,
                                         cfg->stack_size,
                                         pool,
                                         cfg->task_priority,
                                         &pool->workers[i],
                                         cfg->core_id);
        } else {
            ok = xTaskCreate(lot_threadpool_worker,
                             task_name,
                             cfg->stack_size,
                             pool,
                             cfg->task_priority,
                             &pool->workers[i]);
        }

        if (ok != pdPASS) {
            ESP_LOGE(TAG, "create worker %u failed", (unsigned)i);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t lot_threadpool_create(const lot_threadpool_config_t *cfg, lot_threadpool_t **out_pool)
{
    if (cfg == NULL || out_pool == NULL || cfg->worker_count == 0 || cfg->queue_length == 0 || cfg->stack_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lot_threadpool_t *pool = (lot_threadpool_t *)calloc(1, sizeof(lot_threadpool_t));
    if (pool == NULL) {
        return ESP_ERR_NO_MEM;
    }

    pool->workers = (TaskHandle_t *)calloc(cfg->worker_count, sizeof(TaskHandle_t));
    if (pool->workers == NULL) {
        free(pool);
        return ESP_ERR_NO_MEM;
    }

    pool->queue = xQueueCreate(cfg->queue_length, sizeof(lot_threadpool_work_t));
    if (pool->queue == NULL) {
        free(pool->workers);
        free(pool);
        return ESP_ERR_NO_MEM;
    }

    pool->worker_count = cfg->worker_count;
    snprintf(pool->name, sizeof(pool->name), "%s", (cfg->name != NULL) ? cfg->name : "lot_tpool");

    esp_err_t ret = lot_threadpool_start_workers(pool, cfg);
    if (ret != ESP_OK) {
        vQueueDelete(pool->queue);
        free(pool->workers);
        free(pool);
        return ret;
    }

    pool->running = true;
    *out_pool = pool;
    ESP_LOGI(TAG, "threadpool started: workers=%u queue=%u", (unsigned)cfg->worker_count, (unsigned)cfg->queue_length);
    return ESP_OK;
}

static esp_err_t lot_threadpool_destroy(lot_threadpool_t *pool, TickType_t wait_ticks)
{
    if (pool == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!pool->running) {
        return ESP_OK;
    }

    lot_threadpool_work_t stop_work = {0};
    for (uint32_t i = 0; i < pool->worker_count; i++) {
        if (xQueueSend(pool->queue, &stop_work, wait_ticks) != pdTRUE) {
            ESP_LOGW(TAG, "stop signal enqueue timeout");
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    for (uint32_t i = 0; i < pool->worker_count; i++) {
        if (pool->workers[i] != NULL) {
            eTaskState st = eTaskGetState(pool->workers[i]);
            if (st != eDeleted) {
                vTaskDelete(pool->workers[i]);
            }
        }
    }

    vQueueDelete(pool->queue);
    free(pool->workers);
    pool->running = false;
    free(pool);
    return ESP_OK;
}

static esp_err_t lot_threadpool_submit_to_pool(lot_threadpool_t *pool,
                                               lot_threadpool_task_fn_t task_fn,
                                               void *task_arg,
                                               TickType_t wait_ticks)
{
    if (pool == NULL || task_fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!pool->running) {
        return ESP_ERR_INVALID_STATE;
    }

    lot_threadpool_work_t work = {
        .fn = task_fn,
        .arg = task_arg,
    };

    if (xQueueSend(pool->queue, &work, wait_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t lot_threadpool_start(void)
{
    if (s_default_pool != NULL) {
        return ESP_OK;
    }

    const lot_threadpool_config_t cfg = {
        .worker_count = CONFIG_LOT_THREADPOOL_DEFAULT_WORKERS,
        .queue_length = CONFIG_LOT_THREADPOOL_DEFAULT_QUEUE_LEN,
        .stack_size = CONFIG_LOT_THREADPOOL_DEFAULT_STACK_SIZE,
        .task_priority = CONFIG_LOT_THREADPOOL_DEFAULT_TASK_PRIORITY,
        .core_id = CONFIG_LOT_THREADPOOL_DEFAULT_CORE_ID,
        .name = "lot_tp",
    };

    return lot_threadpool_create(&cfg, &s_default_pool);
}

esp_err_t lot_threadpool_stop(TickType_t wait_ticks)
{
    if (s_default_pool == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = lot_threadpool_destroy(s_default_pool, wait_ticks);
    s_default_pool = NULL;
    return ret;
}

esp_err_t lot_threadpool_submit(lot_threadpool_task_fn_t task_fn,
                                void *task_arg,
                                TickType_t wait_ticks)
{
    if (task_fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_default_pool == NULL || !s_default_pool->running) {
        return ESP_ERR_INVALID_STATE;
    }
    return lot_threadpool_submit_to_pool(s_default_pool, task_fn, task_arg, wait_ticks);
}

bool lot_threadpool_is_running(void)
{
    return (s_default_pool != NULL) && s_default_pool->running;
}

esp_err_t lot_threadpool_main(void)
{
#if CONFIG_LOT_THREADPOOL_ENABLE && CONFIG_LOT_THREADPOOL_AUTO_START
    return lot_threadpool_start();
#else
    return ESP_OK;
#endif
}

launch(3, lot_threadpool_main);
