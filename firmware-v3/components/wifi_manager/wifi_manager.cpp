#include "wifi_manager/wifi_manager.hpp"

#include <cstdio>
#include <cstring>

#include "diagnostics/diagnostics.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "settings_store/settings_store.hpp"

static const char *TAG = "WIFI";

namespace wifi_manager {

const char *state_label(WifiState s) {
    switch (s) {
        case WifiState::kUninitialized:
            return "Uninitialized";
        case WifiState::kIdle:
            return "Idle";
        case WifiState::kStarting:
            return "Starting";
        case WifiState::kConnecting:
            return "Connecting";
        case WifiState::kConnected:
            return "Connected";
        case WifiState::kGotIp:
            return "GotIp";
        case WifiState::kDisconnected:
            return "Disconnected";
        case WifiState::kError:
            return "Error";
        case WifiState::kNotConfigured:
            return "NotConfigured";
    }
    return "?";
}

}  // namespace wifi_manager

namespace {

struct WifiManagerCtx {
    wifi_manager::WifiState state{wifi_manager::WifiState::kUninitialized};
    bool driver_ready{false};
    bool station_started{false};
    uint32_t reconnect_attempts{0};
};

WifiManagerCtx s_ctx;

void set_state(wifi_manager::WifiState next) {
    if (s_ctx.state == next) {
        return;
    }
    s_ctx.state = next;
    const char *n = wifi_manager::state_label(next);
    switch (next) {
        case wifi_manager::WifiState::kDisconnected:
            ESP_LOGW(TAG, "State -> %s", n);
            break;
        case wifi_manager::WifiState::kNotConfigured:
            ESP_LOGW(TAG, "State -> %s", n);
            break;
        case wifi_manager::WifiState::kError:
            ESP_LOGE(TAG, "State -> %s", n);
            break;
        default:
            ESP_LOGI(TAG, "State -> %s", n);
            break;
    }

    char buf[56];
    std::snprintf(buf, sizeof(buf), "wifi %s", n);
    diagnostics::log_compact_status(buf);
}

esp_err_t apply_sta_config(const settings_store::WifiSettings &ws) {
    wifi_config_t cfg{};
    std::memset(&cfg, 0, sizeof(cfg));
    std::strncpy(reinterpret_cast<char *>(cfg.sta.ssid), ws.ssid, sizeof(cfg.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(cfg.sta.password), ws.password, sizeof(cfg.sta.password) - 1);
    if (ws.password[0] == '\0') {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    return esp_wifi_set_config(WIFI_IF_STA, &cfg);
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_STA_START) {
        set_state(wifi_manager::WifiState::kConnecting);
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
            set_state(wifi_manager::WifiState::kError);
        }
        return;
    }

    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        set_state(wifi_manager::WifiState::kConnected);
        ESP_LOGI(TAG, "Connected to AP");
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto *ev = static_cast<wifi_event_sta_disconnected_t *>(event_data);
        s_ctx.reconnect_attempts++;
        ESP_LOGW(TAG, "STA disconnected (reason=%d), reconnect poging %lu", static_cast<int>(ev->reason),
                 static_cast<unsigned long>(s_ctx.reconnect_attempts));
        // TODO: exponentiële backoff, max pogingen, en aparte handling bij AUTH_FAIL.
        set_state(wifi_manager::WifiState::kDisconnected);
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect na disconnect failed: %s", esp_err_to_name(err));
            set_state(wifi_manager::WifiState::kError);
        } else {
            set_state(wifi_manager::WifiState::kConnecting);
        }
        return;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_STA_GOT_IP) {
        const auto *ev = static_cast<ip_event_got_ip_t *>(event_data);
        s_ctx.reconnect_attempts = 0;
        set_state(wifi_manager::WifiState::kGotIp);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

}  // namespace

namespace wifi_manager {

esp_err_t init() {
    if (s_ctx.driver_ready) {
        return ESP_OK;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, nullptr, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    s_ctx.driver_ready = true;
    set_state(WifiState::kIdle);
    ESP_LOGI(TAG, "WiFi init klaar (STA mode)");
    return ESP_OK;
}

esp_err_t start() {
    if (!s_ctx.driver_ready) {
        ESP_LOGE(TAG, "start: init niet gedaan");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.station_started) {
        ESP_LOGW(TAG, "start: station al gestart");
        return ESP_OK;
    }

    settings_store::WifiSettingsResult wr{};
    esp_err_t err = settings_store::resolve_wifi_settings(&wr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "resolve_wifi_settings failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    if (!wr.settings.valid) {
        ESP_LOGW(TAG, "No valid WiFi settings, starting skipped");
        set_state(WifiState::kNotConfigured);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting STA (settings source=%s)", settings_store::wifi_settings_source_label(wr.source));

    err = apply_sta_config(wr.settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    set_state(WifiState::kStarting);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        set_state(WifiState::kError);
        return err;
    }

    s_ctx.station_started = true;
    s_ctx.reconnect_attempts = 0;
    ESP_LOGI(TAG, "WiFi gestart, wacht op STA_START / connect");
    return ESP_OK;
}

WifiState get_state() { return s_ctx.state; }

}  // namespace wifi_manager
