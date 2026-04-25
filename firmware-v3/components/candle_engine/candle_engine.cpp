#include "candle_engine/candle_engine.hpp"

#include "esp_log.h"

static const char *TAG = "candle_engine";

namespace candle_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    return ESP_OK;
}

}  // namespace candle_engine
