#include "ntfy_client/ntfy_client.hpp"

#include "esp_log.h"

static const char *TAG = "ntfy_client";

namespace ntfy_client {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub) — geen POST zonder topic/URL-config");
    // TODO: settings_store → topic/base URL; esp_http_client sessie.
    return ESP_OK;
}

}  // namespace ntfy_client
