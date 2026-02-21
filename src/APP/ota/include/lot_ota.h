#pragma once

#include "esp_err.h"

void lot_ota_mark_running_app_valid_if_needed(void);
void lot_ota_log_partition_info(void);
esp_err_t lot_ota_perform_https_ota(void);
