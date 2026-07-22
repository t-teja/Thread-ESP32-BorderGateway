#pragma once

#include "device_types.h"

#define HUB_FW_VERSION           "0.3.0"
#define HUB_HTTP_PORT            80
#define HUB_PAIR_DEFAULT_SEC     HUB_PAIR_WINDOW_SEC

#define HUB_SETUP_AP_PREFIX      "ThreadHub-Setup"
#define HUB_SETUP_AP_PASS        ""
#define HUB_AP_IP_ADDR           "192.168.4.1"
#define HUB_DEFAULT_HOSTNAME     "thread-hub"
#define HUB_DEFAULT_TOPIC_BASE   "home"

/* BOOT button factory reset (active low) */
#define HUB_FACTORY_BTN_GPIO     0
#define HUB_FACTORY_HOLD_MS      5000
#define HUB_FACTORY_BOOT_HOLD_MS 8000

/* Status / identify LED on ESP32-S3 host (active low on many kits; set 1 if active high) */
#define HUB_STATUS_LED_GPIO      48
#define HUB_STATUS_LED_ACTIVE    1

/* Spinel UART to on-board / linked ESP32-H2 RCP
 * IDF ot_br default for discrete modules: host RX=4 TX=5 @ 460800 UART1.
 * On the Espressif Thread BR board the H2 is wired onboard — override here if needed.
 */
#define HUB_RCP_UART_PORT        1
#define HUB_RCP_UART_BAUD        460800
#define HUB_RCP_UART_RX_GPIO     4
#define HUB_RCP_UART_TX_GPIO     5

/* Real OTBR enabled when OpenThread is compiled in */
#define HUB_OTBR_ENABLED         1
