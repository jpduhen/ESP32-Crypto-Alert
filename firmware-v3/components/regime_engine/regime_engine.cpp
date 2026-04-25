#include "regime_engine/regime_engine.hpp"

#include "esp_log.h"

static const char *TAG = "regime_engine";

namespace regime_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    return ESP_OK;
}

}  // namespace regime_engine
