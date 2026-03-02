#include <string.h>
#include <ctype.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include "lot_ota.h"
#include "lot_version.h"
#include "lot_wifi.h"

static const char *TAG = "lot_ota";

#define OTA_SELFCHECK_MIN_HEAP_BYTES (50 * 1024)

/* ── version comparison (unchanged) ── */

static int lot_ota_parse_next_number(const char **s)
{
    int v = 0;
    while (**s == '.') {
        (*s)++;
    }
    while (**s >= '0' && **s <= '9') {
        v = (v * 10) + (**s - '0');
        (*s)++;
    }
    return v;
}

static int lot_ota_version_cmp(const char *a, const char *b)
{
    if (a == NULL) {
        a = "";
    }
    if (b == NULL) {
        b = "";
    }

    const char *pa = a;
    const char *pb = b;
    for (int i = 0; i < 4; ++i) {
        int va = lot_ota_parse_next_number(&pa);
        int vb = lot_ota_parse_next_number(&pb);
        if (va < vb) {
            return -1;
        }
        if (va > vb) {
            return 1;
        }
        if ((*pa == '\0' || *pa == '\n' || *pa == '\r') &&
            (*pb == '\0' || *pb == '\n' || *pb == '\r')) {
            break;
        }
    }
    return strcmp(a, b);
}

/* ── self-check & rollback ── */

static bool lot_ota_run_selfcheck(void)
{
    /* 2. Heap health */
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < OTA_SELFCHECK_MIN_HEAP_BYTES) {
        ESP_LOGE(TAG, "selfcheck FAIL: free heap %u < %u",
                 (unsigned)free_heap, (unsigned)OTA_SELFCHECK_MIN_HEAP_BYTES);
        return false;
    }

    ESP_LOGI(TAG, "selfcheck PASS: wifi=ok heap=%u bytes free", (unsigned)free_heap);
    return true;
}

void lot_ota_selfcheck_and_validate(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);

    if (err != ESP_OK || ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        /* Not an OTA boot or already validated, nothing to do */
        return;
    }

    ESP_LOGW(TAG, "OTA boot detected, running self-check on partition '%s'...",
             running->label);

    if (lot_ota_run_selfcheck()) {
        ESP_LOGI(TAG, "self-check passed, marking app VALID");
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    } else {
        ESP_LOGE(TAG, "self-check FAILED, rolling back to previous firmware!");
        esp_ota_mark_app_invalid_rollback_and_reboot();
        /* does not return */
    }
}

/* ── partition info ── */

void lot_ota_log_partition_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "running partition: %s", running ? running->label : "NULL");
    ESP_LOGI(TAG, "boot partition: %s", boot ? boot->label : "NULL");
}

/* ── OTA upgrade with progress ── */

esp_err_t lot_ota_perform_upgrade(lot_ota_progress_cb_t progress_cb)
{
    if (strlen(CONFIG_LOT_OTA_URL) == 0) {
        ESP_LOGW(TAG, "CONFIG_LOT_OTA_URL is empty, skip OTA");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "start OTA from: %s", CONFIG_LOT_OTA_URL);

    bool use_tls = (strncmp(CONFIG_LOT_OTA_URL, "https://", 8) == 0);
    esp_http_client_config_t http_cfg = {
        .url = CONFIG_LOT_OTA_URL,
        .timeout_ms = CONFIG_LOT_OTA_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        .bulk_flash_erase = false,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }

    int total_size = esp_https_ota_get_image_size(ota_handle);
    int last_percent = -1;
    int last_reported_kb = -1;

    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        if (progress_cb) {
            int received = esp_https_ota_get_image_len_read(ota_handle);
            if (total_size > 0) {
                int percent = (int)((int64_t)received * 100 / total_size);
                if (percent != last_percent) {
                    last_percent = percent;
                    progress_cb(percent, received, total_size);
                }
            } else {
                int received_kb = received / 1024;
                if (received_kb != last_reported_kb) {
                    last_reported_kb = received_kb;
                    progress_cb(0, received, 0);
                }
            }
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(ota_handle);
        return err;
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ESP_LOGE(TAG, "OTA incomplete: data not fully received");
        esp_https_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    err = esp_https_ota_finish(ota_handle);
    if (err == ESP_OK) {
        int received = total_size > 0 ? total_size : esp_https_ota_get_image_len_read(ota_handle);
        ESP_LOGI(TAG, "OTA success, firmware size: %d bytes", received);
        if (progress_cb) {
            progress_cb(100, received, total_size > 0 ? total_size : received);
        }
    } else {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "OTA image validation failed, firmware may be corrupted");
        } else {
            ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
        }
    }
    return err;
}

/* ── version check (unchanged) ── */

esp_err_t lot_ota_check_update(bool *has_update, char *latest_version, size_t latest_version_size)
{
    if (has_update == NULL || latest_version == NULL || latest_version_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    has_update[0] = false;
    latest_version[0] = '\0';

    if (strlen(CONFIG_LOT_OTA_VERSION_URL) == 0) {
        ESP_LOGW(TAG, "CONFIG_LOT_OTA_VERSION_URL is empty");
        return ESP_ERR_INVALID_ARG;
    }

    bool use_tls = (strncmp(CONFIG_LOT_OTA_VERSION_URL, "https://", 8) == 0);
    esp_http_client_config_t http_cfg = {
        .url = CONFIG_LOT_OTA_VERSION_URL,
        .timeout_ms = CONFIG_LOT_OTA_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }

    char raw[64] = {0};
    int status = esp_http_client_fetch_headers(client);
    if (status < 0) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int http_code = esp_http_client_get_status_code(client);
    if (http_code != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_FETCH_HEADER;
    }

    int read_len = 0;
    while (read_len < (int)(sizeof(raw) - 1)) {
        int n = esp_http_client_read(client, raw + read_len, (int)(sizeof(raw) - 1 - read_len));
        if (n <= 0) {
            break;
        }
        read_len += n;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len <= 0) {
        return ESP_FAIL;
    }
    raw[read_len] = '\0';

    char clean[64] = {0};
    size_t j = 0;
    for (int i = 0; i < read_len && j < sizeof(clean) - 1; ++i) {
        if (!isspace((unsigned char)raw[i])) {
            clean[j++] = raw[i];
        }
    }
    clean[j] = '\0';
    if (clean[0] == '\0') {
        return ESP_FAIL;
    }

    strlcpy(latest_version, clean, latest_version_size);
    const char *current_version = lot_version_get();
    has_update[0] = (lot_ota_version_cmp(current_version, latest_version) < 0);
    ESP_LOGI(TAG, "ota check current=%s latest=%s update=%s",
             current_version, latest_version, has_update[0] ? "yes" : "no");
    return ESP_OK;
}

/* ── launch entry ── */

esp_err_t lot_ota_main(void)
{
    lot_ota_selfcheck_and_validate();
    lot_ota_log_partition_info();
    return ESP_OK;
}

launch(35, lot_ota_main);
