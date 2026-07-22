#pragma once
#include <esp_err.h>
/** Minimal DNS responder that answers every query with the SoftAP IP (captive portal). */
esp_err_t dns_server_start(void);
void dns_server_stop(void);
