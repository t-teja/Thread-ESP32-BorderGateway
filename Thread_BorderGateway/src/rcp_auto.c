/**
 * S3-only RCP auto-update for Espressif Thread BR board.
 * Uses espressif/esp_rcp_update + SPIFFS image packaged at build time.
 */
#include "rcp_auto.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_config.h"
#include "nvs.h"
#include "nvs_flash.h"

#if HUB_RCP_AUTO_UPDATE
#include "esp_ot_config.h"
#include "esp_rcp_update.h"
#if __has_include("esp_ot_rcp_update.h")
#include "esp_ot_rcp_update.h"
#define HAS_OT_RCP_HELPER 1
#endif
#endif

static const char *TAG = "rcp_auto";

#if HUB_RCP_AUTO_UPDATE
static esp_err_t mount_rcp_fw(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/rcp_fw",
        .partition_label = "rcp_fw",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount rcp_fw failed: %s", esp_err_to_name(err));
        return err;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info("rcp_fw", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "rcp_fw SPIFFS total=%u used=%u", (unsigned)total, (unsigned)used);
    }
    /* List expected image path */
    FILE *fp = fopen("/rcp_fw/ot_rcp_0/rcp_image", "r");
    if (fp) {
        fclose(fp);
        ESP_LOGI(TAG, "found /rcp_fw/ot_rcp_0/rcp_image");
    } else {
        ESP_LOGW(TAG, "missing /rcp_fw/ot_rcp_0/rcp_image — rebuild hub with packaged RCP image");
    }
    return ESP_OK;
}

static void do_force_update(void)
{
    ESP_LOGW(TAG, "Updating RCP firmware via UART loader (RESET=%d BOOT=%d)...",
             HUB_RCP_RESET_GPIO, HUB_RCP_BOOT_GPIO);
    esp_err_t err = esp_rcp_update();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RCP update OK — marking verified and rebooting");
        esp_rcp_mark_image_verified(true);
    } else {
        ESP_LOGE(TAG, "RCP update failed: %s — mark failed and reboot", esp_err_to_name(err));
        esp_rcp_mark_image_verified(false);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}
#endif /* HUB_RCP_AUTO_UPDATE */

esp_err_t rcp_auto_init_and_update(void)
{
#if !HUB_RCP_AUTO_UPDATE
    ESP_LOGW(TAG, "RCP auto-update disabled");
    return ESP_OK;
#else
    ESP_ERROR_CHECK(mount_rcp_fw());

    esp_rcp_update_config_t cfg = ESP_OPENTHREAD_RCP_UPDATE_CONFIG();
    /* firmware_dir becomes /rcp_fw/ot_rcp + "_0/rcp_image" */
    strncpy(cfg.firmware_dir, "/rcp_fw/ot_rcp", sizeof(cfg.firmware_dir) - 1);
    cfg.uart_rx_pin = HUB_RCP_UART_RX_GPIO;
    cfg.uart_tx_pin = HUB_RCP_UART_TX_GPIO;
    cfg.uart_port = HUB_RCP_UART_PORT;
    cfg.reset_pin = HUB_RCP_RESET_GPIO;
    cfg.boot_pin = HUB_RCP_BOOT_GPIO;
    cfg.uart_baudrate = 115200;
    cfg.update_baudrate = 460800;
    cfg.target_chip = ESP32H2_CHIP;
    cfg.rcp_type = RCP_TYPE_UART;

    ESP_ERROR_CHECK(esp_rcp_update_init(&cfg));
    ESP_LOGI(TAG, "RCP update ready (dir=%s)", cfg.firmware_dir);

#if HAS_OT_RCP_HELPER
    /* Register failure handlers so a bad RCP triggers re-flash */
    esp_ot_register_rcp_handler();
#endif

    /*
     * Probe version stored in SPIFFS. If we cannot talk to running RCP yet
     * (cold board / wrong FW), force update once. When OT starts later,
     * esp_ot_update_rcp_if_different will reconcile versions.
     *
     * Strategy:
     *  - Always try to load stored version string
     *  - Attempt a lightweight boot: if no prior successful verify flag and image exists, flash
     */
    char ver[100] = {0};
    if (esp_rcp_load_version_in_storage(ver, sizeof(ver)) != ESP_OK) {
        ESP_LOGW(TAG, "No RCP image version in storage");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "RCP image in flash storage: %s", ver);

    /*
     * Always run update on first boot after packaging change, or when H2 is
     * unresponsive. esp_rcp_update() puts H2 in download mode via BOOT/RESET
     * and programs bootloader + app — safe even if stock firmware is present.
     *
     * To avoid flashing every reboot, only force when NVS says not verified
     * OR a one-shot flag. Here: if seq/verify says unverified, update.
     * Additionally, try a probe file once at first deploy using NVS key.
     */
    nvs_handle_t nvs;
    bool force = false;
    if (nvs_open("hub_rcp", NVS_READWRITE, &nvs) == ESP_OK) {
        uint8_t done = 0;
        if (nvs_get_u8(nvs, "flashed", &done) != ESP_OK || done == 0) {
            force = true;
        }
        /* After successful OTBR we'll set flashed=1 in otbr_net */
        nvs_close(nvs);
    } else {
        force = true;
    }


    if (force) {
        ESP_LOGW(TAG, "First-time / forced RCP program from S3 (no manual H2 flash needed)");
        do_force_update(); /* reboots */
    }

    ESP_LOGI(TAG, "RCP auto-update armed (will reflash on mismatch/failure)");
    return ESP_OK;
#endif
}

void rcp_auto_mark_ok(void)
{
#if HUB_RCP_AUTO_UPDATE
    nvs_handle_t nvs;
    if (nvs_open("hub_rcp", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "flashed", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    esp_rcp_mark_image_verified(true);
#endif
}
