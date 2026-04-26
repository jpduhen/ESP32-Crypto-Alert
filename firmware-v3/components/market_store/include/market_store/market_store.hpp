#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace market_store {

/** Eén genormaliseerde tick (bron: Bitvavo ticker JSON; geen tradinglogica). */
struct TickEvent {
    uint64_t ts_ms;
    double price;
    double bid;
    double ask;
    double last_size;
    bool has_trade_price;
    bool has_bid;
    bool has_ask;
};

/** Actuele markt-snapshot voor consumenten (UI/strategie later). */
struct MarketSnapshot {
    uint64_t ts_ms;
    double last_price;
    double best_bid;
    double best_ask;
    bool ws_live;
    /** Laatst gezien transport-rx (van market_ws) bij laatste ingest. */
    uint64_t transport_rx;
};

struct IngestStats {
    uint64_t frames;
    uint64_t ticker_ok;
    uint64_t ignored;
    uint64_t failed;
};

esp_err_t init();

/**
 * Ruwe WS text (transport levert aan). Defensief parsen; bij geldige BTC-EUR ticker:
 * snapshot bijwerken + EV_MARKET_TICK posten naar default event loop.
 * @param transport_rx laatste MWS rx-teller (voor snapshot, geen reverse dependency).
 */
void ingest_ws_text(const char *data, size_t len, uint64_t transport_rx);

MarketSnapshot get_snapshot();

IngestStats get_ingest_stats();

}  // namespace market_store
