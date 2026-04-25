#include "settings_store/settings_store.hpp"

#include "esp_log.h"

static const char *TAG = "settings_store";

namespace settings_store {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub) — nvs_flash al door app_main");
    return ESP_OK;
}

}  // namespace settings_store
