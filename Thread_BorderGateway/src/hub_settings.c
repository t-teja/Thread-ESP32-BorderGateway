#include "hub_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "hub_config.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Optional private compile-time defaults (secrets.h is gitignored). */
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT ""
#endif
#ifndef WIFI_PASSWORD_DEFAULT
#define WIFI_PASSWORD_DEFAULT ""
#endif
#ifndef MQTT_HOST_DEFAULT
#define MQTT_HOST_DEFAULT ""
#endif
#ifndef MQTT_PORT_DEFAULT
#define MQTT_PORT_DEFAULT 1883
#endif
#ifndef MQTT_USER_DEFAULT
#define MQTT_USER_DEFAULT ""
#endif
#ifndef MQTT_PASSWORD_DEFAULT
#define MQTT_PASSWORD_DEFAULT ""
#endif
#ifndef MQTT_TOPIC_BASE_DEFAULT
#define MQTT_TOPIC_BASE_DEFAULT HUB_DEFAULT_TOPIC_BASE
#endif
#ifndef HUB_HOSTNAME_DEFAULT
#define HUB_HOSTNAME_DEFAULT HUB_DEFAULT_HOSTNAME
#endif

static const char *TAG = "settings";
static hub_settings_t s_cfg;
static nvs_handle_t s_nvs;
static bool s_open;

static void apply_compile_defaults(hub_settings_t *s)
{
    if (!s->wifi_ssid[0] && WIFI_SSID_DEFAULT[0]) {
        strncpy(s->wifi_ssid, WIFI_SSID_DEFAULT, sizeof(s->wifi_ssid) - 1);
        strncpy(s->wifi_pass, WIFI_PASSWORD_DEFAULT, sizeof(s->wifi_pass) - 1);
    }
    if (!s->mqtt_host[0] && MQTT_HOST_DEFAULT[0]) {
        strncpy(s->mqtt_host, MQTT_HOST_DEFAULT, sizeof(s->mqtt_host) - 1);
    }
    if (s->mqtt_port == 0) {
        s->mqtt_port = (uint16_t)MQTT_PORT_DEFAULT;
    }
    if (!s->mqtt_user[0] && MQTT_USER_DEFAULT[0]) {
        strncpy(s->mqtt_user, MQTT_USER_DEFAULT, sizeof(s->mqtt_user) - 1);
        strncpy(s->mqtt_pass, MQTT_PASSWORD_DEFAULT, sizeof(s->mqtt_pass) - 1);
    }
    if (!s->topic_base[0]) {
        strncpy(s->topic_base, MQTT_TOPIC_BASE_DEFAULT[0] ? MQTT_TOPIC_BASE_DEFAULT : HUB_DEFAULT_TOPIC_BASE,
                sizeof(s->topic_base) - 1);
    }
    if (!s->hostname[0]) {
        strncpy(s->hostname, HUB_HOSTNAME_DEFAULT[0] ? HUB_HOSTNAME_DEFAULT : HUB_DEFAULT_HOSTNAME,
                sizeof(s->hostname) - 1);
    }
}

void hub_settings_init(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.mqtt_port = 1883;
    strncpy(s_cfg.topic_base, HUB_DEFAULT_TOPIC_BASE, sizeof(s_cfg.topic_base) - 1);
    strncpy(s_cfg.hostname, HUB_DEFAULT_HOSTNAME, sizeof(s_cfg.hostname) - 1);
    if (nvs_open("hub_cfg", NVS_READWRITE, &s_nvs) == ESP_OK) {
        s_open = true;
    } else {
        ESP_LOGE(TAG, "nvs_open failed");
    }
    hub_settings_load();
}

const hub_settings_t *hub_settings_get(void)
{
    return &s_cfg;
}

hub_settings_t *hub_settings_mut(void)
{
    return &s_cfg;
}

void hub_settings_load(void)
{
    if (!s_open) {
        apply_compile_defaults(&s_cfg);
        s_cfg.configured = hub_settings_has_wifi();
        return;
    }
    size_t len;
    uint8_t flag = 0;
    nvs_get_u8(s_nvs, "ok", &flag);
    s_cfg.configured = flag != 0;

#define LOAD_STR(key, field)                       \
    do {                                           \
        len = sizeof(s_cfg.field);                 \
        nvs_get_str(s_nvs, key, s_cfg.field, &len);\
    } while (0)

    LOAD_STR("wssid", wifi_ssid);
    LOAD_STR("wpass", wifi_pass);
    LOAD_STR("mhost", mqtt_host);
    LOAD_STR("muser", mqtt_user);
    LOAD_STR("mpass", mqtt_pass);
    LOAD_STR("tbase", topic_base);
    LOAD_STR("hname", hostname);
#undef LOAD_STR

    uint16_t port = 0;
    if (nvs_get_u16(s_nvs, "mport", &port) == ESP_OK && port) {
        s_cfg.mqtt_port = port;
    }
    apply_compile_defaults(&s_cfg);
    if (!s_cfg.configured) {
        s_cfg.configured = hub_settings_has_wifi();
    }
    ESP_LOGI(TAG, "loaded configured=%d ssid='%s' mqtt='%s:%u'", s_cfg.configured, s_cfg.wifi_ssid,
             s_cfg.mqtt_host, (unsigned)s_cfg.mqtt_port);
}

esp_err_t hub_settings_save(const hub_settings_t *s)
{
    if (!s_open || !s) {
        return ESP_ERR_INVALID_STATE;
    }
    s_cfg = *s;
    if (!s_cfg.topic_base[0]) {
        strncpy(s_cfg.topic_base, HUB_DEFAULT_TOPIC_BASE, sizeof(s_cfg.topic_base) - 1);
    }
    if (!s_cfg.hostname[0]) {
        strncpy(s_cfg.hostname, HUB_DEFAULT_HOSTNAME, sizeof(s_cfg.hostname) - 1);
    }
    if (s_cfg.mqtt_port == 0) {
        s_cfg.mqtt_port = 1883;
    }
    s_cfg.configured = s_cfg.wifi_ssid[0] != 0;

    nvs_set_u8(s_nvs, "ok", s_cfg.configured ? 1 : 0);
    nvs_set_str(s_nvs, "wssid", s_cfg.wifi_ssid);
    nvs_set_str(s_nvs, "wpass", s_cfg.wifi_pass);
    nvs_set_str(s_nvs, "mhost", s_cfg.mqtt_host);
    nvs_set_u16(s_nvs, "mport", s_cfg.mqtt_port);
    nvs_set_str(s_nvs, "muser", s_cfg.mqtt_user);
    nvs_set_str(s_nvs, "mpass", s_cfg.mqtt_pass);
    nvs_set_str(s_nvs, "tbase", s_cfg.topic_base);
    nvs_set_str(s_nvs, "hname", s_cfg.hostname);
    esp_err_t err = nvs_commit(s_nvs);
    ESP_LOGI(TAG, "saved err=%s", esp_err_to_name(err));
    return err;
}

esp_err_t hub_settings_clear(void)
{
    if (!s_open) {
        return ESP_ERR_INVALID_STATE;
    }
    nvs_erase_all(s_nvs);
    nvs_commit(s_nvs);
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.mqtt_port = 1883;
    strncpy(s_cfg.topic_base, HUB_DEFAULT_TOPIC_BASE, sizeof(s_cfg.topic_base) - 1);
    strncpy(s_cfg.hostname, HUB_DEFAULT_HOSTNAME, sizeof(s_cfg.hostname) - 1);
    return ESP_OK;
}

bool hub_settings_has_wifi(void)
{
    return s_cfg.wifi_ssid[0] != 0;
}

bool hub_settings_has_mqtt(void)
{
    return s_cfg.mqtt_host[0] != 0;
}

char *hub_settings_to_json_public(void)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "configured", s_cfg.configured);
    cJSON_AddStringToObject(o, "wifi_ssid", s_cfg.wifi_ssid);
    cJSON_AddBoolToObject(o, "wifi_pass_set", s_cfg.wifi_pass[0] != 0);
    cJSON_AddStringToObject(o, "mqtt_host", s_cfg.mqtt_host);
    cJSON_AddNumberToObject(o, "mqtt_port", s_cfg.mqtt_port);
    cJSON_AddStringToObject(o, "mqtt_user", s_cfg.mqtt_user);
    cJSON_AddBoolToObject(o, "mqtt_pass_set", s_cfg.mqtt_pass[0] != 0);
    cJSON_AddStringToObject(o, "topic_base", s_cfg.topic_base);
    cJSON_AddStringToObject(o, "hostname", s_cfg.hostname);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}
