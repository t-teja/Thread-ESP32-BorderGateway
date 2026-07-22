#include "pairing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ble_central.h"
#include "cJSON.h"
#include "device_registry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hub_config.h"
#include "hub_settings.h"
#include "mqtt_bridge.h"
#include "otbr_net.h"
#include "util_slug.h"

static const char *TAG = "pair";
static bool s_open;
static int64_t s_deadline_us;
static esp_timer_handle_t s_timer;

static void on_timeout(void *arg)
{
    ESP_LOGW(TAG, "pairing window closed");
    pairing_close_window();
}

void pairing_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = &on_timeout,
        .name = "pair_to",
    };
    esp_timer_create(&args, &s_timer);
}

esp_err_t pairing_open_window(int seconds)
{
    if (seconds <= 0) {
        seconds = HUB_PAIR_DEFAULT_SEC;
    }
    pairing_close_window();
    esp_err_t err = ble_central_start_scan();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BLE scan start failed (%s) — will retry candidates empty", esp_err_to_name(err));
    }
    s_open = true;
    s_deadline_us = esp_timer_get_time() + (int64_t)seconds * 1000000LL;
    esp_timer_start_once(s_timer, (uint64_t)seconds * 1000000ULL);
    ESP_LOGI(TAG, "pairing open %d s", seconds);
    mqtt_bridge_publish_event("{\"event\":\"pair_open\"}");
    return ESP_OK;
}

void pairing_close_window(void)
{
    if (s_timer) {
        esp_timer_stop(s_timer);
    }
    ble_central_stop_scan();
    if (s_open) {
        mqtt_bridge_publish_event("{\"event\":\"pair_close\"}");
    }
    s_open = false;
}

bool pairing_is_open(void)
{
    return s_open;
}

int pairing_seconds_left(void)
{
    if (!s_open) {
        return 0;
    }
    int64_t left = s_deadline_us - esp_timer_get_time();
    if (left <= 0) {
        return 0;
    }
    return (int)(left / 1000000LL);
}

static void mac_from_addr(const char *addr, uint8_t mac[6])
{
    unsigned int b[6] = {0};
    sscanf(addr, "%02X:%02X:%02X:%02X:%02X:%02X", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)b[i];
    }
}

esp_err_t pairing_pair_device(const char *ble_addr, const char *type_hint, const char *name,
                             const char *room)
{
    if (!ble_addr || !name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_open) {
        ESP_LOGW(TAG, "pair rejected — window closed");
        return ESP_ERR_INVALID_STATE;
    }

    char type[HUB_MAX_TYPE] = {0};
    if (type_hint && type_hint[0]) {
        strncpy(type, type_hint, sizeof(type) - 1);
    } else {
        strncpy(type, HUB_DEVICE_TYPE_UNKNOWN, sizeof(type) - 1);
    }

    /* Prefer type from BLE candidate list */
    ble_candidate_t cands[BLE_MAX_CANDIDATES];
    int n = ble_central_get_candidates(cands, BLE_MAX_CANDIDATES);
    for (int i = 0; i < n; i++) {
        if (strcmp(cands[i].addr, ble_addr) == 0 && cands[i].type[0]) {
            strncpy(type, cands[i].type, sizeof(type) - 1);
            break;
        }
    }

    uint8_t mac[6];
    mac_from_addr(ble_addr, mac);
    char device_id[HUB_MAX_DEVICE_ID];
    hub_make_device_id(type, mac, device_id, sizeof(device_id));

    char dataset[HUB_MAX_DATASET_B64];
    if (!otbr_net_get_dataset_b64(dataset, sizeof(dataset))) {
        ESP_LOGE(TAG, "no thread dataset");
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "cmd", "provision");
    cJSON_AddStringToObject(root, "dataset", dataset);
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "room", room ? room : "");
    cJSON_AddStringToObject(root, "type", type);
    const hub_settings_t *sc = hub_settings_get();
    cJSON_AddStringToObject(root, "mqtt_host", sc->mqtt_host);
    cJSON_AddNumberToObject(root, "mqtt_port", sc->mqtt_port ? sc->mqtt_port : 1883);
    cJSON_AddStringToObject(root, "mqtt_user", sc->mqtt_user);
    cJSON_AddStringToObject(root, "mqtt_pass", sc->mqtt_pass);
    cJSON_AddStringToObject(root, "topic_base", sc->topic_base[0] ? sc->topic_base : HUB_DEFAULT_TOPIC_BASE);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "provisioning %s -> %s", ble_addr, device_id);
    esp_err_t err = ble_central_provision(ble_addr, json);
    free(json);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE provision failed: %s", esp_err_to_name(err));
        return err;
    }

    hub_device_t dev = {0};
    strncpy(dev.id, device_id, sizeof(dev.id) - 1);
    strncpy(dev.type, type, sizeof(dev.type) - 1);
    strncpy(dev.name, name, sizeof(dev.name) - 1);
    strncpy(dev.room, room ? room : "", sizeof(dev.room) - 1);
    strncpy(dev.mac, ble_addr, sizeof(dev.mac) - 1);
    strncpy(dev.fw, "", sizeof(dev.fw) - 1);
    dev.last_seen = (int64_t)time(NULL);
    dev.online = false;
    dev.in_use = true;
    registry_upsert(&dev);
    mqtt_bridge_publish_registry();

    char ev[192];
    snprintf(ev, sizeof(ev),
             "{\"event\":\"paired\",\"id\":\"%s\",\"name\":\"%s\",\"room\":\"%s\"}", device_id,
             name, room ? room : "");
    mqtt_bridge_publish_event(ev);

    /* Keep window open for multi-pair; user can close manually */
    ble_central_start_scan();
    return ESP_OK;
}
