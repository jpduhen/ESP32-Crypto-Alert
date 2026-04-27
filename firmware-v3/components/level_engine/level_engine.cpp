#include "level_engine/level_engine.hpp"

#include <cmath>
#include <cinttypes>
#include <cstddef>
#include <cstring>

#include "candle_engine/candle_engine.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "event_bus/event_bus.hpp"
#include "freertos/portmacro.h"

static const char *TAG = "LEVEL";

static level_engine::LevelSnapshot s_snapshot{};
static portMUX_TYPE s_snapshot_mux = portMUX_INITIALIZER_UNLOCKED;

namespace {

constexpr size_t kScanMax = 60;         // kijk max ~laatste 60 gesloten 1m candles terug
constexpr size_t kMinCandlesForSwing = 5;  // i-1,i,i+1 met marges vereist

struct SwingPoint {
    bool valid;
    level_engine::LevelType type;
    double price;
    uint64_t ts_ms;
    uint32_t back;
};

bool fetch_candle(size_t back, candle_engine::Candle1m *out) {
    return candle_engine::get_closed_candle_from_latest(back, out);
}

double signed_distance_pct(double level_price, double current_price) {
    if (current_price <= 0.0 || !std::isfinite(current_price) || !std::isfinite(level_price)) {
        return 0.0;
    }
    return ((level_price - current_price) / current_price) * 100.0;
}

bool is_swing_high(size_t back) {
    candle_engine::Candle1m left{};
    candle_engine::Candle1m mid{};
    candle_engine::Candle1m right{};
    if (!fetch_candle(back + 1, &left) || !fetch_candle(back, &mid) || !fetch_candle(back - 1, &right)) {
        return false;
    }
    return mid.valid && left.valid && right.valid && (mid.high > left.high) && (mid.high > right.high);
}

bool is_swing_low(size_t back) {
    candle_engine::Candle1m left{};
    candle_engine::Candle1m mid{};
    candle_engine::Candle1m right{};
    if (!fetch_candle(back + 1, &left) || !fetch_candle(back, &mid) || !fetch_candle(back - 1, &right)) {
        return false;
    }
    return mid.valid && left.valid && right.valid && (mid.low < left.low) && (mid.low < right.low);
}

void set_price_level(level_engine::PriceLevel *dst, const SwingPoint &sp, double current_price) {
    std::memset(dst, 0, sizeof(*dst));
    dst->valid = sp.valid;
    if (!sp.valid) {
        return;
    }
    dst->type = sp.type;
    dst->price = sp.price;
    dst->source_ts_ms = sp.ts_ms;
    dst->candle_index_back = sp.back;
    dst->minutes_away = sp.back;
    dst->distance_pct = signed_distance_pct(sp.price, current_price);
}

void maybe_take_best_support(SwingPoint *first, SwingPoint *second, const SwingPoint &cand, double current_price) {
    if (!cand.valid || cand.price >= current_price) {
        return;
    }
    if (!first->valid || cand.price > first->price) {
        *second = *first;
        *first = cand;
        return;
    }
    if (!second->valid || cand.price > second->price) {
        *second = cand;
    }
}

void maybe_take_best_resistance(SwingPoint *first, SwingPoint *second, const SwingPoint &cand, double current_price) {
    if (!cand.valid || cand.price <= current_price) {
        return;
    }
    if (!first->valid || cand.price < first->price) {
        *second = *first;
        *first = cand;
        return;
    }
    if (!second->valid || cand.price < second->price) {
        *second = cand;
    }
}

void rebuild_snapshot() {
    level_engine::LevelSnapshot out{};

    candle_engine::MarketAnalyticsSnapshot analytics{};
    if (!candle_engine::get_analytics_snapshot(&analytics)) {
        portENTER_CRITICAL(&s_snapshot_mux);
        s_snapshot = out;
        portEXIT_CRITICAL(&s_snapshot_mux);
        return;
    }

    out.ts_ms = analytics.ts_ms;
    const size_t closed_n = candle_engine::get_closed_candle_count();
    if (closed_n < kMinCandlesForSwing || analytics.last_price <= 0.0) {
        out.valid = false;
        portENTER_CRITICAL(&s_snapshot_mux);
        s_snapshot = out;
        portEXIT_CRITICAL(&s_snapshot_mux);
        ESP_LOGD(TAG, "insufficient history closed_1m=%zu (need>=%zu)", closed_n, kMinCandlesForSwing);
        return;
    }

    SwingPoint s1{}, s2{}, r1{}, r2{};
    size_t swings_high = 0;
    size_t swings_low = 0;
    size_t scanned = 0;

    const size_t scan_limit = closed_n < kScanMax ? closed_n : kScanMax;
    // back=0 is meest recente gesloten candle; swing vereist buren links/rechts => start op 1, eindigt op n-2
    for (size_t back = 1; back + 1 < scan_limit; ++back) {
        ++scanned;
        candle_engine::Candle1m c{};
        if (!fetch_candle(back, &c) || !c.valid) {
            continue;
        }
        if (is_swing_high(back)) {
            ++swings_high;
            SwingPoint sp{true, level_engine::LevelType::kResistance, c.high, c.open_ts_ms, static_cast<uint32_t>(back)};
            maybe_take_best_resistance(&r1, &r2, sp, analytics.last_price);
        }
        if (is_swing_low(back)) {
            ++swings_low;
            SwingPoint sp{true, level_engine::LevelType::kSupport, c.low, c.open_ts_ms, static_cast<uint32_t>(back)};
            maybe_take_best_support(&s1, &s2, sp, analytics.last_price);
        }
    }

    set_price_level(&out.nearest_support_1, s1, analytics.last_price);
    set_price_level(&out.nearest_support_2, s2, analytics.last_price);
    set_price_level(&out.nearest_resistance_1, r1, analytics.last_price);
    set_price_level(&out.nearest_resistance_2, r2, analytics.last_price);

    out.valid = out.nearest_support_1.valid || out.nearest_resistance_1.valid;

    portENTER_CRITICAL(&s_snapshot_mux);
    s_snapshot = out;
    portEXIT_CRITICAL(&s_snapshot_mux);

    if (out.nearest_support_1.valid) {
        ESP_LOGD(TAG, "support1=%.1f dist=%.2f%% age=%" PRIu32 "m", out.nearest_support_1.price,
                 out.nearest_support_1.distance_pct, out.nearest_support_1.minutes_away);
    } else {
        ESP_LOGD(TAG, "support1=NA");
    }
    if (out.nearest_support_2.valid) {
        ESP_LOGD(TAG, "support2=%.1f dist=%.2f%% age=%" PRIu32 "m", out.nearest_support_2.price,
                 out.nearest_support_2.distance_pct, out.nearest_support_2.minutes_away);
    } else {
        ESP_LOGD(TAG, "support2=NA");
    }
    if (out.nearest_resistance_1.valid) {
        ESP_LOGD(TAG, "resistance1=%.1f dist=%.2f%% age=%" PRIu32 "m", out.nearest_resistance_1.price,
                 out.nearest_resistance_1.distance_pct, out.nearest_resistance_1.minutes_away);
    } else {
        ESP_LOGD(TAG, "resistance1=NA");
    }
    if (out.nearest_resistance_2.valid) {
        ESP_LOGD(TAG, "resistance2=%.1f dist=%.2f%% age=%" PRIu32 "m", out.nearest_resistance_2.price,
                 out.nearest_resistance_2.distance_pct, out.nearest_resistance_2.minutes_away);
    } else {
        ESP_LOGD(TAG, "resistance2=NA");
    }
    ESP_LOGD(TAG, "swings scanned=%zu highs=%zu lows=%zu selected S=%d R=%d", scanned, swings_high, swings_low,
             (out.nearest_support_1.valid ? 1 : 0) + (out.nearest_support_2.valid ? 1 : 0),
             (out.nearest_resistance_1.valid ? 1 : 0) + (out.nearest_resistance_2.valid ? 1 : 0));
}

extern "C" void level_on_1m_closed(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    (void)arg;
    (void)base;
    (void)event_data;
    if (id != event_bus::EV_1M_CLOSED) {
        return;
    }
    rebuild_snapshot();
}

}  // namespace

namespace level_engine {

esp_err_t init() {
    std::memset(&s_snapshot, 0, sizeof(s_snapshot));
    const esp_err_t err =
        esp_event_handler_register(CRYPTO_V3_EVENTS, event_bus::EV_1M_CLOSED, &level_on_1m_closed, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register EV_1M_CLOSED failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init (light swings op gesloten 1m)");
    return ESP_OK;
}

void update() {
    rebuild_snapshot();
}

bool get_snapshot(LevelSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_snapshot_mux);
    *out = s_snapshot;
    portEXIT_CRITICAL(&s_snapshot_mux);
    return true;
}

}  // namespace level_engine
