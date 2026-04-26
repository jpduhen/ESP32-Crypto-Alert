/**
 * Crypto-Alert V3 — applicatie-ingang.
 * Alleen platform-bootstrap en start van app_core (geen businesslogica hier).
 */
#include "app_core/app_core.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "APP";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Crypto-Alert V3 boot gestart (ESP-IDF)");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS: erase + re-init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(app_core::start());

    ESP_LOGI(TAG, "app_core gestart; main task idle");
}
