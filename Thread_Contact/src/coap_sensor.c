/**
 * Phase-1 stub, mirrors thread_net.c: Thread_Contact does not run a real
 * OpenThread stack yet (CONFIG_OPENTHREAD_ENABLED=n), so this only logs what
 * would be sent to the hub over CoAP. Once thread_net.c gains a real
 * OpenThread attach (see its TODO), replace the body with otCoap* calls the
 * same way Thread_TempHum/src/coap_sensor.c does.
 */
#include "coap_sensor.h"
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "esp_log.h"

static const char *TAG = "coap_s";
static sensor_cfg_t s_cfg;

esp_err_t coap_sensor_start(const sensor_cfg_t *cfg)
{
    if (!cfg || !cfg->hub_addr[0]) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    ESP_LOGW(TAG, "OT stub — would CoAP to hub %s as %s", cfg->hub_addr, cfg->device_id);
    return ESP_OK;
}

void coap_sensor_publish_contact(const char *open_or_closed, int bat, int rssi)
{
    ESP_LOGI(TAG, "(stub) contact=%s bat=%d rssi=%d id=%s", open_or_closed, bat, rssi,
             s_cfg.device_id);
}

bool coap_sensor_is_connected(void) { return s_cfg.hub_addr[0] != 0; }
