#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "market_types/types.hpp"

namespace exchange_bitvavo::ws {

esp_err_t start(market_types::MarketSnapshot *snap_sink, const char *market, SemaphoreHandle_t metrics_mx);
void stop();

} // namespace exchange_bitvavo::ws
