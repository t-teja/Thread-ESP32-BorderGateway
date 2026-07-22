#include "hub_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_config.h"

static const char *TAG = "hub_led";
static int s_flashes;

static void set_led(bool on)
{
    int level = on ? HUB_STATUS_LED_ACTIVE : !HUB_STATUS_LED_ACTIVE;
    gpio_set_level(HUB_STATUS_LED_GPIO, level);
}

static void led_task(void *arg)
{
    (void)arg;
    while (1) {
        if (s_flashes > 0) {
            int n = s_flashes;
            s_flashes = 0;
            ESP_LOGI(TAG, "identify %d flashes", n);
            for (int i = 0; i < n; i++) {
                set_led(true);
                vTaskDelay(pdMS_TO_TICKS(150));
                set_led(false);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void hub_led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << HUB_STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    set_led(false);
    xTaskCreate(led_task, "hub_led", 2048, NULL, 3, NULL);
}

void hub_led_identify(int flashes)
{
    if (flashes < 1) flashes = 3;
    if (flashes > 20) flashes = 20;
    s_flashes = flashes;
}
