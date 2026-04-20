/**
 * RWS-03: SecondSampler / SecondAggregate — parallel aan ticker-canonical pad.
 * Per voltooide wandklok-seconde: trade-statistieken uit WS-trades; ticker alleen als context.
 * Bounded ring; optionele koppeling aan domain_metrics via Kconfig (RWS-04).
 */
#pragma once

#include <cstdint>

#include "market_types/types.hpp"

namespace exchange_bitvavo::second_aggregate {

/** Reset ring + running state (bij nieuwe WS-sessie). */
void reset_on_ws_connect();

/**
 * Trade-event in huidige seconde (caller houdt exchange-mutex vast indien nodig; hier lock-vrij).
 * `wall_sec_id` = zelfde basis als `sync_inbound_tick_stats` (esp_timer s).
 */
void note_trade(double price_eur, uint64_t wall_sec_id);

/** Canonical ticker-tick (apply_price) in huidige seconde — alleen teller / flag. */
void note_canonical_tick(uint64_t wall_sec_id);

/**
 * Voltooide seconde `completed_wall_sec` afronden: ring + `snap` velden RWS-03.
 * Caller moet `metrics_mx` vasthouden.
 */
void finalize_completed_second(uint64_t completed_wall_sec, market_types::MarketSnapshot *snap);

/** Na finalize-keten: running state voor `new_wall_sec` (eerste seconde na catch-up). */
void seed_running_second(uint64_t new_wall_sec);

/**
 * RWS-04: zoek een voltooide seconde in de bounded ring (max. k_ring_cap terug).
 * Lege ring-slots hebben wall_sec_id = UINT64_MAX en worden overgeslagen.
 */
struct AggregatedSecondLookup {
    bool found{false};
    bool has_trades{false};
    uint32_t trade_count{0};
    /** RWS-04: voorkeursrepresentatie = arithmetic mean over trades (zelfde als RWS-03 `mean_eur`). */
    double trade_mean_eur{0.0};
    double trade_last_eur{0.0};
};

bool lookup_completed_second(uint64_t wall_sec_id, AggregatedSecondLookup *out);

} // namespace exchange_bitvavo::second_aggregate
