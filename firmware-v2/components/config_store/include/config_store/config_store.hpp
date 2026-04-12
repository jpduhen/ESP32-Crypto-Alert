#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>

namespace config_store {

/** Verhoog bij schema-wijziging; load() migreert of valt terug op defaults. */
constexpr uint32_t kSchemaVersion = 2;

/** Inclusief null; ESP-IDF wifi_config gebruikt 32/64 byte buffers. */
constexpr size_t kWifiSsidMax = 33;
constexpr size_t kWifiPassMax = 65;

struct RuntimeConfig {
    uint32_t schema_version{kSchemaVersion};
    char default_symbol[24]{"BTC-EUR"};
    uint32_t flags{0};
    /** STA-credentials (NVS); leeg = nog niet geprovisioned via onboarding. */
    char wifi_sta_ssid[kWifiSsidMax]{};
    char wifi_sta_pass[kWifiPassMax]{};
};

esp_err_t init();
/** NVS init + namespace; idempotent waar mogelijk. */
esp_err_t load_or_defaults(RuntimeConfig &out);
esp_err_t save(const RuntimeConfig &cfg);

/** True als er een niet-lege SSID in NVS zit (provisioned). */
bool has_wifi_credentials(const RuntimeConfig &cfg);

/** WiFi-velden leegmaken en committen (factory / geforceerde her-onboarding). */
esp_err_t clear_wifi_credentials();

} // namespace config_store
