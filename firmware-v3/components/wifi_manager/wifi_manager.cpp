#include "wifi_manager/wifi_manager.hpp"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

namespace {

struct wifi_manager_state {
    wifi_manager::phase phase{wifi_manager::phase::uninitialized};
};

wifi_manager_state s_wifi;

const char *TAG = "wifi_manager";

void set_phase(wifi_manager::phase p) {
    if (s_wifi.phase == p) {
        return;
    }
    s_wifi.phase = p;
    switch (p) {
        case wifi_manager::phase::uninitialized:
            ESP_LOGI(TAG, "status: uninitialized");
            break;
        case wifi_manager::phase::initialized:
            ESP_LOGI(TAG, "status: initialized");
            break;
        case wifi_manager::phase::idle_no_credentials:
            ESP_LOGI(TAG, "status: idle (geen credentials)");
            break;
        case wifi_manager::phase::connecting:
            ESP_LOGI(TAG, "status: connecting");
            break;
        case wifi_manager::phase::connected:
            ESP_LOGI(TAG, "status: connected");
            break;
        case wifi_manager::phase::disconnected:
            ESP_LOGW(TAG, "status: disconnected");
            break;
        case wifi_manager::phase::error:
            ESP_LOGE(TAG, "status: error");
            break;
    }
}

}  // namespace

namespace wifi_manager {

esp_err_t init() {
    if (s_wifi.phase != phase::uninitialized) {
        return ESP_OK;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode STA");

    // TODO: credentials uit NVS/settings_store laden; hier geen harde SSID.
    // TODO: event handlers (IP_EVENT_STA_GOT_IP, WIFI_EVENT_STA_*) registreren.
    // TODO: esp_wifi_set_config(WIFI_IF_STA, ...) + esp_wifi_start() zodra SSID bekend is.

    set_phase(phase::initialized);
    return ESP_OK;
}

esp_err_t start() {
    ESP_LOGW(TAG, "start: skeleton — geen esp_wifi_start() zonder geldige STA-config");
    ESP_LOGW(TAG, "TODO: na NVS-credentials: set_config, esp_wifi_start, esp_wifi_connect");
    set_phase(phase::idle_no_credentials);
    return ESP_OK;
}

phase get_phase() { return s_wifi.phase; }

}  // namespace wifi_manager
