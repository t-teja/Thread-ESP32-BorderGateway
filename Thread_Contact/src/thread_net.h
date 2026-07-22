#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sensor_nvs.h"

esp_err_t thread_net_start(const sensor_cfg_t *cfg);
bool thread_net_is_attached(void);
const char *thread_net_status(void);
