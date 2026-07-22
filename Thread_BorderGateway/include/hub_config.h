#pragma once

#include "device_types.h"

#define HUB_FW_VERSION       "0.2.0"
#define HUB_HTTP_PORT        80
#define HUB_PAIR_DEFAULT_SEC HUB_PAIR_WINDOW_SEC

/** SoftAP / captive portal */
#define HUB_SETUP_AP_PREFIX  "ThreadHub-Setup"
#define HUB_SETUP_AP_PASS    ""          /* open network for first-time setup */
#define HUB_AP_IP_ADDR       "192.168.4.1"
#define HUB_DEFAULT_HOSTNAME "thread-hub"
#define HUB_DEFAULT_TOPIC_BASE "home"

/** When OTBR not linked, BLE provision still works; dataset may be placeholder */
#define HUB_OTBR_ENABLED     0

/** Soft stub dataset (base64) so sensors can complete UX flow before OTBR */
#define HUB_STUB_DATASET_B64 "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="
