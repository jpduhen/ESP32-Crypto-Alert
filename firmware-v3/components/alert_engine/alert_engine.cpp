#include "alert_engine/alert_engine.hpp"

#include "esp_log.h"

static const char *TAG = "alert_engine";

namespace alert_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    // TODO: queues/events vanuit strategy_engine; state machine alerts.
    return ESP_OK;
}

esp_err_t start() {
    ESP_LOGI(TAG, "start (stub) — geen verzending");
    return ESP_OK;
}

}  // namespace alert_engine
