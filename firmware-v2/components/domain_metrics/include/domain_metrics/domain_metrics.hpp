#pragma once

#include "market_data/market_data.hpp"
#include "esp_err.h"

namespace domain_metrics {

/**
 * M-010a: compacte prijs-historie (rolling) + afgeleide 1m-move metric.
 * M-010b: canonicalisatie naar 1 representatieve secondewaarde vóór opslag.
 * M-010c: zelfde bufferbasis + 5m %-move metric (parallel aan 1m, geen confluence).
 * M-010f: korte-horizon volatiliteit — gemiddelde |Δprijs| tussen opeenvolgende canonieke secondes (bps).
 * Alleen invoer via `feed(market_data::snapshot)` — geen exchange-details.
 */
esp_err_t init();

/** Voegt een geldige tick toe; negeert ongeldige/lege prijzen. */
void feed(const market_data::MarketSnapshot &snap);

/** Signed procentuele beweging over ~60s: (P_now − P_ref) / P_ref × 100. */
struct Metric1mMovePct {
    bool ready{false};
    double pct{0.0};
    int64_t ref_ts_ms{0};
    double ref_price_eur{0.0};
    int64_t now_ts_ms{0};
    double now_price_eur{0.0};
};

Metric1mMovePct compute_1m_move_pct();

/** Signed procentuele beweging over ~300s (zelfde canonieke secondes als 1m). */
struct Metric5mMovePct {
    bool ready{false};
    double pct{0.0};
    int64_t ref_ts_ms{0};
    double ref_price_eur{0.0};
    int64_t now_ts_ms{0};
    double now_price_eur{0.0};
};

Metric5mMovePct compute_5m_move_pct();

/** Gemiddelde absolute stap tussen opeenvolgende 1s-canonieke samples in het venster (realized vol-proxy). */
struct MetricVolMeanAbsStepBps {
    bool ready{false};
    /** Gemiddelde van |Δp/p|×10⁴ over geldige opeenvolgende paren (~1s). */
    double mean_abs_step_bps{0.0};
    uint32_t pairs_used{0};
};

MetricVolMeanAbsStepBps compute_vol_mean_abs_step_bps();

} // namespace domain_metrics
