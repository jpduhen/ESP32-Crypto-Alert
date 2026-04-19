#pragma once

#include <cstdint>

#include "esp_err.h"

namespace alert_engine {

/**
 * M-010a / M-011b: minimale 1m%-move drempel + cooldown; beslist op `domain_metrics`;
 * bij trigger: payload naar `service_outbound::emit_domain_alert_1m` (symbool via `market_data::snapshot`).
 * M-010c: parallel 5m%-move drempel + cooldown → `emit_domain_alert_5m` (geen confluence met 1m).
 * M-010d: eerste confluence (1m+5m zelfde richting, beide ≥ drempel) → `emit_domain_confluence_1m5m`.
 * M-010e: confluence eerst; daarna losse 1m/5m — tijdelijk onderdrukken zelfde richting na confluence.
 * M-010f: mini-regime (calm/normal/hot) uit vol-proxy — alleen schaal van effectieve 1m/5m-drempels (+ confluence).
 * M-013e: read-only snapshot voor WebUI — geen extra beslislogica.
 * M-013h: read-only laatste beslissing per pad (1m / 5m / confluence) + suppress/cooldown-restant — alleen snapshot.
 * M-003b: basis-drempels en calm/hot-‰ uit `config_store::alert_runtime()` (NVS-overlay op Kconfig).
 * M-003d: confluence-policy booleans uit `config_store::alert_confluence_policy()` (defaults = M-010d/e).
 */

/** Eén alert-pad: laatste evaluatie in `tick()` (M-013h). Status/reason in het Engels, stabiel voor JSON. */
struct AlertPathDecisionSnapshot {
    /** o.a. `not_ready`, `below_threshold`, `cooldown`, `suppressed`, `fired`, `invalid` */
    char status[24]{};
    /** korte code, bv. `metrics_not_ready`, `direction_mismatch`, `confluence_priority_window` */
    char reason[64]{};
    /** Tijd tot volgende mogelijke trigger bij cooldown, of volledige cd na `fired`; `-1` = n.v.t. */
    int64_t remaining_cooldown_ms{-1};
    /** M-010e venster; `-1` = n.v.t. */
    int64_t remaining_suppress_ms{-1};
};

/** Laatste beslissingen — bijgewerkt aan het **einde** van elke `tick()`. */
struct AlertDecisionObservabilitySnapshot {
    AlertPathDecisionSnapshot tf_1m{};
    AlertPathDecisionSnapshot tf_5m{};
    AlertPathDecisionSnapshot confluence_1m5m{};
};

/** C2: randen — alleen transitie-tellingen + laatste epoch (geen extra beslislogica). */
struct AlertPathEdgeStats {
    uint32_t enter_cooldown{};
    uint32_t enter_suppressed{};
    uint32_t enter_not_ready{};
    /** Laatste keer `status` naar deze toestand ging (edge). */
    int64_t last_epoch_ms_enter_cooldown{-1};
    int64_t last_epoch_ms_enter_suppressed{-1};
    int64_t last_epoch_ms_enter_not_ready{-1};
};

/**
 * C1: read-only totalen sinds boot — geen extra beslislogica; alleen voor field-test / spamreview.
 * C2: `edge_*` — transitie-tellers + timestamps voor randgevallen-debug.
 * Cooldown-druk volgt uit `AlertPathDecisionSnapshot` (`status=cooldown`) per tick, niet als globale teller.
 */
struct AlertEngineRuntimeStatsSnapshot {
    uint32_t emit_total_1m{};
    uint32_t emit_total_5m{};
    uint32_t emit_total_conf{};
    /** `esp_timer_get_time()/1000`; `-1` = nog geen emit. */
    int64_t last_emit_epoch_ms_1m{-1};
    int64_t last_emit_epoch_ms_5m{-1};
    int64_t last_emit_epoch_ms_conf{-1};
    /** Losse 1m/5m onderdrukt door M-010e-venster na confluence (zelfde richting). */
    uint32_t suppress_after_conf_window_1m{};
    uint32_t suppress_after_conf_window_5m{};
    AlertPathEdgeStats edge_1m{};
    AlertPathEdgeStats edge_5m{};
    AlertPathEdgeStats edge_confluence{};
};

/** Laatst bekende regime/vol/drempels (bijgewerkt aan het begin van elke `tick()`). Veldnamen stabiel voor JSON. */
struct RegimeObservabilitySnapshot {
    bool vol_metric_ready{false};
    /** `domain_metrics::compute_vol_mean_abs_step_bps` — gemiddelde |Δ| in bps (~1s-stappen). */
    double vol_mean_abs_step_bps{0.0};
    uint32_t vol_pairs_used{0};
    /** Geen vol-metric (warmup): regime gedropt naar normal, schaal 1000‰. */
    bool vol_unavailable_fallback{false};
    /** `"calm"` | `"normal"` | `"hot"` */
    char regime[16]{};
    /** Effectieve schaal na M-010f ‰-clamp ([CONFIG_ALERT_REGIME_THR_SCALE_MIN/MAX_PERMILLE]). */
    int threshold_scale_permille{1000};
    /** Schaal rechtstreeks uit regime + `alert_runtime` vóór clamp (C3). */
    int threshold_scale_permille_raw{1000};
    /** `true` als `threshold_scale_permille_raw` ≠ effectieve schaal (NVS kan 700‰ calm zijn → 750‰ effectief). */
    bool threshold_scale_clamped{false};
    /** Compile-time grenzen calm/normal/hot (Kconfig), voor field-debug naast `vol_mean_abs_step_bps`. */
    int regime_calm_max_step_bps{0};
    int regime_hot_min_step_bps{0};
    double base_threshold_move_pct_1m{0.0};
    double base_threshold_move_pct_5m{0.0};
    double effective_threshold_move_pct_1m{0.0};
    double effective_threshold_move_pct_5m{0.0};
    /** C2: laatste regime-labelwissel (calm/normal/hot); `-1` = nog geen wissel sinds boot. */
    int64_t last_regime_change_epoch_ms{-1};
};

esp_err_t init();

/** Eén evaluatieslag: log + event bij trigger (transport alleen via `service_outbound`). */
void tick();

/** Read-only: laatste `RegimeObservabilitySnapshot` (voor M-013e). */
void get_regime_observability_snapshot(RegimeObservabilitySnapshot *out);

/** Read-only: laatste pad-beslissingen (M-013h). */
void get_alert_decision_observability_snapshot(AlertDecisionObservabilitySnapshot *out);

/** Read-only: emit-/suppress-totalen sinds boot (C1). */
void get_alert_runtime_stats_snapshot(AlertEngineRuntimeStatsSnapshot *out);

} // namespace alert_engine
