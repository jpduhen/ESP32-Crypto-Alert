/**
 * M-010a: eerste alert vertical slice — drempel + cooldown (1m).
 * M-010c: parallel 5m-route (eigen drempel/cooldown/event).
 * M-010d: eenvoudige confluence (1m+5m zelfde richting, beide boven drempel, eigen cooldown).
 * M-010e: confluence vóór losse TF; na confluence kort venster: zelfde richting 1m/5m niet dubbel.
 * M-010f: mini-regime (calm/normal/hot) uit vol-proxy — alleen lichte schaal van 1m/5m/conf-drempels.
 * M-011b: bij 1m-trigger payload naar `service_outbound::emit_domain_alert_1m`.
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
#ifndef CONFIG_ALERT_ENGINE_1M_COOLDOWN_S
#define CONFIG_ALERT_ENGINE_1M_COOLDOWN_S 120
#endif
#ifndef CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS
#define CONFIG_ALERT_ENGINE_5M_THRESHOLD_BPS 32
#endif
#ifndef CONFIG_ALERT_ENGINE_5M_COOLDOWN_S
#define CONFIG_ALERT_ENGINE_5M_COOLDOWN_S 300
#endif
#ifndef CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S
#define CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S 600
#endif
#ifndef CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S
#define CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S 8
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

namespace {

static const char TAG[] = DIAG_TAG_ALERT;

static constexpr int64_t k_cooldown_1m_ms =
    static_cast<int64_t>(CONFIG_ALERT_ENGINE_1M_COOLDOWN_S) * 1000LL;

static constexpr int64_t k_cooldown_5m_ms =
    static_cast<int64_t>(CONFIG_ALERT_ENGINE_5M_COOLDOWN_S) * 1000LL;

static constexpr int64_t k_cooldown_conf_ms =
    static_cast<int64_t>(CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S) * 1000LL;

static constexpr int64_t k_suppress_loose_after_conf_ms =
    static_cast<int64_t>(CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S) * 1000LL;

static int64_t s_last_fire_1m_ms{-1};
static int64_t s_last_fire_5m_ms{-1};
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

/** M-010e: losse alert onderdrukken — alleen binnen venster én zelfde richting als laatste confluence. */
static bool loose_suppressed_after_confluence(int64_t now_ms, bool up_loose)
{
    if (s_suppress_loose_until_ms < 0 || now_ms >= s_suppress_loose_until_ms) {
        return false;
    }
    return up_loose == s_suppress_loose_dir_up;
}

} // namespace

static RegimeObservabilitySnapshot s_regime_obs{};

esp_err_t init()
{
    s_last_fire_1m_ms = -1;
    s_last_fire_5m_ms = -1;
    s_last_fire_conf_ms = -1;
    s_suppress_loose_until_ms = -1;
    s_m010f_vol_ready_logged = false;
    s_m010f_logged_vol_unready_info = false;
    s_m010f_last_regime = Regime::Normal;
    std::memset(&s_regime_obs, 0, sizeof(s_regime_obs));
    std::strncpy(s_regime_obs.regime, "normal", sizeof(s_regime_obs.regime) - 1);
    s_regime_obs.vol_unavailable_fallback = true;
    s_regime_obs.threshold_scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    {
        const config_store::AlertRuntimeConfig &arc = config_store::alert_runtime();
        const double b1 = static_cast<double>(arc.threshold_1m_bps) / 100.0;
        const double b5 = static_cast<double>(arc.threshold_5m_bps) / 100.0;
        s_regime_obs.base_threshold_move_pct_1m = b1;
        s_regime_obs.base_threshold_move_pct_5m = b5;
        s_regime_obs.effective_threshold_move_pct_1m = b1;
        s_regime_obs.effective_threshold_move_pct_5m = b5;
    }
    ESP_LOGI(TAG,
             "M-003b/M-010: alert_engine init (basis 1m=%.2f%% 5m=%.2f%% uit config_store; "
             "cd 1m=%ds 5m=%ds | conf cd=%ds | suppress loose %ds | ‰ clamp [%d,%d])",
             s_regime_obs.base_threshold_move_pct_1m,
             s_regime_obs.base_threshold_move_pct_5m,
             CONFIG_ALERT_ENGINE_1M_COOLDOWN_S,
             CONFIG_ALERT_ENGINE_5M_COOLDOWN_S,
             CONFIG_ALERT_ENGINE_CONF_1M5M_COOLDOWN_S,
             CONFIG_ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S,
             CONFIG_ALERT_REGIME_THR_SCALE_MIN_PERMILLE,
             CONFIG_ALERT_REGIME_THR_SCALE_MAX_PERMILLE);
    return ESP_OK;
}

void tick()
{
    const int64_t now_ms = static_cast<int64_t>(esp_timer_get_time() / 1000LL);

    const config_store::AlertRuntimeConfig &arc = config_store::alert_runtime();
    const double base_1m_pct = static_cast<double>(arc.threshold_1m_bps) / 100.0;
    const double base_5m_pct = static_cast<double>(arc.threshold_5m_bps) / 100.0;

    const domain_metrics::MetricVolMeanAbsStepBps volm = domain_metrics::compute_vol_mean_abs_step_bps();
    Regime regime = Regime::Normal;
    int scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
    if (volm.ready) {
        regime = regime_from_vol_bps(volm.mean_abs_step_bps);
        scale_permille = clamp_thr_scale_permille(thr_scale_permille_for_regime(regime, arc));
        if (!s_m010f_vol_ready_logged) {
            s_m010f_vol_ready_logged = true;
            s_m010f_logged_vol_unready_info = false;
            const double eff1 =
                base_1m_pct * static_cast<double>(scale_permille) / 1000.0;
            const double eff5 =
                base_5m_pct * static_cast<double>(scale_permille) / 1000.0;
            ESP_LOGI(TAG,
                     "M-010f: vol ready vol_mean_abs_step_bps=%.2f pairs=%u regime=%s scale=%d‰ "
                     "(calm|<%d hot|≥%d bps) → eff_thr 1m=%.3f%% 5m=%.3f%% (basis 1m=%.2f%% 5m=%.2f%%)",
                     volm.mean_abs_step_bps,
                     static_cast<unsigned>(volm.pairs_used),
                     regime_label(regime),
                     scale_permille,
                     CONFIG_ALERT_REGIME_CALM_MAX_STEP_BPS,
                     CONFIG_ALERT_REGIME_HOT_MIN_STEP_BPS,
                     eff1,
                     eff5,
                     base_1m_pct,
                     base_5m_pct);
            s_m010f_last_regime = regime;
        } else if (regime != s_m010f_last_regime) {
            s_m010f_last_regime = regime;
            const double eff1 =
                base_1m_pct * static_cast<double>(scale_permille) / 1000.0;
            const double eff5 =
                base_5m_pct * static_cast<double>(scale_permille) / 1000.0;
            ESP_LOGI(TAG,
                     "M-010f: regime → %s vol_mean_abs_step_bps=%.2f scale=%d‰ "
                     "eff_thr 1m=%.3f%% 5m=%.3f%% (cooldown/suppress ongewijzigd)",
                     regime_label(regime),
                     volm.mean_abs_step_bps,
                     scale_permille,
                     eff1,
                     eff5);
        }
    } else {
        scale_permille = CONFIG_ALERT_REGIME_THR_SCALE_NORMAL_PERMILLE;
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

    s_regime_obs.vol_metric_ready = volm.ready;
    s_regime_obs.vol_mean_abs_step_bps = volm.ready ? volm.mean_abs_step_bps : 0.0;
    s_regime_obs.vol_pairs_used = volm.pairs_used;
    s_regime_obs.vol_unavailable_fallback = !volm.ready;
    std::strncpy(s_regime_obs.regime, regime_label(regime), sizeof(s_regime_obs.regime) - 1);
    s_regime_obs.regime[sizeof(s_regime_obs.regime) - 1] = '\0';
    s_regime_obs.threshold_scale_permille = scale_permille;
    s_regime_obs.base_threshold_move_pct_1m = base_1m_pct;
    s_regime_obs.base_threshold_move_pct_5m = base_5m_pct;
    s_regime_obs.effective_threshold_move_pct_1m = eff_thr_1m_pct;
    s_regime_obs.effective_threshold_move_pct_5m = eff_thr_5m_pct;

    const domain_metrics::Metric1mMovePct m1 = domain_metrics::compute_1m_move_pct();
    const domain_metrics::Metric5mMovePct m5 = domain_metrics::compute_5m_move_pct();

    /* M-010d/M-010e: confluence eerst — prioriteit; bij vuur venster voor suppressie losse TF (zelfde richting). */
    if (m1.ready && m5.ready) {
        const double a1 = std::fabs(m1.pct);
        const double a5 = std::fabs(m5.pct);
        const bool up1 = m1.pct >= 0.0;
        const bool up5 = m5.pct >= 0.0;

        if (a1 < eff_thr_1m_pct || a5 < eff_thr_5m_pct) {
            /* Geen log iedere tick: normaal dat |pct| onder drempel blijft. */
        } else if (up1 != up5) {
            ESP_LOGI(TAG,
                     "M-010d: confluence skip — tegenstrijdige richting (1m=%+.4f%% 5m=%+.4f%%)",
                     m1.pct,
                     m5.pct);
        } else if (s_last_fire_conf_ms >= 0 && (now_ms - s_last_fire_conf_ms) < k_cooldown_conf_ms) {
            ESP_LOGD(TAG,
                     "M-010d: confluence skip — cooldown (%lld ms < %lld ms)",
                     (long long)(now_ms - s_last_fire_conf_ms),
                     (long long)k_cooldown_conf_ms);
        } else {
            s_last_fire_conf_ms = now_ms;
            ++s_suppress_gen;
            s_suppress_loose_until_ms = now_ms + k_suppress_loose_after_conf_ms;
            s_suppress_loose_dir_up = up1;

            const char *dirc = up1 ? "UP" : "DOWN";
            ESP_LOGI(TAG,
                     "M-010d: confluence FIRE %s (1m=%+.4f%% 5m=%+.4f%% prijs=%.4f ts_ms=%lld)",
                     dirc,
                     m1.pct,
                     m5.pct,
                     m1.now_price_eur,
                     (long long)m1.now_ts_ms);
            ESP_LOGI(TAG,
                     "M-010e: prioriteit confluence — %lld s geen dubbele losse 1m/5m (zelfde richting %s)",
                     (long long)(k_suppress_loose_after_conf_ms / 1000LL),
                     dirc);

            const market_data::MarketSnapshot snapc = market_data::snapshot();
            service_outbound::DomainConfluence1m5mPayload pc{};
            pc.up = up1;
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
        }
    }

    if (m1.ready) {
        const double ap = std::fabs(m1.pct);
        if (ap >= eff_thr_1m_pct) {
            if (s_last_fire_1m_ms < 0 || (now_ms - s_last_fire_1m_ms) >= k_cooldown_1m_ms) {
                const bool up = m1.pct >= 0.0;
                if (loose_suppressed_after_confluence(now_ms, up)) {
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
                }
            }
        }
    }

    if (m5.ready) {
        const double ap5 = std::fabs(m5.pct);
        if (ap5 >= eff_thr_5m_pct) {
            if (s_last_fire_5m_ms < 0 || (now_ms - s_last_fire_5m_ms) >= k_cooldown_5m_ms) {
                const bool up5 = m5.pct >= 0.0;
                if (loose_suppressed_after_confluence(now_ms, up5)) {
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
                    const char *dir5 = up5 ? "UP" : "DOWN";
                    ESP_LOGI(TAG,
                             "M-010c: 5m move alert %s pct=%.3f (now=%.4f @%lld ref=%.4f @%lld) thr=%.2f%%",
                             dir5,
                             m5.pct,
                             m5.now_price_eur,
                             (long long)m5.now_ts_ms,
                             m5.ref_price_eur,
                             (long long)                             m5.ref_ts_ms,
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
                }
            }
        }
    }
}

void get_regime_observability_snapshot(RegimeObservabilitySnapshot *out)
{
    if (!out) {
        return;
    }
    *out = s_regime_obs;
}

} // namespace alert_engine
