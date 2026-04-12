#pragma once

#include "config_store/config_store.hpp"
#include "esp_err.h"

namespace wifi_onboarding {

/**
 * WiFi-provisioning (SoftAP + minimale browserportal).
 * - Geen Bitvavo/market_data-afhankelijkheid.
 * - Als NVS geen SSID heeft: start AP `CryptoAlert`, HTTP-formulier, save naar NVS, esp_restart().
 * - Anders: direct return.
 *
 * M-002: alleen credentials + restart; STA daarna door net_runtime (ADR-003).
 */
esp_err_t run(config_store::RuntimeConfig &cfg);

/**
 * Geforceerde her-onboarding: WiFi in NVS wissen; volgende `run()` opent portal.
 * (Kleine haak voor knop/factory; nu handmatig aanroepbaar.)
 */
esp_err_t clear_credentials_for_reprovision();

} // namespace wifi_onboarding
