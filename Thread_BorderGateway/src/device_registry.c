#include "device_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "registry";
static hub_device_t s_devs[HUB_REGISTRY_MAX];
static nvs_handle_t s_nvs;

void registry_init(void)
{
    memset(s_devs, 0, sizeof(s_devs));
    esp_err_t err = nvs_open("hub_reg", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return;
    }
    registry_load();
}

int registry_count(void)
{
    int n = 0;
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use) {
            n++;
        }
    }
    return n;
}

const hub_device_t *registry_get_all(int *count)
{
    if (count) {
        *count = HUB_REGISTRY_MAX;
    }
    return s_devs;
}

const hub_device_t *registry_find(const char *id)
{
    if (!id) {
        return NULL;
    }
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) {
            return &s_devs[i];
        }
    }
    return NULL;
}

bool registry_upsert(const hub_device_t *dev)
{
    if (!dev || !dev->id[0]) {
        return false;
    }
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, dev->id) == 0) {
            s_devs[i] = *dev;
            s_devs[i].in_use = true;
            registry_save();
            return true;
        }
    }
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) {
            s_devs[i] = *dev;
            s_devs[i].in_use = true;
            registry_save();
            return true;
        }
    }
    ESP_LOGE(TAG, "registry full");
    return false;
}

bool registry_remove(const char *id)
{
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) {
            memset(&s_devs[i], 0, sizeof(s_devs[i]));
            registry_save();
            return true;
        }
    }
    return false;
}

bool registry_set_online(const char *id, bool online)
{
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) {
            s_devs[i].online = online;
            return true;
        }
    }
    return false;
}

void registry_touch(const char *id, int64_t ts)
{
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) {
            s_devs[i].last_seen = ts;
            s_devs[i].online = true;
            return;
        }
    }
}

char *registry_to_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) {
            continue;
        }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", s_devs[i].id);
        cJSON_AddStringToObject(o, "type", s_devs[i].type);
        cJSON_AddStringToObject(o, "name", s_devs[i].name);
        cJSON_AddStringToObject(o, "room", s_devs[i].room);
        cJSON_AddStringToObject(o, "fw", s_devs[i].fw);
        cJSON_AddStringToObject(o, "mac", s_devs[i].mac);
        cJSON_AddNumberToObject(o, "last_seen", (double)s_devs[i].last_seen);
        cJSON_AddBoolToObject(o, "online", s_devs[i].online);
        cJSON_AddItemToArray(arr, o);
    }
    char *s = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return s;
}

void registry_save(void)
{
    if (!s_nvs) {
        return;
    }
    char *json = registry_to_json();
    if (!json) {
        return;
    }
    nvs_set_str(s_nvs, "devices", json);
    nvs_commit(s_nvs);
    free(json);
}

void registry_load(void)
{
    if (!s_nvs) {
        return;
    }
    size_t len = 0;
    if (nvs_get_str(s_nvs, "devices", NULL, &len) != ESP_OK || len == 0) {
        return;
    }
    char *buf = malloc(len);
    if (!buf) {
        return;
    }
    if (nvs_get_str(s_nvs, "devices", buf, &len) != ESP_OK) {
        free(buf);
        return;
    }
    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        return;
    }
    memset(s_devs, 0, sizeof(s_devs));
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, arr)
    {
        if (idx >= HUB_REGISTRY_MAX) {
            break;
        }
        hub_device_t *d = &s_devs[idx];
        const cJSON *j;
        j = cJSON_GetObjectItem(it, "id");
        if (cJSON_IsString(j)) {
            strncpy(d->id, j->valuestring, sizeof(d->id) - 1);
        }
        j = cJSON_GetObjectItem(it, "type");
        if (cJSON_IsString(j)) {
            strncpy(d->type, j->valuestring, sizeof(d->type) - 1);
        }
        j = cJSON_GetObjectItem(it, "name");
        if (cJSON_IsString(j)) {
            strncpy(d->name, j->valuestring, sizeof(d->name) - 1);
        }
        j = cJSON_GetObjectItem(it, "room");
        if (cJSON_IsString(j)) {
            strncpy(d->room, j->valuestring, sizeof(d->room) - 1);
        }
        j = cJSON_GetObjectItem(it, "fw");
        if (cJSON_IsString(j)) {
            strncpy(d->fw, j->valuestring, sizeof(d->fw) - 1);
        }
        j = cJSON_GetObjectItem(it, "mac");
        if (cJSON_IsString(j)) {
            strncpy(d->mac, j->valuestring, sizeof(d->mac) - 1);
        }
        j = cJSON_GetObjectItem(it, "last_seen");
        if (cJSON_IsNumber(j)) {
            d->last_seen = (int64_t)j->valuedouble;
        }
        d->online = false;
        d->in_use = d->id[0] != 0;
        if (d->in_use) {
            idx++;
        }
    }
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "loaded %d devices", idx);
}
