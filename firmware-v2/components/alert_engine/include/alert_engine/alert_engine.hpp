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
    int threshold_scale_permille{1000};
    double base_threshold_move_pct_1m{0.0};
    double base_threshold_move_pct_5m{0.0};
    double effective_threshold_move_pct_1m{0.0};
    double effective_threshold_move_pct_5m{0.0};
};

esp_err_t init();

/** Eén evaluatieslag: log + event bij trigger (transport alleen via `service_outbound`). */
void tick();

/** Read-only: laatste `RegimeObservabilitySnapshot` (voor M-013e). */
void get_regime_observability_snapshot(RegimeObservabilitySnapshot *out);

/** Read-only: laatste pad-beslissingen (M-013h). */
void get_alert_decision_observability_snapshot(AlertDecisionObservabilitySnapshot *out);

} // namespace alert_engine
