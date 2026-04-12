#include "config_store/config_store.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

namespace config_store {

static const char NVS_NS[] = "v2cfg";
static const char KEY_SCHEMA[] = "schema";
static const char KEY_SYM[] = "sym";
static const char KEY_WIFI_SSID[] = "wifi_ssid";
static const char KEY_WIFI_PASS[] = "wifi_pass";

esp_err_t init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_flash: erase + reinit");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(DIAG_TAG_CFG, "nvs_flash_init: %s", esp_err_to_name(err));
    }
    return err;
}

static void apply_defaults(RuntimeConfig &c)
{
    c.schema_version = kSchemaVersion;
    if (c.default_symbol[0] == '\0') {
        strncpy(c.default_symbol, "BTC-EUR", sizeof(c.default_symbol) - 1);
    }
}

esp_err_t load_or_defaults(RuntimeConfig &out)
{
    apply_defaults(out);
    out.wifi_sta_ssid[0] = '\0';
    out.wifi_sta_pass[0] = '\0';

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(DIAG_TAG_CFG, "no NVS namespace yet — defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_open: %s — defaults", esp_err_to_name(err));
        return ESP_OK;
    }

    uint32_t ver = 0;
    err = nvs_get_u32(h, KEY_SCHEMA, &ver);
    if (err == ESP_OK && ver != kSchemaVersion) {
        ESP_LOGW(DIAG_TAG_CFG, "schema %lu != %u — gedeeltelijke load", (unsigned long)ver,
                (unsigned)kSchemaVersion);
    }

    size_t sz = sizeof(out.default_symbol);
    err = nvs_get_str(h, KEY_SYM, out.default_symbol, &sz);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_get_str sym: %s", esp_err_to_name(err));
    }

    sz = sizeof(out.wifi_sta_ssid);
    err = nvs_get_str(h, KEY_WIFI_SSID, out.wifi_sta_ssid, &sz);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_get_str wifi_ssid: %s", esp_err_to_name(err));
    }
    sz = sizeof(out.wifi_sta_pass);
    err = nvs_get_str(h, KEY_WIFI_PASS, out.wifi_sta_pass, &sz);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_get_str wifi_pass: %s", esp_err_to_name(err));
    }

    nvs_close(h);
    apply_defaults(out);
    ESP_LOGI(DIAG_TAG_CFG, "loaded schema=%u symbol=%s wifi=%s", (unsigned)out.schema_version,
             out.default_symbol, has_wifi_credentials(out) ? "yes" : "no");
    return ESP_OK;
}

esp_err_t save(const RuntimeConfig &cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u32(h, KEY_SCHEMA, cfg.schema_version);
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_SYM, cfg.default_symbol);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_WIFI_SSID, cfg.wifi_sta_ssid);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_WIFI_PASS, cfg.wifi_sta_pass);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    ESP_LOGI(DIAG_TAG_CFG, "save: %s", esp_err_to_name(err));
    return err;
}

bool has_wifi_credentials(const RuntimeConfig &cfg)
{
    return cfg.wifi_sta_ssid[0] != '\0';
}

esp_err_t clear_wifi_credentials()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(h, KEY_WIFI_SSID);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return err;
    }
    err = nvs_erase_key(h, KEY_WIFI_PASS);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(h);
        return err;
    }
    err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(DIAG_TAG_CFG, "wifi credentials cleared");
    return err;
}

} // namespace config_store
