#pragma once

/**
 * Pin map — adjust for your ESP32-H2 module / custom PCB.
 * Defaults suit many H2 devkits (BOOT button = GPIO9).
 */
#define BOARD_BUTTON_GPIO     9
#define BOARD_LED_GPIO        8
#define BOARD_LED_ACTIVE_LEVEL 0

/** I2C for SHT4x / SHTC3 (change if needed) */
#define BOARD_I2C_SDA_GPIO    2
#define BOARD_I2C_SCL_GPIO    3
#define BOARD_I2C_PORT        I2C_NUM_0

#define SENSOR_PRODUCT_NAME   "Thread_TempHum"
#define SENSOR_FW_VERSION     "0.1.0"
#define SENSOR_TYPE_STR       "temp_hum"

/** Telemetry period when attached (ms) */
#define SENSOR_PUBLISH_MS     30000
