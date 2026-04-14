#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>

namespace config_store {

/**
 * M-003a: schema v3 — runtime service-instellingen (NVS-overlay op Kconfig-defaults).
 * Schrijfpad WebUI/settings: bewust nog niet; alleen load + getters.
 */
constexpr uint32_t kSchemaVersion = 3;

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

struct RuntimeConfig {
    uint32_t schema_version{kSchemaVersion};
    char default_symbol[24]{"BTC-EUR"};
    uint32_t flags{0};
    /** STA-credentials (NVS); leeg = nog niet geprovisioned via onboarding. */
    char wifi_sta_ssid[kWifiSsidMax]{};
    char wifi_sta_pass[kWifiPassMax]{};
    /** Effectieve service-instellingen (Kconfig + optionele NVS-keys `svc_*`). */
    ServiceRuntimeConfig services{};
};

esp_err_t init();
/** NVS init + namespace; idempotent waar mogelijk. Vult `out.services` + interne cache. */
esp_err_t load_or_defaults(RuntimeConfig &out);
esp_err_t save(const RuntimeConfig &cfg);

/**
 * Snapshot na `load_or_defaults` — zelfde inhoud als `out.services` van de laatste load.
 * Alleen lezen; geen schrijven vanuit services (M-003a).
 */
const ServiceRuntimeConfig &service_runtime();

/** True als er een niet-lege SSID in NVS zit (provisioned). */
bool has_wifi_credentials(const RuntimeConfig &cfg);

/** WiFi-velden leegmaken en committen (factory / geforceerde her-onboarding). */
esp_err_t clear_wifi_credentials();

} // namespace config_store
