#include "level_engine/level_engine.hpp"

#include "esp_log.h"

static const char *TAG = "level_engine";

namespace level_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    return ESP_OK;
}

}  // namespace level_engine
