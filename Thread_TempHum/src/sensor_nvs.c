#include "sensor_nvs.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "snvs";
static nvs_handle_t s_nvs;

void sensor_nvs_init(void)
{
    ESP_ERROR_CHECK(nvs_open("sensor", NVS_READWRITE, &s_nvs));
}

bool sensor_nvs_load(sensor_cfg_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    uint8_t flag = 0;
    if (nvs_get_u8(s_nvs, "prov", &flag) != ESP_OK || !flag) {
        return false;
    }
    size_t len;
#define LOAD_STR(key, field)                                                                 \
    do {                                                                                     \
        len = sizeof(cfg->field);                                                            \
        nvs_get_str(s_nvs, key, cfg->field, &len);                                           \
    } while (0)
    LOAD_STR("id", device_id);
    LOAD_STR("name", name);
    LOAD_STR("room", room);
    LOAD_STR("type", type);
    LOAD_STR("haddr", hub_addr);
    LOAD_STR("dset", dataset_b64);
#undef LOAD_STR
    cfg->provisioned = cfg->device_id[0] != 0;
    ESP_LOGI(TAG, "loaded provisioned=%d id=%s", cfg->provisioned, cfg->device_id);
    return cfg->provisioned;
}

void sensor_nvs_save(const sensor_cfg_t *cfg)
{
    nvs_set_u8(s_nvs, "prov", cfg->provisioned ? 1 : 0);
    nvs_set_str(s_nvs, "id", cfg->device_id);
    nvs_set_str(s_nvs, "name", cfg->name);
    nvs_set_str(s_nvs, "room", cfg->room);
    nvs_set_str(s_nvs, "type", cfg->type);
    nvs_set_str(s_nvs, "haddr", cfg->hub_addr);
    nvs_set_str(s_nvs, "dset", cfg->dataset_b64);
    nvs_commit(s_nvs);
    ESP_LOGI(TAG, "saved id=%s", cfg->device_id);
}

void sensor_nvs_clear(void)
{
    nvs_erase_all(s_nvs);
    nvs_commit(s_nvs);
}
