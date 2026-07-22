/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Radio Co-Processor (RCP) Example
 *
 * This example code is in the Public Domain (or CC0-1.0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include <unistd.h>

#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_openthread.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/task.h"

#if CONFIG_EXTERNAL_COEX_ENABLE
#include "esp_coexist.h"
#endif

#if !SOC_IEEE802154_SUPPORTED
#error "RCP is only supported for the SoCs which have IEEE 802.15.4 module"
#endif

#define TAG "ot_esp_rcp"

/* Plain GPIO status LED on the RCP (ESP32-H2) board — brief flash every 2s
 * while the RCP app is alive/running. Matches known-good wiring from prior
 * hardware bring-up (normal LED on GPIO22, not addressable). */
#define RCP_LED_GPIO 22
/* Board's LED is active-low (sinks to light) — verified on hardware. */
#define RCP_LED_ON  0
#define RCP_LED_OFF 1
#define RCP_LED_HEARTBEAT_ON_MS   100
#define RCP_LED_HEARTBEAT_OFF_MS  1900

extern void otAppNcpInit(otInstance *instance);

static void rcp_led_task(void *arg)
{
    (void)arg;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RCP_LED_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_set_level(RCP_LED_GPIO, RCP_LED_OFF);

    while (1) {
        gpio_set_level(RCP_LED_GPIO, RCP_LED_ON);
        vTaskDelay(pdMS_TO_TICKS(RCP_LED_HEARTBEAT_ON_MS));
        gpio_set_level(RCP_LED_GPIO, RCP_LED_OFF);
        vTaskDelay(pdMS_TO_TICKS(RCP_LED_HEARTBEAT_OFF_MS));
    }
}

#if CONFIG_EXTERNAL_COEX_ENABLE
#if SOC_EXTERNAL_COEX_ADVANCE
static void ot_external_coexist_init(void)
{
    esp_external_coex_gpio_set_t gpio_pin = ESP_OPENTHREAD_DEFAULT_EXTERNAL_COEX_CONFIG();
    esp_external_coex_set_work_mode(EXTERNAL_COEX_FOLLOWER_ROLE);
    ESP_ERROR_CHECK(esp_enable_extern_coex_gpio_pin(CONFIG_EXTERNAL_COEX_WIRE_TYPE, gpio_pin));
}
#endif // SOC_EXTERNAL_COEX_ADVANCE
#endif // CONFIG_EXTERNAL_COEX_ENABLE

static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_EXTERNAL_COEX_ENABLE
    ot_external_coexist_init();
#endif // CONFIG_EXTERNAL_COEX_ENABLE

    // Initialize the OpenThread ncp
    otAppNcpInit(esp_openthread_get_instance());

    // Run the main loop
    esp_openthread_launch_mainloop();

    // Clean up
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Used eventfds:
    // * ot task queue
    // * radio driver
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 2,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(rcp_led_task, "rcp_led", 1536, NULL, 1, NULL);
    xTaskCreate(ot_task_worker, "ot_rcp_main", 3072, xTaskGetCurrentTaskHandle(), 5, NULL);
}
