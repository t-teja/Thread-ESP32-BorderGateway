/**
 * BLE peripheral in pairing mode: ADV name THS-<type>-XXXX, GATT 0xFFF0/FFF2 write.
 */
#include "ble_peripheral.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "cJSON.h"
#include "device_types.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_periph";
static bool s_active, s_host_ready;
static ble_provisioned_cb_t s_cb;
static uint16_t s_conn;
static char s_adv_name[28];

static int gatt_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0xFFF0),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(0xFFF1),
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0xFFF2),
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0xFFF3),
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                },
                {0},
            },
    },
    {0},
};

static void apply_provision_json(const char *json, uint16_t len)
{
    char buf[1024];
    if (len >= sizeof(buf)) {
        return;
    }
    memcpy(buf, json, len);
    buf[len] = 0;
    ESP_LOGI(TAG, "ctrl write: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return;
    }
    const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "cmd"));
    if (!cmd || strcmp(cmd, "provision") != 0) {
        cJSON_Delete(root);
        return;
    }
    sensor_cfg_t cfg = {0};
    cfg.provisioned = true;
    cfg.mqtt_port = 1883;
    const cJSON *j;
    j = cJSON_GetObjectItem(root, "device_id");
    if (cJSON_IsString(j)) strncpy(cfg.device_id, j->valuestring, sizeof(cfg.device_id) - 1);
    j = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(j)) strncpy(cfg.name, j->valuestring, sizeof(cfg.name) - 1);
    j = cJSON_GetObjectItem(root, "room");
    if (cJSON_IsString(j)) strncpy(cfg.room, j->valuestring, sizeof(cfg.room) - 1);
    j = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(j)) strncpy(cfg.type, j->valuestring, sizeof(cfg.type) - 1);
    else strncpy(cfg.type, SENSOR_TYPE_STR, sizeof(cfg.type) - 1);
    j = cJSON_GetObjectItem(root, "mqtt_host");
    if (cJSON_IsString(j)) strncpy(cfg.mqtt_host, j->valuestring, sizeof(cfg.mqtt_host) - 1);
    j = cJSON_GetObjectItem(root, "mqtt_user");
    if (cJSON_IsString(j)) strncpy(cfg.mqtt_user, j->valuestring, sizeof(cfg.mqtt_user) - 1);
    j = cJSON_GetObjectItem(root, "mqtt_pass");
    if (cJSON_IsString(j)) strncpy(cfg.mqtt_pass, j->valuestring, sizeof(cfg.mqtt_pass) - 1);
    j = cJSON_GetObjectItem(root, "topic_base");
    if (cJSON_IsString(j)) strncpy(cfg.topic_base, j->valuestring, sizeof(cfg.topic_base) - 1);
    j = cJSON_GetObjectItem(root, "dataset");
    if (cJSON_IsString(j)) strncpy(cfg.dataset_b64, j->valuestring, sizeof(cfg.dataset_b64) - 1);
    j = cJSON_GetObjectItem(root, "mqtt_port");
    if (cJSON_IsNumber(j)) cfg.mqtt_port = j->valueint;
    cJSON_Delete(root);

    sensor_nvs_save(&cfg);
    if (s_cb) {
        s_cb(&cfg);
    }
}

static int gatt_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char info[160];
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BT);
        snprintf(info, sizeof(info),
                 "{\"mac\":\"%02X%02X%02X%02X%02X%02X\",\"type\":\"%s\",\"fw\":\"%s\",\"product\":\"%s\"}",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], SENSOR_TYPE_STR, SENSOR_FW_VERSION,
                 SENSOR_PRODUCT_NAME);
        os_mbuf_append(ctxt->om, info, strlen(info));
        return 0;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        char *tmp = malloc(om_len + 1);
        if (!tmp) return BLE_ATT_ERR_INSUFFICIENT_RES;
        ble_hs_mbuf_to_flat(ctxt->om, tmp, om_len, NULL);
        tmp[om_len] = 0;
        apply_provision_json(tmp, om_len);
        free(tmp);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void start_advertise(void)
{
    struct ble_gap_adv_params ap = {0};
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_adv_name;
    fields.name_len = strlen(s_adv_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);
    ap.conn_mode = BLE_GAP_CONN_MODE_UND;
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &ap, NULL, NULL);
    ESP_LOGI(TAG, "advertising as %s", s_adv_name);
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    s_host_ready = true;
    if (s_active) {
        start_advertise();
    }
}

static void host_task(void *p)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static bool s_inited;

static void ensure_host(void)
{
    if (s_inited) return;
    s_inited = true;
    nimble_port_init();
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(s_svcs);
    ble_gatts_add_svcs(s_svcs);
    nimble_port_freertos_init(host_task);
}

esp_err_t ble_peripheral_start_pairing(ble_provisioned_cb_t cb)
{
    s_cb = cb;
    s_active = true;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_adv_name, sizeof(s_adv_name), "%s%s-%02X%02X", HUB_BLE_NAME_PREFIX, SENSOR_TYPE_STR,
             mac[4], mac[5]);
    ensure_host();
    ble_svc_gap_device_name_set(s_adv_name);
    if (s_host_ready) {
        ble_gap_adv_stop();
        start_advertise();
    }
    return ESP_OK;
}

void ble_peripheral_stop(void)
{
    s_active = false;
    ble_gap_adv_stop();
    if (s_conn) {
        ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    }
}

bool ble_peripheral_is_active(void) { return s_active; }
