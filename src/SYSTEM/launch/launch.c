#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "launch.h"
#include "esp_err.h"
#include "esp_log.h"

typedef struct launch_set {
    uint8_t launch_priority;
    bool launched;
    uint16_t reserve;
} launch_set_t;

typedef struct launch_node {
    launch_set_t config;
    init_func_t func;
} launch_node_t;


static launch_node_t s_launch_table[CONFIG_LOT_LAUNCH_MAX_APPS];
static size_t s_launch_count = 0;
static const char *TAG = "launch";

static int launch_cmp_priority(const void *lhs, const void *rhs)
{
    const launch_node_t *l = (const launch_node_t *)lhs;
    const launch_node_t *r = (const launch_node_t *)rhs;
    return (int)l->config.launch_priority - (int)r->config.launch_priority;
}

esp_err_t launch_entry(void)
{
    if (s_launch_count == 0) {
        return ESP_OK;
    }

    qsort(s_launch_table, s_launch_count, sizeof(launch_node_t), launch_cmp_priority);
    esp_err_t first_err = ESP_OK;
    for (size_t i = 0; i < s_launch_count; i++) {
        launch_node_t *node = &s_launch_table[i];
        esp_err_t ret = node->func();
        node->config.launched = (ret == ESP_OK);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "init failed at idx=%u priority=%u: %s",
                     (unsigned)i, (unsigned)node->config.launch_priority, esp_err_to_name(ret));
            if (first_err == ESP_OK) {
                first_err = ret;
            }
        }
    }
    s_launch_count = 0;
    return first_err;
}

esp_err_t launch(uint8_t priority, init_func_t func)
{
    if (func == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_launch_count >= CONFIG_LOT_LAUNCH_MAX_APPS) {
        return ESP_ERR_NO_MEM;
    }

    launch_set_t config = {
        .launched = false,
        .launch_priority = priority,
    };
    s_launch_table[s_launch_count].config = config;
    s_launch_table[s_launch_count].func = func;
    s_launch_count++;
    return ESP_OK;
}
