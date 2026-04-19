#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

namespace display_port {

/**
 * ST7789 + SPI via esp_lcd (ADR-001). Laag-niveau: bus, panel, backlight.
 * LVGL draait in `ui` en gebruikt onderstaande handles (ADR-004); geen exchange/market_data hier.
 */
esp_err_t init();

/** Na `init()`: panel-IO voor `lvgl_port_add_disp` (SPI). */
esp_lcd_panel_io_handle_t io_handle();

/** Na `init()`: ST7789-panel voor flush. */
esp_lcd_panel_handle_t panel_handle();

unsigned panel_width();
unsigned panel_height();

/**
 * T-103 field-test: smalle band onderaan toggelt kleur (directe `esp_lcd`-pixels).
 * Niet combineren met actieve LVGL-vulling op hetzelfde gebied — bij echte UI uitgeschakeld in `app_core`.
 */
void ws_live_validation_strip_toggle();

} // namespace display_port
