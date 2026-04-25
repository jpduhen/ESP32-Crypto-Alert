#include "signal_engine/signal_engine.hpp"

#include "esp_log.h"

static const char *TAG = "signal_engine";

namespace signal_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    return ESP_OK;
}

}  // namespace signal_engine
