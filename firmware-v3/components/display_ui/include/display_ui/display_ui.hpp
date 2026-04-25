#pragma once

#include "esp_err.h"

namespace display_ui {

/** LVGL/display; alleen ui_model als bron. */
esp_err_t init();

}  // namespace display_ui
