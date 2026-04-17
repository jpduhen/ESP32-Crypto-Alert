#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>

namespace config_store {

/**
 * M-003a / M-013b: runtime service-instellingen (NVS-overlay op Kconfig-defaults).
 * M-003b: typed **alert/regime-tuning** (4 velden, non-secret) — schema **v4**.
 * M-013b: beperkte schrijfpad voor mqtt/ntfy via `persist_service_connectivity` (geen webui_*).
 */
constexpr uint32_t kSchemaVersion = 4;

/** Grenzen voor validatie (align met Kconfig `ALERT_ENGINE_*` / `ALERT_REGIME_THR_*`). */
constexpr uint16_t kAlertThreshold1mBpsMin = 1;
constexpr uint16_t kAlertThreshold1mBpsMax = 1000;
constexpr uint16_t kAlertThreshold5mBpsMin = 1;
constexpr uint16_t kAlertThreshold5mBpsMax = 2000;
constexpr uint16_t kAlertRegimeCalmScalePermilleMin = 700;
constexpr uint16_t kAlertRegimeCalmScalePermilleMax = 1000;
constexpr uint16_t kAlertRegimeHotScalePermilleMin = 1000;
constexpr uint16_t kAlertRegimeHotScalePermilleMax = 1500;

/** Inclusief null; ESP-IDF wifi_config gebruikt 32/64 byte buffers. */
constexpr size_t kWifiSsidMax = 33;
constexpr size_t kWifiPassMax = 65;

constexpr size_t kMqttBrokerUriMax = 160;
constexpr size_t kNtfyTopicMax = 96;

/**
 * Typed subset: WebUI + MQTT + NTFY — geen secrets hier (MQTT/NTFY tokens blijven Kconfig
 * tot latere stap).
 */
struct ServiceRuntimeConfig {
    bool webui_enabled{false};
    uint16_t webui_port{8080};
    bool mqtt_enabled{false};
    char mqtt_broker_uri[kMqttBrokerUriMax]{};
    bool ntfy_enabled{false};
    char ntfy_topic[kNtfyTopicMax]{};
};

/**
 * M-003b: minimale runtime-tuning voor alerts/regime — **bps** en **‰ schaal** voor calm/hot.
 * Standaard uit Kconfig; NVS-overlay (`alt_*` keys) optioneel. Geen secrets.
 */
struct AlertRuntimeConfig {
    uint16_t threshold_1m_bps{16};
    uint16_t threshold_5m_bps{32};
    uint16_t regime_calm_scale_permille{900};
    uint16_t regime_hot_scale_permille{1180};
};

struct RuntimeConfig {
    uint32_t schema_version{kSchemaVersion};
    char default_symbol[24]{"BTC-EUR"};
    uint32_t flags{0};
    /** STA-credentials (NVS); leeg = nog niet geprovisioned via onboarding. */
    char wifi_sta_ssid[kWifiSsidMax]{};
    char wifi_sta_pass[kWifiPassMax]{};
    /** Effectieve service-instellingen (Kconfig + optionele NVS-keys `svc_*`). */
    ServiceRuntimeConfig services{};
    /** M-003b: effectieve alert-drempels/schaal (Kconfig + optionele NVS `alt_*`). */
    AlertRuntimeConfig alert_tuning{};
};

esp_err_t init();
/** NVS init + namespace; idempotent waar mogelijk. Vult `out.services` + interne cache. */
esp_err_t load_or_defaults(RuntimeConfig &out);
esp_err_t save(const RuntimeConfig &cfg);

/**
 * Snapshot na `load_or_defaults` — zelfde inhoud als `out.services` van de laatste load.
 * Bijgewerkt na `persist_service_connectivity` (M-013b).
 */
const ServiceRuntimeConfig &service_runtime();

/**
 * Snapshot na `load_or_defaults` — zelfde inhoud als `out.alert_tuning`. Bijgewerkt na
 * `persist_alert_runtime` (M-003b).
 */
const AlertRuntimeConfig &alert_runtime();

/**
 * M-013b: schrijf alleen mqtt_* en ntfy_* naar NVS (`svc_*` keys). Laat `webui_enabled` /
 * `webui_port` ongemoeid. Valideert lengtes en vereist niet-lege `mqtt_broker_uri` als
 * `mqtt_enabled` true. Wijzigt `g_service_cache` bij succes.
 */
esp_err_t persist_service_connectivity(const ServiceRuntimeConfig &mqtt_ntfy);

/**
 * M-003b / M-013f: schrijf alert/regime-tuning naar NVS (`alt_*` keys), zet schema v4.
 * Valideert bereiken; bij succes: `g_alert_cache` bijgewerkt (`alert_engine` leest via `alert_runtime()`).
 */
esp_err_t persist_alert_runtime(const AlertRuntimeConfig &alert);

/** True als er een niet-lege SSID in NVS zit (provisioned). */
bool has_wifi_credentials(const RuntimeConfig &cfg);

/** WiFi-velden leegmaken en committen (factory / geforceerde her-onboarding). */
esp_err_t clear_wifi_credentials();

} // namespace config_store
