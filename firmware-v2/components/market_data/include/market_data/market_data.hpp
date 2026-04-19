#pragma once

#include "config_store/config_store.hpp"
#include "market_data/types.hpp"
#include "esp_err.h"

namespace market_data {

/**
 * Functionele grens naar UI: alleen snapshot/tick — geen Bitvavo- of TLS-details.
 * Backend: mock (menuconfig uit) of `exchange_bitvavo` achter deze API.
 *
 * M-002: enige bron van `MarketSnapshot` voor app_core; exchange_bitvavo niet direct vanuit UI.
 */
esp_err_t init(const config_store::RuntimeConfig &cfg);
void tick();
MarketSnapshot snapshot();

} // namespace market_data
