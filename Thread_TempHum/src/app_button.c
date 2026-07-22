#include "app_button.h"

#include "board.h"
#include "device_types.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "btn";
static button_hold_cb_t s_cb;

bool app_button_is_pressed(void)
{
    return gpio_get_level(BOARD_BUTTON_GPIO) == 0; /* active low */
}

static void btn_task(void *arg)
{
    int held_ms = 0;
    bool fired = false;
    while (1) {
        if (app_button_is_pressed()) {
            held_ms += 20;
            if (!fired && held_ms >= HUB_PAIR_BTN_HOLD_MS) {
                fired = true;
                ESP_LOGI(TAG, "pair hold detected");
                if (s_cb) {
                    s_cb();
                }
            }
        } else {
            held_ms = 0;
            fired = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_button_init(button_hold_cb_t on_hold_pair)
{
    s_cb = on_hold_pair;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    xTaskCreate(btn_task, "btn", 2048, NULL, 5, NULL);
}
