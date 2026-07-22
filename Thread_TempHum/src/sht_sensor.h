#pragma once

#include <esp_err.h>
#include <stdbool.h>

esp_err_t sht_sensor_init(void);
/** Returns true if a real sensor was read; false uses demo values. */
bool sht_sensor_read(float *t_c, float *rh);
