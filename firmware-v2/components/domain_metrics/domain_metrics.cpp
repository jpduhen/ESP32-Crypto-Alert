/**
 * M-010a: rolling samples (prijs + tijd) — sober; cadence volgt caller (typ. app_core-lus).
 * M-010b: canonicalisatie naar 1 representatieve secondewaarde (TWAP-achtig gemiddelde).
 * M-010c: 5m-metric op dezelfde ring (cap > 5 min canonieke seconden).
 * S30-1: ring verbreed naar ≥30m + marge; 30m-% parallel aan 1m/5m (geen alert_engine).
 * S2H-1: aparte minuut-decimatie-ring voor 2h-%-metric (RAM << 7200 canonieke samples).
 * M-010f: gemiddelde | stap | tussen opeenvolgende canonieke secondes (bps) als vol-proxy.
 */
#include "domain_metrics/domain_metrics.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include <cmath>
#include <cstring>

#if CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE && CONFIG_MD_USE_EXCHANGE_BITVAVO
#include "exchange_bitvavo/second_aggregate.hpp"
#endif

#ifndef CONFIG_MD_USE_EXCHANGE_BITVAVO
#define CONFIG_MD_USE_EXCHANGE_BITVAVO 0
#endif
#ifndef CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE
#define CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE 0
#endif
#ifndef CONFIG_DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS
#define CONFIG_DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS 0
#endif

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

#ifndef CONFIG_DOMAIN_METRICS_30M_WINDOW_S
#define CONFIG_DOMAIN_METRICS_30M_WINDOW_S 1800
#endif
#ifndef CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S
#define CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S 120
#endif
#ifndef CONFIG_DOMAIN_METRICS_2H_WINDOW_S
#define CONFIG_DOMAIN_METRICS_2H_WINDOW_S 7200
#endif
#ifndef CONFIG_DOMAIN_METRICS_2H_MINUTE_RING_EXTRA_SLOTS
#define CONFIG_DOMAIN_METRICS_2H_MINUTE_RING_EXTRA_SLOTS 24
#endif

namespace domain_metrics {

namespace {

static const char TAG[] = DIAG_TAG_METRICS;

/** S30-1: venster (30m) + marge — één canonieke 1 Hz-ring voor 1m/5m/30m. */
static constexpr size_t k_ring_cap = static_cast<size_t>(
    CONFIG_DOMAIN_METRICS_30M_WINDOW_S + CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S);

static constexpr int64_t k_ring_horizon_ms =
    static_cast<int64_t>(CONFIG_DOMAIN_METRICS_30M_WINDOW_S + CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S) *
    1000LL;

static constexpr int64_t k_30m_window_ms =
    static_cast<int64_t>(CONFIG_DOMAIN_METRICS_30M_WINDOW_S) * 1000LL;

static constexpr int64_t k_2h_window_ms =
    static_cast<int64_t>(CONFIG_DOMAIN_METRICS_2H_WINDOW_S) * 1000LL;

/** S2H-1: één slot per wandklok-minuut (slot-ts = laatste canonieke sample in die minuut). */
static constexpr size_t k_2h_minute_ring_cap =
    static_cast<size_t>((CONFIG_DOMAIN_METRICS_2H_WINDOW_S + 59) / 60 +
                        CONFIG_DOMAIN_METRICS_2H_MINUTE_RING_EXTRA_SLOTS + 2U);

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

static Sample s_2h_minute_ring[k_2h_minute_ring_cap]{};
static size_t s_2h_minute_head{0};
static size_t s_2h_minute_count{0};
static int64_t s_2h_curr_min_key{-1};
static Sample s_2h_pending_close{};

#if CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE && CONFIG_MD_USE_EXCHANGE_BITVAVO
static uint32_t s_rws04_sec_trade_mean{0};
static uint32_t s_rws04_sec_fallback{0};
static const char k_src_mean[] = "trade_mean";
static const char k_src_twap[] = "canonical_twap";
static const char k_src_carry[] = "carry";
static const char k_fb_none[] = "";
static const char k_fb_not_in_ring[] = "aggregate_not_in_ring";
static const char k_fb_no_trades[] = "no_trades_in_second";
static const char k_fb_bad_mean[] = "invalid_trade_mean";
static const char k_ab_aggregate[] = "aggregate";
static const char k_ab_agg_large[] = "aggregate_large_delta";
static const char k_ab_fallback[] = "fallback";
static const char k_ab_no_trades[] = "no_trades";
static const char k_ab_not_in_ring[] = "aggregate_not_in_ring";
static const char *s_last_finalize_src = k_fb_none;
static const char *s_last_fallback_reason = k_fb_none;
static uint32_t s_ab_seconds_total{0};
static uint32_t s_ab_trade_seconds_total{0};
static uint32_t s_ab_large_delta_events_total{0};
static double s_last_ticker_canonical_eur{0.0};
static double s_last_trade_mean_eur{0.0};
static double s_last_delta_abs_eur{0.0};
static double s_last_delta_bps{0.0};
static double s_last_delta_pct{0.0};
static uint32_t s_last_trade_count_ab{0};
static uint64_t s_last_compare_wall_sec{0};
static bool s_last_ab_large_delta_flag{false};
static const char *s_last_ab_class = k_fb_none;
static uint64_t s_rws04ab_last_log_wall_sec{0};
#endif

static void minute_ring_push(const Sample &s)
{
    if (s_2h_minute_count == k_2h_minute_ring_cap) {
        s_2h_minute_head = (s_2h_minute_head + 1U) % k_2h_minute_ring_cap;
        --s_2h_minute_count;
    }
    const size_t idx = (s_2h_minute_head + s_2h_minute_count) % k_2h_minute_ring_cap;
    s_2h_minute_ring[idx] = s;
    ++s_2h_minute_count;
}

/** Vult minuut-ring vanuit 1 Hz-canoniek; bij minuutwissel sluiten we vorige minuut af met laatste sample. */
static void feed_2h_minute_from_canonical(const Sample &s)
{
    const int64_t mk = s.ts_ms / 60000LL;
    if (s_2h_curr_min_key < 0) {
        s_2h_curr_min_key = mk;
        s_2h_pending_close = s;
        return;
    }
    if (mk > s_2h_curr_min_key) {
        minute_ring_push(s_2h_pending_close);
        s_2h_curr_min_key = mk;
        s_2h_pending_close = s;
    } else {
        s_2h_pending_close = s;
    }
}

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
        ring_prune_before(s.ts_ms, k_ring_horizon_ms);
        ring_push(s);
        feed_2h_minute_from_canonical(s);
    }
}

static void bucket_reset()
{
    s_bucket = {};
}

/** Bestaande M-010b-canonical zonder RWS-04-override. */
static bool legacy_canonical_price(double *out)
{
    if (!out) {
        return false;
    }
    if (s_bucket.tick_count > 0) {
        *out = s_bucket.sum_price / static_cast<double>(s_bucket.tick_count);
        return true;
    }
    if (s_carry_price_eur > 0.0) {
        *out = s_carry_price_eur;
        return true;
    }
    return false;
}

static void finalize_bucket()
{
    if (!s_bucket.active) {
        return;
    }
    double canonical_price = 0.0;
    const char *rws04_note = "";

#if CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE && CONFIG_MD_USE_EXCHANGE_BITVAVO
    double ticker_canonical_eur = 0.0;
    const bool have_ticker_ref = legacy_canonical_price(&ticker_canonical_eur);

    exchange_bitvavo::second_aggregate::AggregatedSecondLookup lk{};
    const bool have_lk = exchange_bitvavo::second_aggregate::lookup_completed_second(
        static_cast<uint64_t>(s_bucket.sec_epoch), &lk);

    ++s_ab_seconds_total;
    s_last_compare_wall_sec = static_cast<uint64_t>(s_bucket.sec_epoch);
    s_last_ticker_canonical_eur = have_ticker_ref ? ticker_canonical_eur : 0.0;
    s_last_trade_mean_eur =
        (have_lk && lk.has_trades && lk.trade_mean_eur > 0.0) ? lk.trade_mean_eur : 0.0;
    s_last_trade_count_ab = have_lk ? lk.trade_count : 0U;
    s_last_delta_abs_eur = 0.0;
    s_last_delta_bps = 0.0;
    s_last_delta_pct = 0.0;
    s_last_ab_large_delta_flag = false;
    s_last_ab_class = k_fb_none;

    if (have_lk && lk.has_trades) {
        ++s_ab_trade_seconds_total;
    }

    const bool ab_pair_ok = have_ticker_ref && ticker_canonical_eur > 0.0 && have_lk && lk.has_trades &&
                            lk.trade_mean_eur > 0.0;
    if (ab_pair_ok) {
        s_last_delta_abs_eur = std::fabs(lk.trade_mean_eur - ticker_canonical_eur);
        s_last_delta_bps = (s_last_delta_abs_eur / ticker_canonical_eur) * 10000.0;
        s_last_delta_pct = (s_last_delta_abs_eur / ticker_canonical_eur) * 100.0;
    }

    const int large_th = CONFIG_DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS;
    if (large_th > 0 && ab_pair_ok && s_last_delta_bps >= static_cast<double>(large_th)) {
        ++s_ab_large_delta_events_total;
        s_last_ab_large_delta_flag = true;
        const uint64_t ws = static_cast<uint64_t>(s_bucket.sec_epoch);
        if (s_rws04ab_last_log_wall_sec == 0ULL || (ws - s_rws04ab_last_log_wall_sec) >= 90ULL) {
            s_rws04ab_last_log_wall_sec = ws;
            ESP_LOGW(TAG,
                     "[RWS04AB] grote Δ ticker↔mean: %.1f bps (ticker=%.4f mean=%.4f trades=%u) sec=%llu",
                     s_last_delta_bps,
                     ticker_canonical_eur,
                     lk.trade_mean_eur,
                     static_cast<unsigned>(lk.trade_count),
                     static_cast<unsigned long long>(ws));
        }
    }

    if (have_lk && lk.has_trades && lk.trade_mean_eur > 0.0) {
        canonical_price = lk.trade_mean_eur;
        ++s_rws04_sec_trade_mean;
        s_last_finalize_src = k_src_mean;
        s_last_fallback_reason = k_fb_none;
        rws04_note = " rws04=trade_mean";
        s_last_ab_class = s_last_ab_large_delta_flag ? k_ab_agg_large : k_ab_aggregate;
    } else {
        ++s_rws04_sec_fallback;
        if (!have_lk) {
            s_last_fallback_reason = k_fb_not_in_ring;
        } else if (!lk.has_trades) {
            s_last_fallback_reason = k_fb_no_trades;
        } else {
            s_last_fallback_reason = k_fb_bad_mean;
        }
        if (!legacy_canonical_price(&canonical_price)) {
            return;
        }
        s_last_finalize_src = (s_bucket.tick_count > 0) ? k_src_twap : k_src_carry;
        rws04_note = " rws04=fallback";
        if (!have_lk) {
            s_last_ab_class = k_ab_not_in_ring;
        } else if (!lk.has_trades) {
            s_last_ab_class = k_ab_no_trades;
        } else {
            s_last_ab_class = k_ab_fallback;
        }
        ESP_LOGD(TAG,
                 "[RWS04] fallback sec=%lld reason=%s lk=%d trades=%d mean=%.4f",
                 (long long)s_bucket.sec_epoch,
                 s_last_fallback_reason,
                 have_lk ? 1 : 0,
                 have_lk && lk.has_trades ? 1 : 0,
                 have_lk ? lk.trade_mean_eur : 0.0);
    }
#else
    if (!legacy_canonical_price(&canonical_price)) {
        return;
    }
#endif

    Sample s{};
    s.ts_ms = (s_bucket.sec_epoch * 1000LL) + 999LL;
    s.price_eur = canonical_price;
    ring_prune_before(s.ts_ms, k_ring_horizon_ms); // S30-1: ≥30m historie + marge
    ring_push(s);
    feed_2h_minute_from_canonical(s);
    ESP_LOGI(TAG,
             "M-010b: sec=%lld ticks=%u canonical=%.4f open=%.4f close=%.4f%s",
             (long long)s_bucket.sec_epoch,
             static_cast<unsigned>(s_bucket.tick_count),
             canonical_price,
             s_bucket.first_price,
             s_bucket.last_price,
             rws04_note);
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
    std::memset(s_2h_minute_ring, 0, sizeof(s_2h_minute_ring));
    s_2h_minute_head = 0;
    s_2h_minute_count = 0;
    s_2h_curr_min_key = -1;
    s_2h_pending_close = {};
    ESP_LOGI(TAG,
             "M-010b/c + S30-1 + S2H-1: domain_metrics init (1 Hz ring=%u horizon≈%ds; 2h min ring=%u slots, "
             "2h_window=%ds)",
             static_cast<unsigned>(k_ring_cap),
             CONFIG_DOMAIN_METRICS_30M_WINDOW_S + CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S,
             static_cast<unsigned>(k_2h_minute_ring_cap),
             CONFIG_DOMAIN_METRICS_2H_WINDOW_S);
#if CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE && CONFIG_MD_USE_EXCHANGE_BITVAVO
    ESP_LOGI(TAG,
             "[RWS04] domain_metrics invoer: second_aggregate trade-mean actief (fallback=ticker TWAP/carry)");
    ESP_LOGI(TAG,
             "[RWS04b] A/B observability: large_delta drempel=%d bps (0=uit)",
             CONFIG_DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS);
#endif
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

Metric30mMovePct compute_30m_move_pct()
{
    Metric30mMovePct m{};
    Sample latest{};
    if (!ring_latest(&latest)) {
        return m;
    }
    m.now_ts_ms = latest.ts_ms;
    m.now_price_eur = latest.price_eur;
    const int64_t target_ts = latest.ts_ms - k_30m_window_ms;
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
    m.ref_span_ms = m.now_ts_ms - m.ref_ts_ms;
    ESP_LOGD(TAG,
             "S30-1: metric-30m now=%.4f@%lld ref=%.4f@%lld pct=%.4f span_ms=%lld",
             m.now_price_eur,
             (long long)m.now_ts_ms,
             m.ref_price_eur,
             (long long)m.ref_ts_ms,
             m.pct,
             (long long)m.ref_span_ms);
    return m;
}

Metric2hMovePct compute_2h_move_pct()
{
    Metric2hMovePct m{};
    m.minute_ring_used = static_cast<uint32_t>(s_2h_minute_count);
    Sample latest{};
    if (!ring_latest(&latest)) {
        return m;
    }
    m.now_ts_ms = latest.ts_ms;
    m.now_price_eur = latest.price_eur;
    const int64_t target_ts = latest.ts_ms - k_2h_window_ms;
    bool have_ref = false;
    for (size_t k = 0; k < s_2h_minute_count; ++k) {
        const Sample &c = s_2h_minute_ring[(s_2h_minute_head + k) % k_2h_minute_ring_cap];
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
    m.ref_span_ms = m.now_ts_ms - m.ref_ts_ms;
    ESP_LOGD(TAG,
             "S2H-1: metric-2h now=%.4f@%lld ref=%.4f@%lld pct=%.4f span_ms=%lld min_slots=%u",
             m.now_price_eur,
             (long long)m.now_ts_ms,
             m.ref_price_eur,
             (long long)m.ref_ts_ms,
             m.pct,
             (long long)m.ref_span_ms,
             static_cast<unsigned>(s_2h_minute_count));
    return m;
}

size_t canonical_ring_count()
{
    return s_count;
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

MetricsInputSourceObservability metrics_input_source_observability()
{
    MetricsInputSourceObservability o{};
#if CONFIG_DOMAIN_METRICS_RWS04_SECOND_AGGREGATE && CONFIG_MD_USE_EXCHANGE_BITVAVO
    o.rws04_enabled = true;
    o.seconds_via_trade_mean = s_rws04_sec_trade_mean;
    o.seconds_via_fallback = s_rws04_sec_fallback;
    o.last_finalize_source = s_last_finalize_src;
    o.last_fallback_reason = s_last_fallback_reason;
    o.ab_compare_seconds_total = s_ab_seconds_total;
    o.ab_compare_trade_seconds_total = s_ab_trade_seconds_total;
    o.ab_compare_large_delta_total = s_ab_large_delta_events_total;
    o.last_compare_wall_sec = s_last_compare_wall_sec;
    o.last_ticker_canonical_eur = s_last_ticker_canonical_eur;
    o.last_trade_mean_eur = s_last_trade_mean_eur;
    o.last_delta_abs_eur = s_last_delta_abs_eur;
    o.last_delta_bps = s_last_delta_bps;
    o.last_delta_pct = s_last_delta_pct;
    o.last_trade_count = s_last_trade_count_ab;
    o.last_large_delta = s_last_ab_large_delta_flag;
    o.last_ab_class = s_last_ab_class ? s_last_ab_class : "";
    o.large_delta_threshold_bps = CONFIG_DOMAIN_METRICS_RWS04B_LARGE_DELTA_BPS;
#endif
    return o;
}

} // namespace domain_metrics
