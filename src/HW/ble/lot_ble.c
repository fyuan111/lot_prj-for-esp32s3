#include "lot_ble.h"

#include <string.h>

#include "esp_log.h"

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#include "esp_nimble_hci.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char *TAG = "lot_ble";

#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
static bool s_ble_started;
static uint8_t s_own_addr_type = BLE_OWN_ADDR_PUBLIC;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_echo_val_handle = 0;
static uint8_t s_echo_buf[128];
static uint16_t s_echo_len;
static int lot_ble_gap_event(struct ble_gap_event *event, void *arg);
static int lot_ble_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);

static const ble_uuid128_t s_lot_svc_uuid =
    BLE_UUID128_INIT(0xd6, 0x39, 0xa5, 0x6b, 0x12, 0x93, 0x4d, 0x22,
                     0x8a, 0x63, 0x5b, 0xc7, 0x90, 0x00, 0x10, 0x01);
static const ble_uuid128_t s_lot_chr_uuid =
    BLE_UUID128_INIT(0xd6, 0x39, 0xa5, 0x6b, 0x12, 0x93, 0x4d, 0x22,
                     0x8a, 0x63, 0x5b, 0xc7, 0x90, 0x00, 0x10, 0x02);

static const struct ble_gatt_svc_def s_lot_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_lot_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]) {
                {
                    .uuid = &s_lot_chr_uuid.u,
                    .access_cb = lot_ble_gatt_access_cb,
                    .flags = BLE_GATT_CHR_F_READ |
                             BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &s_echo_val_handle,
                },
                {0},
            },
    },
    {0},
};

static int lot_ble_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (os_mbuf_append(ctxt->om, s_echo_buf, s_echo_len) != 0) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > sizeof(s_echo_buf)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, s_echo_buf, sizeof(s_echo_buf), &out_len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        s_echo_len = out_len;
        ESP_LOGI(TAG, "BLE RX %u bytes", (unsigned)s_echo_len);

        if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE && s_echo_val_handle != 0) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(s_echo_buf, s_echo_len);
            if (om != NULL) {
                rc = ble_gatts_notify_custom(s_conn_handle, s_echo_val_handle, om);
                if (rc != 0) {
                    ESP_LOGW(TAG, "notify failed rc=%d", rc);
                }
            }
        }
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static void lot_ble_start_advertising(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (const uint8_t *)CONFIG_LOT_BLE_DEVICE_NAME;
    fields.name_len = strlen(CONFIG_LOT_BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, lot_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "advertising as '%s'", CONFIG_LOT_BLE_DEVICE_NAME);
    }
}

static int lot_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "phone connected, conn_handle=%d", event->connect.conn_handle);
        } else {
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGW(TAG, "connect failed status=%d, restart adv", event->connect.status);
            lot_ble_start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "phone disconnected reason=%d, restart adv", event->disconnect.reason);
        lot_ble_start_advertising();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "adv complete reason=%d, restart adv", event->adv_complete.reason);
        lot_ble_start_advertising();
        return 0;

    default:
        return 0;
    }
}

static void lot_ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto rc=%d", rc);
        return;
    }

    rc = ble_svc_gap_device_name_set(CONFIG_LOT_BLE_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set rc=%d", rc);
    }

    lot_ble_start_advertising();
}

static void lot_ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#endif

esp_err_t lot_ble_init(void)
{
#if !CONFIG_LOT_BLE_ENABLE
    return ESP_OK;
#else
#if !(CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED)
    ESP_LOGE(TAG, "Enable CONFIG_BT_ENABLED and CONFIG_BT_NIMBLE_ENABLED in menuconfig");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_ble_started) {
        return ESP_OK;
    }

    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "release classic bt mem failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_lot_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg rc=%d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(s_lot_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs rc=%d", rc);
        return ESP_FAIL;
    }

    ble_hs_cfg.sync_cb = lot_ble_on_sync;

    nimble_port_freertos_init(lot_ble_host_task);
    s_ble_started = true;
    ESP_LOGI(TAG, "BLE init done");
    return ESP_OK;
#endif
#endif
}

esp_err_t lot_ble_main(void)
{
    return lot_ble_init();
}

launch(30, lot_ble_main);
