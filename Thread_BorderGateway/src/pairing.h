#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

void pairing_init(void);
esp_err_t pairing_open_window(int seconds);
void pairing_close_window(void);
bool pairing_is_open(void);
int pairing_seconds_left(void);

/**
 * Pair discovered BLE addr with name/room.
 * Builds provision payload, writes via BLE, adds registry entry.
 */
esp_err_t pairing_pair_device(const char *ble_addr, const char *type_hint,
                             const char *name, const char *room);
