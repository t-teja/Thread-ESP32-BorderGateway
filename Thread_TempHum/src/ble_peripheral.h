#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include "sensor_nvs.h"

typedef void (*ble_provisioned_cb_t)(const sensor_cfg_t *cfg);

esp_err_t ble_peripheral_start_pairing(ble_provisioned_cb_t cb);
void ble_peripheral_stop(void);
bool ble_peripheral_is_active(void);
