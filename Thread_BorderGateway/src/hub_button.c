#include "hub_button.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_config.h"
#include "hub_settings.h"

static const char *TAG = "btn";

static bool pressed(void)
{
    return gpio_get_level(HUB_FACTORY_BTN_GPIO) == 0;
}

static void do_factory_reset(const char *reason)
{
    ESP_LOGW(TAG, "FACTORY RESET (%s) - clearing Wi-Fi/MQTT, reboot to setup AP", reason);
    hub_settings_clear();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

bool hub_button_check_boot_factory_reset(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HUB_FACTORY_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    if (!pressed()) {
        return false;
    }

    ESP_LOGW(TAG, "BOOT held at power-on - keep holding %d ms for factory reset",
             HUB_FACTORY_BOOT_HOLD_MS);
    int held = 0;
    const int step = 50;
    while (pressed() && held < HUB_FACTORY_BOOT_HOLD_MS) {
        vTaskDelay(pdMS_TO_TICKS(step));
        held += step;
        if ((held % 1000) < step) {
            ESP_LOGW(TAG, "factory reset in %d ms ...", HUB_FACTORY_BOOT_HOLD_MS - held);
        }
    }
    if (held >= HUB_FACTORY_BOOT_HOLD_MS && pressed()) {
        do_factory_reset("boot hold");
        return true;
    }
    ESP_LOGI(TAG, "boot hold released early - normal start");
    return false;
}

static void btn_task(void *arg)
{
    (void)arg;
    int held_ms = 0;
    bool armed = true;
    while (1) {
        if (pressed()) {
            held_ms += 50;
            if (armed && held_ms >= HUB_FACTORY_HOLD_MS) {
                armed = false;
                do_factory_reset("runtime hold");
            } else if (held_ms == 1000 || held_ms == 2000 || held_ms == 3000 || held_ms == 4000) {
                ESP_LOGW(TAG, "hold BOOT %d/%d ms for factory reset", held_ms, HUB_FACTORY_HOLD_MS);
            }
        } else {
            held_ms = 0;
            armed = true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t hub_button_start(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HUB_FACTORY_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    xTaskCreate(btn_task, "hub_btn", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "factory reset: hold BOOT GPIO%d for %d ms", HUB_FACTORY_BTN_GPIO,
             HUB_FACTORY_HOLD_MS);
    return ESP_OK;
}
