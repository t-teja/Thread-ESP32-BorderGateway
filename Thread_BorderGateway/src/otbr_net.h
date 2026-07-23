#pragma once
#include <esp_err.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

esp_err_t otbr_net_init(void);
bool otbr_net_is_ready(void);
bool otbr_net_get_dataset_b64(char *buf, size_t buflen);
/** Hub's own Thread mesh-local address (text form), for sensors to reach it via CoAP. */
bool otbr_net_get_mesh_local_addr(char *buf, size_t buflen);
const char *otbr_net_status_text(void);
/** JSON snapshot for dashboard map: hub + thread role + neighbor count. Caller frees. */
char *otbr_net_info_json(void);
int otbr_net_neighbor_count(void);
