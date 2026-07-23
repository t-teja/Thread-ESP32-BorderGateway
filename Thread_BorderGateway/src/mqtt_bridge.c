#include "mqtt_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "coap_bridge.h"
#include "device_registry.h"
#include "esp_log.h"
#include "hub_config.h"
#include "hub_led.h"
#include "hub_settings.h"
#include "mqtt_client.h"
#include "pairing.h"
#include "util_slug.h"
#include "wifi_net.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_client;
static bool s_connected;
static char s_hub_id[32];
static char s_status_topic[96];
static char s_info_topic[96];
static char s_reg_topic[96];
static char s_evt_topic[96];
static char s_uri[160];
static char s_sub_set[32];
static char s_sub_cmd_hub[96];

static void make_topics(void)
{
    const hub_settings_t *c = hub_settings_get();
    const char *base = c->topic_base[0] ? c->topic_base : HUB_DEFAULT_TOPIC_BASE;
    wifi_net_get_hub_id(s_hub_id, sizeof(s_hub_id));
    snprintf(s_status_topic, sizeof(s_status_topic), "%s/hub/%s/status", base, s_hub_id);
    snprintf(s_info_topic, sizeof(s_info_topic), "%s/hub/%s/info", base, s_hub_id);
    snprintf(s_reg_topic, sizeof(s_reg_topic), "%s/hub/%s/registry", base, s_hub_id);
    snprintf(s_evt_topic, sizeof(s_evt_topic), "%s/hub/%s/event", base, s_hub_id);
    /* Devices no longer connect to MQTT directly; this lets external MQTT
     * clients (e.g. Node-RED automations) still command a device — the hub
     * relays it over CoAP (see coap_bridge_send_cmd). */
    snprintf(s_sub_set, sizeof(s_sub_set), "%s/+/+/set/#", base);
    snprintf(s_sub_cmd_hub, sizeof(s_sub_cmd_hub), "%s/hub/%s/cmd", base, s_hub_id);
}

static void handle_set_msg(const char *topic, const char *data, int len)
{
    /* home/room/id/set/... */
    char tcopy[128];
    snprintf(tcopy, sizeof(tcopy), "%.*s", (int)sizeof(tcopy) - 1, topic);
    char *save = NULL;
    strtok_r(tcopy, "/", &save); /* base */
    strtok_r(NULL, "/", &save); /* room */
    char *id = strtok_r(NULL, "/", &save);
    char *leaf = strtok_r(NULL, "/", &save);
    if (!id || !leaf || strcmp(leaf, "set") != 0) return;

    char payload[192];
    int n = len < (int)sizeof(payload) - 1 ? len : (int)sizeof(payload) - 1;
    memcpy(payload, data, n);
    payload[n] = 0;
    coap_bridge_send_cmd(id, payload);
}

static void handle_hub_cmd(const char *data, int len)
{
    char payload[256];
    int n = len < (int)sizeof(payload) - 1 ? len : (int)sizeof(payload) - 1;
    memcpy(payload, data, n);
    payload[n] = 0;
    cJSON *j = cJSON_Parse(payload);
    if (!j) return;
    const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(j, "cmd"));
    if (cmd && strcmp(cmd, "identify") == 0) {
        hub_led_identify(5);
    }
    cJSON_Delete(j);
}

static void on_mqtt(void *args, esp_event_base_t base, int32_t id, void *data)
{
    (void)args; (void)base;
    esp_mqtt_event_handle_t e = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected %s", s_uri);
        esp_mqtt_client_publish(s_client, s_status_topic, "online", 0, 1, 1);
        esp_mqtt_client_subscribe(s_client, s_sub_set, 0);
        esp_mqtt_client_subscribe(s_client, s_sub_cmd_hub, 0);
        { const hub_settings_t *c = hub_settings_get();
          const char *base = c->topic_base[0] ? c->topic_base : HUB_DEFAULT_TOPIC_BASE;
          char bcast[48]; snprintf(bcast,sizeof(bcast), "%s/broadcast/identify", base);
          esp_mqtt_client_subscribe(s_client, bcast, 0); }
        mqtt_bridge_publish_info();
        mqtt_bridge_publish_registry();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected (retry)");
        break;
    case MQTT_EVENT_DATA: {
        char topic[128];
        int tl = e->topic_len < (int)sizeof(topic) - 1 ? e->topic_len : (int)sizeof(topic) - 1;
        memcpy(topic, e->topic, tl);
        topic[tl] = 0;
        if (strstr(topic, "/set/")) handle_set_msg(topic, e->data, e->data_len);
        else if (strstr(topic, "/cmd")) handle_hub_cmd(e->data, e->data_len);
        else if (strstr(topic, "broadcast/identify")) hub_led_identify(5);
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "cannot reach broker %s — check Mosquitto 0.0.0.0:1883 and firewall", s_uri);
        break;
    default:
        break;
    }
}

esp_err_t mqtt_bridge_start(void)
{
    if (!hub_settings_has_mqtt()) return ESP_ERR_INVALID_STATE;
    const hub_settings_t *c = hub_settings_get();
    make_topics();
    snprintf(s_uri, sizeof(s_uri), "mqtt://%s:%u", c->mqtt_host,
             (unsigned)(c->mqtt_port ? c->mqtt_port : 1883));
    ESP_LOGI(TAG, "connecting %s as %s", s_uri, s_hub_id);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_uri,
        .credentials.client_id = s_hub_id,
        .network.reconnect_timeout_ms = 10000,
        .network.timeout_ms = 10000,
        .session.last_will = {
            .topic = s_status_topic, .msg = "offline", .qos = 1, .retain = true,
        },
    };
    if (c->mqtt_user[0]) {
        cfg.credentials.username = c->mqtt_user;
        cfg.credentials.authentication.password = c->mqtt_pass;
    }
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) return ESP_FAIL;
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    return esp_mqtt_client_start(s_client);
}

bool mqtt_bridge_is_connected(void) { return s_connected; }

void mqtt_bridge_publish_registry(void)
{
    if (!s_client || !s_connected) return;
    char *json = registry_to_json();
    if (!json) return;
    esp_mqtt_client_publish(s_client, s_reg_topic, json, 0, 1, 1);
    free(json);
}

void mqtt_bridge_publish_info(void)
{
    if (!s_client || !s_connected) return;
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"ip\":\"%s\",\"fw\":\"%s\",\"devices\":%d,\"online\":%d,\"pair_open\":%s,\"hub_id\":\"%s\"}",
             wifi_net_get_ip(), HUB_FW_VERSION, registry_count(), registry_online_count(),
             pairing_is_open() ? "true" : "false", s_hub_id);
    esp_mqtt_client_publish(s_client, s_info_topic, buf, 0, 1, 1);
}

void mqtt_bridge_publish_event(const char *json_object)
{
    if (!s_client || !s_connected || !json_object) return;
    esp_mqtt_client_publish(s_client, s_evt_topic, json_object, 0, 0, 0);
}

static void device_topic(char *out, size_t outlen, const char *room, const char *device_id,
                          const char *leaf)
{
    const hub_settings_t *c = hub_settings_get();
    const char *base = c->topic_base[0] ? c->topic_base : HUB_DEFAULT_TOPIC_BASE;
    char room_slug[40];
    hub_slugify(room ? room : "", room_slug, sizeof(room_slug));
    snprintf(out, outlen, "%s/%s/%s/%s", base, room_slug, device_id, leaf);
}

void mqtt_bridge_publish_device_state(const char *room, const char *device_id, const char *json_object)
{
    if (!s_client || !s_connected || !device_id || !json_object) return;
    char topic[128];
    device_topic(topic, sizeof(topic), room, device_id, "state");
    esp_mqtt_client_publish(s_client, topic, json_object, 0, 1, 1);
}

void mqtt_bridge_publish_device_meta(const char *room, const char *device_id, const char *json_object)
{
    if (!s_client || !s_connected || !device_id || !json_object) return;
    char topic[128];
    device_topic(topic, sizeof(topic), room, device_id, "meta");
    esp_mqtt_client_publish(s_client, topic, json_object, 0, 1, 1);
}

void mqtt_bridge_publish_device_status(const char *room, const char *device_id, bool online)
{
    if (!s_client || !s_connected || !device_id) return;
    char topic[128];
    device_topic(topic, sizeof(topic), room, device_id, "status");
    esp_mqtt_client_publish(s_client, topic, online ? "online" : "offline", 0, 1, 1);
}
