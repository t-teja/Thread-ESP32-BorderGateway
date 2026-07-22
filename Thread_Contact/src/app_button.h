#pragma once

#include <stdbool.h>

typedef void (*button_hold_cb_t)(void);

void app_button_init(button_hold_cb_t on_hold_pair);
/** Factory reset if held > 10s at boot handled in main if needed */
bool app_button_is_pressed(void);
