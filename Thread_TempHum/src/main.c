/** ESP32-H2 Temp/Humidity — BLE pair, Thread stub, MQTT telemetry. */
#include <string.h>
#include "app_button.h"
#include "app_led.h"
#include "ble_peripheral.h"
#include "board.h"
#include "device_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_sensor.h"
#include "nvs_flash.h"
#include "sensor_nvs.h"
#include "sht_sensor.h"
#include "thread_net.h"

static const char *TAG = "main";
static sensor_cfg_t s_cfg;
static bool s_run_net;

static void start_network_path(void)
{
    ble_peripheral_stop();
    app_led_set(LED_BLINK_SLOW);
    thread_net_start(&s_cfg);
    esp_netif_init();
    esp_event_loop_create_default();
    mqtt_sensor_start(&s_cfg);
    app_led_set(LED_ON);
    s_run_net = true;
}

static void on_provisioned(const sensor_cfg_t *cfg)
{
    ESP_LOGI(TAG, "provisioned as %s (%s)", cfg->device_id, cfg->name);
    s_cfg = *cfg;
    start_network_path();
}

static void on_pair_hold(void)
{
    ESP_LOGI(TAG, "pairing mode");
    app_led_set(LED_BLINK_FAST);
    ble_peripheral_start_pairing(on_provisioned);
}

static void telemetry_task(void *arg)
{
    while (1) {
        if (s_run_net && mqtt_sensor_is_connected()) {
            float t = 0, h = 0;
            sht_sensor_read(&t, &h);
            mqtt_sensor_publish_state(t, h, 100, -50);
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PUBLISH_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "%s %s", SENSOR_PRODUCT_NAME, SENSOR_FW_VERSION);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    sensor_nvs_init();
    app_led_init();
    app_button_init(on_pair_hold);
    sht_sensor_init();

    if (app_button_is_pressed()) {
        for (int i = 0; i < 500 && app_button_is_pressed(); i++) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (i == 499) {
                ESP_LOGW(TAG, "factory reset");
                sensor_nvs_clear();
                esp_restart();
            }
        }
    }

    if (sensor_nvs_load(&s_cfg)) {
        ESP_LOGI(TAG, "paired: %s", s_cfg.device_id);
        start_network_path();
    } else {
        ESP_LOGI(TAG, "unpaired — hold btn %d ms", HUB_PAIR_BTN_HOLD_MS);
        app_led_set(LED_BLINK_SLOW);
    }
    xTaskCreate(telemetry_task, "tele", 4096, NULL, 4, NULL);
}
