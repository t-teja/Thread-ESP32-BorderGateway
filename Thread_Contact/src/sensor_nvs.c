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
    cfg->mqtt_port = 1883;
    strncpy(cfg->topic_base, HUB_MQTT_TOPIC_BASE_DEFAULT, sizeof(cfg->topic_base) - 1);
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
    LOAD_STR("mhost", mqtt_host);
    LOAD_STR("muser", mqtt_user);
    LOAD_STR("mpass", mqtt_pass);
    LOAD_STR("tbase", topic_base);
    LOAD_STR("dset", dataset_b64);
#undef LOAD_STR
    int32_t port = 1883;
    nvs_get_i32(s_nvs, "mport", &port);
    cfg->mqtt_port = (int)port;
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
    nvs_set_str(s_nvs, "mhost", cfg->mqtt_host);
    nvs_set_str(s_nvs, "muser", cfg->mqtt_user);
    nvs_set_str(s_nvs, "mpass", cfg->mqtt_pass);
    nvs_set_str(s_nvs, "tbase", cfg->topic_base);
    nvs_set_str(s_nvs, "dset", cfg->dataset_b64);
    nvs_set_i32(s_nvs, "mport", cfg->mqtt_port);
    nvs_commit(s_nvs);
    ESP_LOGI(TAG, "saved id=%s", cfg->device_id);
}

void sensor_nvs_clear(void)
{
    nvs_erase_all(s_nvs);
    nvs_commit(s_nvs);
}
