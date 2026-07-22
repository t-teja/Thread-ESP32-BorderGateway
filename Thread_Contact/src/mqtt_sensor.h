#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include "sensor_nvs.h"

esp_err_t mqtt_sensor_start(const sensor_cfg_t *cfg);
void mqtt_sensor_publish_contact(const char *open_or_closed, int bat, int rssi);
bool mqtt_sensor_is_connected(void);
