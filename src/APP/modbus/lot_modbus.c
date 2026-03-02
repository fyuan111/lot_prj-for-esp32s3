#include "lot_modbus.h"

#include <string.h>
#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"

static const char *TAG = "lot_modbus";

#if CONFIG_LOT_MODBUS_ENABLE

/* ── channel table ── */

typedef struct {
    uint8_t  slave_id;
    uint16_t reg_addr;
    float    scale;      /* engineering value = raw * scale */
    uint16_t alarm_hi;   /* raw threshold, 0 = disabled */
    uint16_t alarm_lo;   /* raw threshold, 0 = disabled */
} modbus_ch_t;

static const modbus_ch_t s_channels[] = {
#if CONFIG_LOT_MODBUS_CH_COUNT >= 1
    {
        .slave_id = CONFIG_LOT_MODBUS_CH0_SLAVE_ID,
        .reg_addr = CONFIG_LOT_MODBUS_CH0_REG_ADDR,
        .scale    = CONFIG_LOT_MODBUS_CH0_SCALE_X100 / 100.0f,
        .alarm_hi = CONFIG_LOT_MODBUS_CH0_ALARM_HI,
        .alarm_lo = CONFIG_LOT_MODBUS_CH0_ALARM_LO,
    },
#endif
#if CONFIG_LOT_MODBUS_CH_COUNT >= 2
    {
        .slave_id = CONFIG_LOT_MODBUS_CH1_SLAVE_ID,
        .reg_addr = CONFIG_LOT_MODBUS_CH1_REG_ADDR,
        .scale    = CONFIG_LOT_MODBUS_CH1_SCALE_X100 / 100.0f,
        .alarm_hi = CONFIG_LOT_MODBUS_CH1_ALARM_HI,
        .alarm_lo = CONFIG_LOT_MODBUS_CH1_ALARM_LO,
    },
#endif
};

#define MODBUS_CH_COUNT  (sizeof(s_channels) / sizeof(s_channels[0]))

/* ── snapshot (polled by modbus task, read by MQTT task) ── */

static lot_proto_sensor_t s_snapshot[LOT_PROTO_SENSORS_MAX];
static uint8_t            s_snapshot_count = 0;
static portMUX_TYPE       s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ── CRC-16/IBM (Modbus) ── */

static uint16_t crc16(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xA001u : crc >> 1;
        }
    }
    return crc;
}

/* ── RS485 direction pin ── */

static inline void rs485_tx(bool enable)
{
#if CONFIG_LOT_MODBUS_DE_PIN >= 0
    gpio_set_level(CONFIG_LOT_MODBUS_DE_PIN, enable ? 1 : 0);
#else
    (void)enable;
#endif
}

/* ── FC03: read single holding register ── */

static esp_err_t read_holding_reg(uint8_t slave_id, uint16_t addr, uint16_t *out)
{
    /* build request */
    uint8_t req[8];
    req[0] = slave_id;
    req[1] = 0x03;
    req[2] = (uint8_t)(addr >> 8);
    req[3] = (uint8_t)(addr);
    req[4] = 0x00;
    req[5] = 0x01;          /* quantity = 1 register */
    uint16_t c = crc16(req, 6);
    req[6] = (uint8_t)(c);
    req[7] = (uint8_t)(c >> 8);

    uart_flush_input(CONFIG_LOT_MODBUS_UART_PORT);

    rs485_tx(true);
    uart_write_bytes(CONFIG_LOT_MODBUS_UART_PORT, req, sizeof(req));
    uart_wait_tx_done(CONFIG_LOT_MODBUS_UART_PORT, pdMS_TO_TICKS(50));
    rs485_tx(false);

    /* response: addr(1)+fc(1)+bytes(1)+data(2)+crc(2) = 7 bytes */
    uint8_t resp[7];
    int n = uart_read_bytes(CONFIG_LOT_MODBUS_UART_PORT, resp,
                            sizeof(resp), pdMS_TO_TICKS(300));
    if (n < (int)sizeof(resp)) {
        ESP_LOGW(TAG, "slave=%u reg=0x%04X: timeout (got %d)", slave_id, addr, n);
        return ESP_ERR_TIMEOUT;
    }

    uint16_t recv_crc = (uint16_t)resp[5] | ((uint16_t)resp[6] << 8);
    if (crc16(resp, 5) != recv_crc) {
        ESP_LOGW(TAG, "slave=%u reg=0x%04X: CRC mismatch", slave_id, addr);
        return ESP_ERR_INVALID_CRC;
    }
    if (resp[0] != slave_id || resp[1] != 0x03 || resp[2] != 2) {
        ESP_LOGW(TAG, "slave=%u reg=0x%04X: unexpected frame", slave_id, addr);
        return ESP_FAIL;
    }

    *out = ((uint16_t)resp[3] << 8) | resp[4];
    return ESP_OK;
}

/* ── polling task ── */

static void modbus_poll_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        lot_proto_sensor_t buf[LOT_PROTO_SENSORS_MAX];
        uint8_t count = 0;

        for (uint8_t i = 0; i < MODBUS_CH_COUNT && count < LOT_PROTO_SENSORS_MAX; i++) {
            const modbus_ch_t *ch = &s_channels[i];
            uint16_t raw = 0;

            if (read_holding_reg(ch->slave_id, ch->reg_addr, &raw) != ESP_OK) {
                continue;   /* skip failed channel, try next poll */
            }

            float val = (float)raw * ch->scale;

            uint32_t alarm = 0;
            if (ch->alarm_hi > 0 && raw > ch->alarm_hi) {
                alarm = 1;  /* high */
            } else if (ch->alarm_lo > 0 && raw < ch->alarm_lo) {
                alarm = 2;  /* low */
            }

            buf[count].ch    = i;
            buf[count].value = val;
            buf[count].alarm = alarm;
            ESP_LOGD(TAG, "ch%u raw=%u val=%.2f alarm=%" PRIu32,
                     i, raw, (double)val, alarm);
            count++;
        }

        /* atomic snapshot update */
        taskENTER_CRITICAL(&s_mux);
        memcpy(s_snapshot, buf, sizeof(lot_proto_sensor_t) * count);
        s_snapshot_count = count;
        taskEXIT_CRITICAL(&s_mux);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_LOT_MODBUS_POLL_MS));
    }
}

#endif /* CONFIG_LOT_MODBUS_ENABLE */

/* ── public API ── */

void lot_modbus_get_snapshot(lot_proto_sensor_t *out, uint8_t max, uint8_t *count)
{
#if CONFIG_LOT_MODBUS_ENABLE
    taskENTER_CRITICAL(&s_mux);
    uint8_t n = (s_snapshot_count < max) ? s_snapshot_count : max;
    memcpy(out, s_snapshot, sizeof(lot_proto_sensor_t) * n);
    *count = n;
    taskEXIT_CRITICAL(&s_mux);
#else
    (void)out;
    (void)max;
    *count = 0;
#endif
}

esp_err_t lot_modbus_start(void)
{
#if !CONFIG_LOT_MODBUS_ENABLE
    return ESP_OK;
#else
    const uart_config_t uart_cfg = {
        .baud_rate  = CONFIG_LOT_MODBUS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(CONFIG_LOT_MODBUS_UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_LOT_MODBUS_UART_PORT,
                                 CONFIG_LOT_MODBUS_TX_PIN,
                                 CONFIG_LOT_MODBUS_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_LOT_MODBUS_UART_PORT,
                                        256, 0, 0, NULL, 0));

#if CONFIG_LOT_MODBUS_DE_PIN >= 0
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_LOT_MODBUS_DE_PIN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(CONFIG_LOT_MODBUS_DE_PIN, 0);  /* default RX */
#endif

    xTaskCreate(modbus_poll_task, "modbus_poll", 2560, NULL, 5, NULL);

    ESP_LOGI(TAG, "started uart=%d baud=%d channels=%d poll=%dms",
             CONFIG_LOT_MODBUS_UART_PORT, CONFIG_LOT_MODBUS_BAUD,
             (int)MODBUS_CH_COUNT, CONFIG_LOT_MODBUS_POLL_MS);
    return ESP_OK;
#endif
}

launch(50, lot_modbus_start);
