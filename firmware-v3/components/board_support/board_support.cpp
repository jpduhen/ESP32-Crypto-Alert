#include "board_support/board_support.hpp"

#include "esp_log.h"

static const char *TAG = "board_support";

namespace board_support {

esp_err_t init() {
    ESP_LOGI(TAG, "board_support: ESP32-S3-GEEK (primair target) — display/peripherals TODO");
    return ESP_OK;
}

}  // namespace board_support
