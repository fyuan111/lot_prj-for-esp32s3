#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t cpu_load_avg_percent;
    uint8_t cpu_load_core_percent[2];
    uint32_t cpu_freq_mhz;
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    const char *chip_model;
} lot_devinfo_snapshot_t;

esp_err_t lot_devinfo_main(void);
esp_err_t lot_devinfo_init(void);
void lot_devinfo_get_snapshot(lot_devinfo_snapshot_t *out);

#ifdef __cplusplus
}
#endif
