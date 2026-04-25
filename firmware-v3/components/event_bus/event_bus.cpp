#include "event_bus/event_bus.hpp"

#include "esp_event.h"
#include "esp_log.h"

ESP_EVENT_DEFINE_BASE(CRYPTO_V3_EVENTS);

static const char *TAG = "event_bus";

namespace event_bus {

esp_err_t init() {
    ESP_LOGI(TAG, "event_bus: CRYPTO_V3_EVENTS geregistreerd (default loop)");
    return ESP_OK;
}

}  // namespace event_bus
