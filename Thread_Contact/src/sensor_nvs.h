#pragma once

#include <stdbool.h>
#include "device_types.h"

typedef struct {
    bool provisioned;
    char device_id[HUB_MAX_DEVICE_ID];
    char name[HUB_MAX_NAME];
    char room[HUB_MAX_ROOM];
    char type[HUB_MAX_TYPE];
    char mqtt_host[HUB_MAX_MQTT_HOST];
    int mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    char topic_base[HUB_MAX_TOPIC_BASE];
    char dataset_b64[HUB_MAX_DATASET_B64];
} sensor_cfg_t;

void sensor_nvs_init(void);
bool sensor_nvs_load(sensor_cfg_t *cfg);
void sensor_nvs_save(const sensor_cfg_t *cfg);
void sensor_nvs_clear(void);
