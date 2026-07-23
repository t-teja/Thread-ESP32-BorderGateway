/** NimBLE central: scan THS-* sensors; GATT write 0xFFF2 to provision. */
#include "ble_central.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "ble_cent";
static ble_candidate_t s_cands[BLE_MAX_CANDIDATES];
static bool s_scanning, s_ready;
static SemaphoreHandle_t s_lock, s_prov_done;
static esp_err_t s_prov_result;
static char s_prov_json[1024];
static uint16_t s_conn_handle;

static void cand_upsert(const ble_candidate_t *c)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < BLE_MAX_CANDIDATES; i++) {
        if (s_cands[i].valid && strcmp(s_cands[i].addr, c->addr) == 0) {
            s_cands[i] = *c; s_cands[i].valid = true;
            xSemaphoreGive(s_lock); return;
        }
    }
    for (int i = 0; i < BLE_MAX_CANDIDATES; i++) {
        if (!s_cands[i].valid) { s_cands[i] = *c; s_cands[i].valid = true; break; }
    }
    xSemaphoreGive(s_lock);
}

static void parse_adv_name(const uint8_t *data, uint8_t len, char *name, size_t nlen)
{
    name[0] = 0;
    for (uint8_t i = 0; i + 1 < len;) {
        uint8_t flen = data[i];
        if (flen == 0 || i + flen >= len) break;
        uint8_t typ = data[i + 1];
        if ((typ == 0x08 || typ == 0x09) && flen >= 2) {
            size_t cpy = flen - 1; if (cpy >= nlen) cpy = nlen - 1;
            memcpy(name, &data[i + 2], cpy); name[cpy] = 0; return;
        }
        i = (uint8_t)(i + flen + 1);
    }
}

static int write_long_cb(uint16_t conn, const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg)
{
    (void)attr; (void)arg;
    int status = error ? error->status : 0;
    ESP_LOGI(TAG, "write_long provision status=%d", status);
    s_prov_result = (status == 0) ? ESP_OK : ESP_FAIL;
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    xSemaphoreGive(s_prov_done);
    return 0;
}

static int gatt_chr_cb(uint16_t conn, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg);

static int mtu_cb(uint16_t conn, const struct ble_gatt_error *error, uint16_t mtu, void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "mtu exchange status=%d mtu=%d", error ? error->status : 0, mtu);
    /* Whether or not the MTU exchange succeeded, proceed to discover the
     * provision characteristic and write to it. A larger MTU just means
     * fewer ATT fragments for the write-long procedure, so it finishes
     * faster and is less likely to straddle a Thread-stack-induced BLE
     * scheduling stall on the hub. */
    ble_uuid16_t u = BLE_UUID16_INIT(0xFFF2);
    ble_gattc_disc_chrs_by_uuid(conn, 1, 0xFFFF, &u.u, gatt_chr_cb, NULL);
    return 0;
}

static int gatt_chr_cb(uint16_t conn, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == BLE_HS_EDONE) return 0;
    if (error->status != 0) { s_prov_result = ESP_FAIL; xSemaphoreGive(s_prov_done); return 0; }
    if (!chr) return 0;
    /*
     * The provision JSON (Thread dataset + ids) is far larger than a single
     * ATT packet (~20 bytes with the default MTU). ble_gattc_write_no_rsp*
     * silently truncates to one packet; ble_gattc_write_long uses the
     * prepare-write/execute-write queued procedure to send it in full.
     */
    size_t len = strlen(s_prov_json);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(s_prov_json, len);
    if (!om) {
        ESP_LOGE(TAG, "mbuf alloc failed for %d-byte provision payload", (int)len);
        s_prov_result = ESP_ERR_NO_MEM;
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        xSemaphoreGive(s_prov_done);
        return 0;
    }
    int rc = ble_gattc_write_long(conn, chr->val_handle, 0, om, write_long_cb, NULL);
    ESP_LOGI(TAG, "write_long provision len=%d rc=%d", (int)len, rc);
    if (rc != 0) {
        s_prov_result = ESP_FAIL;
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        xSemaphoreGive(s_prov_done);
    }
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        char name[32];
        parse_adv_name(event->disc.data, event->disc.length_data, name, sizeof(name));
        if (strncmp(name, HUB_BLE_NAME_PREFIX, strlen(HUB_BLE_NAME_PREFIX)) != 0) return 0;
        ble_candidate_t c = {0};
        snprintf(c.addr, sizeof(c.addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
                 event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0]);
        strncpy(c.name, name, sizeof(c.name) - 1);
        c.rssi = event->disc.rssi;
        const char *rest = name + strlen(HUB_BLE_NAME_PREFIX);
        const char *dash = strchr(rest, '-');
        if (dash && dash > rest) {
            size_t tl = (size_t)(dash - rest); if (tl >= sizeof(c.type)) tl = sizeof(c.type) - 1;
            memcpy(c.type, rest, tl); c.type[tl] = 0;
        } else {
            strncpy(c.type, rest, sizeof(c.type) - 1);
        }
        strncpy(c.product, name, sizeof(c.product) - 1);
        c.valid = true; cand_upsert(&c); return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE: s_scanning = false; return 0;
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            s_prov_result = ESP_FAIL; xSemaphoreGive(s_prov_done); return 0;
        }
        s_conn_handle = event->connect.conn_handle;
        ble_gattc_exchange_mtu(s_conn_handle, mtu_cb, NULL);
        return 0;
    default: return 0;
    }
}

static void on_sync(void) { ble_hs_util_ensure_addr(0); s_ready = true; ESP_LOGI(TAG, "ready"); }
static void host_task(void *param) { nimble_port_run(); nimble_port_freertos_deinit(); }

esp_err_t ble_central_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_prov_done = xSemaphoreCreateBinary();
    memset(s_cands, 0, sizeof(s_cands));
    /*
     * Central-only: do NOT call ble_svc_gap_* / ble_svc_gatt_*.
     * Those APIs live behind BLE_GATTS and are compiled out when
     * peripheral/GATT server roles are disabled (our hub config).
     */
    nimble_port_init();
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

esp_err_t ble_central_start_scan(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(s_cands, 0, sizeof(s_cands));
    xSemaphoreGive(s_lock);
    struct ble_gap_disc_params p = {0};
    p.itvl = 0x50; p.window = 0x30; p.filter_duplicates = 1;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &p, gap_event, NULL);
    if (rc != 0) { ESP_LOGE(TAG, "disc rc=%d", rc); return ESP_FAIL; }
    s_scanning = true; return ESP_OK;
}

void ble_central_stop_scan(void)
{ if (s_scanning) { ble_gap_disc_cancel(); s_scanning = false; } }

bool ble_central_is_scanning(void) { return s_scanning; }

int ble_central_get_candidates(ble_candidate_t *out, int max)
{
    int n = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < BLE_MAX_CANDIDATES && n < max; i++)
        if (s_cands[i].valid) out[n++] = s_cands[i];
    xSemaphoreGive(s_lock);
    return n;
}

char *ble_central_candidates_json(void)
{
    ble_candidate_t tmp[BLE_MAX_CANDIDATES];
    int n = ble_central_get_candidates(tmp, BLE_MAX_CANDIDATES);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "addr", tmp[i].addr);
        cJSON_AddStringToObject(o, "name", tmp[i].name);
        cJSON_AddStringToObject(o, "type", tmp[i].type);
        cJSON_AddStringToObject(o, "product", tmp[i].product);
        cJSON_AddNumberToObject(o, "rssi", tmp[i].rssi);
        cJSON_AddItemToArray(arr, o);
    }
    char *s = cJSON_PrintUnformatted(arr); cJSON_Delete(arr); return s;
}

static int parse_addr(const char *str, ble_addr_t *addr)
{
    unsigned int b[6];
    if (sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return -1;
    addr->type = BLE_ADDR_PUBLIC;
    for (int i = 0; i < 6; i++) addr->val[i] = (uint8_t)b[5 - i];
    return 0;
}

static esp_err_t provision_attempt(const ble_addr_t *addr)
{
    s_prov_result = ESP_FAIL;
    xSemaphoreTake(s_prov_done, 0);
    /*
     * write_long sends the ~270+ byte provision JSON as a series of
     * prepare-write PDUs over several connection intervals. With the
     * NimBLE-default conn params (params=NULL) the supervision timeout is
     * only ~2.56s, which is too short while the hub's OpenThread stack is
     * busy (mesh attach/parent-request, Wi-Fi/BT coexistence) and can steal
     * enough air-time to miss connection events -> link drops mid-write
     * (BLE_HS_ENOTCONN). Use a longer supervision timeout so the link
     * survives those stalls; the write itself is still bounded by the
     * 20s semaphore wait below.
     */
    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 0x10,
        .scan_window = 0x10,
        .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,
        .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,
        .latency = 0,
        .supervision_timeout = 800, /* 8s, in 10ms units */
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, addr, 30000, &conn_params, gap_event, NULL);
    if (rc != 0) { ESP_LOGE(TAG, "connect rc=%d", rc); return ESP_FAIL; }
    if (xSemaphoreTake(s_prov_done, pdMS_TO_TICKS(20000)) != pdTRUE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return ESP_ERR_TIMEOUT;
    }
    return s_prov_result;
}

esp_err_t ble_central_provision(const char *addr_str, const char *provision_json)
{
    if (!addr_str || !provision_json) return ESP_ERR_INVALID_ARG;
    ble_central_stop_scan();
    strncpy(s_prov_json, provision_json, sizeof(s_prov_json) - 1);
    ble_addr_t addr;
    if (parse_addr(addr_str, &addr) != 0) return ESP_ERR_INVALID_ARG;

    /*
     * The hub's OpenThread stack periodically needs to re-attach/refresh its
     * mesh role, which can starve the BLE link of air-time for a couple of
     * seconds and drop it mid write-long (see provision_attempt() comment).
     * That's a transient collision, not a permanent failure, so retry once
     * before giving up and surfacing an error to the dashboard.
     */
    esp_err_t err = provision_attempt(&addr);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "provision attempt 1 failed (%s) — retrying", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(500));
        err = provision_attempt(&addr);
    }
    return err;
}
