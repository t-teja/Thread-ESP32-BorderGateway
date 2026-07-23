#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include "sensor_nvs.h"

/** Start CoAP client toward the hub (cfg->hub_addr). Thread must be up. */
esp_err_t coap_sensor_start(const sensor_cfg_t *cfg);

/** Push a contact reading ("open"/"closed") to the hub. */
void coap_sensor_publish_contact(const char *open_or_closed, int bat, int rssi);

/** True if the last report to the hub was acknowledged. */
bool coap_sensor_is_connected(void);
