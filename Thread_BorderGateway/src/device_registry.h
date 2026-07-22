#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "device_types.h"

typedef struct {
    char id[HUB_MAX_DEVICE_ID];
    char type[HUB_MAX_TYPE];
    char name[HUB_MAX_NAME];
    char room[HUB_MAX_ROOM];
    char fw[HUB_MAX_FW];
    char mac[18];
    int64_t last_seen;
    bool online;
    bool in_use;
    /* live telemetry for dashboard / map */
    float t;
    float h;
    int bat;
    int rssi;
    char contact[12]; /* open/closed or empty */
    bool has_telem;
} hub_device_t;

void registry_init(void);
int registry_count(void);
int registry_online_count(void);
const hub_device_t *registry_get_all(int *count);
const hub_device_t *registry_find(const char *id);
bool registry_upsert(const hub_device_t *dev);
bool registry_remove(const char *id);
bool registry_set_online(const char *id, bool online);
void registry_touch(const char *id, int64_t ts);
void registry_update_telemetry(const char *id, float t, float h, int bat, int rssi, const char *contact);
char *registry_to_json(void);
/** Graph JSON for mermaid/network map. Caller frees. */
char *registry_map_json(const char *hub_id, const char *thread_role);
void registry_save(void);
void registry_load(void);
