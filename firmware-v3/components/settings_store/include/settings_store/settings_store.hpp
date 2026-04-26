#pragma once

#include "esp_err.h"

namespace settings_store {

/** WiFi STA-instellingen (geen runtime-mutable globals buiten deze module). */
struct WifiSettings {
    char ssid[33];
    char password[65];
    bool dhcp_enabled;
    bool valid;
};

/** Bron van de effectieve WiFi-instellingen na resolve. */
enum class WifiSettingsSource {
    kNone,
    kNvs,
    kDevFallback,
};

struct WifiSettingsResult {
    WifiSettings settings{};
    WifiSettingsSource source{WifiSettingsSource::kNone};
};

/** Stabiel label voor logs (Engels). */
const char *wifi_settings_source_label(WifiSettingsSource s);

/** Eénmalig; NVS-partitie moet al geïnitialiseerd zijn (zie app_main). */
esp_err_t init();

/**
 * Alleen NVS namespace "cfg" — geen dev-fallback.
 * Logt SSID en dhcp bij succes, nooit het wachtwoord.
 */
esp_err_t load_wifi_settings(WifiSettings *out);

/**
 * NVS eerst; bij ongeldig/ontbrekend optioneel build-time fallback (menuconfig).
 * Resultaat wordt één keer per boot gecached tot save_wifi_settings NVS wijzigt.
 * Nooit het wachtwoord loggen.
 */
esp_err_t resolve_wifi_settings(WifiSettingsResult *out);

/**
 * Schrijft WiFi-instellingen naar NVS (provisioning / dev-persist).
 * Wachtwoord wordt niet gelogd. Invalidates resolve-cache bij succes.
 */
esp_err_t save_wifi_settings(const WifiSettings &in);

}  // namespace settings_store
