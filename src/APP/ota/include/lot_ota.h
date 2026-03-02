#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

/**
 * OTA progress callback.
 * @param percent  0-100 progress percentage
 * @param received bytes received so far
 * @param total    total firmware size (0 if unknown)
 */
typedef void (*lot_ota_progress_cb_t)(int percent, int received, int total);

/**
 * Self-check after OTA boot. Validates WiFi + heap health.
 * If pending verify: pass → mark valid; fail → rollback and reboot.
 */
void lot_ota_selfcheck_and_validate(void);

void lot_ota_log_partition_info(void);

/**
 * Perform OTA with progress callback.
 * @param progress_cb  optional callback, NULL to skip progress reporting
 */
esp_err_t lot_ota_perform_upgrade(lot_ota_progress_cb_t progress_cb);

esp_err_t lot_ota_check_update(bool *has_update, char *latest_version, size_t latest_version_size);
esp_err_t lot_ota_main(void);
