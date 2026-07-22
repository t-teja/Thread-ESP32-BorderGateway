/**
 * OpenThread Border Router on ESP32-S3 + UART Spinel RCP (ESP32-H2).
 */
#include "otbr_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hub_config.h"
#include "mbedtls/base64.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/link.h"
#include "openthread/thread.h"
#include "openthread/thread_ftd.h"
#include "wifi_net.h"

static const char *TAG = "otbr";
static bool s_ready;
static bool s_started;
static esp_netif_t *s_ot_netif;
static SemaphoreHandle_t s_ready_sem;

static void form_network_if_needed(void)
{
    otInstance *inst = esp_openthread_get_instance();
    otOperationalDataset dataset;
    otError err = otDatasetGetActive(inst, &dataset);
    if (err == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "active dataset present");
        return;
    }
    ESP_LOGI(TAG, "no active dataset — creating new Thread network");
    memset(&dataset, 0, sizeof(dataset));
    dataset.mActiveTimestamp.mSeconds = 1;
    dataset.mActiveTimestamp.mTicks = 0;
    dataset.mActiveTimestamp.mAuthoritative = false;
    dataset.mComponents.mIsActiveTimestampPresent = true;

    otNetworkName name;
    strncpy(name.m8, "ThreadHub", sizeof(name.m8) - 1);
    dataset.mNetworkName = name;
    dataset.mComponents.mIsNetworkNamePresent = true;

    dataset.mChannel = 15;
    dataset.mComponents.mIsChannelPresent = true;

    dataset.mPanId = 0x1234;
    dataset.mComponents.mIsPanIdPresent = true;

    uint8_t extpan[OT_EXT_PAN_ID_SIZE];
    for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) extpan[i] = (uint8_t)(esp_random() & 0xff);
    memcpy(dataset.mExtendedPanId.m8, extpan, sizeof(extpan));
    dataset.mComponents.mIsExtendedPanIdPresent = true;

    uint8_t netkey[OT_NETWORK_KEY_SIZE];
    for (int i = 0; i < OT_NETWORK_KEY_SIZE; i++) netkey[i] = (uint8_t)(esp_random() & 0xff);
    memcpy(dataset.mNetworkKey.m8, netkey, sizeof(netkey));
    dataset.mComponents.mIsNetworkKeyPresent = true;

    uint8_t pskc[OT_PSKC_MAX_SIZE];
    for (int i = 0; i < OT_PSKC_MAX_SIZE; i++) pskc[i] = (uint8_t)(esp_random() & 0xff);
    memcpy(dataset.mPskc.m8, pskc, sizeof(pskc));
    dataset.mComponents.mIsPskcPresent = true;

    ESP_ERROR_CHECK(otDatasetSetActive(inst, &dataset) == OT_ERROR_NONE ? ESP_OK : ESP_FAIL);
}

static void start_thread(void)
{
    otInstance *inst = esp_openthread_get_instance();
    form_network_if_needed();
    ESP_ERROR_CHECK(otIp6SetEnabled(inst, true) == OT_ERROR_NONE ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(otThreadSetEnabled(inst, true) == OT_ERROR_NONE ? ESP_OK : ESP_FAIL);
    s_ready = true;
    if (s_ready_sem) xSemaphoreGive(s_ready_sem);
    ESP_LOGI(TAG, "Thread stack enabled, role will become leader/router");
}

static void ot_task(void *arg)
{
    (void)arg;
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    s_ot_netif = esp_netif_new(&cfg);
    assert(s_ot_netif);

    ESP_ERROR_CHECK(esp_openthread_init(&config));
    ESP_ERROR_CHECK(esp_netif_attach(s_ot_netif, esp_openthread_netif_glue_init(&config)));

    /* Backbone = Wi-Fi STA netif (must already be connected) */
    esp_netif_t *backbone = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (backbone) {
        esp_openthread_set_backbone_netif(backbone);
        esp_err_t br = esp_openthread_border_router_init();
        if (br != ESP_OK) {
            ESP_LOGE(TAG, "border_router_init failed: %s", esp_err_to_name(br));
        } else {
            ESP_LOGI(TAG, "border router init OK (NAT64/routing via Wi-Fi backbone)");
        }
    } else {
        ESP_LOGW(TAG, "no WIFI_STA_DEF — BR without backbone");
    }

    esp_openthread_lock_acquire(portMAX_DELAY);
    start_thread();
    esp_openthread_lock_release();

    s_started = true;
    esp_openthread_launch_mainloop();

    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(s_ot_netif);
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

esp_err_t otbr_net_init(void)
{
#if !HUB_OTBR_ENABLED
    ESP_LOGW(TAG, "OTBR disabled");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_started) return ESP_OK;
    s_ready_sem = xSemaphoreCreateBinary();

    esp_vfs_eventfd_config_t eventfd_config = {.max_fds = 3};
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    xTaskCreate(ot_task, "otbr", 10240, NULL, 5, NULL);

    /* Wait briefly for stack / RCP */
    if (xSemaphoreTake(s_ready_sem, pdMS_TO_TICKS(15000)) != pdTRUE) {
        ESP_LOGE(TAG, "OTBR start timeout — is RCP flashed and UART %d RX=%d TX=%d connected?",
                 HUB_RCP_UART_PORT, HUB_RCP_UART_RX_GPIO, HUB_RCP_UART_TX_GPIO);
        /* keep trying; s_ready may become true later if RCP comes up */
    } else {
        ESP_LOGI(TAG, "OTBR ready");
    }
    return ESP_OK;
#endif
}

bool otbr_net_is_ready(void) { return s_ready; }

bool otbr_net_get_dataset_b64(char *buf, size_t buflen)
{
    if (!buf || buflen < 8 || !s_ready) return false;
    otOperationalDatasetTlvs tlvs;
    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &tlvs);
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE || tlvs.mLength == 0) return false;

    size_t olen = 0;
    int rc = mbedtls_base64_encode((unsigned char *)buf, buflen - 1, &olen, tlvs.mTlvs, tlvs.mLength);
    if (rc != 0) return false;
    buf[olen] = 0;
    return true;
}

const char *otbr_net_status_text(void)
{
    if (!s_ready) return "starting";
    otDeviceRole role = OT_DEVICE_ROLE_DISABLED;
    if (esp_openthread_get_instance()) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        role = otThreadGetDeviceRole(esp_openthread_get_instance());
        esp_openthread_lock_release();
    }
    switch (role) {
    case OT_DEVICE_ROLE_LEADER: return "leader";
    case OT_DEVICE_ROLE_ROUTER: return "router";
    case OT_DEVICE_ROLE_CHILD: return "child";
    case OT_DEVICE_ROLE_DETACHED: return "detached";
    case OT_DEVICE_ROLE_DISABLED: return "disabled";
    default: return "unknown";
    }
}

int otbr_net_neighbor_count(void)
{
    if (!s_ready || !esp_openthread_get_instance()) return 0;
    int n = 0;
    esp_openthread_lock_acquire(portMAX_DELAY);
    otNeighborInfoIterator it = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo info;
    while (otThreadGetNextNeighborInfo(esp_openthread_get_instance(), &it, &info) == OT_ERROR_NONE) {
        n++;
    }
    esp_openthread_lock_release();
    return n;
}

char *otbr_net_info_json(void)
{
    char *out = malloc(256);
    if (!out) return NULL;
    snprintf(out, 256,
             "{\"ready\":%s,\"role\":\"%s\",\"neighbors\":%d,\"rcp_uart\":%d,\"rcp_rx\":%d,\"rcp_tx\":%d}",
             s_ready ? "true" : "false", otbr_net_status_text(), otbr_net_neighbor_count(),
             HUB_RCP_UART_PORT, HUB_RCP_UART_RX_GPIO, HUB_RCP_UART_TX_GPIO);
    return out;
}

