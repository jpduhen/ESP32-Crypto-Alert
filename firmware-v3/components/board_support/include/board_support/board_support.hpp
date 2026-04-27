#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

namespace board_support {

/** Board-specifieke hooks (S3-GEEK). */
esp_err_t init();

bool display_ready();
esp_lcd_panel_handle_t display_panel();
unsigned display_width();
unsigned display_height();

}  // namespace board_support
