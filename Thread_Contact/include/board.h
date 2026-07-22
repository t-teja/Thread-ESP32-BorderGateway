#pragma once

#define BOARD_BUTTON_GPIO      9
#define BOARD_LED_GPIO         8
#define BOARD_LED_ACTIVE_LEVEL 0

/** Reed switch / magnetic contact — closed = GPIO low with pull-up */
#define CONTACT_GPIO           4
#define CONTACT_CLOSED_LEVEL   0

#define SENSOR_PRODUCT_NAME    "Thread_Contact"
#define SENSOR_FW_VERSION      "0.1.0"
#define SENSOR_TYPE_STR        "contact"

/** Heartbeat when idle (ms); contact changes publish immediately */
#define SENSOR_HEARTBEAT_MS    300000
