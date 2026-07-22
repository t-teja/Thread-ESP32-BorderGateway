#include "mqtt_sensor.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_led.h"
#include "board.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_s";
static esp_mqtt_client_handle_t s_client;
static bool s_ok;
static char s_status_t[96], s_meta_t[96], s_state_t[96], s_cmd_t[112];
static sensor_cfg_t s_cfg;
static void (*s_identify_cb)(void);

void mqtt_sensor_set_identify_cb(void (*cb)(void)) { s_identify_cb = cb; }

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
    esp_mqtt_event_handle_t e = data;
    if (id == MQTT_EVENT_CONNECTED) {
        s_ok = true;
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_publish(s_client, s_status_t, "online", 0, 1, 1);
        char meta[256];
        snprintf(meta, sizeof(meta),
                 "{\"type\":\"%s\",\"name\":\"%s\",\"room\":\"%s\",\"fw\":\"%s\",\"product\":\"%s\"}",
                 s_cfg.type[0] ? s_cfg.type : SENSOR_TYPE_STR, s_cfg.name, s_cfg.room,
                 SENSOR_FW_VERSION, SENSOR_PRODUCT_NAME);
        esp_mqtt_client_publish(s_client, s_meta_t, meta, 0, 1, 1);
        esp_mqtt_client_subscribe(s_client, s_cmd_t, 0);
    } else if (id == MQTT_EVENT_DISCONNECTED) {
        s_ok = false;
    } else if (id == MQTT_EVENT_DATA) {
        char payload[128];
        int n = e->data_len < (int)sizeof(payload) - 1 ? e->data_len : (int)sizeof(payload) - 1;
        memcpy(payload, e->data, n); payload[n] = 0;
        cJSON *j = cJSON_Parse(payload);
        if (j) {
            const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(j, "cmd"));
            if (cmd && strcmp(cmd, "identify") == 0) {
                ESP_LOGI(TAG, "identify command");
                if (s_identify_cb) s_identify_cb();
                else app_led_set(LED_BLINK_FAST);
            }
            cJSON_Delete(j);
        }
    }
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
    snprintf(s_cmd_t, sizeof(s_cmd_t), "%s/%s/%s/set/cmd", base, room, cfg->device_id);

    char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", cfg->mqtt_host, cfg->mqtt_port > 0 ? cfg->mqtt_port : 1883);
    ESP_LOGI(TAG, "MQTT %s topics under %s/%s/%s", uri, base, room, cfg->device_id);

    esp_mqtt_client_config_t mc = {
        .broker.address.uri = uri,
        .credentials.client_id = cfg->device_id,
        .session.last_will = {.topic = s_status_t, .msg = "offline", .qos = 1, .retain = true},
        .network.reconnect_timeout_ms = 10000,
        .network.timeout_ms = 15000,
    };
    if (cfg->mqtt_user[0]) {
        mc.credentials.username = cfg->mqtt_user;
        mc.credentials.authentication.password = cfg->mqtt_pass;
    }
    s_client = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    return esp_mqtt_client_start(s_client);
}

void mqtt_sensor_publish_state(float t_c, float rh, int bat, int rssi)
{
    if (!s_client || !s_ok) return;
    char payload[192];
    snprintf(payload, sizeof(payload),
             "{\"type\":\"temp_hum\",\"t\":%.2f,\"h\":%.2f,\"bat\":%d,\"rssi\":%d,\"ts\":%ld}",
             t_c, rh, bat, rssi, (long)time(NULL));
    esp_mqtt_client_publish(s_client, s_state_t, payload, 0, 1, 1);
}

bool mqtt_sensor_is_connected(void) { return s_ok; }

void mqtt_sensor_publish_identify(void)
{
    if (!s_client || !s_ok) return;
    const char *base = s_cfg.topic_base[0] ? s_cfg.topic_base : "home";
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/broadcast/identify", base);
    char payload[96];
    /* Build JSON without nested quote hell: {"id":"<id>","cmd":"identify"} */
    snprintf(payload, sizeof(payload), "{\"%s\":\"%s\",\"%s\":\"%s\"}",
             "id", s_cfg.device_id, "cmd", "identify");
    esp_mqtt_client_publish(s_client, topic, payload, 0, 0, 0);
}

