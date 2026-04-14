#include "config_store/config_store.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"

#include <cstring>

namespace config_store {

static const char NVS_NS[] = "v2cfg";
static const char KEY_SCHEMA[] = "schema";
static const char KEY_SYM[] = "sym";
static const char KEY_WIFI_SSID[] = "wifi_ssid";
static const char KEY_WIFI_PASS[] = "wifi_pass";

/** M-003a — korte sleutels (≤15) voor NVS. */
static const char KEY_SVC_WUI_EN[] = "svc_wui_en";
static const char KEY_SVC_WUI_PT[] = "svc_wui_pt";
static const char KEY_SVC_MQ_EN[] = "svc_mqtt_en";
static const char KEY_SVC_MQ_URI[] = "svc_mqtt_uri";
static const char KEY_SVC_NT_EN[] = "svc_ntfy_en";
static const char KEY_SVC_NT_TP[] = "svc_ntfy_tp";

static ServiceRuntimeConfig g_service_cache{};

static void service_apply_kconfig_defaults(ServiceRuntimeConfig &s)
{
    // Keep config_store buildable even when a service symbol is absent.
#ifdef CONFIG_WEBUI_ENABLE
    s.webui_enabled = CONFIG_WEBUI_ENABLE;
#else
    s.webui_enabled = false;
#endif

#ifdef CONFIG_WEBUI_PORT
    const int webui_port = CONFIG_WEBUI_PORT;
    if (webui_port >= 1024 && webui_port <= 65535) {
        s.webui_port = static_cast<uint16_t>(webui_port);
    } else {
        s.webui_port = 8080;
    }
#else
    s.webui_port = 8080;
#endif

#ifdef CONFIG_MQTT_BRIDGE_ENABLE
    s.mqtt_enabled = CONFIG_MQTT_BRIDGE_ENABLE;
#else
    s.mqtt_enabled = false;
#endif

#ifdef CONFIG_MQTT_BROKER_URI
    strncpy(s.mqtt_broker_uri, CONFIG_MQTT_BROKER_URI, sizeof(s.mqtt_broker_uri) - 1);
#else
    s.mqtt_broker_uri[0] = '\0';
#endif
    s.mqtt_broker_uri[sizeof(s.mqtt_broker_uri) - 1] = '\0';

#ifdef CONFIG_NTFY_CLIENT_ENABLE
    s.ntfy_enabled = CONFIG_NTFY_CLIENT_ENABLE;
#else
    s.ntfy_enabled = false;
#endif

#ifdef CONFIG_NTFY_TOPIC
    strncpy(s.ntfy_topic, CONFIG_NTFY_TOPIC, sizeof(s.ntfy_topic) - 1);
#else
    s.ntfy_topic[0] = '\0';
#endif
    s.ntfy_topic[sizeof(s.ntfy_topic) - 1] = '\0';
}

static void load_service_overlay(nvs_handle_t h, ServiceRuntimeConfig &s)
{
    uint8_t u8v = 0;
    esp_err_t e = nvs_get_u8(h, KEY_SVC_WUI_EN, &u8v);
    if (e == ESP_OK) {
        s.webui_enabled = (u8v != 0);
    }
    uint16_t u16v = 0;
    e = nvs_get_u16(h, KEY_SVC_WUI_PT, &u16v);
    if (e == ESP_OK && u16v >= 1024) {
        s.webui_port = u16v;
    }

    e = nvs_get_u8(h, KEY_SVC_MQ_EN, &u8v);
    if (e == ESP_OK) {
        s.mqtt_enabled = (u8v != 0);
    }
    size_t sz = sizeof(s.mqtt_broker_uri);
    e = nvs_get_str(h, KEY_SVC_MQ_URI, s.mqtt_broker_uri, &sz);
    if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_get_str svc_mqtt_uri: %s", esp_err_to_name(e));
    }

    e = nvs_get_u8(h, KEY_SVC_NT_EN, &u8v);
    if (e == ESP_OK) {
        s.ntfy_enabled = (u8v != 0);
    }
    sz = sizeof(s.ntfy_topic);
    e = nvs_get_str(h, KEY_SVC_NT_TP, s.ntfy_topic, &sz);
    if (e != ESP_OK && e != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_get_str svc_ntfy_tp: %s", esp_err_to_name(e));
    }
}

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
    service_apply_kconfig_defaults(out.services);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(DIAG_TAG_CFG, "no NVS namespace yet — defaults");
        g_service_cache = out.services;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_open: %s — defaults", esp_err_to_name(err));
        g_service_cache = out.services;
        return ESP_OK;
    }

    uint32_t ver = 0;
    err = nvs_get_u32(h, KEY_SCHEMA, &ver);
    if (err == ESP_OK && ver != kSchemaVersion) {
        ESP_LOGW(DIAG_TAG_CFG, "schema %lu != %u — gedeeltelijke load (M-003a: services vullen vanaf Kconfig+NVS)",
                 (unsigned long)ver, (unsigned)kSchemaVersion);
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

    load_service_overlay(h, out.services);

    nvs_close(h);
    apply_defaults(out);
    g_service_cache = out.services;

    ESP_LOGI(DIAG_TAG_CFG, "loaded schema=%u symbol=%s wifi=%s", (unsigned)out.schema_version, out.default_symbol,
             has_wifi_credentials(out) ? "yes" : "no");
    ESP_LOGI(DIAG_TAG_CFG,
             "M-003a services: webui=%s:%u mqtt=%s ntfy=%s (runtime; NVS override indien gezet)",
             out.services.webui_enabled ? "on" : "off", (unsigned)out.services.webui_port,
             out.services.mqtt_enabled ? "on" : "off", out.services.ntfy_enabled ? "on" : "off");
    return ESP_OK;
}

const ServiceRuntimeConfig &service_runtime()
{
    return g_service_cache;
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
