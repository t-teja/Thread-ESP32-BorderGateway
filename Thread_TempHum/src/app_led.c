#include "app_led.h"
#include "board.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static led_mode_t s_mode = LED_OFF;

static void apply(bool on)
{
    gpio_set_level(BOARD_LED_GPIO, on ? BOARD_LED_ACTIVE_LEVEL : !BOARD_LED_ACTIVE_LEVEL);
}

static void led_task(void *arg)
{
    while (1) {
        switch (s_mode) {
        case LED_BLINK_SLOW:
            apply(true); vTaskDelay(pdMS_TO_TICKS(100));
            apply(false); vTaskDelay(pdMS_TO_TICKS(900));
            break;
        case LED_BLINK_FAST:
            apply(true); vTaskDelay(pdMS_TO_TICKS(80));
            apply(false); vTaskDelay(pdMS_TO_TICKS(80));
            break;
        case LED_ON:
            apply(true); vTaskDelay(pdMS_TO_TICKS(200));
            break;
        default:
            apply(false); vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
    }
}

void app_led_init(void)
{
    gpio_config_t io = {.pin_bit_mask = 1ULL << BOARD_LED_GPIO, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&io);
    apply(false);
    xTaskCreate(led_task, "led", 2048, NULL, 3, NULL);
}

void app_led_set(led_mode_t mode) { s_mode = mode; }
