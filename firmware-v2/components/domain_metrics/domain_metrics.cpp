/**
 * M-010a: rolling samples (prijs + tijd) — sober; cadence volgt caller (typ. app_core-lus).
 * M-010b: canonicalisatie naar 1 representatieve secondewaarde (TWAP-achtig gemiddelde).
 * M-010c: 5m-metric op dezelfde ring (cap > 5 min canonieke seconden).
 * M-010f: gemiddelde | stap | tussen opeenvolgende canonieke secondes (bps) als vol-proxy.
 */
#include "domain_metrics/domain_metrics.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include <cmath>
#include <cstring>

#ifndef CONFIG_ALERT_REGIME_VOL_WINDOW_SEC
#define CONFIG_ALERT_REGIME_VOL_WINDOW_SEC 90
#endif
#ifndef CONFIG_ALERT_REGIME_VOL_MIN_PAIRS
#define CONFIG_ALERT_REGIME_VOL_MIN_PAIRS 30
#endif
#ifndef CONFIG_ALERT_REGIME_VOL_PAIR_MIN_MS
#define CONFIG_ALERT_REGIME_VOL_PAIR_MIN_MS 800
#endif
#ifndef CONFIG_ALERT_REGIME_VOL_PAIR_MAX_MS
#define CONFIG_ALERT_REGIME_VOL_PAIR_MAX_MS 2200
#endif

namespace domain_metrics {

namespace {

static const char TAG[] = DIAG_TAG_METRICS;

/** ≥5m canonieke seconden + marge (1m- en 5m-metrics delen dezelfde ring). */
static constexpr size_t k_ring_cap = 400;

struct Sample {
    int64_t ts_ms{0};
    double price_eur{0.0};
};

struct SecondBucket {
    bool active{false};
    int64_t sec_epoch{0};
    double sum_price{0.0};
    uint16_t tick_count{0};
    double first_price{0.0};
    double last_price{0.0};
};

static Sample s_ring[k_ring_cap]{};
static size_t s_head{0};
static size_t s_count{0};
static SecondBucket s_bucket{};
/** Laatste bekende EUR-prijs; voor carry-forward als een wandklok-seconde geen nieuwe WS-ts kreeg. */
static double s_carry_price_eur{0.0};
/** Dedup: zelfde `last_tick.ts_ms` niet opnieuw in dezelfde seconde tellen (voorkomt 10×/s spam bij 100 ms-loop). */
static int64_t s_last_merged_tick_ts_ms{-1};

static void ring_push(const Sample &s)
{
    if (s_count == k_ring_cap) {
        s_head = (s_head + 1U) % k_ring_cap;
        --s_count;
    }
    const size_t idx = (s_head + s_count) % k_ring_cap;
    s_ring[idx] = s;
    ++s_count;
}

/** Houdt buffer beperkt: verwijder alles ouder dan `horizon_ms` vóór `now_ms`. */
static void ring_prune_before(int64_t now_ms, int64_t horizon_ms)
{
    const int64_t cutoff = now_ms - horizon_ms;
    while (s_count > 0 && s_ring[s_head].ts_ms < cutoff) {
        s_head = (s_head + 1U) % k_ring_cap;
        --s_count;
    }
}

static bool ring_latest(Sample *out)
{
    if (s_count == 0 || !out) {
        return false;
    }
    const size_t idx = (s_head + s_count - 1U) % k_ring_cap;
    *out = s_ring[idx];
    return true;
}

/** Vul ontbrekende wandklok-seconden (carry-prijs) als de hoofdloop een seconde oversloeg. */
static void push_carry_seconds(int64_t from_sec_inclusive, int64_t to_sec_exclusive)
{
    if (s_carry_price_eur <= 0.0 || from_sec_inclusive >= to_sec_exclusive) {
        return;
    }
    for (int64_t sec = from_sec_inclusive; sec < to_sec_exclusive; ++sec) {
        Sample s{};
        s.ts_ms = sec * 1000LL + 999LL;
        s.price_eur = s_carry_price_eur;
        ring_prune_before(s.ts_ms, 360000);
        ring_push(s);
    }
}

static void bucket_reset()
{
    s_bucket = {};
}

static void finalize_bucket()
{
    if (!s_bucket.active) {
        return;
    }
    double canonical_price = 0.0;
    if (s_bucket.tick_count > 0) {
        canonical_price =
            s_bucket.sum_price / static_cast<double>(s_bucket.tick_count);
    } else if (s_carry_price_eur > 0.0) {
        /* Geen nieuw WS-bericht in deze wandklok-seconde — prijs gelijk houden voor 1×/s ring. */
        canonical_price = s_carry_price_eur;
    } else {
        return;
    }
    Sample s{};
    s.ts_ms = (s_bucket.sec_epoch * 1000LL) + 999LL;
    s.price_eur = canonical_price;
    ring_prune_before(s.ts_ms, 360000); // M-010c: ≥5m historie + marge (parallel aan 1m-metric)
    ring_push(s);
    ESP_LOGI(TAG,
             "M-010b: sec=%lld ticks=%u canonical=%.4f open=%.4f close=%.4f",
             (long long)s_bucket.sec_epoch,
             static_cast<unsigned>(s_bucket.tick_count),
             canonical_price,
             s_bucket.first_price,
             s_bucket.last_price);
    bucket_reset();
}

} // namespace

esp_err_t init()
{
    s_head = 0;
    s_count = 0;
    s_carry_price_eur = 0.0;
    s_last_merged_tick_ts_ms = -1;
    bucket_reset();
    std::memset(s_ring, 0, sizeof(s_ring));
    ESP_LOGI(TAG,
             "M-010b/c: domain_metrics init (ring=%u, per-second canonical, 1m+5m)",
             static_cast<unsigned>(k_ring_cap));
    return ESP_OK;
}

void feed(const market_data::MarketSnapshot &snap)
{
    if (!snap.valid || snap.last_tick.price_eur <= 0.0) {
        return;
    }
    s_carry_price_eur = snap.last_tick.price_eur;
    const int64_t wall_ms = static_cast<int64_t>(esp_timer_get_time() / 1000LL);
    const int64_t wsec = wall_ms / 1000LL;

    if (!s_bucket.active) {
        s_bucket.active = true;
        s_bucket.sec_epoch = wsec;
        s_last_merged_tick_ts_ms = snap.last_tick.ts_ms;
        s_bucket.sum_price = snap.last_tick.price_eur;
        s_bucket.tick_count = 1;
        s_bucket.first_price = snap.last_tick.price_eur;
        s_bucket.last_price = snap.last_tick.price_eur;
        return;
    }

    if (wsec > s_bucket.sec_epoch) {
        const int64_t closing_sec = s_bucket.sec_epoch;
        finalize_bucket();
        push_carry_seconds(closing_sec + 1, wsec);
        s_bucket.active = true;
        s_bucket.sec_epoch = wsec;
        s_last_merged_tick_ts_ms = snap.last_tick.ts_ms;
        s_bucket.sum_price = snap.last_tick.price_eur;
        s_bucket.tick_count = 1;
        s_bucket.first_price = snap.last_tick.price_eur;
        s_bucket.last_price = snap.last_tick.price_eur;
        return;
    }

    if (wsec != s_bucket.sec_epoch) {
        /* Tolerantie voor kleine klok- of volgorde-anomalieën: negeren. */
        return;
    }

    if (snap.last_tick.ts_ms == s_last_merged_tick_ts_ms) {
        return;
    }
    s_last_merged_tick_ts_ms = snap.last_tick.ts_ms;
    s_bucket.sum_price += snap.last_tick.price_eur;
    ++s_bucket.tick_count;
    s_bucket.last_price = snap.last_tick.price_eur;
}

Metric1mMovePct compute_1m_move_pct()
{
    Metric1mMovePct m{};
    Sample latest{};
    if (!ring_latest(&latest)) {
        return m;
    }
    m.now_ts_ms = latest.ts_ms;
    m.now_price_eur = latest.price_eur;
    const int64_t target_ts = latest.ts_ms - 60000;
    bool have_ref = false;
    for (size_t k = 0; k < s_count; ++k) {
        const Sample &c = s_ring[(s_head + k) % k_ring_cap];
        if (c.ts_ms <= target_ts) {
            m.ref_ts_ms = c.ts_ms;
            m.ref_price_eur = c.price_eur;
            have_ref = true;
        }
    }
    if (!have_ref || m.ref_price_eur <= 0.0) {
        return m;
    }
    m.ready = true;
    m.pct = (m.now_price_eur - m.ref_price_eur) / m.ref_price_eur * 100.0;
    ESP_LOGD(TAG,
             "M-010b: metric-1m now=%.4f@%lld ref=%.4f@%lld pct=%.4f",
             m.now_price_eur,
             (long long)m.now_ts_ms,
             m.ref_price_eur,
             (long long)m.ref_ts_ms,
             m.pct);
    return m;
}

Metric5mMovePct compute_5m_move_pct()
{
    Metric5mMovePct m{};
    Sample latest{};
    if (!ring_latest(&latest)) {
        return m;
    }
    m.now_ts_ms = latest.ts_ms;
    m.now_price_eur = latest.price_eur;
    const int64_t target_ts = latest.ts_ms - 300000;
    bool have_ref = false;
    for (size_t k = 0; k < s_count; ++k) {
        const Sample &c = s_ring[(s_head + k) % k_ring_cap];
        if (c.ts_ms <= target_ts) {
            m.ref_ts_ms = c.ts_ms;
            m.ref_price_eur = c.price_eur;
            have_ref = true;
        }
    }
    if (!have_ref || m.ref_price_eur <= 0.0) {
        return m;
    }
    m.ready = true;
    m.pct = (m.now_price_eur - m.ref_price_eur) / m.ref_price_eur * 100.0;
    ESP_LOGD(TAG,
             "M-010c: metric-5m now=%.4f@%lld ref=%.4f@%lld pct=%.4f",
             m.now_price_eur,
             (long long)m.now_ts_ms,
             m.ref_price_eur,
             (long long)m.ref_ts_ms,
             m.pct);
    return m;
}

MetricVolMeanAbsStepBps compute_vol_mean_abs_step_bps()
{
    MetricVolMeanAbsStepBps out{};
    if (s_count < 2) {
        return out;
    }
    Sample latest{};
    if (!ring_latest(&latest)) {
        return out;
    }
    const int64_t window_ms = static_cast<int64_t>(CONFIG_ALERT_REGIME_VOL_WINDOW_SEC) * 1000LL;
    const int64_t cutoff_ts = latest.ts_ms - window_ms;
    const int64_t dt_min = static_cast<int64_t>(CONFIG_ALERT_REGIME_VOL_PAIR_MIN_MS);
    const int64_t dt_max = static_cast<int64_t>(CONFIG_ALERT_REGIME_VOL_PAIR_MAX_MS);

    double sum_bps = 0.0;
    uint32_t n = 0;
    for (size_t k = 1; k < s_count; ++k) {
        const Sample &p0 = s_ring[(s_head + k - 1U) % k_ring_cap];
        const Sample &p1 = s_ring[(s_head + k) % k_ring_cap];
        if (p0.ts_ms < cutoff_ts || p1.ts_ms < cutoff_ts) {
            continue;
        }
        if (p0.price_eur <= 0.0 || p1.price_eur <= 0.0) {
            continue;
        }
        const int64_t dt = p1.ts_ms - p0.ts_ms;
        if (dt < dt_min || dt > dt_max) {
            continue;
        }
        const double step_bps =
            std::fabs((p1.price_eur - p0.price_eur) / p0.price_eur) * 10000.0;
        sum_bps += step_bps;
        ++n;
    }
    const uint32_t need = static_cast<uint32_t>(CONFIG_ALERT_REGIME_VOL_MIN_PAIRS);
    if (n < need) {
        return out;
    }
    out.ready = true;
    out.pairs_used = n;
    out.mean_abs_step_bps = sum_bps / static_cast<double>(n);
    return out;
}

} // namespace domain_metrics
