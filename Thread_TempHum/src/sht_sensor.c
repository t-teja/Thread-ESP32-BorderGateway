/**
 * Minimal SHT4x probe (I2C 0x44). Falls back to simulated values if absent.
 */
#include "sht_sensor.h"

#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sht";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static bool s_have;

esp_err_t sht_sensor_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus, &s_bus) != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus failed — demo mode");
        return ESP_OK;
    }
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x44,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(s_bus, &dev, &s_dev) != ESP_OK) {
        return ESP_OK;
    }
    /* soft reset */
    uint8_t cmd = 0x94;
    if (i2c_master_transmit(s_dev, &cmd, 1, 100) == ESP_OK) {
        s_have = true;
        ESP_LOGI(TAG, "SHT4x present");
    } else {
        ESP_LOGW(TAG, "No SHT4x at 0x44 — publishing demo T/H");
    }
    return ESP_OK;
}

bool sht_sensor_read(float *t_c, float *rh)
{
    if (!s_have) {
        /* gentle demo wander */
        static float t = 22.0f, h = 45.0f;
        t += ((int)(esp_random() % 5) - 2) * 0.05f;
        h += ((int)(esp_random() % 5) - 2) * 0.1f;
        *t_c = t;
        *rh = h;
        return false;
    }
    uint8_t cmd = 0xFD; /* high precision */
    uint8_t raw[6];
    if (i2c_master_transmit(s_dev, &cmd, 1, 100) != ESP_OK) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (i2c_master_receive(s_dev, raw, 6, 100) != ESP_OK) {
        return false;
    }
    uint16_t t_ticks = (raw[0] << 8) | raw[1];
    uint16_t rh_ticks = (raw[3] << 8) | raw[4];
    *t_c = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);
    *rh = -6.0f + 125.0f * ((float)rh_ticks / 65535.0f);
    if (*rh < 0) *rh = 0;
    if (*rh > 100) *rh = 100;
    return true;
}
