/**
 * Thread Border Gateway hub (ESP32-S3).
 * Factory reset: hold BOOT 8s at power-on, or 5s while running -> captive portal.
 */
#include <stdio.h>

#include "ble_central.h"
#include "device_registry.h"
#include "dns_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_button.h"
#include "hub_config.h"
#include "hub_settings.h"
#include "mqtt_bridge.h"
#include "nvs_flash.h"
#include "otbr_net.h"
#include "pairing.h"
#include "web_server.h"
#include "wifi_net.h"

static const char *TAG = "hub";

void app_main(void)
{
    ESP_LOGI(TAG, "Thread Hub %s", HUB_FW_VERSION);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    hub_settings_init();
    hub_button_check_boot_factory_reset();

    registry_init();
    pairing_init();
    hub_button_start();

    err = wifi_net_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed");
    }

    web_server_start();

    if (wifi_net_is_ap_mode()) {
        dns_server_start();
        ESP_LOGW(TAG, "Captive portal active - SSID '%s', open http://%s/", wifi_net_get_ap_ssid(),
                 wifi_net_get_ip());
    } else {
        otbr_net_init();
        ble_central_init();
        if (hub_settings_has_mqtt()) {
            const hub_settings_t *s = hub_settings_get();
            ESP_LOGI(TAG, "MQTT target %s:%u", s->mqtt_host, (unsigned)s->mqtt_port);
            mqtt_bridge_start();
        } else {
            ESP_LOGW(TAG, "No MQTT host configured - use Settings");
        }
        char hub_id[32];
        wifi_net_get_hub_id(hub_id, sizeof(hub_id));
        ESP_LOGI(TAG, "Hub ready id=%s ip=%s", hub_id, wifi_net_get_ip());
        ESP_LOGI(TAG, "Dashboard http://%s/", wifi_net_get_ip());
        ESP_LOGI(TAG, "Factory reset: hold BOOT %d ms (or Settings page)", HUB_FACTORY_HOLD_MS);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (mqtt_bridge_is_connected()) {
            mqtt_bridge_publish_info();
        }
    }
}
