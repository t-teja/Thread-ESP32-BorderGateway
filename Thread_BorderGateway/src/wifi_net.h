#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

typedef enum {
    WIFI_NET_MODE_NONE = 0,
    WIFI_NET_MODE_STA,
    WIFI_NET_MODE_AP,
} wifi_net_mode_t;

esp_err_t wifi_net_start(void);
bool wifi_net_is_connected(void);
bool wifi_net_is_ap_mode(void);
wifi_net_mode_t wifi_net_get_mode(void);
/** STA IP or AP gateway IP. */
const char *wifi_net_get_ip(void);
void wifi_net_get_hub_id(char *out, size_t out_len);
/** SoftAP SSID currently advertised (setup mode). */
const char *wifi_net_get_ap_ssid(void);
