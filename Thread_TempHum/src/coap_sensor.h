#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include "sensor_nvs.h"

/** Start CoAP client/server toward the hub (cfg->hub_addr). Thread must be up. */
esp_err_t coap_sensor_start(const sensor_cfg_t *cfg);

/** Push a temp/humidity reading to the hub. */
void coap_sensor_publish_state(float t_c, float rh, int bat, int rssi);

/** True if the last report to the hub was acknowledged. */
bool coap_sensor_is_connected(void);

/** Called when the hub sends {"cmd":"identify"} to this device. */
void coap_sensor_set_identify_cb(void (*cb)(void));

/** Report a local button-press identify event to the hub. */
void coap_sensor_publish_identify(void);
