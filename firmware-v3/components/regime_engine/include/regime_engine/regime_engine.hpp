#pragma once

#include "esp_err.h"

namespace regime_engine {

/** Regime/volatiliteit; input later vanuit candle/market_store. */
esp_err_t init();

}  // namespace regime_engine
