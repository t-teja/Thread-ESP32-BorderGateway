#pragma once

#include <esp_err.h>
#include <stdbool.h>

/** Hold BOOT (GPIO0) while running to factory-reset into captive portal. */
esp_err_t hub_button_start(void);

/**
 * Call early after NVS init. If BOOT held long enough at power-on,
 * clear settings and reboot into setup AP. Does not return on reset.
 */
bool hub_button_check_boot_factory_reset(void);
