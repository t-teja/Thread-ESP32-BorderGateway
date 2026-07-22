#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "device_types.h"

typedef struct {
    char addr[18];
    char name[32];
    char type[HUB_MAX_TYPE];
    char fw[HUB_MAX_FW];
    char product[32];
    int bat;
    int rssi;
    bool valid;
} ble_candidate_t;

#define BLE_MAX_CANDIDATES 8

esp_err_t ble_central_init(void);
esp_err_t ble_central_start_scan(void);
void ble_central_stop_scan(void);
bool ble_central_is_scanning(void);
int ble_central_get_candidates(ble_candidate_t *out, int max);
/** Connect and write provision JSON to sensor Ctrl char. */
esp_err_t ble_central_provision(const char *addr, const char *provision_json);
/** JSON array of candidates; caller frees. */
char *ble_central_candidates_json(void);
