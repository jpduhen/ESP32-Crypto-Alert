#include "settings_store/settings_store.hpp"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace {

const char *TAG = "SETTINGS";

constexpr const char *kNvsNamespace = "cfg";
constexpr const char *kKeySsid = "wifi_ssid";
constexpr const char *kKeyPass = "wifi_pass";
constexpr const char *kKeyDhcp = "wifi_dhcp";

bool s_resolution_cache_valid = false;
settings_store::WifiSettingsResult s_resolution_cache{};

void clear_wifi_settings(settings_store::WifiSettings *out) {
    if (!out) {
        return;
    }
    std::memset(out->ssid, 0, sizeof(out->ssid));
    std::memset(out->password, 0, sizeof(out->password));
    out->dhcp_enabled = true;
    out->valid = false;
}

bool wifi_settings_well_formed(const settings_store::WifiSettings &s) {
    return s.ssid[0] != '\0';
}

/**
 * Leest alleen NVS; geen fallback. Geen "loaded from NVS" success-log
 * (resolve_wifi_settings voegt bron-specifieke logs toe).
 */
esp_err_t read_wifi_from_nvs(settings_store::WifiSettings *out, bool *valid_out) {
    if (!out || !valid_out) {
        return ESP_ERR_INVALID_ARG;
    }
    clear_wifi_settings(out);
    *valid_out = false;

    nvs_handle_t h;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open RO failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t ssid_len = sizeof(out->ssid);
    err = nvs_get_str(h, kKeySsid, out->ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(h);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "nvs_get_str ssid failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t pass_len = sizeof(out->password);
    err = nvs_get_str(h, kKeyPass, out->password, &pass_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out->password[0] = '\0';
        err = ESP_OK;
    } else if (err != ESP_OK) {
        nvs_close(h);
        ESP_LOGE(TAG, "nvs_get_str pass failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t dhcp_u8 = 1;
    err = nvs_get_u8(h, kKeyDhcp, &dhcp_u8);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dhcp_u8 = 1;
        err = ESP_OK;
    } else if (err != ESP_OK) {
        nvs_close(h);
        ESP_LOGE(TAG, "nvs_get_u8 dhcp failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_close(h);
    out->dhcp_enabled = dhcp_u8 != 0;
    *valid_out = wifi_settings_well_formed(*out);
    if (!*valid_out) {
        clear_wifi_settings(out);
    }
    return ESP_OK;
}

void apply_dev_fallback_to(settings_store::WifiSettings *out) {
    if (!out) {
        return;
    }
    clear_wifi_settings(out);
#if CONFIG_CRYPTO_ALERT_WIFI_USE_DEV_FALLBACK
    std::strncpy(out->ssid, CONFIG_CRYPTO_ALERT_WIFI_SSID, sizeof(out->ssid) - 1);
    out->ssid[sizeof(out->ssid) - 1] = '\0';
    std::strncpy(out->password, CONFIG_CRYPTO_ALERT_WIFI_PASSWORD, sizeof(out->password) - 1);
    out->password[sizeof(out->password) - 1] = '\0';
#if CONFIG_CRYPTO_ALERT_WIFI_DHCP
    out->dhcp_enabled = true;
#else
    out->dhcp_enabled = false;
#endif
    out->valid = wifi_settings_well_formed(*out);
#else
    (void)out;
#endif
}

}  // namespace

namespace settings_store {

const char *wifi_settings_source_label(WifiSettingsSource s) {
    switch (s) {
        case WifiSettingsSource::kNone:
            return "NONE";
        case WifiSettingsSource::kNvs:
            return "NVS";
        case WifiSettingsSource::kDevFallback:
            return "DEV_FALLBACK";
    }
    return "?";
}

esp_err_t init() {
    ESP_LOGI(TAG, "init (NVS klaar vanuit app_main)");
    return ESP_OK;
}

esp_err_t load_wifi_settings(WifiSettings *out) {
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    bool valid = false;
    esp_err_t err = read_wifi_from_nvs(out, &valid);
    if (err != ESP_OK) {
        return err;
    }

    if (!valid) {
        nvs_handle_t h;
        err = nvs_open(kNvsNamespace, NVS_READONLY, &h);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Geen NVS-namespace '%s' — geen opgeslagen WiFi-config", kNvsNamespace);
        } else if (err == ESP_OK) {
            nvs_close(h);
            ESP_LOGW(TAG, "Geen geldige WiFi-instellingen in NVS (alleen NVS-pad)");
        } else {
            ESP_LOGE(TAG, "nvs_open (status) failed: %s", esp_err_to_name(err));
        }
        clear_wifi_settings(out);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "WiFi settings loaded: ssid=\"%s\", dhcp=%d", out->ssid, out->dhcp_enabled ? 1 : 0);
    if (!out->dhcp_enabled) {
        ESP_LOGW(TAG, "dhcp=0: statisch IP is in deze stap nog niet geïmplementeerd; STA blijft bij DHCP (TODO)");
    }
    return ESP_OK;
}

esp_err_t save_wifi_settings(const WifiSettings &in) {
    if (!wifi_settings_well_formed(in)) {
        ESP_LOGW(TAG, "save_wifi_settings: geweigerd (SSID ontbreekt)");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open RW failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(h, kKeySsid, in.ssid);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    err = nvs_set_str(h, kKeyPass, in.password);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    err = nvs_set_u8(h, kKeyDhcp, in.dhcp_enabled ? 1 : 0);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }

    err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi settings saved (ssid=\"%s\", dhcp=%d)", in.ssid, in.dhcp_enabled ? 1 : 0);
        s_resolution_cache_valid = false;
    }
    return err;
}

esp_err_t resolve_wifi_settings(WifiSettingsResult *out) {
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_resolution_cache_valid) {
        *out = s_resolution_cache;
        return ESP_OK;
    }

    std::memset(out, 0, sizeof(*out));
    out->source = WifiSettingsSource::kNone;
    clear_wifi_settings(&out->settings);

    bool nvs_valid = false;
    esp_err_t err = read_wifi_from_nvs(&out->settings, &nvs_valid);
    if (err != ESP_OK) {
        return err;
    }

    if (nvs_valid) {
        out->source = WifiSettingsSource::kNvs;
        out->settings.valid = true;
        ESP_LOGI(TAG, "WiFi settings loaded from NVS");
        ESP_LOGI(TAG, "ssid=\"%s\", dhcp=%d", out->settings.ssid, out->settings.dhcp_enabled ? 1 : 0);
        if (!out->settings.dhcp_enabled) {
            ESP_LOGW(TAG, "dhcp=0: statisch IP nog niet ondersteund; STA gebruikt DHCP (TODO)");
        }
        s_resolution_cache = *out;
        s_resolution_cache_valid = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "No valid WiFi settings in NVS");

#if CONFIG_CRYPTO_ALERT_WIFI_USE_DEV_FALLBACK
    apply_dev_fallback_to(&out->settings);
    if (out->settings.valid) {
        out->source = WifiSettingsSource::kDevFallback;
        ESP_LOGI(TAG, "Using development fallback WiFi settings");
        ESP_LOGI(TAG, "ssid=\"%s\", dhcp=%d", out->settings.ssid, out->settings.dhcp_enabled ? 1 : 0);

#if CONFIG_CRYPTO_ALERT_WIFI_SAVE_FALLBACK_TO_NVS
        esp_err_t se = save_wifi_settings(out->settings);
        if (se == ESP_OK) {
            ESP_LOGI(TAG, "Development WiFi settings persisted to NVS for next boot");
            out->source = WifiSettingsSource::kNvs;
        } else {
            ESP_LOGW(TAG, "Could not persist development WiFi to NVS: %s", esp_err_to_name(se));
        }
#endif
        s_resolution_cache = *out;
        s_resolution_cache_valid = true;
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Development fallback WiFi settings not configured");
#else
    ESP_LOGW(TAG, "Development WiFi fallback is disabled in menuconfig");
#endif

    out->source = WifiSettingsSource::kNone;
    clear_wifi_settings(&out->settings);
    s_resolution_cache = *out;
    s_resolution_cache_valid = true;
    return ESP_OK;
}

}  // namespace settings_store
