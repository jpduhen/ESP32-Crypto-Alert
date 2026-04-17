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
 * M-003b: basis-drempels en calm/hot-‰ uit `config_store::alert_runtime()` (NVS-overlay op Kconfig).
 */

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

} // namespace alert_engine
