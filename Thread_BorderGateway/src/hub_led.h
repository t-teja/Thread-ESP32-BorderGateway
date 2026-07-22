#pragma once
#include <esp_err.h>
void hub_led_init(void);
/** Blink status LED n times (non-blocking request). */
void hub_led_identify(int flashes);
