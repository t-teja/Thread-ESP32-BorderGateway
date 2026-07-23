#pragma once

#include "device_types.h"

#define HUB_FW_VERSION           "0.4.0"
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

/* Status / identify LED on ESP32-S3 host */
#define HUB_STATUS_LED_GPIO      48
#define HUB_STATUS_LED_ACTIVE    1

/*
 * Espressif Thread BR / Zigbee GW board (S3 + onboard H2) wiring.
 * Matches esp-thread-br defaults for the dev kit:
 *   host RX <- RCP TX = GPIO17
 *   host TX -> RCP RX = GPIO18
 *   RCP RESET = GPIO7, RCP BOOT = GPIO8
 * Spinel runtime baud 460800 after RCP is running.
 */
#define HUB_RCP_UART_PORT        1
#define HUB_RCP_UART_BAUD        460800
#define HUB_RCP_UART_RX_GPIO     17
#define HUB_RCP_UART_TX_GPIO     18
#define HUB_RCP_RESET_GPIO       7
#define HUB_RCP_BOOT_GPIO        8

#define HUB_OTBR_ENABLED         1
#define HUB_RCP_AUTO_UPDATE      1

/*
 * Sensor <-> hub transport is CoAP over Thread (device pushes state/meta/event,
 * hub pushes commands back). MQTT is spoken only between the hub and the
 * broker — see coap_bridge.c / mqtt_bridge.c.
 */
#define HUB_COAP_PORT               5683 /* OT_DEFAULT_COAP_PORT */
/* Mark a device offline if no CoAP report seen for this long. Must exceed the
 * slowest sensor heartbeat (Thread_Contact idles at 300 s) with margin. */
#define HUB_DEVICE_OFFLINE_TIMEOUT_SEC 600
