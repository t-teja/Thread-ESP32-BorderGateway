#include "thread_net.h"
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "openthread/dataset.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"

static const char *TAG = "thread";
static bool s_attached;
static bool s_started;
static char s_status[32] = "down";
static esp_netif_t *s_ot_netif;

/*
 * Role changes only get processed once esp_openthread_launch_mainloop() is
 * pumping the stack (radio + timer events), so track attach via this
 * callback instead of polling otThreadGetDeviceRole() before the mainloop
 * has started (that loop would spin for its whole timeout without ever
 * seeing a role change).
 */
static void on_state_changed(otChangedFlags flags, void *ctx)
{
    (void)ctx;
    if (!(flags & OT_CHANGED_THREAD_ROLE)) return;
    otInstance *inst = esp_openthread_get_instance();
    if (!inst) return;
    otDeviceRole role = otThreadGetDeviceRole(inst);
    bool attached = (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER ||
                      role == OT_DEVICE_ROLE_LEADER);
    s_attached = attached;
    strncpy(s_status, attached ? "attached" : "detached", sizeof(s_status) - 1);
    ESP_LOGI(TAG, "Thread role changed -> %d (attached=%d)", (int)role, attached);
}

/* Radio native on H2 */
static const esp_openthread_platform_config_t s_cfg = {
    .radio_config = { .radio_mode = RADIO_MODE_NATIVE },
    .host_config = {
        .host_connection_mode = HOST_CONNECTION_MODE_CLI_USB,
        .host_usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT(),
    },
    .port_config = {
        .storage_partition_name = "nvs",
        .netif_queue_size = 10,
        .task_queue_size = 10,
    },
};

static void ot_task(void *arg)
{
    sensor_cfg_t *cfg = (sensor_cfg_t *)arg;

    esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    s_ot_netif = esp_netif_new(&ncfg);
    ESP_ERROR_CHECK(esp_openthread_init(&s_cfg));
    ESP_ERROR_CHECK(esp_netif_attach(s_ot_netif, esp_openthread_netif_glue_init(&s_cfg)));
    esp_netif_set_default_netif(s_ot_netif);

    /* Decode dataset */
    uint8_t tlvs[256];
    size_t tlv_len = 0;
    int rc = mbedtls_base64_decode(tlvs, sizeof(tlvs), &tlv_len,
                                   (const unsigned char *)cfg->dataset_b64, strlen(cfg->dataset_b64));
    if (rc != 0 || tlv_len == 0) {
        ESP_LOGE(TAG, "dataset b64 decode failed rc=%d", rc);
        strncpy(s_status, "bad_dataset", sizeof(s_status) - 1);
    } else {
        otOperationalDatasetTlvs ds = {0};
        if (tlv_len > sizeof(ds.mTlvs)) tlv_len = sizeof(ds.mTlvs);
        memcpy(ds.mTlvs, tlvs, tlv_len);
        ds.mLength = (uint8_t)tlv_len;

        esp_openthread_lock_acquire(portMAX_DELAY);
        otInstance *inst = esp_openthread_get_instance();
        otSetStateChangedCallback(inst, on_state_changed, NULL);
        otError err = otDatasetSetActiveTlvs(inst, &ds);
        if (err != OT_ERROR_NONE) {
            ESP_LOGE(TAG, "otDatasetSetActiveTlvs %d", (int)err);
            strncpy(s_status, "set_fail", sizeof(s_status) - 1);
        } else {
            otIp6SetEnabled(inst, true);
            otThreadSetEnabled(inst, true);
            strncpy(s_status, "attaching", sizeof(s_status) - 1);
            ESP_LOGI(TAG, "Thread start with provisioned dataset (%u bytes)", (unsigned)tlv_len);
        }
        esp_openthread_lock_release();
    }

    /* Role changes (and thus s_attached) are now reported asynchronously by
     * on_state_changed() once this call starts pumping the OT stack. */
    esp_openthread_launch_mainloop();
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(s_ot_netif);
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

esp_err_t thread_net_start(const sensor_cfg_t *cfg)
{
    if (!cfg || !cfg->dataset_b64[0]) {
        strncpy(s_status, "no_dataset", sizeof(s_status) - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (s_started) return ESP_OK;
    s_started = true;

    static sensor_cfg_t s_cfg_copy;
    s_cfg_copy = *cfg;

    esp_vfs_eventfd_config_t eventfd_config = {.max_fds = 3};
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(ot_task, "ot_sensor", 10240, &s_cfg_copy, 5, NULL);
    return ESP_OK;
}

bool thread_net_is_attached(void) { return s_attached; }
const char *thread_net_status(void) { return s_status; }
