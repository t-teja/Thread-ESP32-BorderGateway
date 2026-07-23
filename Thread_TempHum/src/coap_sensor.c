#include "coap_sensor.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_led.h"
#include "board.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "freertos/FreeRTOS.h"
#include "openthread/coap.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/message.h"

static const char *TAG = "coap_s";
static sensor_cfg_t s_cfg;
static otIp6Address s_hub_addr;
static bool s_hub_addr_ok;
static bool s_ok;
static otCoapResource s_cmd_res;
static void (*s_identify_cb)(void);

void coap_sensor_set_identify_cb(void (*cb)(void)) { s_identify_cb = cb; }

static void on_cmd(void *ctx, otMessage *msg, const otMessageInfo *info)
{
    (void)ctx;
    char buf[128];
    uint16_t off = otMessageGetOffset(msg);
    uint16_t len = otMessageGetLength(msg) - off;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    otMessageRead(msg, off, buf, len);
    buf[len] = 0;

    cJSON *j = cJSON_Parse(buf);
    if (j) {
        const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(j, "cmd"));
        if (cmd && strcmp(cmd, "identify") == 0) {
            ESP_LOGI(TAG, "identify command");
            if (s_identify_cb) s_identify_cb();
            else app_led_set(LED_BLINK_FAST);
        }
        cJSON_Delete(j);
    }

    otInstance *inst = esp_openthread_get_instance();
    otMessage *resp = otCoapNewMessage(inst, NULL);
    if (resp) {
        if (otCoapMessageInitResponse(resp, msg, OT_COAP_TYPE_ACKNOWLEDGMENT, OT_COAP_CODE_CHANGED) ==
            OT_ERROR_NONE) {
            otCoapSendResponse(inst, resp, info);
        } else {
            otMessageFree(resp);
        }
    }
}

static void on_response(void *ctx, otMessage *msg, const otMessageInfo *info, otError result)
{
    (void)ctx; (void)msg; (void)info;
    s_ok = (result == OT_ERROR_NONE);
}

static void coap_send(const char *path, const char *json)
{
    if (!s_hub_addr_ok) return;
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *inst = esp_openthread_get_instance();
    otMessage *msg = otCoapNewMessage(inst, NULL);
    if (!msg) { esp_openthread_lock_release(); return; }
    otCoapMessageInit(msg, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_POST);
    otCoapMessageGenerateToken(msg, OT_COAP_DEFAULT_TOKEN_LENGTH);
    otError err = otCoapMessageAppendUriPathOptions(msg, path);
    if (err == OT_ERROR_NONE) err = otCoapMessageSetPayloadMarker(msg);
    if (err == OT_ERROR_NONE) err = otMessageAppend(msg, json, (uint16_t)strlen(json));
    if (err != OT_ERROR_NONE) {
        otMessageFree(msg);
        esp_openthread_lock_release();
        return;
    }
    otMessageInfo info;
    memset(&info, 0, sizeof(info));
    info.mPeerAddr = s_hub_addr;
    info.mPeerPort = OT_DEFAULT_COAP_PORT;
    err = otCoapSendRequest(inst, msg, &info, on_response, NULL);
    if (err != OT_ERROR_NONE) otMessageFree(msg);
    esp_openthread_lock_release();
}

esp_err_t coap_sensor_start(const sensor_cfg_t *cfg)
{
    if (!cfg || !cfg->hub_addr[0]) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    s_hub_addr_ok = (otIp6AddressFromString(cfg->hub_addr, &s_hub_addr) == OT_ERROR_NONE);
    if (!s_hub_addr_ok) {
        ESP_LOGE(TAG, "bad hub_addr %s", cfg->hub_addr);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "hub %s, id %s", cfg->hub_addr, cfg->device_id);

    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *inst = esp_openthread_get_instance();
    otCoapStart(inst, OT_DEFAULT_COAP_PORT);
    s_cmd_res.mUriPath = "c";
    s_cmd_res.mHandler = on_cmd;
    s_cmd_res.mContext = NULL;
    otCoapAddResource(inst, &s_cmd_res);
    esp_openthread_lock_release();

    char meta[192];
    snprintf(meta, sizeof(meta), "{\"id\":\"%s\",\"fw\":\"%s\",\"product\":\"%s\"}",
             cfg->device_id, SENSOR_FW_VERSION, SENSOR_PRODUCT_NAME);
    coap_send("m", meta);
    return ESP_OK;
}

void coap_sensor_publish_state(float t_c, float rh, int bat, int rssi)
{
    char payload[224];
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"type\":\"temp_hum\",\"t\":%.2f,\"h\":%.2f,\"bat\":%d,\"rssi\":%d,\"ts\":%ld}",
             s_cfg.device_id, t_c, rh, bat, rssi, (long)time(NULL));
    coap_send("s", payload);
}

bool coap_sensor_is_connected(void) { return s_ok; }

void coap_sensor_publish_identify(void)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"id\":\"%s\"}", s_cfg.device_id);
    coap_send("e", payload);
}
