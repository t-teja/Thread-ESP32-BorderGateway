#include "mqtt_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_registry.h"
#include "esp_log.h"
#include "hub_config.h"
#include "hub_settings.h"
#include "mqtt_client.h"
#include "pairing.h"
#include "wifi_net.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_client;
static bool s_connected;
static char s_hub_id[32];
static char s_status_topic[96];
static char s_info_topic[96];
static char s_reg_topic[96];
static char s_evt_topic[96];

static void make_topics(void)
{
    const hub_settings_t *c = hub_settings_get();
    const char *base = c->topic_base[0] ? c->topic_base : HUB_DEFAULT_TOPIC_BASE;
    wifi_net_get_hub_id(s_hub_id, sizeof(s_hub_id));
    snprintf(s_status_topic, sizeof(s_status_topic), "%s/hub/%s/status", base, s_hub_id);
    snprintf(s_info_topic, sizeof(s_info_topic), "%s/hub/%s/info", base, s_hub_id);
    snprintf(s_reg_topic, sizeof(s_reg_topic), "%s/hub/%s/registry", base, s_hub_id);
    snprintf(s_evt_topic, sizeof(s_evt_topic), "%s/hub/%s/event", base, s_hub_id);
}

static void on_mqtt(void *args, esp_event_base_t base, int32_t id, void *data)
{
    (void)args;
    (void)base;
    (void)data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_publish(s_client, s_status_topic, "online", 0, 1, 1);
        mqtt_bridge_publish_info();
        mqtt_bridge_publish_registry();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_bridge_start(void)
{
    if (!hub_settings_has_mqtt()) {
        ESP_LOGW(TAG, "MQTT host not configured — skip");
        return ESP_ERR_INVALID_STATE;
    }
    const hub_settings_t *c = hub_settings_get();
    make_topics();
    char uri[160];
    snprintf(uri, sizeof(uri), "mqtt://%s:%u", c->mqtt_host, (unsigned)(c->mqtt_port ? c->mqtt_port : 1883));

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = s_hub_id,
        .session.last_will =
            {
                .topic = s_status_topic,
                .msg = "offline",
                .qos = 1,
                .retain = true,
            },
    };
    if (c->mqtt_user[0]) {
        cfg.credentials.username = c->mqtt_user;
        cfg.credentials.authentication.password = c->mqtt_pass;
    }

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    return esp_mqtt_client_start(s_client);
}

bool mqtt_bridge_is_connected(void)
{
    return s_connected;
}

void mqtt_bridge_publish_registry(void)
{
    if (!s_client || !s_connected) {
        return;
    }
    char *json = registry_to_json();
    if (!json) {
        return;
    }
    esp_mqtt_client_publish(s_client, s_reg_topic, json, 0, 1, 1);
    free(json);
}

void mqtt_bridge_publish_info(void)
{
    if (!s_client || !s_connected) {
        return;
    }
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"ip\":\"%s\",\"fw\":\"%s\",\"devices\":%d,\"pair_open\":%s,\"hub_id\":\"%s\"}",
             wifi_net_get_ip(), HUB_FW_VERSION, registry_count(),
             pairing_is_open() ? "true" : "false", s_hub_id);
    esp_mqtt_client_publish(s_client, s_info_topic, buf, 0, 1, 1);
}

void mqtt_bridge_publish_event(const char *json_object)
{
    if (!s_client || !s_connected || !json_object) {
        return;
    }
    esp_mqtt_client_publish(s_client, s_evt_topic, json_object, 0, 0, 0);
}
