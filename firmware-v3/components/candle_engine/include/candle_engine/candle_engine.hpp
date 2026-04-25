#pragma once

#include "esp_err.h"

namespace candle_engine {

/** Aggregatie OHLCV; single writer op tick-stream uit market_store. */
esp_err_t init();

}  // namespace candle_engine
