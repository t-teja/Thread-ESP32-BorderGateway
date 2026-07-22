#pragma once
#include <esp_err.h>
void hub_led_init(void);
/** Blink status LED n times (non-blocking request). */
void hub_led_identify(int flashes);
/**
 * Blink status LED n times, blocking the calling task.
 * Safe to call before hub_led_init() / very early in boot (e.g. RCP flash
 * result) since it configures the GPIO itself if needed.
 */
void hub_led_blink_sync(int count, int on_ms, int off_ms);
