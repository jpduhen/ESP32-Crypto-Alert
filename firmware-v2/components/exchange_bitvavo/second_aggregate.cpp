/**
 * RWS-03: bounded seconde-aggregaten uit trade-events; ticker = aanvullende context.
 */
#include "exchange_bitvavo/second_aggregate.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>

namespace exchange_bitvavo::second_aggregate {

namespace {

static const char TAG[] = "sec_agg";

static constexpr uint64_t k_slot_unused_wall_sec = UINT64_MAX;

/** WS-event task en `exchange_bitvavo::tick()` kunnen gelijktijdig lopen — serialiseer state. */
static SemaphoreHandle_t s_mu{};

static constexpr size_t k_ring_cap = 32;

struct RingSlot {
    uint64_t wall_sec_id{k_slot_unused_wall_sec};
    uint32_t trade_count{0};
    double first_eur{0.0};
    double last_eur{0.0};
    double min_eur{0.0};
    double max_eur{0.0};
    double mean_eur{0.0};
    bool has_trades{false};
    uint32_t canonical_ticks{0};
    bool ticker_seen{false};
};

static RingSlot s_ring[k_ring_cap]{};
static size_t s_ring_widx{0};
static size_t s_ring_count{0};
static uint32_t s_writes_total{0};

/** Running accumulators voor de seconde `s_run_sec` (lopen met WS-tijd). */
static uint64_t s_run_sec{UINT64_MAX};
static uint32_t s_trade_n{0};
static double s_sum{0.0};
static double s_first{0.0};
static double s_last{0.0};
static double s_min{0.0};
static double s_max{0.0};
static uint32_t s_canonical_n{0};

static void clear_running()
{
    s_run_sec = UINT64_MAX;
    s_trade_n = 0;
    s_sum = 0.0;
    s_first = 0.0;
    s_last = 0.0;
    s_min = 0.0;
    s_max = 0.0;
    s_canonical_n = 0;
}

static void ensure_running(uint64_t wall_sec_id)
{
    if (s_run_sec != wall_sec_id) {
        s_run_sec = wall_sec_id;
        s_trade_n = 0;
        s_sum = 0.0;
        s_canonical_n = 0;
    }
}

static void push_ring(const RingSlot &s)
{
    s_ring[s_ring_widx] = s;
    s_ring_widx = (s_ring_widx + 1) % k_ring_cap;
    if (s_ring_count < k_ring_cap) {
        ++s_ring_count;
    }
    ++s_writes_total;
}

static bool take_mu()
{
    if (!s_mu) {
        s_mu = xSemaphoreCreateMutex();
    }
    return s_mu != nullptr && xSemaphoreTake(s_mu, pdMS_TO_TICKS(80)) == pdTRUE;
}

static void give_mu()
{
    if (s_mu) {
        xSemaphoreGive(s_mu);
    }
}

static void copy_latest_to_snap(const RingSlot &s, market_types::MarketSnapshot *snap)
{
    if (!snap) {
        return;
    }
    snap->ws_second_agg_wall_sec = s.wall_sec_id;
    snap->ws_second_agg_has_trades = s.has_trades;
    snap->ws_second_agg_trade_count = s.trade_count;
    snap->ws_second_agg_first_eur = s.first_eur;
    snap->ws_second_agg_last_eur = s.last_eur;
    snap->ws_second_agg_min_eur = s.min_eur;
    snap->ws_second_agg_max_eur = s.max_eur;
    snap->ws_second_agg_mean_eur = s.mean_eur;
    snap->ws_second_agg_ticker_seen = s.ticker_seen;
    snap->ws_second_agg_canonical_ticks = s.canonical_ticks;
    snap->ws_second_ring_capacity = static_cast<uint16_t>(k_ring_cap);
    snap->ws_second_ring_used = static_cast<uint16_t>(s_ring_count);
    snap->ws_second_ring_writes_total = s_writes_total;
}

} // namespace

void reset_on_ws_connect()
{
    if (!take_mu()) {
        return;
    }
    s_ring_widx = 0;
    s_ring_count = 0;
    s_writes_total = 0;
    clear_running();
    for (auto &r : s_ring) {
        r = RingSlot{};
    }
    give_mu();
}

void note_trade(double price_eur, uint64_t wall_sec_id)
{
    if (!take_mu()) {
        return;
    }
    ensure_running(wall_sec_id);
    if (s_trade_n == 0) {
        s_first = s_last = s_min = s_max = price_eur;
    } else {
        if (price_eur < s_min) {
            s_min = price_eur;
        }
        if (price_eur > s_max) {
            s_max = price_eur;
        }
        s_last = price_eur;
    }
    ++s_trade_n;
    s_sum += price_eur;
    give_mu();
}

void note_canonical_tick(uint64_t wall_sec_id)
{
    if (!take_mu()) {
        return;
    }
    ensure_running(wall_sec_id);
    ++s_canonical_n;
    give_mu();
}

void finalize_completed_second(uint64_t completed_wall_sec, market_types::MarketSnapshot *snap)
{
    if (!take_mu()) {
        return;
    }
    RingSlot out{};
    out.wall_sec_id = completed_wall_sec;

    if (s_run_sec == completed_wall_sec) {
        out.trade_count = s_trade_n;
        out.has_trades = s_trade_n > 0;
        out.canonical_ticks = s_canonical_n;
        out.ticker_seen = s_canonical_n > 0;
        if (out.has_trades) {
            out.first_eur = s_first;
            out.last_eur = s_last;
            out.min_eur = s_min;
            out.max_eur = s_max;
            out.mean_eur = s_sum / static_cast<double>(s_trade_n);
        }
    } else {
        /* Geen state voor deze seconde (catch-up gap): lege sample. */
        out.has_trades = false;
        out.ticker_seen = false;
    }

    push_ring(out);
    copy_latest_to_snap(out, snap);

    const char *agg_note = out.has_trades ? "trade_mean" : "no_trades";
    const double agg_px = out.has_trades ? out.mean_eur : 0.0;
    give_mu();
    ESP_LOGD(TAG,
             "[WS_SEC] sec=%llu trades=%u mean=%.4f (%s) ticker_seen=%s ticks=%u",
             static_cast<unsigned long long>(out.wall_sec_id),
             static_cast<unsigned>(out.trade_count),
             agg_px,
             agg_note,
             out.ticker_seen ? "ja" : "nee",
             static_cast<unsigned>(out.canonical_ticks));
}

void seed_running_second(uint64_t new_wall_sec)
{
    if (!take_mu()) {
        return;
    }
    s_run_sec = new_wall_sec;
    s_trade_n = 0;
    s_sum = 0.0;
    s_canonical_n = 0;
    give_mu();
}

bool lookup_completed_second(uint64_t wall_sec_id, AggregatedSecondLookup *out)
{
    if (!out) {
        return false;
    }
    *out = AggregatedSecondLookup{};
    if (!take_mu()) {
        return false;
    }
    for (size_t i = 0; i < k_ring_cap; ++i) {
        const RingSlot &r = s_ring[i];
        if (r.wall_sec_id != wall_sec_id || r.wall_sec_id == k_slot_unused_wall_sec) {
            continue;
        }
        out->found = true;
        out->has_trades = r.has_trades;
        out->trade_count = r.trade_count;
        out->trade_mean_eur = r.mean_eur;
        out->trade_last_eur = r.last_eur;
        give_mu();
        return true;
    }
    give_mu();
    return false;
}

} // namespace exchange_bitvavo::second_aggregate
