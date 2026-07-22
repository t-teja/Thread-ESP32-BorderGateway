#pragma once
#include <esp_err.h>

/** Mount SPIFFS image, flash H2 if needed, leave H2 in run mode. Call before OTBR. */
esp_err_t rcp_auto_init_and_update(void);

/** Pulse RESET with BOOT high so H2 runs application firmware. */
void rcp_auto_hw_reset_run(void);

void rcp_auto_mark_ok(void);
