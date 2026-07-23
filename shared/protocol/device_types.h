/**
 * Shared device type identifiers (hub + sensors).
 * Keep in sync across all firmware projects.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define HUB_DEVICE_TYPE_TEMP_HUM   "temp_hum"
#define HUB_DEVICE_TYPE_CONTACT    "contact"
#define HUB_DEVICE_TYPE_MOTION     "motion"
#define HUB_DEVICE_TYPE_LEAK       "leak"
#define HUB_DEVICE_TYPE_BUTTON     "button"
#define HUB_DEVICE_TYPE_UNKNOWN    "unknown"

/** MQTT topic root (override via provision) */
#define HUB_MQTT_TOPIC_BASE_DEFAULT "home"

/** BLE pairing service (128-bit UUID string form for NimBLE) */
#define HUB_BLE_SVC_UUID      "A1B2C3D4-0000-4000-8000-00000000FFF0"
#define HUB_BLE_CHR_INFO_UUID "A1B2C3D4-0000-4000-8000-00000000FFF1"
#define HUB_BLE_CHR_CTRL_UUID "A1B2C3D4-0000-4000-8000-00000000FFF2"
#define HUB_BLE_CHR_STAT_UUID "A1B2C3D4-0000-4000-8000-00000000FFF3"

/** BLE advertised local name prefix — hub filters on this */
#define HUB_BLE_NAME_PREFIX   "THS-"

/** Pairing window default (seconds) */
#define HUB_PAIR_WINDOW_SEC   120

/** Button hold to enter pair mode (ms) */
#define HUB_PAIR_BTN_HOLD_MS  3000

/** Max lengths */
#define HUB_MAX_DEVICE_ID     32
#define HUB_MAX_NAME          48
#define HUB_MAX_ROOM          32
#define HUB_MAX_TYPE          16
#define HUB_MAX_FW            16
#define HUB_MAX_DATASET_B64   512
#define HUB_REGISTRY_MAX      32
/** Text IPv6 address (hub mesh-local EID given to sensors, or a sensor's
 *  reporting address remembered by the hub) — OT_IP6_ADDRESS_STRING_SIZE is 40. */
#define HUB_MAX_ADDR          40

#ifdef __cplusplus
}
#endif
