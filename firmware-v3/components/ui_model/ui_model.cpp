#include "ui_model/ui_model.hpp"

#include "esp_log.h"

static const char *TAG = "ui_model";

namespace ui_model {

esp_err_t init() {
    ESP_LOGI(TAG, "init (stub) — geen UI-run in deze basisbuild");
    return ESP_OK;
}

}  // namespace ui_model
