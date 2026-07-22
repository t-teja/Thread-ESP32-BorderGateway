#include "mqtt_sensor.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "board.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_s";
static esp_mqtt_client_handle_t s_client;
static bool s_ok;
static char s_status_t[96], s_meta_t[96], s_state_t[96];
static sensor_cfg_t s_cfg;

static void slug_room(const char *room, char *out, size_t n)
{
    if (!room || !room[0]) { snprintf(out, n, "unassigned"); return; }
    size_t j = 0;
    for (size_t i = 0; room[i] && j + 1 < n; i++) {
        char c = room[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[j++] = c;
        else if ((c == ' ' || c == '-') && j && out[j - 1] != '_') out[j++] = '_';
    }
    out[j] = 0;
    if (!j) snprintf(out, n, "unassigned");
}

static void on_mqtt(void *a, esp_event_base_t b, int32_t id, void *data)
{
    if (id == MQTT_EVENT_CONNECTED) {
        s_ok = true;
        esp_mqtt_client_publish(s_client, s_status_t, "online", 0, 1, 1);
        char meta[256];
        snprintf(meta, sizeof(meta),
                 "{\"type\":\"contact\",\"name\":\"%s\",\"room\":\"%s\",\"fw\":\"%s\",\"product\":\"%s\"}",
                 s_cfg.name, s_cfg.room, SENSOR_FW_VERSION, SENSOR_PRODUCT_NAME);
        esp_mqtt_client_publish(s_client, s_meta_t, meta, 0, 1, 1);
    } else if (id == MQTT_EVENT_DISCONNECTED) s_ok = false;
}

esp_err_t mqtt_sensor_start(const sensor_cfg_t *cfg)
{
    if (!cfg || !cfg->mqtt_host[0]) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    char room[40];
    slug_room(cfg->room, room, sizeof(room));
    const char *base = cfg->topic_base[0] ? cfg->topic_base : "home";
    snprintf(s_status_t, sizeof(s_status_t), "%s/%s/%s/status", base, room, cfg->device_id);
    snprintf(s_meta_t, sizeof(s_meta_t), "%s/%s/%s/meta", base, room, cfg->device_id);
    snprintf(s_state_t, sizeof(s_state_t), "%s/%s/%s/state", base, room, cfg->device_id);
    char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", cfg->mqtt_host, cfg->mqtt_port > 0 ? cfg->mqtt_port : 1883);
    esp_mqtt_client_config_t mc = {
        .broker.address.uri = uri,
        .credentials.client_id = cfg->device_id,
        .session.last_will = {.topic = s_status_t, .msg = "offline", .qos = 1, .retain = true},
    };
    if (cfg->mqtt_user[0]) {
        mc.credentials.username = cfg->mqtt_user;
        mc.credentials.authentication.password = cfg->mqtt_pass;
    }
    s_client = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    return esp_mqtt_client_start(s_client);
}

void mqtt_sensor_publish_contact(const char *open_or_closed, int bat, int rssi)
{
    if (!s_client || !s_ok) return;
    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"type\":\"contact\",\"contact\":\"%s\",\"bat\":%d,\"rssi\":%d,\"ts\":%ld}",
             open_or_closed, bat, rssi, (long)time(NULL));
    esp_mqtt_client_publish(s_client, s_state_t, payload, 0, 1, 1);
}

bool mqtt_sensor_is_connected(void) { return s_ok; }
