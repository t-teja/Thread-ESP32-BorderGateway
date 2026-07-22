/**
 * OpenThread attach using provisioned active dataset (base64 TLVs).
 * Phase-1 stub: logs dataset and reports "attached" after delay so MQTT path
 * can be tested on Wi-Fi-capable alternative builds. On real H2, enable
 * CONFIG_OPENTHREAD_ENABLED and replace body with otInstance dataset set + start.
 */
#include "thread_net.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "thread";
static bool s_attached;
static char s_status[32] = "down";

esp_err_t thread_net_start(const sensor_cfg_t *cfg)
{
    if (!cfg || !cfg->dataset_b64[0]) {
        strncpy(s_status, "no_dataset", sizeof(s_status) - 1);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "dataset b64 len=%d (OT stub — enable OpenThread for real attach)",
             (int)strlen(cfg->dataset_b64));
    /* TODO: base64 decode → otDatasetSetActiveTlvs → otIp6SetEnabled → otThreadSetEnabled */
    strncpy(s_status, "stub_attached", sizeof(s_status) - 1);
    s_attached = true;
    ESP_LOGW(TAG, "Thread stub attached — link MQTT via NAT64 when OTBR is live");
    return ESP_OK;
}

bool thread_net_is_attached(void) { return s_attached; }

const char *thread_net_status(void) { return s_status; }
