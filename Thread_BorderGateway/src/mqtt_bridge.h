#pragma once
#include <esp_err.h>
#include <stdbool.h>
esp_err_t mqtt_bridge_start(void);
bool mqtt_bridge_is_connected(void);
void mqtt_bridge_publish_registry(void);
void mqtt_bridge_publish_info(void);
void mqtt_bridge_publish_event(const char *json_object);
/** Publish a device's telemetry (retained) — payload forwarded as received over CoAP. */
void mqtt_bridge_publish_device_state(const char *room, const char *device_id, const char *json_object);
/** Publish a device's identity/meta (retained). */
void mqtt_bridge_publish_device_meta(const char *room, const char *device_id, const char *json_object);
/** Publish a device's online/offline status (retained, LWT-equivalent). */
void mqtt_bridge_publish_device_status(const char *room, const char *device_id, bool online);
