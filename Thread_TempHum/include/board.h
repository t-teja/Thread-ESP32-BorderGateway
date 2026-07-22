#pragma once

#define BOARD_BUTTON_GPIO      9
#define BOARD_LED_GPIO         8
#define BOARD_LED_ACTIVE_LEVEL 0

/* User-proven SHT30 wiring */
#define BOARD_I2C_SDA_GPIO     5
#define BOARD_I2C_SCL_GPIO     4
#define BOARD_I2C_PORT         I2C_NUM_0
#define SHT30_I2C_ADDR         0x44

#define SENSOR_PRODUCT_NAME    "Thread_TempHum"
#define SENSOR_FW_VERSION      "0.3.0"
#define SENSOR_TYPE_STR        "temp_hum"
#define SENSOR_PUBLISH_MS      30000
