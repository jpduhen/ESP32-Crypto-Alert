#pragma once

#include <cstdint>

#include "candle_engine/candle_engine.hpp"
#include "esp_err.h"

namespace regime_engine {

enum class RegimeClass {
    kUnknown,
    kRange,
    kNeutral,
    kTrend,
};

/**
 * Lichtgewicht regime/context op basis van gesloten 1m + rolling analytics.
 * Geen signalen of tradingbeslissingen.
 */
struct RegimeSnapshot {
    uint64_t ts_ms;
    /** Minimaal ~2 gesloten candles + bruikbare returns/ranges waar nodig. */
    bool valid;
    RegimeClass regime;
    double abs_ret_1m;
    double abs_ret_5m;
    double abs_ret_30m;
    /** ((max-min)/close)*100 op laatste gesloten close als noemer; 0 als venster ongeldig. */
    double range_5m_pct;
    double range_30m_pct;
    /** |ret| t.o.v. range (5m als beschikbaar, anders 1m), genormaliseerd [0,1]. */
    double trend_strength_score;
    /** Range t.o.v. |returns| (5m/1m); hoog bij veel wisseling weinig netto, [0,1]. */
    double choppiness_score;
};

esp_err_t init();

/** Laatste berekende regime (thread: default event loop). */
bool get_snapshot(RegimeSnapshot *out);

/**
 * Herberekent regime uit analytics (idem als interne 1m-close handler).
 * Publiek voor tests; normale pad: `EV_1M_CLOSED` → intern `get_analytics_snapshot`.
 */
void update_from_analytics(const candle_engine::MarketAnalyticsSnapshot &analytics);

}  // namespace regime_engine
