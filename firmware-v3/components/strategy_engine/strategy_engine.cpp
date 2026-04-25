#include "strategy_engine/strategy_engine.hpp"

#include "level_engine/level_engine.hpp"
#include "regime_engine/regime_engine.hpp"
#include "signal_engine/signal_engine.hpp"

#include "esp_log.h"

static const char *TAG = "strategy_engine";

namespace strategy_engine {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub)");
    ESP_RETURN_ON_ERROR(regime_engine::init(), TAG, "regime_engine");
    ESP_RETURN_ON_ERROR(level_engine::init(), TAG, "level_engine");
    ESP_RETURN_ON_ERROR(signal_engine::init(), TAG, "signal_engine");
    return ESP_OK;
}

esp_err_t start() {
    ESP_LOGI(TAG, "start (stub) — geen live ticks tot market_store/candles");
    return ESP_OK;
}

}  // namespace strategy_engine
