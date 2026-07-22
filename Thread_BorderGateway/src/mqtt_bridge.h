#pragma once
#include <esp_err.h>
#include <stdbool.h>
esp_err_t mqtt_bridge_start(void);
bool mqtt_bridge_is_connected(void);
void mqtt_bridge_publish_registry(void);
void mqtt_bridge_publish_info(void);
void mqtt_bridge_publish_event(const char *json_object);
/** Publish identify command to device topic. */
void mqtt_bridge_publish_device_cmd(const char *room, const char *device_id, const char *cmd_json);
