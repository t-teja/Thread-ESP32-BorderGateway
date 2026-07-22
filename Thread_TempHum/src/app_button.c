#include "app_button.h"
#include "board.h"
#include "device_types.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "btn";
static button_hold_cb_t s_hold_cb;
static button_click_cb_t s_click_cb;

bool app_button_is_pressed(void) { return gpio_get_level(BOARD_BUTTON_GPIO) == 0; }

static void btn_task(void *arg)
{
    (void)arg;
    int held_ms = 0;
    bool hold_fired = false;
    while (1) {
        if (app_button_is_pressed()) {
            held_ms += 20;
            if (!hold_fired && held_ms >= HUB_PAIR_BTN_HOLD_MS) {
                hold_fired = true;
                ESP_LOGI(TAG, "pair hold");
                if (s_hold_cb) s_hold_cb();
            }
        } else {
            if (held_ms > 40 && held_ms < HUB_PAIR_BTN_HOLD_MS && !hold_fired) {
                ESP_LOGI(TAG, "click");
                if (s_click_cb) s_click_cb();
            }
            held_ms = 0;
            hold_fired = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_button_init(button_hold_cb_t on_hold_pair, button_click_cb_t on_click)
{
    s_hold_cb = on_hold_pair;
    s_click_cb = on_click;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    xTaskCreate(btn_task, "btn", 3072, NULL, 5, NULL);
}
