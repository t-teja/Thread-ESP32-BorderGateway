#pragma once

typedef enum {
    LED_OFF = 0,
    LED_ON,
    LED_BLINK_SLOW,  /* idle unpaired */
    LED_BLINK_FAST,  /* pairing mode */
    LED_BLINK_OK,    /* short success pattern handled externally */
} led_mode_t;

void app_led_init(void);
void app_led_set(led_mode_t mode);
