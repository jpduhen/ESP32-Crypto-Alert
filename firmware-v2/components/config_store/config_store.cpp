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

/** M-003b — NVS keys ≤15 chars, namespace `v2cfg`. */
static const char KEY_ALT_1M_BPS[] = "alt_1m_bps";
static const char KEY_ALT_5M_BPS[] = "alt_5m_bps";
static const char KEY_ALT_SC_CL[] = "alt_sc_calm";
static const char KEY_ALT_SC_HT[] = "alt_sc_hot";

static ServiceRuntimeConfig g_service_cache{};
static AlertRuntimeConfig g_alert_cache{};

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

static void alert_apply_kconfig_defaults(AlertRuntimeConfig &a)
{
#ifdef CONFIG_ALERT_ENGINE_1M_THRESHOLD_BPS
    a.threshold_1m_bps = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_1M_THRESHOLD_BPS);
#else
    a.threshold_1m_bps = 16;
#endif
#ifdef CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS
    a.threshold_5m_bps = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS);
#else
    a.threshold_5m_bps = 32;
#endif
#ifdef CONFIG_ALERT_REGIME_THR_SCALE_CALM_PERMILLE
    a.regime_calm_scale_permille = static_cast<uint16_t>(CONFIG_ALERT_REGIME_THR_SCALE_CALM_PERMILLE);
#else
    a.regime_calm_scale_permille = 900;
#endif
#ifdef CONFIG_ALERT_REGIME_THR_SCALE_HOT_PERMILLE
    a.regime_hot_scale_permille = static_cast<uint16_t>(CONFIG_ALERT_REGIME_THR_SCALE_HOT_PERMILLE);
#else
    a.regime_hot_scale_permille = 1180;
#endif
}

static void load_alert_overlay(nvs_handle_t h, AlertRuntimeConfig &a)
{
    uint16_t u16 = 0;
    esp_err_t e = nvs_get_u16(h, KEY_ALT_1M_BPS, &u16);
    if (e == ESP_OK && u16 >= kAlertThreshold1mBpsMin && u16 <= kAlertThreshold1mBpsMax) {
        a.threshold_1m_bps = u16;
    }
    e = nvs_get_u16(h, KEY_ALT_5M_BPS, &u16);
    if (e == ESP_OK && u16 >= kAlertThreshold5mBpsMin && u16 <= kAlertThreshold5mBpsMax) {
        a.threshold_5m_bps = u16;
    }
    e = nvs_get_u16(h, KEY_ALT_SC_CL, &u16);
    if (e == ESP_OK && u16 >= kAlertRegimeCalmScalePermilleMin && u16 <= kAlertRegimeCalmScalePermilleMax) {
        a.regime_calm_scale_permille = u16;
    }
    e = nvs_get_u16(h, KEY_ALT_SC_HT, &u16);
    if (e == ESP_OK && u16 >= kAlertRegimeHotScalePermilleMin && u16 <= kAlertRegimeHotScalePermilleMax) {
        a.regime_hot_scale_permille = u16;
    }
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
    alert_apply_kconfig_defaults(out.alert_tuning);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(DIAG_TAG_CFG, "no NVS namespace yet — defaults");
        g_service_cache = out.services;
        g_alert_cache = out.alert_tuning;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_open: %s — defaults", esp_err_to_name(err));
        g_service_cache = out.services;
        g_alert_cache = out.alert_tuning;
        return ESP_OK;
    }

    uint32_t ver = 0;
    err = nvs_get_u32(h, KEY_SCHEMA, &ver);
    if (err == ESP_OK && ver != kSchemaVersion) {
        ESP_LOGW(DIAG_TAG_CFG,
                 "schema %lu != %u — gedeeltelijke load (services + M-003b alert-overlay vanaf Kconfig+NVS)",
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
    load_alert_overlay(h, out.alert_tuning);

    nvs_close(h);
    apply_defaults(out);
    g_service_cache = out.services;
    g_alert_cache = out.alert_tuning;

    ESP_LOGI(DIAG_TAG_CFG, "loaded schema=%u symbol=%s wifi=%s", (unsigned)out.schema_version, out.default_symbol,
             has_wifi_credentials(out) ? "yes" : "no");
    ESP_LOGI(DIAG_TAG_CFG,
             "M-003a services: webui=%s:%u mqtt=%s ntfy=%s (runtime; NVS override indien gezet)",
             out.services.webui_enabled ? "on" : "off", (unsigned)out.services.webui_port,
             out.services.mqtt_enabled ? "on" : "off", out.services.ntfy_enabled ? "on" : "off");
    ESP_LOGI(DIAG_TAG_CFG,
             "M-003b alert: 1m=%u 5m=%u bps calm=%u‰ hot=%u‰ (Kconfig+NVS alt_*)",
             (unsigned)out.alert_tuning.threshold_1m_bps, (unsigned)out.alert_tuning.threshold_5m_bps,
             (unsigned)out.alert_tuning.regime_calm_scale_permille,
             (unsigned)out.alert_tuning.regime_hot_scale_permille);
    return ESP_OK;
}

const ServiceRuntimeConfig &service_runtime()
{
    return g_service_cache;
}

const AlertRuntimeConfig &alert_runtime()
{
    return g_alert_cache;
}

static bool svc_string_ok(const char *s)
{
    for (const char *p = s; *p; ++p) {
        if (*p == '\r' || *p == '\n') {
            return false;
        }
    }
    return true;
}

esp_err_t persist_service_connectivity(const ServiceRuntimeConfig &mqtt_ntfy)
{
    if (!svc_string_ok(mqtt_ntfy.mqtt_broker_uri) || !svc_string_ok(mqtt_ntfy.ntfy_topic)) {
        ESP_LOGW(DIAG_TAG_CFG, "M-013b: mqtt/ntfy string bevat ongeldige tekens");
        return ESP_ERR_INVALID_ARG;
    }
    if (mqtt_ntfy.mqtt_enabled && mqtt_ntfy.mqtt_broker_uri[0] == '\0') {
        ESP_LOGW(DIAG_TAG_CFG, "M-013b: mqtt_enabled zonder broker-URI");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(mqtt_ntfy.mqtt_broker_uri) >= kMqttBrokerUriMax ||
        strlen(mqtt_ntfy.ntfy_topic) >= kNtfyTopicMax) {
        return ESP_ERR_INVALID_ARG;
    }

    ServiceRuntimeConfig next = g_service_cache;
    next.mqtt_enabled = mqtt_ntfy.mqtt_enabled;
    strncpy(next.mqtt_broker_uri, mqtt_ntfy.mqtt_broker_uri, sizeof(next.mqtt_broker_uri) - 1);
    next.mqtt_broker_uri[sizeof(next.mqtt_broker_uri) - 1] = '\0';
    next.ntfy_enabled = mqtt_ntfy.ntfy_enabled;
    strncpy(next.ntfy_topic, mqtt_ntfy.ntfy_topic, sizeof(next.ntfy_topic) - 1);
    next.ntfy_topic[sizeof(next.ntfy_topic) - 1] = '\0';

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "M-013b: nvs_open: %s", esp_err_to_name(err));
        return err;
    }

    const uint8_t mq_en = next.mqtt_enabled ? 1u : 0u;
    const uint8_t nt_en = next.ntfy_enabled ? 1u : 0u;
    err = nvs_set_u32(h, KEY_SCHEMA, kSchemaVersion);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_SVC_MQ_EN, mq_en);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_SVC_MQ_URI, next.mqtt_broker_uri);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_SVC_NT_EN, nt_en);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, KEY_SVC_NT_TP, next.ntfy_topic);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        g_service_cache = next;
        ESP_LOGI(DIAG_TAG_CFG, "M-013b: service connectivity opgeslagen (mqtt=%s ntfy=%s)",
                 next.mqtt_enabled ? "on" : "off", next.ntfy_enabled ? "on" : "off");
    } else {
        ESP_LOGW(DIAG_TAG_CFG, "M-013b: persist: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t persist_alert_runtime(const AlertRuntimeConfig &alert)
{
    if (alert.threshold_1m_bps < kAlertThreshold1mBpsMin || alert.threshold_1m_bps > kAlertThreshold1mBpsMax ||
        alert.threshold_5m_bps < kAlertThreshold5mBpsMin || alert.threshold_5m_bps > kAlertThreshold5mBpsMax ||
        alert.regime_calm_scale_permille < kAlertRegimeCalmScalePermilleMin ||
        alert.regime_calm_scale_permille > kAlertRegimeCalmScalePermilleMax ||
        alert.regime_hot_scale_permille < kAlertRegimeHotScalePermilleMin ||
        alert.regime_hot_scale_permille > kAlertRegimeHotScalePermilleMax) {
        ESP_LOGW(DIAG_TAG_CFG, "M-003b: persist_alert_runtime: buiten toegestaan bereik");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "M-003b: nvs_open: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u32(h, KEY_SCHEMA, kSchemaVersion);
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALT_1M_BPS, alert.threshold_1m_bps);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALT_5M_BPS, alert.threshold_5m_bps);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALT_SC_CL, alert.regime_calm_scale_permille);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALT_SC_HT, alert.regime_hot_scale_permille);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        g_alert_cache = alert;
        ESP_LOGI(DIAG_TAG_CFG,
                 "M-003b: alert runtime opgeslagen (1m=%u 5m=%u bps calm=%u‰ hot=%u‰)",
                 (unsigned)alert.threshold_1m_bps, (unsigned)alert.threshold_5m_bps,
                 (unsigned)alert.regime_calm_scale_permille, (unsigned)alert.regime_hot_scale_permille);
    } else {
        ESP_LOGW(DIAG_TAG_CFG, "M-003b: persist alert: %s", esp_err_to_name(err));
    }
    return err;
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
