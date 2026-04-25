#pragma once

#include "esp_err.h"

namespace market_store {

/** Ringbuffer / ruwe ticks; single writer: market_ws. */
esp_err_t init();

}  // namespace market_store
