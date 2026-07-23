#include "coap_bridge.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "device_registry.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "freertos/FreeRTOS.h"
#include "hub_config.h"
#include "hub_led.h"
#include "mqtt_bridge.h"
#include "openthread/coap.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/message.h"

static const char *TAG = "coap_br";
static otCoapResource s_res_state, s_res_meta, s_res_event;

static void send_ack(otMessage *req, const otMessageInfo *info)
{
    /* Called from within the OpenThread task's own processing — no lock needed. */
    otInstance *inst = esp_openthread_get_instance();
    otMessage *resp = otCoapNewMessage(inst, NULL);
    if (!resp) return;
    if (otCoapMessageInitResponse(resp, req, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CHANGED) != OT_ERROR_NONE) {
        otMessageFree(resp);
        return;
    }
    otCoapSendResponse(inst, resp, info);
}

static uint16_t read_payload(otMessage *msg, char *out, size_t outlen)
{
    uint16_t off = otMessageGetOffset(msg);
    uint16_t len = otMessageGetLength(msg) - off;
    if (len >= outlen) len = (uint16_t)outlen - 1;
    otMessageRead(msg, off, out, len);
    out[len] = 0;
    return len;
}

static void on_state(void *ctx, otMessage *msg, const otMessageInfo *info)
{
    (void)ctx;
    char payload[256];
    read_payload(msg, payload, sizeof(payload));
    ESP_LOGI(TAG, "POST /s: %s", payload);

    cJSON *j = cJSON_Parse(payload);
    const char *id = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "id")) : NULL;
    const hub_device_t *d = id ? registry_find(id) : NULL;
    if (!d) ESP_LOGW(TAG, "/s from unknown device id=%s", id ? id : "(none/bad json)");
    if (d) {
        bool was_online = d->online;
        float t = 0, h = 0;
        int bat = -1, rssi = 0;
        const char *contact = NULL;
        const cJSON *x;
        x = cJSON_GetObjectItem(j, "t"); if (cJSON_IsNumber(x)) t = (float)x->valuedouble;
        x = cJSON_GetObjectItem(j, "h"); if (cJSON_IsNumber(x)) h = (float)x->valuedouble;
        x = cJSON_GetObjectItem(j, "bat"); if (cJSON_IsNumber(x)) bat = x->valueint;
        x = cJSON_GetObjectItem(j, "rssi"); if (cJSON_IsNumber(x)) rssi = x->valueint;
        x = cJSON_GetObjectItem(j, "contact"); if (cJSON_IsString(x)) contact = x->valuestring;
        registry_update_telemetry(id, t, h, bat, rssi, contact);

        char addr[HUB_MAX_ADDR];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        registry_set_addr(id, addr);

        mqtt_bridge_publish_device_state(d->room, id, payload);
        if (!was_online) mqtt_bridge_publish_device_status(d->room, id, true);
    }
    if (j) cJSON_Delete(j);
    send_ack(msg, info);
}

static void on_meta(void *ctx, otMessage *msg, const otMessageInfo *info)
{
    (void)ctx;
    char payload[192];
    read_payload(msg, payload, sizeof(payload));
    ESP_LOGI(TAG, "POST /m: %s", payload);

    cJSON *j = cJSON_Parse(payload);
    const char *id = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "id")) : NULL;
    const hub_device_t *d = id ? registry_find(id) : NULL;
    if (!d) ESP_LOGW(TAG, "/m from unknown device id=%s", id ? id : "(none/bad json)");
    if (d) {
        const char *fw = cJSON_GetStringValue(cJSON_GetObjectItem(j, "fw"));
        const char *product = cJSON_GetStringValue(cJSON_GetObjectItem(j, "product"));
        if (fw) registry_set_fw(id, fw);

        char addr[HUB_MAX_ADDR];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        registry_set_addr(id, addr);

        char meta[256];
        snprintf(meta, sizeof(meta),
                 "{\"type\":\"%s\",\"name\":\"%s\",\"room\":\"%s\",\"fw\":\"%s\",\"product\":\"%s\"}",
                 d->type, d->name, d->room, fw ? fw : d->fw, product ? product : "");
        mqtt_bridge_publish_device_meta(d->room, id, meta);
    }
    if (j) cJSON_Delete(j);
    send_ack(msg, info);
}

static void on_event(void *ctx, otMessage *msg, const otMessageInfo *info)
{
    (void)ctx;
    char payload[96];
    read_payload(msg, payload, sizeof(payload));
    ESP_LOGI(TAG, "POST /e: %s", payload);

    cJSON *j = cJSON_Parse(payload);
    const char *id = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "id")) : NULL;
    if (id) {
        char addr[HUB_MAX_ADDR];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        registry_set_addr(id, addr);
        hub_led_identify(5);
        char ev[96];
        snprintf(ev, sizeof(ev), "{\"event\":\"identify\",\"id\":\"%s\"}", id);
        mqtt_bridge_publish_event(ev);
    }
    if (j) cJSON_Delete(j);
    send_ack(msg, info);
}

esp_err_t coap_bridge_init(void)
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *inst = esp_openthread_get_instance();
    if (!inst) {
        esp_openthread_lock_release();
        return ESP_ERR_INVALID_STATE;
    }
    otError err = otCoapStart(inst, HUB_COAP_PORT);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "otCoapStart failed: %d", (int)err);
        return ESP_FAIL;
    }
    s_res_state.mUriPath = "s"; s_res_state.mHandler = on_state; s_res_state.mContext = NULL;
    s_res_meta.mUriPath = "m"; s_res_meta.mHandler = on_meta; s_res_meta.mContext = NULL;
    s_res_event.mUriPath = "e"; s_res_event.mHandler = on_event; s_res_event.mContext = NULL;
    otCoapAddResource(inst, &s_res_state);
    otCoapAddResource(inst, &s_res_meta);
    otCoapAddResource(inst, &s_res_event);
    esp_openthread_lock_release();
    ESP_LOGI(TAG, "CoAP bridge listening on port %d", HUB_COAP_PORT);
    return ESP_OK;
}

void coap_bridge_send_cmd(const char *device_id, const char *cmd_json)
{
    if (!device_id || !cmd_json) return;
    const hub_device_t *d = registry_find(device_id);
    if (!d || !d->ot_addr[0]) {
        ESP_LOGW(TAG, "no known Thread address for %s yet", device_id);
        return;
    }
    otIp6Address peer;
    if (otIp6AddressFromString(d->ot_addr, &peer) != OT_ERROR_NONE) return;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *inst = esp_openthread_get_instance();
    otMessage *msg = otCoapNewMessage(inst, NULL);
    if (!msg) {
        esp_openthread_lock_release();
        return;
    }
    otCoapMessageInit(msg, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_POST);
    otCoapMessageGenerateToken(msg, OT_COAP_DEFAULT_TOKEN_LENGTH);
    otError err = otCoapMessageAppendUriPathOptions(msg, "c");
    if (err == OT_ERROR_NONE) err = otCoapMessageSetPayloadMarker(msg);
    if (err == OT_ERROR_NONE) err = otMessageAppend(msg, cmd_json, (uint16_t)strlen(cmd_json));
    if (err != OT_ERROR_NONE) {
        otMessageFree(msg);
        esp_openthread_lock_release();
        ESP_LOGW(TAG, "cmd build failed: %d", (int)err);
        return;
    }
    otMessageInfo info;
    memset(&info, 0, sizeof(info));
    info.mPeerAddr = peer;
    info.mPeerPort = HUB_COAP_PORT;
    err = otCoapSendRequest(inst, msg, &info, NULL, NULL);
    if (err != OT_ERROR_NONE) {
        otMessageFree(msg);
        ESP_LOGW(TAG, "cmd send failed: %d", (int)err);
    }
    esp_openthread_lock_release();
}

void coap_bridge_check_timeouts(void)
{
    int count = 0;
    const hub_device_t *devs = registry_get_all(&count);
    int64_t now = (int64_t)time(NULL);
    for (int i = 0; i < count; i++) {
        const hub_device_t *d = &devs[i];
        if (!d->in_use || !d->online) continue;
        if (now - d->last_seen > HUB_DEVICE_OFFLINE_TIMEOUT_SEC) {
            char id[HUB_MAX_DEVICE_ID], room[HUB_MAX_ROOM];
            strncpy(id, d->id, sizeof(id) - 1); id[sizeof(id) - 1] = 0;
            strncpy(room, d->room, sizeof(room) - 1); room[sizeof(room) - 1] = 0;
            registry_set_online(id, false);
            mqtt_bridge_publish_device_status(room, id, false);
            ESP_LOGW(TAG, "device %s timed out -> offline", id);
        }
    }
}
