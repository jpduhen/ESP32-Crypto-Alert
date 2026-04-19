#pragma once

#include "esp_err.h"
#include "market_types/types.hpp"

namespace exchange_bitvavo {

/**
 * Bitvavo exchange-laag (REST bootstrap + WS live) — alleen door `market_data` aan te roepen.
 * Geen UI- of display-afhankelijkheden.
 *
 * M-002: TLS/REST/WS hier; WiFi in net_runtime. WS-reconnect: esp_websocket_client-intern + events.
 */
esp_err_t init(const char *market_symbol);
void tick();
market_types::MarketSnapshot snapshot();

} // namespace exchange_bitvavo
