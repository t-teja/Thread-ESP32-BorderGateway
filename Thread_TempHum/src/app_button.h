#pragma once
#include <stdbool.h>
typedef void (*button_hold_cb_t)(void);
typedef void (*button_click_cb_t)(void);
void app_button_init(button_hold_cb_t on_hold_pair, button_click_cb_t on_click);
bool app_button_is_pressed(void);
