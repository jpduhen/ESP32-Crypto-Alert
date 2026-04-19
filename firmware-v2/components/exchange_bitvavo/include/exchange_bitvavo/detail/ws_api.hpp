#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "market_types/types.hpp"

namespace exchange_bitvavo::ws {

esp_err_t start(market_types::MarketSnapshot *snap_sink, const char *market, SemaphoreHandle_t metrics_mx);
void stop();
/** Wandklok-seconde bijwerken (ook bij geen WS-bericht) + per-seconde tellers in snapshot. */
void sync_inbound_tick_stats();

/** RWS-01: `ws_gap_sec_since_last_*` bijwerken + eventueel `[WS_GAP]` (canonical ≥12 s). */
void publish_gap_metrics();

} // namespace exchange_bitvavo::ws
