/**
 * S3-only RCP auto-update for Espressif Thread BR board.
 * Uses espressif/esp_rcp_update + SPIFFS image packaged at build time.
 *
 * SPIFFS layout (CONFIG_RCP_PATH_NAME=rcp):
 *   /rcp_fw/rcp_0/rcp_image
 * firmware_dir config value: /rcp_fw/rcp
 */
#include "rcp_auto.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hub_config.h"
#include "hub_led.h"
#include "nvs.h"
#include "nvs_flash.h"

#if HUB_RCP_AUTO_UPDATE
#include "esp_rcp_update.h"
#if __has_include("esp_ot_rcp_update.h")
#include "esp_ot_rcp_update.h"
#define HAS_OT_RCP_HELPER 1
#endif
#endif

static const char *TAG = "rcp_auto";

/* Bump when tools/rcp_fw.bin packaging changes */
#define HUB_RCP_PKG_VER 6

void rcp_auto_hw_reset_run(void)
{
    /* Drive BOOT high (normal run, not download mode) and pulse RESET so the
     * onboard H2 restarts running its application (RCP) firmware. Needed on
     * every boot because these GPIOs are not guaranteed driven at S3 POR. */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HUB_RCP_BOOT_GPIO) | (1ULL << HUB_RCP_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(HUB_RCP_BOOT_GPIO, 1);
    gpio_set_level(HUB_RCP_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_set_level(HUB_RCP_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "H2 RCP reset pulsed (BOOT=GPIO%d high, RESET=GPIO%d)",
             HUB_RCP_BOOT_GPIO, HUB_RCP_RESET_GPIO);
}

#if HUB_RCP_AUTO_UPDATE
/* Matches ESP_RCP_UPDATE_DEFAULT_CONFIG() firmware_dir with path name "rcp" */
#define RCP_FW_BASE_PATH   "/rcp_fw"
#define RCP_FIRMWARE_DIR   "/rcp_fw/rcp"
#define RCP_IMAGE_PATH     "/rcp_fw/rcp_0/rcp_image"

static esp_err_t mount_rcp_fw(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = RCP_FW_BASE_PATH,
        .partition_label = "rcp_fw",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount rcp_fw failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Was rcp_fw partition flashed? Re-upload hub (full image, not app-only).");
        return err;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info("rcp_fw", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "rcp_fw SPIFFS total=%u used=%u", (unsigned)total, (unsigned)used);
        if (used == 0) {
            ESP_LOGE(TAG, "rcp_fw partition is empty — upload did not include tools/rcp_fw.bin");
            return ESP_ERR_NOT_FOUND;
        }
    }
    FILE *fp = fopen(RCP_IMAGE_PATH, "r");
    if (fp) {
        fclose(fp);
        ESP_LOGI(TAG, "found %s", RCP_IMAGE_PATH);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "missing %s — wrong SPIFFS packaging or empty partition", RCP_IMAGE_PATH);
    return ESP_ERR_NOT_FOUND;
}

static void do_force_update(void)
{
    ESP_LOGW(TAG, "Updating RCP via UART loader (RESET=GPIO%d BOOT=GPIO%d RX=GPIO%d TX=GPIO%d)...",
             HUB_RCP_RESET_GPIO, HUB_RCP_BOOT_GPIO, HUB_RCP_UART_RX_GPIO, HUB_RCP_UART_TX_GPIO);
    esp_err_t err = esp_rcp_update();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RCP update OK — mark verified and reboot");
        esp_rcp_mark_image_verified(true);
        nvs_handle_t nvs;
        if (nvs_open("hub_rcp", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_u8(nvs, "flashed", 1);
            nvs_set_u8(nvs, "pkg_ver", HUB_RCP_PKG_VER);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        /* 3 slow blinks = RCP flashed OK */
        hub_led_blink_sync(3, 300, 200);
    } else {
        ESP_LOGE(TAG, "RCP update failed: %s", esp_err_to_name(err));
        esp_rcp_mark_image_verified(false);
        /* 8 fast blinks = RCP flash FAILED */
        hub_led_blink_sync(8, 100, 80);
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}
#endif /* HUB_RCP_AUTO_UPDATE */

esp_err_t rcp_auto_init_and_update(void)
{
#if !HUB_RCP_AUTO_UPDATE
    ESP_LOGW(TAG, "RCP auto-update disabled at compile time");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "RCP auto-update start (S3 programs onboard H2)");

    esp_err_t err = mount_rcp_fw();
    if (err != ESP_OK) {
        return err;
    }

    esp_rcp_update_config_t cfg = ESP_RCP_UPDATE_DEFAULT_CONFIG();
    /* Pins: default config uses CONFIG_DEFAULT_PIN_TO_RCP_* which we set to BR board */
    cfg.uart_rx_pin = HUB_RCP_UART_RX_GPIO;
    cfg.uart_tx_pin = HUB_RCP_UART_TX_GPIO;
    cfg.uart_port = HUB_RCP_UART_PORT;
    cfg.reset_pin = HUB_RCP_RESET_GPIO;
    cfg.boot_pin = HUB_RCP_BOOT_GPIO;
    cfg.uart_baudrate = 115200;
    cfg.update_baudrate = 460800;
    cfg.target_chip = ESP32H2_CHIP;
    cfg.rcp_type = RCP_TYPE_UART;
    strncpy(cfg.firmware_dir, RCP_FIRMWARE_DIR, sizeof(cfg.firmware_dir) - 1);
    cfg.firmware_dir[sizeof(cfg.firmware_dir) - 1] = '\0';

    err = esp_rcp_update_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_rcp_update_init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "RCP update ready dir=%s", cfg.firmware_dir);

#if HAS_OT_RCP_HELPER
    esp_ot_register_rcp_handler();
#endif

    char ver[100] = {0};
    if (esp_rcp_load_version_in_storage(ver, sizeof(ver)) != ESP_OK) {
        ESP_LOGE(TAG, "No RCP version in SPIFFS image");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "RCP image in storage: %s", ver);

    /*
     * pkg_ver bumps force a one-time H2 reflash after packaging/path fixes.
     */
    bool force = false;
    nvs_handle_t nvs;
    if (nvs_open("hub_rcp", NVS_READWRITE, &nvs) == ESP_OK) {
        uint8_t done = 0;
        uint8_t pkg = 0;
        if (nvs_get_u8(nvs, "flashed", &done) != ESP_OK || done == 0) {
            force = true;
        }
        if (nvs_get_u8(nvs, "pkg_ver", &pkg) != ESP_OK || pkg != HUB_RCP_PKG_VER) {
            ESP_LOGW(TAG, "RCP package version changed (%u -> %u) - force update",
                     (unsigned)pkg, (unsigned)HUB_RCP_PKG_VER);
            force = true;
        }
        if (force) {
            nvs_set_u8(nvs, "pkg_ver", HUB_RCP_PKG_VER);
            nvs_set_u8(nvs, "flashed", 0);
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    } else {
        force = true;
    }

#ifdef CONFIG_HUB_FORCE_RCP_UPDATE
#if CONFIG_HUB_FORCE_RCP_UPDATE
    force = true;
#endif
#endif

    if (force) {
        ESP_LOGW(TAG, "First-time / forced RCP program from S3");
        do_force_update(); /* does not return */
    }

    ESP_LOGI(TAG, "RCP already marked flashed — skip force update (OT may still reconcile)");
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
