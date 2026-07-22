#pragma once
#include "esp_openthread_types.h"
#include "hub_config.h"

#if __has_include("esp_rcp_update.h")
#include "esp_rcp_update.h"
#endif

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()              \
    {                                                      \
        .radio_mode = RADIO_MODE_UART_RCP,                 \
        .radio_uart_config = {                             \
            .port = HUB_RCP_UART_PORT,                     \
            .uart_config =                                 \
                {                                          \
                    .baud_rate = HUB_RCP_UART_BAUD,        \
                    .data_bits = UART_DATA_8_BITS,         \
                    .parity = UART_PARITY_DISABLE,         \
                    .stop_bits = UART_STOP_BITS_1,         \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
                    .rx_flow_ctrl_thresh = 0,              \
                    .source_clk = UART_SCLK_DEFAULT,       \
                },                                         \
            .rx_pin = HUB_RCP_UART_RX_GPIO,                \
            .tx_pin = HUB_RCP_UART_TX_GPIO,                \
        },                                                 \
    }

/* Match official esp-thread-br: no CLI host port; RCP is UART only */
#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()               \
    {                                                      \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG() \
    {                                        \
        .storage_partition_name = "nvs",     \
        .netif_queue_size = 10,              \
        .task_queue_size = 10,               \
    }

#if defined(ESP_RCP_UPDATE_DEFAULT_CONFIG) || 1
/* Explicit Espressif BR board RCP update pin map */
#define ESP_OPENTHREAD_RCP_UPDATE_CONFIG()                                      \
    {                                                                           \
        .rcp_type = RCP_TYPE_UART,                                              \
        .uart_rx_pin = HUB_RCP_UART_RX_GPIO,                                    \
        .uart_tx_pin = HUB_RCP_UART_TX_GPIO,                                    \
        .uart_port = HUB_RCP_UART_PORT,                                         \
        .uart_baudrate = 115200,                                                \
        .reset_pin = HUB_RCP_RESET_GPIO,                                        \
        .boot_pin = HUB_RCP_BOOT_GPIO,                                          \
        .update_baudrate = 460800,                                              \
        .firmware_dir = "/rcp_fw/rcp",                                          \
        .target_chip = ESP32H2_CHIP,                                            \
    }
#endif
