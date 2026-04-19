/**
 * M-010a: eerste alert vertical slice — drempel + cooldown (1m).
 * M-010c: parallel 5m-route (eigen drempel/cooldown/event).
 * S30-2/S30-3: parallel 30m-route — drempel/cooldown + logs + decision-observability; S30-3: outbound via `service_outbound`.
 * S2H-2/S2H-3: parallel 2h-route op `compute_2h_move_pct` — drempel/cooldown + logs; S2H-3: outbound via `service_outbound`.
 * M-010d: eenvoudige confluence (1m+5m zelfde richting, beide boven drempel, eigen cooldown).
 * M-010e: confluence vóór losse TF; na confluence kort venster: zelfde richting 1m/5m niet dubbel.
 * M-010f: mini-regime (calm/normal/hot) uit vol-proxy — alleen lichte schaal van 1m/5m/conf-drempels.
 * M-011b: bij 1m-trigger payload naar `service_outbound::emit_domain_alert_1m`.
 * M-013h: decision-observability snapshot aan einde `tick()` (geen extra beslislogica buiten bestaande paden).
 * C1/C2: `AlertEngineRuntimeStatsSnapshot` — emit-tellers + suppress-venster + edge-transities (read-only).
 * M-003c: cooldown/suppress-timing uit `config_store::alert_policy_timing()` (fallback Kconfig via config_store).
 * M-003d: confluence-policy flags uit `config_store::alert_confluence_policy()` (defaults = M-010d/e).
 */
#include "alert_engine/alert_engine.hpp"
#include "config_store/config_store.hpp"
#include "domain_metrics/domain_metrics.hpp"
#include "diagnostics/diagnostics.hpp"
#include "market_data/market_data.hpp"
#include "service_outbound/service_outbound.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include <cstdint>
#include <cmath>
#include <cstring>

#ifndef CONFIG_ALERT_ENGINE_1M_THRESHOLD_BPS
#define CONFIG_ALERT_ENGINE_1M_THRESHOLD_BPS 16
#endif
#ifndef CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS
#define CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS 32
#endif
#ifndef CONFIG_ALERT_ENGINE_30M_THRESHOLD_BPS
#define CONFIG_ALERT_ENGINE_30M_THRESHOLD_BPS 64
#endif
#ifndef CONFIG_ALERT_ENGINE_30M_COOLDOWN_S
#define CONFIG_ALERT_ENGINE_30M_COOLDOWN_S 1800
#endif
#ifndef CONFIG_ALERT_ENGINE_2H_THRESHOLD_BPS
#define CONFIG_ALERT_ENGINE_2H_THRESHOLD_BPS 128
#endif
#ifndef CONFIG_ALERT_ENGINE_2H_COOLDOWN_S
#define CONFIG_ALERT_ENGINE_2H_COOLDOWN_S 7200
#endif
#ifndef CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS
#define CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS 6
#endif
#ifndef CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS
#define CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS 28
#endif
#ifndef CONFIG_ALERT_REGIME_THR_SCALE_CALM_PERMILLE
#define CONFIG_ALERT_REGIME_THR_SCALE_CALM_PERMILLE 900
#endif
#ifndef CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE
#define CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE 1000
#endif
#ifndef CONFIG_ALERT_REGIME_THR_SCALE_HOT_PERMILLE
#define CONFIG_ALERT_REGIME_THR_SCALE_HOT_PERMILLE 1180
#endif
#ifndef CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE
#define CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE 750
#endif
#ifndef CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE
#define CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE 1350
#endif

namespace alert_engine {

static AlertDecisionObservabilitySnapshot s_decision_obs{};
static AlertEngineRuntimeStatsSnapshot s_runtime_stats{};

namespace {

static const char TAG[] = DIAG_TAG_ALERT;

static int64_t s_last_fire_1m_ms{-1};
static int64_t s_last_fire_5m_ms{-1};
static int64_t s_last_fire_30m_ms{-1};
static int64_t s_last_fire_2h_ms{-1};
static int64_t s_last_fire_conf_ms{-1};

/** Na confluence: tot deze tijd (esp-timer ms) geen losse 1m/5m met zelfde richting als confluence. */
static int64_t s_suppress_loose_until_ms{-1};
static bool s_suppress_loose_dir_up{true};
/** Verhoogt bij iedere confluence-fire; max. één suppress-log per TF per venster (geen tick-spam). */
static uint32_t s_suppress_gen{0};
static uint32_t s_suppress_logged_1m_gen{0};
static uint32_t s_suppress_logged_5m_gen{0};

/** M-010f: calm = rustig (lagere effectieve drempel), hot = strengere drempel. */
enum class Regime : uint8_t { Calm = 0, Normal = 1, Hot = 2 };

static const char *regime_label(Regime r)
{
    switch (r) {
    case Regime::Calm:
        return "calm";
    case Regime::Hot:
        return "hot";
    default:
        return "normal";
    }
}

static Regime regime_from_vol_bps(double mean_abs_step_bps)
{
    if (mean_abs_step_bps < static_cast<double>(CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS)) {
        return Regime::Calm;
    }
    if (mean_abs_step_bps >= static_cast<double>(CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS)) {
        return Regime::Hot;
    }
    return Regime::Normal;
}

/** M-003b: calm/hot ‰ uit `config_store::alert_runtime()`; normal blijft compile-time 1000‰. */
static int thr_scale_permille_for_regime(Regime r, const config_store::AlertRuntimeConfig &arc)
{
    switch (r) {
    case Regime::Calm:
        return static_cast<int>(arc.regime_calm_scale_permille);
    case Regime::Hot:
        return static_cast<int>(arc.regime_hot_scale_permille);
    default:
        return CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    }
}

static int clamp_thr_scale_permille(int p)
{
    const int lo = CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE;
    const int hi = CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE;
    if (p < lo) {
        return lo;
    }
    if (p > hi) {
        return hi;
    }
    return p;
}

static bool s_m010f_vol_ready_logged{false};
static bool s_m010f_logged_vol_unready_info{false};
static Regime s_m010f_last_regime{Regime::Normal};

/** C1: één suppress-telling per “episode” (niet elke tick tijdens het venster). */
static bool s_sup_episode_active_1m{false};
static bool s_sup_episode_active_5m{false};

/** C2: vorig regime-label — wissel → `last_regime_change_epoch_ms`. */
static char s_prev_regime_label_c2[16] = "normal";

/** M-010e: losse alert onderdrukken — alleen binnen venster én zelfde richting als laatste confluence. */
static bool loose_suppressed_after_confluence(int64_t now_ms, bool up_loose)
{
    if (s_suppress_loose_until_ms < 0 || now_ms >= s_suppress_loose_until_ms) {
        return false;
    }
    return up_loose == s_suppress_loose_dir_up;
}

/** M-003d: effectieve drempelpoort voor confluence (AND t.o.v. OR). */
static bool confluence_thresholds_pass(const config_store::AlertConfluencePolicyConfig &cfp,
                                       double a1,
                                       double a5,
                                       double eff1,
                                       double eff2)
{
    if (cfp.confluence_require_both_thresholds) {
        return a1 >= eff1 && a5 >= eff2;
    }
    return a1 >= eff1 || a5 >= eff2;
}

static void bump_path_edge(const AlertPathDecisionSnapshot &prev,
                            const AlertPathDecisionSnapshot &cur,
                            int64_t now_ms,
                            AlertPathEdgeStats *out)
{
    if (!out) {
        return;
    }
    if (std::strcmp(prev.status, cur.status) == 0) {
        return;
    }
    if (std::strcmp(cur.status, "cooldown") == 0) {
        ++out->enter_cooldown;
        out->last_epoch_ms_enter_cooldown = now_ms;
    } else if (std::strcmp(cur.status, "suppressed") == 0) {
        ++out->enter_suppressed;
        out->last_epoch_ms_enter_suppressed = now_ms;
    } else if (std::strcmp(cur.status, "not_ready") == 0) {
        ++out->enter_not_ready;
        out->last_epoch_ms_enter_not_ready = now_ms;
    }
}

static void path_set(AlertPathDecisionSnapshot *p,
                     const char *status,
                     const char *reason,
                     int64_t rem_cd_ms,
                     int64_t rem_sup_ms)
{
    if (!p) {
        return;
    }
    std::strncpy(p->status, status, sizeof(p->status) - 1);
    p->status[sizeof(p->status) - 1] = '\0';
    if (reason && reason[0] != '\0') {
        std::strncpy(p->reason, reason, sizeof(p->reason) - 1);
        p->reason[sizeof(p->reason) - 1] = '\0';
    } else {
        p->reason[0] = '\0';
    }
    p->remaining_cooldown_ms = rem_cd_ms;
    p->remaining_suppress_ms = rem_sup_ms;
}

/** M-013h: één plek —zelfde voorwaarden als de emit-paden in `tick()`, na bijwerken van fire-timestamps. */
static void refresh_decision_observability(const domain_metrics::Metric1mMovePct &m1,
                                           const domain_metrics::Metric5mMovePct &m5,
                                           const domain_metrics::Metric30mMovePct &m30,
                                           const domain_metrics::Metric2hMovePct &m2h,
                                           double eff_thr_1m_pct,
                                           double eff_thr_5m_pct,
                                           double eff_thr_30m_pct,
                                           double eff_thr_2h_pct,
                                           int64_t now_ms,
                                           int64_t cd1_ms,
                                           int64_t cd5_ms,
                                           int64_t cd30_ms,
                                           int64_t cd2h_ms,
                                           int64_t cd_cf_ms,
                                           bool fired_conf_this_tick,
                                           bool fired_1m_this_tick,
                                           bool fired_5m_this_tick,
                                           bool fired_30m_this_tick,
                                           bool fired_2h_this_tick,
                                           bool loose_blocked_by_conf_policy,
                                           const config_store::AlertConfluencePolicyConfig &cfp)
{
    const AlertDecisionObservabilitySnapshot prev_obs = s_decision_obs;

    /* --- Confluence --- */
    if (!cfp.confluence_enabled) {
        path_set(&s_decision_obs.confluence_1m5m, "disabled", "confluence_disabled", -1, -1);
    } else if (!m1.ready || !m5.ready) {
        path_set(&s_decision_obs.confluence_1m5m, "not_ready", "metrics_not_ready", -1, -1);
    } else {
        const double a1 = std::fabs(m1.pct);
        const double a5 = std::fabs(m5.pct);
        const bool up1 = m1.pct >= 0.0;
        const bool up5 = m5.pct >= 0.0;
        if (!confluence_thresholds_pass(cfp, a1, a5, eff_thr_1m_pct, eff_thr_5m_pct)) {
            path_set(&s_decision_obs.confluence_1m5m, "below_threshold", "", -1, -1);
        } else if (cfp.confluence_require_same_direction && (up1 != up5)) {
            path_set(&s_decision_obs.confluence_1m5m, "invalid", "direction_mismatch", -1, -1);
        } else if (!fired_conf_this_tick && s_last_fire_conf_ms >= 0 &&
                   (now_ms - s_last_fire_conf_ms) < cd_cf_ms) {
            const int64_t rem = cd_cf_ms - (now_ms - s_last_fire_conf_ms);
            path_set(&s_decision_obs.confluence_1m5m, "cooldown", "", rem, -1);
        } else if (fired_conf_this_tick) {
            path_set(&s_decision_obs.confluence_1m5m, "fired", "", cd_cf_ms, -1);
        } else {
            path_set(&s_decision_obs.confluence_1m5m, "invalid", "internal", -1, -1);
        }
    }

    /* --- 1m --- */
    if (!m1.ready) {
        path_set(&s_decision_obs.tf_1m, "not_ready", "metrics_not_ready", -1, -1);
    } else {
        const double ap = std::fabs(m1.pct);
        const bool up = m1.pct >= 0.0;
        if (ap < eff_thr_1m_pct) {
            path_set(&s_decision_obs.tf_1m, "below_threshold", "", -1, -1);
        } else if (!fired_1m_this_tick && s_last_fire_1m_ms >= 0 &&
                   (now_ms - s_last_fire_1m_ms) < cd1_ms) {
            const int64_t rem = cd1_ms - (now_ms - s_last_fire_1m_ms);
            path_set(&s_decision_obs.tf_1m, "cooldown", "", rem, -1);
        } else if (!fired_1m_this_tick && loose_blocked_by_conf_policy) {
            path_set(&s_decision_obs.tf_1m, "suppressed", "confluence_loose_gate", -1, -1);
        } else if (!fired_1m_this_tick && loose_suppressed_after_confluence(now_ms, up)) {
            int64_t sup_rem = -1;
            if (s_suppress_loose_until_ms >= 0 && now_ms < s_suppress_loose_until_ms) {
                sup_rem = s_suppress_loose_until_ms - now_ms;
            }
            path_set(&s_decision_obs.tf_1m, "suppressed", "confluence_priority_window", -1, sup_rem);
        } else if (fired_1m_this_tick) {
            path_set(&s_decision_obs.tf_1m, "fired", "", cd1_ms, -1);
        } else {
            path_set(&s_decision_obs.tf_1m, "invalid", "internal", -1, -1);
        }
    }

    /* --- 5m --- */
    if (!m5.ready) {
        path_set(&s_decision_obs.tf_5m, "not_ready", "metrics_not_ready", -1, -1);
    } else {
        const double ap5 = std::fabs(m5.pct);
        const bool up5 = m5.pct >= 0.0;
        if (ap5 < eff_thr_5m_pct) {
            path_set(&s_decision_obs.tf_5m, "below_threshold", "", -1, -1);
        } else if (!fired_5m_this_tick && s_last_fire_5m_ms >= 0 &&
                   (now_ms - s_last_fire_5m_ms) < cd5_ms) {
            const int64_t rem5 = cd5_ms - (now_ms - s_last_fire_5m_ms);
            path_set(&s_decision_obs.tf_5m, "cooldown", "", rem5, -1);
        } else if (!fired_5m_this_tick && loose_blocked_by_conf_policy) {
            path_set(&s_decision_obs.tf_5m, "suppressed", "confluence_loose_gate", -1, -1);
        } else if (!fired_5m_this_tick && loose_suppressed_after_confluence(now_ms, up5)) {
            int64_t sup_rem = -1;
            if (s_suppress_loose_until_ms >= 0 && now_ms < s_suppress_loose_until_ms) {
                sup_rem = s_suppress_loose_until_ms - now_ms;
            }
            path_set(&s_decision_obs.tf_5m, "suppressed", "confluence_priority_window", -1, sup_rem);
        } else if (fired_5m_this_tick) {
            path_set(&s_decision_obs.tf_5m, "fired", "", cd5_ms, -1);
        } else {
            path_set(&s_decision_obs.tf_5m, "invalid", "internal", -1, -1);
        }
    }

    /* --- 30m (S30-2: geen confluence/suppress uit M-010e) --- */
    if (!m30.ready) {
        path_set(&s_decision_obs.tf_30m, "not_ready", "metrics_not_ready", -1, -1);
    } else {
        const double ap30 = std::fabs(m30.pct);
        if (ap30 < eff_thr_30m_pct) {
            path_set(&s_decision_obs.tf_30m, "below_threshold", "", -1, -1);
        } else if (!fired_30m_this_tick && s_last_fire_30m_ms >= 0 &&
                   (now_ms - s_last_fire_30m_ms) < cd30_ms) {
            const int64_t rem30 = cd30_ms - (now_ms - s_last_fire_30m_ms);
            path_set(&s_decision_obs.tf_30m, "cooldown", "", rem30, -1);
        } else if (fired_30m_this_tick) {
            path_set(&s_decision_obs.tf_30m, "fired", "", cd30_ms, -1);
        } else {
            path_set(&s_decision_obs.tf_30m, "invalid", "internal", -1, -1);
        }
    }

    /* --- 2h (S2H-2: geen confluence/suppress uit M-010e) --- */
    if (!m2h.ready) {
        path_set(&s_decision_obs.tf_2h, "not_ready", "metrics_not_ready", -1, -1);
    } else {
        const double ap2h = std::fabs(m2h.pct);
        if (ap2h < eff_thr_2h_pct) {
            path_set(&s_decision_obs.tf_2h, "below_threshold", "", -1, -1);
        } else if (!fired_2h_this_tick && s_last_fire_2h_ms >= 0 &&
                   (now_ms - s_last_fire_2h_ms) < cd2h_ms) {
            const int64_t rem2h = cd2h_ms - (now_ms - s_last_fire_2h_ms);
            path_set(&s_decision_obs.tf_2h, "cooldown", "", rem2h, -1);
        } else if (fired_2h_this_tick) {
            path_set(&s_decision_obs.tf_2h, "fired", "", cd2h_ms, -1);
        } else {
            path_set(&s_decision_obs.tf_2h, "invalid", "internal", -1, -1);
        }
    }

    bump_path_edge(prev_obs.tf_1m, s_decision_obs.tf_1m, now_ms, &s_runtime_stats.edge_1m);
    bump_path_edge(prev_obs.tf_5m, s_decision_obs.tf_5m, now_ms, &s_runtime_stats.edge_5m);
    bump_path_edge(prev_obs.tf_30m, s_decision_obs.tf_30m, now_ms, &s_runtime_stats.edge_30m);
    bump_path_edge(prev_obs.tf_2h, s_decision_obs.tf_2h, now_ms, &s_runtime_stats.edge_2h);
    bump_path_edge(prev_obs.confluence_1m5m, s_decision_obs.confluence_1m5m, now_ms,
                   &s_runtime_stats.edge_confluence);
}

} // namespace

static RegimeObservabilitySnapshot s_regime_obs{};

esp_err_t init()
{
    s_last_fire_1m_ms = -1;
    s_last_fire_5m_ms = -1;
    s_last_fire_30m_ms = -1;
    s_last_fire_2h_ms = -1;
    s_last_fire_conf_ms = -1;
    s_runtime_stats = AlertEngineRuntimeStatsSnapshot{};
    s_suppress_loose_until_ms = -1;
    s_m010f_vol_ready_logged = false;
    s_m010f_logged_vol_unready_info = false;
    s_m010f_last_regime = Regime::Normal;
    s_sup_episode_active_1m = false;
    s_sup_episode_active_5m = false;
    std::memset(&s_regime_obs, 0, sizeof(s_regime_obs));
    std::strncpy(s_regime_obs.regime, "normal", sizeof(s_regime_obs.regime) - 1);
    s_regime_obs.last_regime_change_epoch_ms = -1;
    std::strncpy(s_prev_regime_label_c2, "normal", sizeof(s_prev_regime_label_c2) - 1);
    s_prev_regime_label_c2[sizeof(s_prev_regime_label_c2) - 1] = '\0';
    s_regime_obs.vol_unavailable_fallback = true;
    s_regime_obs.threshold_scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    s_regime_obs.threshold_scale_permille_raw = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    s_regime_obs.threshold_scale_clamped = false;
    s_regime_obs.regime_calm_max_step_bps = CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS;
    s_regime_obs.regime_hot_min_step_bps = CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS;
    {
        const config_store::AlertRuntimeConfig &arc = config_store::alert_runtime();
        const double b1 = static_cast<double>(arc.threshold_1m_bps) / 100.0;
        const double b5 = static_cast<double>(arc.threshold_5m_bps) / 100.0;
        const double b30 = static_cast<double>(CONFIG_ALERT_ENGINE_30M_THRESHOLD_BPS) / 100.0;
        const double b2h = static_cast<double>(CONFIG_ALERT_ENGINE_2H_THRESHOLD_BPS) / 100.0;
        s_regime_obs.base_threshold_move_pct_1m = b1;
        s_regime_obs.base_threshold_move_pct_5m = b5;
        s_regime_obs.base_threshold_move_pct_30m = b30;
        s_regime_obs.base_threshold_move_pct_2h = b2h;
        s_regime_obs.effective_threshold_move_pct_1m = b1;
        s_regime_obs.effective_threshold_move_pct_5m = b5;
        s_regime_obs.effective_threshold_move_pct_30m = b30;
        s_regime_obs.effective_threshold_move_pct_2h = b2h;
    }
    {
        const config_store::AlertPolicyTimingConfig &pol = config_store::alert_policy_timing();
        const config_store::AlertConfluencePolicyConfig &cfp = config_store::alert_confluence_policy();
        ESP_LOGI(TAG,
                 "M-003b/M-003c/M-003d/M-010 + S30-2 + S2H-2: alert_engine init (basis 1m=%.2f%% 5m=%.2f%% uit config_store; "
                 "30m=%.2f%% 2h=%.2f%% uit Kconfig; cd 1m=%us 5m=%us 30m=%us 2h=%us | conf cd=%us | suppress loose %us | ‰ "
                 "clamp [%d,%d] | conf en=%d same_dir=%d both_thr=%d emit_loose=%d)",
                 s_regime_obs.base_threshold_move_pct_1m,
                 s_regime_obs.base_threshold_move_pct_5m,
                 s_regime_obs.base_threshold_move_pct_30m,
                 s_regime_obs.base_threshold_move_pct_2h,
                 (unsigned)pol.cooldown_1m_s,
                 (unsigned)pol.cooldown_5m_s,
                 (unsigned)CONFIG_ALERT_ENGINE_30M_COOLDOWN_S,
                 (unsigned)CONFIG_ALERT_ENGINE_2H_COOLDOWN_S,
                 (unsigned)pol.cooldown_conf_1m5m_s,
                 (unsigned)pol.suppress_loose_after_conf_s,
                 CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE,
                 CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE,
                 cfp.confluence_enabled ? 1 : 0,
                 cfp.confluence_require_same_direction ? 1 : 0,
                 cfp.confluence_require_both_thresholds ? 1 : 0,
                 cfp.confluence_emit_loose_alerts_when_conf_fails ? 1 : 0);
    }
    std::memset(&s_decision_obs, 0, sizeof(s_decision_obs));
    path_set(&s_decision_obs.tf_1m, "not_ready", "metrics_not_ready", -1, -1);
    path_set(&s_decision_obs.tf_5m, "not_ready", "metrics_not_ready", -1, -1);
    path_set(&s_decision_obs.tf_30m, "not_ready", "metrics_not_ready", -1, -1);
    path_set(&s_decision_obs.tf_2h, "not_ready", "metrics_not_ready", -1, -1);
    path_set(&s_decision_obs.confluence_1m5m, "not_ready", "metrics_not_ready", -1, -1);
    return ESP_OK;
}

void tick()
{
    const int64_t now_ms = static_cast<int64_t>(esp_timer_get_time() / 1000LL);
    bool fired_conf_this_tick = false;
    bool fired_1m_this_tick = false;
    bool fired_5m_this_tick = false;
    bool fired_30m_this_tick = false;
    bool fired_2h_this_tick = false;

    const config_store::AlertPolicyTimingConfig &pol = config_store::alert_policy_timing();
    const int64_t cd1_ms = static_cast<int64_t>(pol.cooldown_1m_s) * 1000LL;
    const int64_t cd5_ms = static_cast<int64_t>(pol.cooldown_5m_s) * 1000LL;
    const int64_t cd30_ms = static_cast<int64_t>(CONFIG_ALERT_ENGINE_30M_COOLDOWN_S) * 1000LL;
    const int64_t cd2h_ms = static_cast<int64_t>(CONFIG_ALERT_ENGINE_2H_COOLDOWN_S) * 1000LL;
    const int64_t cd_cf_ms = static_cast<int64_t>(pol.cooldown_conf_1m5m_s) * 1000LL;
    const int64_t sup_loose_ms = static_cast<int64_t>(pol.suppress_loose_after_conf_s) * 1000LL;

    const config_store::AlertRuntimeConfig &arc = config_store::alert_runtime();
    const double base_1m_pct = static_cast<double>(arc.threshold_1m_bps) / 100.0;
    const double base_5m_pct = static_cast<double>(arc.threshold_5m_bps) / 100.0;

    const domain_metrics::MetricVolMeanAbsStepBps volm = domain_metrics::compute_vol_mean_abs_step_bps();
    Regime regime = Regime::Normal;
    int scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    int scale_permille_raw = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    bool scale_clamped = false;
    if (volm.ready) {
        regime = regime_from_vol_bps(volm.mean_abs_step_bps);
        scale_permille_raw = thr_scale_permille_for_regime(regime, arc);
        scale_permille = clamp_thr_scale_permille(scale_permille_raw);
        scale_clamped = (scale_permille != scale_permille_raw);
        if (!s_m010f_vol_ready_logged) {
            s_m010f_vol_ready_logged = true;
            s_m010f_logged_vol_unready_info = false;
            const double eff1 =
                base_1m_pct * static_cast<double>(scale_permille) / 1000.0;
            const double eff5 =
                base_5m_pct * static_cast<double>(scale_permille) / 1000.0;
            ESP_LOGI(TAG,
                     "M-010f: vol ready vol_mean_abs_step_bps=%.2f pairs=%u regime=%s scale=%d‰ "
                     "(calm|<%d hot|≥%d bps) → eff_thr 1m=%.3f%% 5m=%.3f%% (basis 1m=%.2f%% 5m=%.2f%%)%s",
                     volm.mean_abs_step_bps,
                     static_cast<unsigned>(volm.pairs_used),
                     regime_label(regime),
                     scale_permille,
                     CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS,
                     CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS,
                     eff1,
                     eff5,
                     base_1m_pct,
                     base_5m_pct,
                     scale_clamped ? " · ‰ clamp actief (zie status.json)" : "");
            if (scale_clamped) {
                ESP_LOGW(TAG,
                         "M-010f: ‰-schaal begrensd [%d,%d]: raw=%d‰ → eff=%d‰",
                         CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE,
                         CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE,
                         scale_permille_raw,
                         scale_permille);
            }
            s_m010f_last_regime = regime;
        } else if (regime != s_m010f_last_regime) {
            s_m010f_last_regime = regime;
            const double eff1 =
                base_1m_pct * static_cast<double>(scale_permille) / 1000.0;
            const double eff5 =
                base_5m_pct * static_cast<double>(scale_permille) / 1000.0;
            ESP_LOGI(TAG,
                     "M-010f: regime → %s vol_mean_abs_step_bps=%.2f scale=%d‰ "
                     "eff_thr 1m=%.3f%% 5m=%.3f%% (cooldown/suppress ongewijzigd)%s",
                     regime_label(regime),
                     volm.mean_abs_step_bps,
                     scale_permille,
                     eff1,
                     eff5,
                     scale_clamped ? " · ‰ raw≠eff (clamp)" : "");
            if (scale_clamped) {
                ESP_LOGW(TAG,
                         "M-010f: ‰-schaal begrensd [%d,%d]: raw=%d‰ → eff=%d‰",
                         CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE,
                         CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE,
                         scale_permille_raw,
                         scale_permille);
            }
        }
    } else {
        scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
        scale_permille_raw = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
        scale_clamped = false;
        if (!s_m010f_logged_vol_unready_info) {
            s_m010f_logged_vol_unready_info = true;
            ESP_LOGI(TAG,
                     "M-010f: vol metric nog niet klaar (min. paren in venster) — eff. drempels = "
                     "basis (normal ‰) tot vol beschikbaar is");
        }
    }

    const double eff_thr_1m_pct =
        base_1m_pct * static_cast<double>(scale_permille) / 1000.0;
    const double eff_thr_5m_pct =
        base_5m_pct * static_cast<double>(scale_permille) / 1000.0;
    const double base_30m_pct = static_cast<double>(CONFIG_ALERT_ENGINE_30M_THRESHOLD_BPS) / 100.0;
    const double eff_thr_30m_pct =
        base_30m_pct * static_cast<double>(scale_permille) / 1000.0;
    const double base_2h_pct = static_cast<double>(CONFIG_ALERT_ENGINE_2H_THRESHOLD_BPS) / 100.0;
    const double eff_thr_2h_pct =
        base_2h_pct * static_cast<double>(scale_permille) / 1000.0;

    s_regime_obs.vol_metric_ready = volm.ready;
    s_regime_obs.vol_mean_abs_step_bps = volm.ready ? volm.mean_abs_step_bps : 0.0;
    s_regime_obs.vol_pairs_used = volm.pairs_used;
    s_regime_obs.vol_unavailable_fallback = !volm.ready;
    std::strncpy(s_regime_obs.regime, regime_label(regime), sizeof(s_regime_obs.regime) - 1);
    s_regime_obs.regime[sizeof(s_regime_obs.regime) - 1] = '\0';
    if (std::strcmp(s_prev_regime_label_c2, s_regime_obs.regime) != 0) {
        s_regime_obs.last_regime_change_epoch_ms = now_ms;
        std::strncpy(s_prev_regime_label_c2, s_regime_obs.regime, sizeof(s_prev_regime_label_c2) - 1);
        s_prev_regime_label_c2[sizeof(s_prev_regime_label_c2) - 1] = '\0';
    }
    s_regime_obs.threshold_scale_permille = scale_permille;
    s_regime_obs.threshold_scale_permille_raw = scale_permille_raw;
    s_regime_obs.threshold_scale_clamped = scale_clamped;
    s_regime_obs.regime_calm_max_step_bps = CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS;
    s_regime_obs.regime_hot_min_step_bps = CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS;
    s_regime_obs.base_threshold_move_pct_1m = base_1m_pct;
    s_regime_obs.base_threshold_move_pct_5m = base_5m_pct;
    s_regime_obs.base_threshold_move_pct_30m = base_30m_pct;
    s_regime_obs.base_threshold_move_pct_2h = base_2h_pct;
    s_regime_obs.effective_threshold_move_pct_1m = eff_thr_1m_pct;
    s_regime_obs.effective_threshold_move_pct_5m = eff_thr_5m_pct;
    s_regime_obs.effective_threshold_move_pct_30m = eff_thr_30m_pct;
    s_regime_obs.effective_threshold_move_pct_2h = eff_thr_2h_pct;

    const domain_metrics::Metric1mMovePct m1 = domain_metrics::compute_1m_move_pct();
    const domain_metrics::Metric5mMovePct m5 = domain_metrics::compute_5m_move_pct();
    const domain_metrics::Metric30mMovePct m30 = domain_metrics::compute_30m_move_pct();
    const domain_metrics::Metric2hMovePct m2h = domain_metrics::compute_2h_move_pct();

    const config_store::AlertConfluencePolicyConfig &cfp = config_store::alert_confluence_policy();

    /* M-010d/M-010e + M-003d: confluence eerst — prioriteit; bij vuur venster voor suppressie losse TF. */
    if (cfp.confluence_enabled && m1.ready && m5.ready) {
        const double a1 = std::fabs(m1.pct);
        const double a5 = std::fabs(m5.pct);
        const bool up1 = m1.pct >= 0.0;
        const bool up5 = m5.pct >= 0.0;

        if (!confluence_thresholds_pass(cfp, a1, a5, eff_thr_1m_pct, eff_thr_5m_pct)) {
            /* Geen log iedere tick: normaal dat gate niet voldaan is. */
        } else if (cfp.confluence_require_same_direction && (up1 != up5)) {
            ESP_LOGI(TAG,
                     "M-010d: confluence skip — tegenstrijdige richting (1m=%+.4f%% 5m=%+.4f%%)",
                     m1.pct,
                     m5.pct);
        } else if (s_last_fire_conf_ms >= 0 && (now_ms - s_last_fire_conf_ms) < cd_cf_ms) {
            ESP_LOGD(TAG,
                     "M-010d: confluence skip — cooldown (%lld ms < %lld ms)",
                     (long long)(now_ms - s_last_fire_conf_ms),
                     (long long)cd_cf_ms);
        } else {
            const bool conf_up = up1;
            s_last_fire_conf_ms = now_ms;
            ++s_suppress_gen;
            s_suppress_loose_until_ms = now_ms + sup_loose_ms;
            s_suppress_loose_dir_up = conf_up;

            const char *dirc = conf_up ? "UP" : "DOWN";
            ESP_LOGI(TAG,
                     "M-010d: confluence FIRE %s (1m=%+.4f%% 5m=%+.4f%% prijs=%.4f ts_ms=%lld)",
                     dirc,
                     m1.pct,
                     m5.pct,
                     m1.now_price_eur,
                     (long long)m1.now_ts_ms);
            ESP_LOGI(TAG,
                     "M-010e: prioriteit confluence — %lld s geen dubbele losse 1m/5m (zelfde richting %s)",
                     (long long)(sup_loose_ms / 1000LL),
                     dirc);

            const market_data::MarketSnapshot snapc = market_data::snapshot();
            service_outbound::DomainConfluence1m5mPayload pc{};
            pc.up = conf_up;
            pc.price_eur = m1.now_price_eur;
            pc.pct_1m = m1.pct;
            pc.pct_5m = m5.pct;
            pc.ts_ms = m1.now_ts_ms;
            if (snapc.market_label[0] != '\0') {
                std::strncpy(pc.symbol, snapc.market_label, sizeof(pc.symbol) - 1);
            } else {
                std::strncpy(pc.symbol, "—", sizeof(pc.symbol) - 1);
            }
            pc.symbol[sizeof(pc.symbol) - 1] = '\0';
            ESP_LOGI(TAG, "M-010d: queue DomainAlertConfluence1m5m (sym=%s)", pc.symbol);
            service_outbound::emit_domain_confluence_1m5m(pc);
            fired_conf_this_tick = true;
            ++s_runtime_stats.emit_total_conf;
            s_runtime_stats.last_emit_epoch_ms_conf = now_ms;
        }
    }

    bool loose_blocked_by_conf_policy = false;
    if (cfp.confluence_enabled && !cfp.confluence_emit_loose_alerts_when_conf_fails && m1.ready &&
        m5.ready) {
        const double a1 = std::fabs(m1.pct);
        const double a5 = std::fabs(m5.pct);
        const bool up1 = m1.pct >= 0.0;
        const bool up5 = m5.pct >= 0.0;
        if (confluence_thresholds_pass(cfp, a1, a5, eff_thr_1m_pct, eff_thr_5m_pct)) {
            if (cfp.confluence_require_same_direction && (up1 != up5)) {
                loose_blocked_by_conf_policy = true;
            } else if (!fired_conf_this_tick && s_last_fire_conf_ms >= 0 &&
                       (now_ms - s_last_fire_conf_ms) < cd_cf_ms) {
                loose_blocked_by_conf_policy = true;
            }
        }
    }

    if (m1.ready) {
        const double ap = std::fabs(m1.pct);
        if (ap < eff_thr_1m_pct) {
            s_sup_episode_active_1m = false;
        }
        if (ap >= eff_thr_1m_pct) {
            if (s_last_fire_1m_ms < 0 || (now_ms - s_last_fire_1m_ms) >= cd1_ms) {
                const bool up = m1.pct >= 0.0;
                if (loose_blocked_by_conf_policy) {
                    ESP_LOGD(TAG,
                             "M-003d: 1m alert suppressed — confluence policy loose gate (emit_loose_when_conf_fails=0)");
                } else if (loose_suppressed_after_confluence(now_ms, up)) {
                    if (!s_sup_episode_active_1m) {
                        ++s_runtime_stats.suppress_after_conf_window_1m;
                        s_sup_episode_active_1m = true;
                    }
                    if (s_suppress_logged_1m_gen != s_suppress_gen) {
                        s_suppress_logged_1m_gen = s_suppress_gen;
                        const int64_t rem = s_suppress_loose_until_ms - now_ms;
                        ESP_LOGI(TAG,
                                 "M-010e: 1m alert suppressed — confluence priority window (dir=%s rem_ms=%lld "
                                 "reason=same_dir_as_conf)",
                                 up ? "UP" : "DOWN",
                                 (long long)rem);
                    }
                } else {
                    s_sup_episode_active_1m = false;
                    const char *dir = up ? "UP" : "DOWN";
                    ESP_LOGI(TAG,
                             "M-011b: 1m move alert %s pct=%.3f (now=%.4f @%lld ref=%.4f @%lld) thr=%.2f%%",
                             dir,
                             m1.pct,
                             m1.now_price_eur,
                             (long long)m1.now_ts_ms,
                             m1.ref_price_eur,
                             (long long)m1.ref_ts_ms,
                             eff_thr_1m_pct);
                    s_last_fire_1m_ms = now_ms;

                    const market_data::MarketSnapshot snap = market_data::snapshot();
                    service_outbound::DomainAlert1mMovePayload payload{};
                    payload.up = up;
                    payload.price_eur = m1.now_price_eur;
                    payload.pct_1m = m1.pct;
                    payload.ts_ms = m1.now_ts_ms;
                    if (snap.market_label[0] != '\0') {
                        std::strncpy(payload.symbol, snap.market_label, sizeof(payload.symbol) - 1);
                    } else {
                        std::strncpy(payload.symbol, "—", sizeof(payload.symbol) - 1);
                    }
                    payload.symbol[sizeof(payload.symbol) - 1] = '\0';
                    ESP_LOGI(TAG, "M-011b: queue DomainAlert1mMove (sym=%s)", payload.symbol);
                    service_outbound::emit_domain_alert_1m(payload);
                    fired_1m_this_tick = true;
                    ++s_runtime_stats.emit_total_1m;
                    s_runtime_stats.last_emit_epoch_ms_1m = now_ms;
                }
            }
        }
    }

    if (m5.ready) {
        const double ap5 = std::fabs(m5.pct);
        if (ap5 < eff_thr_5m_pct) {
            s_sup_episode_active_5m = false;
        }
        if (ap5 >= eff_thr_5m_pct) {
            if (s_last_fire_5m_ms < 0 || (now_ms - s_last_fire_5m_ms) >= cd5_ms) {
                const bool up5 = m5.pct >= 0.0;
                if (loose_blocked_by_conf_policy) {
                    ESP_LOGD(TAG,
                             "M-003d: 5m alert suppressed — confluence policy loose gate (emit_loose_when_conf_fails=0)");
                } else if (loose_suppressed_after_confluence(now_ms, up5)) {
                    if (!s_sup_episode_active_5m) {
                        ++s_runtime_stats.suppress_after_conf_window_5m;
                        s_sup_episode_active_5m = true;
                    }
                    if (s_suppress_logged_5m_gen != s_suppress_gen) {
                        s_suppress_logged_5m_gen = s_suppress_gen;
                        const int64_t rem = s_suppress_loose_until_ms - now_ms;
                        ESP_LOGI(TAG,
                                 "M-010e: 5m alert suppressed — confluence priority window (dir=%s rem_ms=%lld "
                                 "reason=same_dir_as_conf)",
                                 up5 ? "UP" : "DOWN",
                                 (long long)rem);
                    }
                } else {
                    s_sup_episode_active_5m = false;
                    const char *dir5 = up5 ? "UP" : "DOWN";
                    ESP_LOGI(TAG,
                             "M-010c: 5m move alert %s pct=%.3f (now=%.4f @%lld ref=%.4f @%lld) thr=%.2f%%",
                             dir5,
                             m5.pct,
                             m5.now_price_eur,
                             (long long)m5.now_ts_ms,
                             m5.ref_price_eur,
                             (long long)m5.ref_ts_ms,
                             eff_thr_5m_pct);
                    s_last_fire_5m_ms = now_ms;

                    const market_data::MarketSnapshot snap5 = market_data::snapshot();
                    service_outbound::DomainAlert5mMovePayload p5{};
                    p5.up = up5;
                    p5.price_eur = m5.now_price_eur;
                    p5.pct_5m = m5.pct;
                    p5.ts_ms = m5.now_ts_ms;
                    if (snap5.market_label[0] != '\0') {
                        std::strncpy(p5.symbol, snap5.market_label, sizeof(p5.symbol) - 1);
                    } else {
                        std::strncpy(p5.symbol, "—", sizeof(p5.symbol) - 1);
                    }
                    p5.symbol[sizeof(p5.symbol) - 1] = '\0';
                    ESP_LOGI(TAG, "M-010c: queue DomainAlert5mMove (sym=%s)", p5.symbol);
                    service_outbound::emit_domain_alert_5m(p5);
                    fired_5m_this_tick = true;
                    ++s_runtime_stats.emit_total_5m;
                    s_runtime_stats.last_emit_epoch_ms_5m = now_ms;
                }
            }
        }
    }

    /* S30-3: 30m-beslissing — zelfde ‰-schaal; outbound parallel aan 5m. */
    if (m30.ready) {
        const double ap30 = std::fabs(m30.pct);
        if (ap30 >= eff_thr_30m_pct) {
            if (s_last_fire_30m_ms < 0 || (now_ms - s_last_fire_30m_ms) >= cd30_ms) {
                const bool up30 = m30.pct >= 0.0;
                const char *dir30 = up30 ? "UP" : "DOWN";
                ESP_LOGI(TAG,
                         "S30-3: 30m move alert %s pct=%.3f (now=%.4f @%lld ref=%.4f @%lld span_ms=%lld) "
                         "thr=%.3f%% (basis=%.2f%% · ‰=%d) → queue outbound",
                         dir30,
                         m30.pct,
                         m30.now_price_eur,
                         (long long)m30.now_ts_ms,
                         m30.ref_price_eur,
                         (long long)m30.ref_ts_ms,
                         (long long)m30.ref_span_ms,
                         eff_thr_30m_pct,
                         base_30m_pct,
                         scale_permille);
                s_last_fire_30m_ms = now_ms;
                fired_30m_this_tick = true;
                ++s_runtime_stats.emit_total_30m;
                s_runtime_stats.last_emit_epoch_ms_30m = now_ms;

                const market_data::MarketSnapshot snap30 = market_data::snapshot();
                service_outbound::DomainAlert30mMovePayload p30{};
                p30.up = up30;
                p30.price_eur = m30.now_price_eur;
                p30.pct_30m = m30.pct;
                p30.ts_ms = m30.now_ts_ms;
                if (snap30.market_label[0] != '\0') {
                    std::strncpy(p30.symbol, snap30.market_label, sizeof(p30.symbol) - 1);
                } else {
                    std::strncpy(p30.symbol, "—", sizeof(p30.symbol) - 1);
                }
                p30.symbol[sizeof(p30.symbol) - 1] = '\0';
                ESP_LOGI(TAG, "S30-3: queue DomainAlert30mMove (sym=%s)", p30.symbol);
                service_outbound::emit_domain_alert_30m(p30);
            }
        }
    }

    /* S2H-3: 2h-beslissing — zelfde ‰-schaal; outbound parallel aan 30m. */
    if (m2h.ready) {
        const double ap2h = std::fabs(m2h.pct);
        if (ap2h >= eff_thr_2h_pct) {
            if (s_last_fire_2h_ms < 0 || (now_ms - s_last_fire_2h_ms) >= cd2h_ms) {
                const bool up2h = m2h.pct >= 0.0;
                const char *dir2h = up2h ? "UP" : "DOWN";
                ESP_LOGI(TAG,
                         "S2H-3: 2h move alert %s pct=%.3f (now=%.4f @%lld ref=%.4f @%lld span_ms=%lld) "
                         "thr=%.3f%% (basis=%.2f%% · ‰=%d) → queue outbound",
                         dir2h,
                         m2h.pct,
                         m2h.now_price_eur,
                         (long long)m2h.now_ts_ms,
                         m2h.ref_price_eur,
                         (long long)m2h.ref_ts_ms,
                         (long long)m2h.ref_span_ms,
                         eff_thr_2h_pct,
                         base_2h_pct,
                         scale_permille);
                s_last_fire_2h_ms = now_ms;
                fired_2h_this_tick = true;
                ++s_runtime_stats.emit_total_2h;
                s_runtime_stats.last_emit_epoch_ms_2h = now_ms;

                const market_data::MarketSnapshot snap2h = market_data::snapshot();
                service_outbound::DomainAlert2hMovePayload p2h{};
                p2h.up = up2h;
                p2h.price_eur = m2h.now_price_eur;
                p2h.pct_2h = m2h.pct;
                p2h.ts_ms = m2h.now_ts_ms;
                if (snap2h.market_label[0] != '\0') {
                    std::strncpy(p2h.symbol, snap2h.market_label, sizeof(p2h.symbol) - 1);
                } else {
                    std::strncpy(p2h.symbol, "—", sizeof(p2h.symbol) - 1);
                }
                p2h.symbol[sizeof(p2h.symbol) - 1] = '\0';
                ESP_LOGI(TAG, "S2H-3: queue DomainAlert2hMove (sym=%s)", p2h.symbol);
                service_outbound::emit_domain_alert_2h(p2h);
            }
        }
    }

    refresh_decision_observability(m1,
                                   m5,
                                   m30,
                                   m2h,
                                   eff_thr_1m_pct,
                                   eff_thr_5m_pct,
                                   eff_thr_30m_pct,
                                   eff_thr_2h_pct,
                                   now_ms,
                                   cd1_ms,
                                   cd5_ms,
                                   cd30_ms,
                                   cd2h_ms,
                                   cd_cf_ms,
                                   fired_conf_this_tick,
                                   fired_1m_this_tick,
                                   fired_5m_this_tick,
                                   fired_30m_this_tick,
                                   fired_2h_this_tick,
                                   loose_blocked_by_conf_policy,
                                   cfp);
}

void get_regime_observability_snapshot(RegimeObservabilitySnapshot *out)
{
    if (!out) {
        return;
    }
    *out = s_regime_obs;
}

void get_alert_decision_observability_snapshot(AlertDecisionObservabilitySnapshot *out)
{
    if (!out) {
        return;
    }
    *out = s_decision_obs;
}

void get_alert_runtime_stats_snapshot(AlertEngineRuntimeStatsSnapshot *out)
{
    if (!out) {
        return;
    }
    *out = s_runtime_stats;
}

} // namespace alert_engine
