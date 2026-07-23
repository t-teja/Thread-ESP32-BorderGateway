#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include "sensor_nvs.h"

esp_err_t thread_net_start(const sensor_cfg_t *cfg);
bool thread_net_is_attached(void);
const char *thread_net_status(void);

/**
 * Stop Thread protocol operation (role/IP6 disabled) without tearing down
 * the OT task, so the 802.15.4 radio goes quiet and the shared 2.4GHz radio
 * is free for a reliable BLE pairing exchange. Call before re-entering BLE
 * pairing mode on an already-provisioned sensor.
 */
void thread_net_pause(void);
