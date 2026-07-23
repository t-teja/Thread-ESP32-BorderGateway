#pragma once
#include <esp_err.h>

/**
 * Thread-side transport for paired sensors: sensors push state/meta/identify
 * events to the hub via CoAP; this module republishes them on the same MQTT
 * topics mqtt_bridge.c would have used if the device published directly.
 * Commands (e.g. identify) are unicast back to a device's last-seen address.
 *
 * CoAP resources served by the hub (port 5683 / OT_DEFAULT_COAP_PORT):
 *   POST /s  telemetry  {"id":..,"type":..,...}      -> home/<room>/<id>/state
 *   POST /m  meta       {"id":..,"fw":..,"product":..} -> home/<room>/<id>/meta
 *   POST /e  event      {"id":..}  (button identify)  -> hub LED + mqtt event
 * Resource served by each sensor:
 *   POST /c  command    {"cmd":"identify"} sent by coap_bridge_send_cmd()
 */

/** Start the CoAP server + resources. Call once OTBR/Thread is ready. */
esp_err_t coap_bridge_init(void);

/** Send a JSON command to a paired device's last-known Thread address. */
void coap_bridge_send_cmd(const char *device_id, const char *cmd_json);

/** Call periodically (e.g. every 10 s) to mark stale devices offline. */
void coap_bridge_check_timeouts(void);
