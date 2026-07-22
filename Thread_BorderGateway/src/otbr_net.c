/**
 * OpenThread Border Router glue.
 * Phase-1: stub dataset so BLE pairing UX can be developed.
 * Enable real OTBR per docs/OTBR_IDF.md and set HUB_OTBR_ENABLED 1.
 */
#include "otbr_net.h"

#include <string.h>

#include "esp_log.h"
#include "hub_config.h"

static const char *TAG = "otbr";
static bool s_ready;

esp_err_t otbr_net_init(void)
{
#if HUB_OTBR_ENABLED
    /* TODO: start esp-thread-br / ot_br, form network, enable NAT64 */
    ESP_LOGW(TAG, "HUB_OTBR_ENABLED set but BR stack not linked yet");
    s_ready = false;
#else
    ESP_LOGW(TAG, "OTBR stub mode — using placeholder dataset for pairing UX");
    s_ready = true;
#endif
    return ESP_OK;
}

bool otbr_net_is_ready(void)
{
    return s_ready;
}

bool otbr_net_get_dataset_b64(char *buf, size_t buflen)
{
    if (!buf || buflen < 8) {
        return false;
    }
#if HUB_OTBR_ENABLED
    /* TODO: otDatasetGetActive TLVs → base64 */
    return false;
#else
    strncpy(buf, HUB_STUB_DATASET_B64, buflen - 1);
    buf[buflen - 1] = '\0';
    return true;
#endif
}

const char *otbr_net_status_text(void)
{
#if HUB_OTBR_ENABLED
    return s_ready ? "otbr_up" : "otbr_down";
#else
    return "stub_no_radio";
#endif
}
