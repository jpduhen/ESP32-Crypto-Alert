#include "market_store/market_store.hpp"

#include "esp_log.h"

static const char *TAG = "market_store";

namespace market_store {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub) — nog niet gekoppeld vanuit app_core");
    return ESP_OK;
}

}  // namespace market_store
