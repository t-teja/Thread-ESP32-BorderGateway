/** ESP32-H2 contact sensor — BLE pair, CoAP to hub on change + heartbeat. */
#include <string.h>
#include "app_button.h"
#include "app_led.h"
#include "ble_peripheral.h"
#include "board.h"
#include "coap_sensor.h"
#include "contact_input.h"
#include "device_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sensor_nvs.h"
#include "thread_net.h"

static const char *TAG = "main";
static sensor_cfg_t s_cfg;
static bool s_run_net;

static void start_network_path(void)
{
    ble_peripheral_stop();
    thread_net_start(&s_cfg);
    esp_netif_init();
    esp_event_loop_create_default();
    coap_sensor_start(&s_cfg);
    app_led_set(LED_ON);
    s_run_net = true;
}

static void on_provisioned(const sensor_cfg_t *cfg)
{
    s_cfg = *cfg;
    start_network_path();
}

static void on_pair_hold(void)
{
    app_led_set(LED_BLINK_FAST);
    ble_peripheral_start_pairing(on_provisioned);
}

static void contact_task(void *arg)
{
    bool last_open = contact_input_is_open();
    int hb = 0;
    /* CoAP has no persistent "connected" session to gate on (unlike MQTT) —
     * always report; coap_sensor_is_connected() only reflects the last ack. */
    if (s_run_net) {
        coap_sensor_publish_contact(contact_input_state_str(), 100, -50);
    }
    while (1) {
        bool open = contact_input_is_open();
        if (s_run_net) {
            if (open != last_open) {
                last_open = open;
                coap_sensor_publish_contact(contact_input_state_str(), 100, -50);
                app_led_set(open ? LED_BLINK_FAST : LED_ON);
                vTaskDelay(pdMS_TO_TICKS(300));
                app_led_set(LED_ON);
            }
            hb += 50;
            if (hb >= SENSOR_HEARTBEAT_MS) {
                hb = 0;
                coap_sensor_publish_contact(contact_input_state_str(), 100, -50);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
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
    contact_input_init();

    if (app_button_is_pressed()) {
        for (int i = 0; i < 500 && app_button_is_pressed(); i++) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (i == 499) {
                sensor_nvs_clear();
                esp_restart();
            }
        }
    }

    if (sensor_nvs_load(&s_cfg)) {
        start_network_path();
    } else {
        app_led_set(LED_BLINK_SLOW);
    }
    xTaskCreate(contact_task, "contact", 4096, NULL, 4, NULL);
}
