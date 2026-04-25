#include "display_ui/display_ui.hpp"

#include "esp_log.h"

static const char *TAG = "display_ui";

namespace display_ui {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub) — S3-GEEK display pipeline TODO");
    return ESP_OK;
}

}  // namespace display_ui
