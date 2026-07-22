#pragma once

#include <esp_err.h>
#include "sensor_nvs.h"

esp_err_t mqtt_sensor_start(const sensor_cfg_t *cfg);
void mqtt_sensor_publish_state(float t_c, float rh, int bat, int rssi);
bool mqtt_sensor_is_connected(void);
