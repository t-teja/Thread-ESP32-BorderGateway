/**
 * Sensirion SHT30 over I2C (single-shot high repeatability).
 */
#include "sht_sensor.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sht30";
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
        ESP_LOGW(TAG, "I2C bus fail — demo mode");
        return ESP_OK;
    }
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT30_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(s_bus, &dev, &s_dev) != ESP_OK) return ESP_OK;

    /* soft reset 0x30A2 */
    uint8_t rst[2] = {0x30, 0xA2};
    if (i2c_master_transmit(s_dev, rst, 2, 200) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        s_have = true;
        ESP_LOGI(TAG, "SHT30 present at 0x%02x SDA=%d SCL=%d", SHT30_I2C_ADDR,
                 BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);
    } else {
        ESP_LOGW(TAG, "No SHT30 — demo T/H");
    }
    return ESP_OK;
}

bool sht_sensor_read(float *t_c, float *rh)
{
    if (!s_have) {
        static float t = 22.0f, h = 45.0f;
        t += ((int)(esp_random() % 5) - 2) * 0.05f;
        h += ((int)(esp_random() % 5) - 2) * 0.1f;
        *t_c = t; *rh = h;
        return false;
    }
    /* single shot high repeatability clock stretching disabled: 0x2400 */
    uint8_t cmd[2] = {0x24, 0x00};
    uint8_t raw[6];
    if (i2c_master_transmit(s_dev, cmd, 2, 200) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (i2c_master_receive(s_dev, raw, 6, 200) != ESP_OK) return false;
    uint16_t t_raw = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t rh_raw = ((uint16_t)raw[3] << 8) | raw[4];
    *t_c = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    *rh = 100.0f * ((float)rh_raw / 65535.0f);
    if (*rh < 0) *rh = 0;
    if (*rh > 100) *rh = 100;
    return true;
}
