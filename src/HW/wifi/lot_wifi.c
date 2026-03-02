#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#include "lot_wifi.h"
#include "lot_power.h"
#include "lot_ntp.h"
#include "time.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define BACKOFF_INIT_MS    1000
#define BACKOFF_MAX_MS     (CONFIG_LOT_WIFI_BACKOFF_MAX_SEC * 1000)

static const char *TAG = "lot_wifi";
static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif = NULL;

static int s_retry_count = 0;
static uint32_t s_backoff_ms = BACKOFF_INIT_MS;
static esp_timer_handle_t s_reconnect_timer = NULL;
static bool s_ps_target_low_power = false;

static lot_wifi_connected_cb_t s_connected_cb = NULL;

static void lot_wifi_apply_runtime_ps(bool low_power)
{
    wifi_ps_type_t mode = low_power ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE;
    esp_err_t ret = esp_wifi_set_ps(mode);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "wifi ps -> %s", low_power ? "MIN_MODEM" : "NONE");
        return;
    }

    if (ret != ESP_ERR_WIFI_NOT_INIT && ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "set wifi ps failed: %s", esp_err_to_name(ret));
    }
}

static void lot_wifi_power_listener(const lot_power_event_data_t *evt, void *user_data)
{
    (void)user_data;
    if (evt == NULL) {
        return;
    }

    switch (evt->event) {
    case LOT_POWER_EVT_PM_CONFIG_CHANGED:
        s_ps_target_low_power = (evt->value_u32 <= 120U) ? true : false;
        lot_wifi_apply_runtime_ps(s_ps_target_low_power);
        break;

    case LOT_POWER_EVT_PREPARE_LIGHT_SLEEP:
        lot_wifi_apply_runtime_ps(true);
        break;

    case LOT_POWER_EVT_PREPARE_DEEP_SLEEP:
        (void)esp_wifi_disconnect();
        {
            esp_err_t ret = esp_wifi_stop();
            if (ret != ESP_OK &&
                ret != ESP_ERR_WIFI_NOT_INIT &&
                ret != ESP_ERR_WIFI_NOT_STARTED) {
                ESP_LOGW(TAG, "wifi stop before deep sleep failed: %s", esp_err_to_name(ret));
            }
        }
        break;

    default:
        return;
    }
}

/* ── reconnect timer callback ── */

static void reconnect_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "wifi reconnecting (retry #%d, backoff was %lums)...",
             s_retry_count, (unsigned long)s_backoff_ms);
    esp_wifi_connect();
}

/* ── schedule next reconnect with exponential backoff ── */

static void lot_wifi_schedule_reconnect(void)
{
    s_retry_count++;

    /* double backoff, cap at max */
    s_backoff_ms *= 2;
    if (s_backoff_ms > BACKOFF_MAX_MS) {
        s_backoff_ms = BACKOFF_MAX_MS;
    }

    ESP_LOGW(TAG, "wifi disconnected, retry #%d in %lus",
             s_retry_count, (unsigned long)(s_backoff_ms / 1000));

    esp_timer_stop(s_reconnect_timer);
    esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);
}

/* ── DNS override ── */
static void lot_wifi_apply_dns_override(void)
{
#if CONFIG_LOT_WIFI_FORCE_DNS
    if (s_sta_netif == NULL) {
        return;
    }

    uint32_t dns_addr = ipaddr_addr(CONFIG_LOT_WIFI_DNS_SERVER);
    if (dns_addr == IPADDR_NONE || dns_addr == 0) {
        ESP_LOGE(TAG, "invalid dns address: %s", CONFIG_LOT_WIFI_DNS_SERVER);
        return;
    }

    esp_netif_dns_info_t dns = {0};
    dns.ip.u_addr.ip4.addr = dns_addr;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_err_t ret = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "dns override: %s", CONFIG_LOT_WIFI_DNS_SERVER);
    } else {
        ESP_LOGE(TAG, "dns override failed: %s", esp_err_to_name(ret));
    }
#endif
}

/* ── NTP resync ── */

static volatile bool s_ntp_syncing = false;

static void ntp_resync_task(void *arg)
{
    if (s_ntp_syncing) {
        vTaskDelete(NULL);
        return;
    }
    s_ntp_syncing = true;
    if (lot_ntp_sync_once() == ESP_OK) {
        lot_time_sync_from_system_clock();
    }
    s_ntp_syncing = false;
    vTaskDelete(NULL);
}


/* ── event handler ── */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        /* initial connection phase: use FAIL_BIT to unblock the wait */
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        bool initial_phase = !(bits & WIFI_CONNECTED_BIT) && (s_retry_count == 0);

        if (initial_phase && s_backoff_ms == BACKOFF_INIT_MS) {
            /* first ever disconnect: report failure immediately to unblock init */
            s_retry_count++;
            s_backoff_ms *= 2;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);
            ESP_LOGW(TAG, "wifi initial connect failed, retry in %lus",
                     (unsigned long)(s_backoff_ms / 1000));
        } else {
            lot_wifi_schedule_reconnect();
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR " (after %d retries)",
                 IP2STR(&event->ip_info.ip), s_retry_count);

        lot_wifi_apply_dns_override();

        /* reset backoff state */
        s_retry_count = 0;
        s_backoff_ms = BACKOFF_INIT_MS;
        esp_timer_stop(s_reconnect_timer);

        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        xTaskCreate(ntp_resync_task, "ntp_resync", 4096, NULL, 3, NULL);

        if (s_connected_cb) {
            s_connected_cb();
        }
    }
}

/* ── public API ── */
void lot_wifi_register_connected_cb(lot_wifi_connected_cb_t cb)
{
    s_connected_cb = cb;
}

bool lot_wifi_is_connected(void)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

int lot_wifi_get_retry_count(void)
{
    return s_retry_count;
}

esp_err_t lot_wifi_init_and_wait_connected(void)
{
    esp_err_t ret = lot_power_register_listener(lot_wifi_power_listener, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "power listener register failed: %s", esp_err_to_name(ret));
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* create backoff timer */
    esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strlcpy((char *)wifi_cfg.sta.ssid, CONFIG_LOT_WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, CONFIG_LOT_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi connecting to ssid=\"%s\"", CONFIG_LOT_WIFI_SSID);

    /* wait for first connect attempt result */
    xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(CONFIG_LOT_WIFI_CONNECT_TIMEOUT_MS));

    if (lot_wifi_is_connected()) {
        ESP_LOGI(TAG, "wifi connected");
        return ESP_OK;
    }

    /* not connected yet, background reconnect will keep trying */
    ESP_LOGW(TAG, "wifi not connected at boot, background retry active");
    return ESP_OK;
}

launch(5, lot_wifi_init_and_wait_connected);
