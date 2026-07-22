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
} hub_device_t;

void registry_init(void);
int registry_count(void);
const hub_device_t *registry_get_all(int *count);
const hub_device_t *registry_find(const char *id);
bool registry_upsert(const hub_device_t *dev);
bool registry_remove(const char *id);
bool registry_set_online(const char *id, bool online);
void registry_touch(const char *id, int64_t ts);
/** Caller frees with free(). */
char *registry_to_json(void);
void registry_save(void);
void registry_load(void);
