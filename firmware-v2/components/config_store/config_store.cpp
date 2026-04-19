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

/** M-003c — NVS keys ≤15 chars. */
static const char KEY_ALTP_CD_1M[] = "altp_cd_1m";
static const char KEY_ALTP_CD_5M[] = "altp_cd_5m";
static const char KEY_ALTP_CD_CF[] = "altp_cd_cf";
static const char KEY_ALTP_SUP_LO[] = "altp_sup_lo";

/** M-003d — bools als u8 0/1. */
static const char KEY_ALT_CF_EN[] = "altcf_en";
static const char KEY_ALT_CF_SD[] = "altcf_sd";
static const char KEY_ALT_CF_BT[] = "altcf_bt";
static const char KEY_ALT_CF_LO[] = "altcf_lo";

static ServiceRuntimeConfig g_service_cache{};
static AlertRuntimeConfig g_alert_cache{};
static AlertPolicyTimingConfig g_policy_cache{};
static AlertConfluencePolicyConfig g_conf_policy_cache{};

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

static void alert_policy_apply_kconfig_defaults(AlertPolicyTimingConfig &p)
{
#ifdef CONFIG_ALERT_ENGINE_1M_COOLDOWN_S
    p.cooldown_1m_s = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_1M_COOLDOWN_S);
#else
    p.cooldown_1m_s = 120;
#endif
#ifdef CONFIG_ALERT_ENGINE_5M_COOLDOWN_S
    p.cooldown_5m_s = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_5M_COOLDOWN_S);
#else
    p.cooldown_5m_s = 300;
#endif
#ifdef CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S
    p.cooldown_conf_1m5m_s = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S);
#else
    p.cooldown_conf_1m5m_s = 600;
#endif
#ifdef CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S
    p.suppress_loose_after_conf_s = static_cast<uint16_t>(CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S);
#else
    p.suppress_loose_after_conf_s = 8;
#endif
}

static void alert_confluence_apply_defaults(AlertConfluencePolicyConfig &c)
{
    c.confluence_enabled = true;
    c.confluence_require_same_direction = true;
    c.confluence_require_both_thresholds = true;
    c.confluence_emit_loose_alerts_when_conf_fails = true;
}

static void load_confluence_overlay(nvs_handle_t h, AlertConfluencePolicyConfig &c)
{
    uint8_t u8v = 0;
    esp_err_t e = nvs_get_u8(h, KEY_ALT_CF_EN, &u8v);
    if (e == ESP_OK) {
        c.confluence_enabled = (u8v != 0);
    }
    e = nvs_get_u8(h, KEY_ALT_CF_SD, &u8v);
    if (e == ESP_OK) {
        c.confluence_require_same_direction = (u8v != 0);
    }
    e = nvs_get_u8(h, KEY_ALT_CF_BT, &u8v);
    if (e == ESP_OK) {
        c.confluence_require_both_thresholds = (u8v != 0);
    }
    e = nvs_get_u8(h, KEY_ALT_CF_LO, &u8v);
    if (e == ESP_OK) {
        c.confluence_emit_loose_alerts_when_conf_fails = (u8v != 0);
    }
}

static void load_policy_overlay(nvs_handle_t h, AlertPolicyTimingConfig &p)
{
    uint16_t u16 = 0;
    esp_err_t e = nvs_get_u16(h, KEY_ALTP_CD_1M, &u16);
    if (e == ESP_OK && u16 >= kAlertPolicyCooldown1mSMin && u16 <= kAlertPolicyCooldown1mSMax) {
        p.cooldown_1m_s = u16;
    }
    e = nvs_get_u16(h, KEY_ALTP_CD_5M, &u16);
    if (e == ESP_OK && u16 >= kAlertPolicyCooldown5mSMin && u16 <= kAlertPolicyCooldown5mSMax) {
        p.cooldown_5m_s = u16;
    }
    e = nvs_get_u16(h, KEY_ALTP_CD_CF, &u16);
    if (e == ESP_OK && u16 >= kAlertPolicyCooldownConfSMin && u16 <= kAlertPolicyCooldownConfSMax) {
        p.cooldown_conf_1m5m_s = u16;
    }
    e = nvs_get_u16(h, KEY_ALTP_SUP_LO, &u16);
    if (e == ESP_OK && u16 >= kAlertPolicySuppressLooseSMin && u16 <= kAlertPolicySuppressLooseSMax) {
        p.suppress_loose_after_conf_s = u16;
    }
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
    alert_policy_apply_kconfig_defaults(out.alert_policy);
    alert_confluence_apply_defaults(out.alert_confluence);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(DIAG_TAG_CFG, "no NVS namespace yet — defaults");
        g_service_cache = out.services;
        g_alert_cache = out.alert_tuning;
        g_policy_cache = out.alert_policy;
        g_conf_policy_cache = out.alert_confluence;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "nvs_open: %s — defaults", esp_err_to_name(err));
        g_service_cache = out.services;
        g_alert_cache = out.alert_tuning;
        g_policy_cache = out.alert_policy;
        g_conf_policy_cache = out.alert_confluence;
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
    load_policy_overlay(h, out.alert_policy);
    load_confluence_overlay(h, out.alert_confluence);

    nvs_close(h);
    apply_defaults(out);
    g_service_cache = out.services;
    g_alert_cache = out.alert_tuning;
    g_policy_cache = out.alert_policy;
    g_conf_policy_cache = out.alert_confluence;

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
    ESP_LOGI(DIAG_TAG_CFG,
             "M-003c policy timing: 1m_cd=%us 5m_cd=%us conf_cd=%us sup_loose=%us (Kconfig+NVS altp_*)",
             (unsigned)out.alert_policy.cooldown_1m_s, (unsigned)out.alert_policy.cooldown_5m_s,
             (unsigned)out.alert_policy.cooldown_conf_1m5m_s,
             (unsigned)out.alert_policy.suppress_loose_after_conf_s);
    ESP_LOGI(DIAG_TAG_CFG,
             "M-003d confluence policy: en=%s same_dir=%s both_thr=%s emit_loose=%s (defaults+NVS altcf_*)",
             out.alert_confluence.confluence_enabled ? "on" : "off",
             out.alert_confluence.confluence_require_same_direction ? "on" : "off",
             out.alert_confluence.confluence_require_both_thresholds ? "on" : "off",
             out.alert_confluence.confluence_emit_loose_alerts_when_conf_fails ? "on" : "off");
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

const AlertPolicyTimingConfig &alert_policy_timing()
{
    return g_policy_cache;
}

const AlertConfluencePolicyConfig &alert_confluence_policy()
{
    return g_conf_policy_cache;
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

esp_err_t persist_alert_policy_timing(const AlertPolicyTimingConfig &policy)
{
    if (policy.cooldown_1m_s < kAlertPolicyCooldown1mSMin || policy.cooldown_1m_s > kAlertPolicyCooldown1mSMax ||
        policy.cooldown_5m_s < kAlertPolicyCooldown5mSMin || policy.cooldown_5m_s > kAlertPolicyCooldown5mSMax ||
        policy.cooldown_conf_1m5m_s < kAlertPolicyCooldownConfSMin ||
        policy.cooldown_conf_1m5m_s > kAlertPolicyCooldownConfSMax ||
        policy.suppress_loose_after_conf_s < kAlertPolicySuppressLooseSMin ||
        policy.suppress_loose_after_conf_s > kAlertPolicySuppressLooseSMax) {
        ESP_LOGW(DIAG_TAG_CFG, "M-003c/M-013i: persist_alert_policy_timing: buiten toegestaan bereik");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "M-013i: nvs_open: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u32(h, KEY_SCHEMA, kSchemaVersion);
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALTP_CD_1M, policy.cooldown_1m_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALTP_CD_5M, policy.cooldown_5m_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALTP_CD_CF, policy.cooldown_conf_1m5m_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(h, KEY_ALTP_SUP_LO, policy.suppress_loose_after_conf_s);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        g_policy_cache = policy;
        ESP_LOGI(DIAG_TAG_CFG,
                 "M-013i: alert policy timing opgeslagen (1m_cd=%us 5m_cd=%us conf_cd=%us sup_loose=%us)",
                 (unsigned)policy.cooldown_1m_s, (unsigned)policy.cooldown_5m_s,
                 (unsigned)policy.cooldown_conf_1m5m_s, (unsigned)policy.suppress_loose_after_conf_s);
    } else {
        ESP_LOGW(DIAG_TAG_CFG, "M-013i: persist policy timing: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t persist_alert_confluence_policy(const AlertConfluencePolicyConfig &policy)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_CFG, "M-013k: nvs_open: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u32(h, KEY_SCHEMA, kSchemaVersion);
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_ALT_CF_EN, policy.confluence_enabled ? 1u : 0u);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_ALT_CF_SD, policy.confluence_require_same_direction ? 1u : 0u);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_ALT_CF_BT, policy.confluence_require_both_thresholds ? 1u : 0u);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, KEY_ALT_CF_LO, policy.confluence_emit_loose_alerts_when_conf_fails ? 1u : 0u);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    if (err == ESP_OK) {
        g_conf_policy_cache = policy;
        ESP_LOGI(DIAG_TAG_CFG,
                 "M-013k: confluence policy opgeslagen (en=%s same_dir=%s both_thr=%s emit_loose=%s)",
                 policy.confluence_enabled ? "on" : "off",
                 policy.confluence_require_same_direction ? "on" : "off",
                 policy.confluence_require_both_thresholds ? "on" : "off",
                 policy.confluence_emit_loose_alerts_when_conf_fails ? "on" : "off");
    } else {
        ESP_LOGW(DIAG_TAG_CFG, "M-013k: persist confluence policy: %s", esp_err_to_name(err));
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
