#pragma once

#include <esp_err.h>
#include <stddef.h>
#include <stdbool.h>

esp_err_t otbr_net_init(void);
bool otbr_net_is_ready(void);
/** Write base64 active dataset into buf. Returns false if unavailable. */
bool otbr_net_get_dataset_b64(char *buf, size_t buflen);
const char *otbr_net_status_text(void);
