#pragma once

#include "esp_err.h"
#include "market_data/types.hpp"

namespace ui {

/**
 * LVGL + esp_lvgl_port op de bestaande esp_lcd-panel (ADR-004).
 * Live data uitsluitend via `market_data::snapshot()` / `refresh_from_snapshot` — geen exchange in `ui`.
 */
esp_err_t init();

void refresh_from_snapshot(const market_data::MarketSnapshot &snap);

} // namespace ui
