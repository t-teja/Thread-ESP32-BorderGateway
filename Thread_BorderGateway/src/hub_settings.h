#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#define HUB_CFG_SSID_LEN     33
#define HUB_CFG_PASS_LEN     65
#define HUB_CFG_HOST_LEN     64
#define HUB_CFG_USER_LEN     32
#define HUB_CFG_BASE_LEN     16
#define HUB_CFG_NAME_LEN     32

typedef struct {
    char wifi_ssid[HUB_CFG_SSID_LEN];
    char wifi_pass[HUB_CFG_PASS_LEN];
    char mqtt_host[HUB_CFG_HOST_LEN];
    uint16_t mqtt_port;
    char mqtt_user[HUB_CFG_USER_LEN];
    char mqtt_pass[HUB_CFG_PASS_LEN];
    char topic_base[HUB_CFG_BASE_LEN];
    char hostname[HUB_CFG_NAME_LEN];
    bool configured; /* true when saved via portal or NVS */
} hub_settings_t;

void hub_settings_init(void);
const hub_settings_t *hub_settings_get(void);
hub_settings_t *hub_settings_mut(void);

/** Load from NVS; merge optional compile-time defaults if empty. */
void hub_settings_load(void);
esp_err_t hub_settings_save(const hub_settings_t *s);
esp_err_t hub_settings_clear(void);

bool hub_settings_has_wifi(void);
bool hub_settings_has_mqtt(void);

/** JSON object for API (passwords masked). Caller frees. */
char *hub_settings_to_json_public(void);
