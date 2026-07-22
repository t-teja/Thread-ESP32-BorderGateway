#include "wifi_net.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "hub_config.h"
#include "hub_settings.h"

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_events;
static const int WIFI_OK = BIT0;
static const int WIFI_FAIL = BIT1;
static char s_ip[16];
static char s_ap_ssid[33];
static bool s_connected;
static bool s_netif_ready;
static wifi_net_mode_t s_mode = WIFI_NET_MODE_NONE;
static int s_retry;
static const int MAX_RETRY = 10;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_mode == WIFI_NET_MODE_STA) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        s_ip[0] = 0;
        if (s_mode == WIFI_NET_MODE_STA) {
            if (s_retry < MAX_RETRY) {
                s_retry++;
                ESP_LOGW(TAG, "STA disconnect, retry %d/%d", s_retry, MAX_RETRY);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "STA failed after %d retries", MAX_RETRY);
                xEventGroupSetBits(s_wifi_events, WIFI_FAIL);
            }
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "softAP up: %s", s_ap_ssid);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry = 0;
        ESP_LOGI(TAG, "STA ip %s", s_ip);
        xEventGroupSetBits(s_wifi_events, WIFI_OK);
    }
}

static void ensure_wifi_stack(void)
{
    if (!s_netif_ready) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_netif_ready = true;
    }
    static bool wifi_inited;
    if (!wifi_inited) {
        wifi_init_config_t winit = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&winit));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi, NULL));
        wifi_inited = true;
    }
}

static esp_err_t do_sta(const hub_settings_t *cfg)
{
    s_mode = WIFI_NET_MODE_STA;
    s_retry = 0;
    xEventGroupClearBits(s_wifi_events, WIFI_OK | WIFI_FAIL);

    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    esp_netif_set_hostname(s_sta_netif, cfg->hostname[0] ? cfg->hostname : HUB_DEFAULT_HOSTNAME);

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid, cfg->wifi_ssid, sizeof(wcfg.sta.ssid));
    strncpy((char *)wcfg.sta.password, cfg->wifi_pass, sizeof(wcfg.sta.password));
    wcfg.sta.threshold.authmode = cfg->wifi_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_OK | WIFI_FAIL, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(60000));
    if (bits & WIFI_OK) {
        return ESP_OK;
    }
    esp_wifi_stop();
    return ESP_ERR_TIMEOUT;
}

static esp_err_t do_ap(void)
{
    s_mode = WIFI_NET_MODE_AP;
    s_connected = false;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s-%02X%02X", HUB_SETUP_AP_PREFIX, mac[4], mac[5]);

    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }
    esp_netif_ip_info_t ip = {0};
    IP4_ADDR(&ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(s_ap_netif);
    esp_netif_set_ip_info(s_ap_netif, &ip);
    esp_netif_dhcps_start(s_ap_netif);

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.ap.ssid, s_ap_ssid, sizeof(wcfg.ap.ssid));
    wcfg.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    wcfg.ap.channel = 6;
    wcfg.ap.max_connection = 4;
    wcfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    strncpy(s_ip, HUB_AP_IP_ADDR, sizeof(s_ip) - 1);
    ESP_LOGW(TAG, "Setup AP '%s' — open http://%s/", s_ap_ssid, s_ip);
    return ESP_OK;
}

esp_err_t wifi_net_start(void)
{
    s_wifi_events = xEventGroupCreate();
    ensure_wifi_stack();

    const hub_settings_t *cfg = hub_settings_get();
    if (hub_settings_has_wifi()) {
        ESP_LOGI(TAG, "trying STA ssid='%s'", cfg->wifi_ssid);
        if (do_sta(cfg) == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "STA failed — starting setup portal so you can fix credentials");
    }
    return do_ap();
}

bool wifi_net_is_connected(void)
{
    return s_mode == WIFI_NET_MODE_STA && s_connected;
}

bool wifi_net_is_ap_mode(void)
{
    return s_mode == WIFI_NET_MODE_AP;
}

wifi_net_mode_t wifi_net_get_mode(void)
{
    return s_mode;
}

const char *wifi_net_get_ip(void)
{
    return s_ip;
}

const char *wifi_net_get_ap_ssid(void)
{
    return s_ap_ssid;
}

void wifi_net_get_hub_id(char *out, size_t out_len)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_len, "hub-%02x%02x%02x", mac[3], mac[4], mac[5]);
}
