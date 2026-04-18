#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "market_types/types.hpp"

namespace exchange_bitvavo::ws {

esp_err_t start(market_types::MarketSnapshot *snap_sink, const char *market, SemaphoreHandle_t metrics_mx);
void stop();
/** Wandklok-seconde bijwerken (ook bij geen WS-bericht) + `ws_inbound_ticks_last_sec` in snapshot. */
void sync_inbound_tick_stats();

} // namespace exchange_bitvavo::ws
