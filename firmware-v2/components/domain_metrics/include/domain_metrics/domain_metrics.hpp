#pragma once

#include <cstddef>

#include "market_data/market_data.hpp"
#include "esp_err.h"

namespace domain_metrics {

/**
 * M-010a: compacte prijs-historie (rolling) + afgeleide 1m-move metric.
 * M-010b: canonicalisatie naar 1 representatieve secondewaarde vóór opslag.
 * M-010c: zelfde bufferbasis + 5m %-move metric (parallel aan 1m, geen confluence).
 * S30-1: langere canonieke ring + 30m %-move (parallel aan 1m/5m; nog geen alerts).
 * S2H-1: 2h %-move via aparte minuut-decimatie-ring (geen 7200×1 Hz — RAM-trade-off); nog geen 2h-alerts.
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

/**
 * S30-1: signed procentuele beweging over ~30m (Kconfig DOMAIN_METRICS_30M_WINDOW_S), zelfde canonieke pad als 1m/5m.
 * `ready` als er een referentiesample ≤ now−window in de ring staat en prijzen > 0.
 */
struct Metric30mMovePct {
    bool ready{false};
    double pct{0.0};
    int64_t ref_ts_ms{0};
    double ref_price_eur{0.0};
    int64_t now_ts_ms{0};
    double now_price_eur{0.0};
    /** `now_ts_ms − ref_ts_ms` wanneer ready (typisch ≈ venster). */
    int64_t ref_span_ms{0};
};

Metric30mMovePct compute_30m_move_pct();

/**
 * S2H-1: signed move over ~2h (Kconfig `DOMAIN_METRICS_2H_WINDOW_S`).
 * Referentie uit minuut-buffer; `now` uit canonieke ring. `ready=false` bij onvoldoende minuut-historie.
 */
struct Metric2hMovePct {
    bool ready{false};
    double pct{0.0};
    int64_t ref_ts_ms{0};
    double ref_price_eur{0.0};
    int64_t now_ts_ms{0};
    double now_price_eur{0.0};
    int64_t ref_span_ms{0};
    /** Aantal minuut-slots in gebruik (observability). */
    uint32_t minute_ring_used{0};
};

Metric2hMovePct compute_2h_move_pct();

/** Aantal canonieke secondesamples in RAM (≤ ringcap). */
size_t canonical_ring_count();

/** Gemiddelde absolute stap tussen opeenvolgende 1s-canonieke samples in het venster (realized vol-proxy). */
struct MetricVolMeanAbsStepBps {
    bool ready{false};
    /** Gemiddelde van |Δp/p|×10⁴ over geldige opeenvolgende paren (~1s). */
    double mean_abs_step_bps{0.0};
    uint32_t pairs_used{0};
};

MetricVolMeanAbsStepBps compute_vol_mean_abs_step_bps();

/**
 * RWS-04 / RWS-04b: read-only observability — metrics-ingang + A/B ticker-canonical vs trade-mean.
 * `last_*` / `last_ab_class` wijzen naar interne stringconstanten (geldig tot volgende finalize).
 */
struct MetricsInputSourceObservability {
    bool rws04_enabled{false};
    uint32_t seconds_via_trade_mean{0};
    uint32_t seconds_via_fallback{0};
    const char *last_finalize_source{""};
    const char *last_fallback_reason{""};
    /** RWS-04b: afgeronde seconden waarin A/B-snapshot is bijgewerkt (RWS-04 aan). */
    uint32_t ab_compare_seconds_total{0};
    /** RWS-04b: seconden met ≥1 trade in aggregate-ring. */
    uint32_t ab_compare_trade_seconds_total{0};
    /** RWS-04b: keren dat |ticker−mean| ≥ drempel (bps). */
    uint32_t ab_compare_large_delta_total{0};
    uint64_t last_compare_wall_sec{0};
    double last_ticker_canonical_eur{0.0};
    double last_trade_mean_eur{0.0};
    double last_delta_abs_eur{0.0};
    double last_delta_bps{0.0};
    double last_delta_pct{0.0};
    uint32_t last_trade_count{0};
    bool last_large_delta{false};
    /** bv. `aggregate`, `aggregate_large_delta`, `fallback`, `no_trades` */
    const char *last_ab_class{""};
    /** Kconfig `DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS` (0 = uit). */
    int large_delta_threshold_bps{0};
};

MetricsInputSourceObservability metrics_input_source_observability();

} // namespace domain_metrics
