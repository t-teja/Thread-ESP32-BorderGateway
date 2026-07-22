#include "device_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    if (nvs_open("hub_reg", NVS_READWRITE, &s_nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    registry_load();
}

int registry_count(void)
{
    int n = 0;
    for (int i = 0; i < HUB_REGISTRY_MAX; i++)
        if (s_devs[i].in_use) n++;
    return n;
}

int registry_online_count(void)
{
    int n = 0;
    for (int i = 0; i < HUB_REGISTRY_MAX; i++)
        if (s_devs[i].in_use && s_devs[i].online) n++;
    return n;
}

const hub_device_t *registry_get_all(int *count)
{
    if (count) *count = HUB_REGISTRY_MAX;
    return s_devs;
}

const hub_device_t *registry_find(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < HUB_REGISTRY_MAX; i++)
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) return &s_devs[i];
    return NULL;
}

bool registry_upsert(const hub_device_t *dev)
{
    if (!dev || !dev->id[0]) return false;
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, dev->id) == 0) {
            /* preserve telemetry */
            float t = s_devs[i].t, h = s_devs[i].h;
            int bat = s_devs[i].bat, rssi = s_devs[i].rssi;
            char contact[12];
            memcpy(contact, s_devs[i].contact, sizeof(contact));
            bool has = s_devs[i].has_telem;
            s_devs[i] = *dev;
            s_devs[i].in_use = true;
            s_devs[i].t = t; s_devs[i].h = h; s_devs[i].bat = bat; s_devs[i].rssi = rssi;
            memcpy(s_devs[i].contact, contact, sizeof(contact));
            s_devs[i].has_telem = has;
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
            if (online) s_devs[i].last_seen = (int64_t)time(NULL);
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

void registry_update_telemetry(const char *id, float t, float h, int bat, int rssi, const char *contact)
{
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (s_devs[i].in_use && strcmp(s_devs[i].id, id) == 0) {
            s_devs[i].t = t;
            s_devs[i].h = h;
            s_devs[i].bat = bat;
            s_devs[i].rssi = rssi;
            if (contact) {
                strncpy(s_devs[i].contact, contact, sizeof(s_devs[i].contact) - 1);
            }
            s_devs[i].has_telem = true;
            s_devs[i].online = true;
            s_devs[i].last_seen = (int64_t)time(NULL);
            return;
        }
    }
}

static void add_dev_json(cJSON *o, const hub_device_t *d)
{
    cJSON_AddStringToObject(o, "id", d->id);
    cJSON_AddStringToObject(o, "type", d->type);
    cJSON_AddStringToObject(o, "name", d->name);
    cJSON_AddStringToObject(o, "room", d->room);
    cJSON_AddStringToObject(o, "fw", d->fw);
    cJSON_AddStringToObject(o, "mac", d->mac);
    cJSON_AddNumberToObject(o, "last_seen", (double)d->last_seen);
    cJSON_AddBoolToObject(o, "online", d->online);
    cJSON_AddBoolToObject(o, "has_telem", d->has_telem);
    if (d->has_telem) {
        if (strcmp(d->type, "temp_hum") == 0) {
            cJSON_AddNumberToObject(o, "t", d->t);
            cJSON_AddNumberToObject(o, "h", d->h);
        }
        if (d->contact[0]) cJSON_AddStringToObject(o, "contact", d->contact);
        cJSON_AddNumberToObject(o, "bat", d->bat);
        cJSON_AddNumberToObject(o, "rssi", d->rssi);
    }
}

char *registry_to_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) continue;
        cJSON *o = cJSON_CreateObject();
        add_dev_json(o, &s_devs[i]);
        cJSON_AddItemToArray(arr, o);
    }
    char *s = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return s;
}

char *registry_map_json(const char *hub_id, const char *thread_role)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "hub_id", hub_id ? hub_id : "hub");
    cJSON_AddStringToObject(root, "thread_role", thread_role ? thread_role : "unknown");
    cJSON *nodes = cJSON_CreateArray();
    cJSON *hub = cJSON_CreateObject();
    cJSON_AddStringToObject(hub, "id", hub_id ? hub_id : "hub");
    cJSON_AddStringToObject(hub, "kind", "hub");
    cJSON_AddStringToObject(hub, "label", "Thread Hub");
    cJSON_AddBoolToObject(hub, "online", true);
    cJSON_AddItemToArray(nodes, hub);
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) continue;
        cJSON *n = cJSON_CreateObject();
        cJSON_AddStringToObject(n, "id", s_devs[i].id);
        cJSON_AddStringToObject(n, "kind", "sensor");
        cJSON_AddStringToObject(n, "type", s_devs[i].type);
        char label[96];
        snprintf(label, sizeof(label), "%s", s_devs[i].name[0] ? s_devs[i].name : s_devs[i].id);
        cJSON_AddStringToObject(n, "label", label);
        cJSON_AddStringToObject(n, "room", s_devs[i].room);
        cJSON_AddBoolToObject(n, "online", s_devs[i].online);
        cJSON_AddBoolToObject(n, "has_telem", s_devs[i].has_telem);
        if (s_devs[i].has_telem) {
            cJSON_AddNumberToObject(n, "t", s_devs[i].t);
            cJSON_AddNumberToObject(n, "h", s_devs[i].h);
            if (s_devs[i].contact[0]) cJSON_AddStringToObject(n, "contact", s_devs[i].contact);
        }
        cJSON_AddItemToArray(nodes, n);
    }
    cJSON_AddItemToObject(root, "nodes", nodes);
    cJSON *edges = cJSON_CreateArray();
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) continue;
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "from", hub_id ? hub_id : "hub");
        cJSON_AddStringToObject(e, "to", s_devs[i].id);
        cJSON_AddStringToObject(e, "link", s_devs[i].online ? "thread" : "offline");
        cJSON_AddItemToArray(edges, e);
    }
    cJSON_AddItemToObject(root, "edges", edges);

    /* mermaid source string */
    char mm[2048];
    size_t off = 0;
    off += snprintf(mm + off, sizeof(mm) - off, "flowchart TD\n");
    off += snprintf(mm + off, sizeof(mm) - off, "  H[\"Thread Hub\\n%s\"]:::hub\n",
                    thread_role ? thread_role : "");
    for (int i = 0; i < HUB_REGISTRY_MAX && off + 120 < sizeof(mm); i++) {
        if (!s_devs[i].in_use) continue;
        char telem[48] = "";
        if (s_devs[i].has_telem && strcmp(s_devs[i].type, "temp_hum") == 0) {
            snprintf(telem, sizeof(telem), "\\n%.1fC %.0f%%", s_devs[i].t, s_devs[i].h);
        } else if (s_devs[i].has_telem && s_devs[i].contact[0]) {
            snprintf(telem, sizeof(telem), "\\n%s", s_devs[i].contact);
        }
        const char *cls = s_devs[i].online ? "online" : "offline";
        off += snprintf(mm + off, sizeof(mm) - off, "  %s[\"%s%s\"]:::%s\n",
                        s_devs[i].id, s_devs[i].name[0] ? s_devs[i].name : s_devs[i].id, telem, cls);
        off += snprintf(mm + off, sizeof(mm) - off, "  H --- %s\n", s_devs[i].id);
    }
    off += snprintf(mm + off, sizeof(mm) - off,
                    "  classDef hub fill:#5b8cff,stroke:#1e3a8a,color:#fff\n"
                    "  classDef online fill:#163527,stroke:#34d399,color:#e7fff2\n"
                    "  classDef offline fill:#3a2a12,stroke:#fbbf24,color:#fff7e6\n");
    cJSON_AddStringToObject(root, "mermaid", mm);

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

void registry_save(void)
{
    if (!s_nvs) return;
    /* persist identity only */
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < HUB_REGISTRY_MAX; i++) {
        if (!s_devs[i].in_use) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", s_devs[i].id);
        cJSON_AddStringToObject(o, "type", s_devs[i].type);
        cJSON_AddStringToObject(o, "name", s_devs[i].name);
        cJSON_AddStringToObject(o, "room", s_devs[i].room);
        cJSON_AddStringToObject(o, "fw", s_devs[i].fw);
        cJSON_AddStringToObject(o, "mac", s_devs[i].mac);
        cJSON_AddNumberToObject(o, "last_seen", (double)s_devs[i].last_seen);
        cJSON_AddItemToArray(arr, o);
    }
    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return;
    nvs_set_str(s_nvs, "devices", json);
    nvs_commit(s_nvs);
    free(json);
}

void registry_load(void)
{
    if (!s_nvs) return;
    size_t len = 0;
    if (nvs_get_str(s_nvs, "devices", NULL, &len) != ESP_OK || len == 0) return;
    char *buf = malloc(len);
    if (!buf) return;
    if (nvs_get_str(s_nvs, "devices", buf, &len) != ESP_OK) { free(buf); return; }
    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return; }
    memset(s_devs, 0, sizeof(s_devs));
    int idx = 0;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, arr) {
        if (idx >= HUB_REGISTRY_MAX) break;
        hub_device_t *d = &s_devs[idx];
        const cJSON *j;
        j = cJSON_GetObjectItem(it, "id"); if (cJSON_IsString(j)) strncpy(d->id, j->valuestring, sizeof(d->id)-1);
        j = cJSON_GetObjectItem(it, "type"); if (cJSON_IsString(j)) strncpy(d->type, j->valuestring, sizeof(d->type)-1);
        j = cJSON_GetObjectItem(it, "name"); if (cJSON_IsString(j)) strncpy(d->name, j->valuestring, sizeof(d->name)-1);
        j = cJSON_GetObjectItem(it, "room"); if (cJSON_IsString(j)) strncpy(d->room, j->valuestring, sizeof(d->room)-1);
        j = cJSON_GetObjectItem(it, "fw"); if (cJSON_IsString(j)) strncpy(d->fw, j->valuestring, sizeof(d->fw)-1);
        j = cJSON_GetObjectItem(it, "mac"); if (cJSON_IsString(j)) strncpy(d->mac, j->valuestring, sizeof(d->mac)-1);
        j = cJSON_GetObjectItem(it, "last_seen"); if (cJSON_IsNumber(j)) d->last_seen = (int64_t)j->valuedouble;
        d->online = false;
        d->in_use = d->id[0] != 0;
        if (d->in_use) idx++;
    }
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "loaded %d devices", idx);
}
