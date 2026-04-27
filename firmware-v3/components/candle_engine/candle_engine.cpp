#include "candle_engine/candle_engine.hpp"

#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_bus/event_bus.hpp"
#include "market_store/market_store.hpp"

static const char *TAG = "CANDLE";
static const char *TAG_AN = "ANALYTICS";

static uint64_t s_cur_sec = UINT64_MAX;
static double s_o = 0;
static double s_h = 0;
static double s_l = 0;
static double s_c = 0;
static uint32_t s_sec_ticks = 0;

static uint64_t s_min_id = UINT64_MAX;
static uint64_t s_1m_open_ts_ms = 0;
static double m_o = 0;
static double m_h = 0;
static double m_l = 0;
static double m_c = 0;
static uint32_t m_secs_with_data = 0;

/** Ring van gesloten 1m candles; `s_recent_idx` = aantal pushes (nooit verlaagd). */
static constexpr int kRingCap = 64;
static candle_engine::Candle1m s_recent1m[kRingCap]{};
static uint32_t s_recent_idx = 0;

namespace {

size_t closed_candle_count() {
    if (s_recent_idx == 0) {
        return 0;
    }
    const uint32_t c = s_recent_idx;
    return c < static_cast<uint32_t>(kRingCap) ? static_cast<size_t>(c) : static_cast<size_t>(kRingCap);
}

bool read_closed_at_back(size_t back, candle_engine::Candle1m *out) {
    if (out == nullptr) {
        return false;
    }
    const size_t n = closed_candle_count();
    if (back >= n || s_recent_idx == 0) {
        return false;
    }
    const size_t idx =
        (static_cast<size_t>(s_recent_idx) - 1u - back + static_cast<size_t>(kRingCap) * 2u) %
        static_cast<size_t>(kRingCap);
    *out = s_recent1m[idx];
    return out->valid;
}

double ret_pct_close_to_close(double close_now, double close_then) {
    if (close_then <= 0.0 || !std::isfinite(close_now) || !std::isfinite(close_then)) {
        return 0.0;
    }
    return (close_now / close_then - 1.0) * 100.0;
}

void fill_return_snapshot(candle_engine::ReturnSnapshot *rs) {
    std::memset(rs, 0, sizeof(*rs));
    const size_t n = closed_candle_count();
    candle_engine::Candle1m c0{};
    if (n < 2 || !read_closed_at_back(0, &c0)) {
        return;
    }
    candle_engine::Candle1m c1{};
    if (n >= 2 && read_closed_at_back(1, &c1) && c1.close > 0.0) {
        rs->valid_1m = true;
        rs->ret_1m_pct = ret_pct_close_to_close(c0.close, c1.close);
    }
    candle_engine::Candle1m c5{};
    if (n >= 6 && read_closed_at_back(5, &c5) && c5.close > 0.0) {
        rs->valid_5m = true;
        rs->ret_5m_pct = ret_pct_close_to_close(c0.close, c5.close);
    }
    candle_engine::Candle1m c30{};
    if (n >= 31 && read_closed_at_back(30, &c30) && c30.close > 0.0) {
        rs->valid_30m = true;
        rs->ret_30m_pct = ret_pct_close_to_close(c0.close, c30.close);
    }
}

void window_min_max(size_t window_candles, double *out_min, double *out_max) {
    *out_min = 0.0;
    *out_max = 0.0;
    if (window_candles == 0) {
        return;
    }
    bool init = false;
    for (size_t b = 0; b < window_candles; ++b) {
        candle_engine::Candle1m c{};
        if (!read_closed_at_back(b, &c)) {
            break;
        }
        if (!init) {
            *out_min = c.low;
            *out_max = c.high;
            init = true;
        } else {
            *out_min = std::fmin(*out_min, c.low);
            *out_max = std::fmax(*out_max, c.high);
        }
    }
}

void fill_range_snapshot(candle_engine::WindowRangeSnapshot *ws) {
    std::memset(ws, 0, sizeof(*ws));
    const size_t n = closed_candle_count();
    if (n >= 1) {
        ws->valid_1m = true;
        window_min_max(1, &ws->min_1m, &ws->max_1m);
    }
    if (n >= 5) {
        ws->valid_5m = true;
        window_min_max(5, &ws->min_5m, &ws->max_5m);
    }
    if (n >= 30) {
        ws->valid_30m = true;
        window_min_max(30, &ws->min_30m, &ws->max_30m);
    }
}

void log_analytics_after_1m_close() {
    candle_engine::ReturnSnapshot rs{};
    candle_engine::WindowRangeSnapshot ws{};
    fill_return_snapshot(&rs);
    fill_range_snapshot(&ws);
    const size_t n = closed_candle_count();

    char r1[20], r5[20], r30[20];
    if (rs.valid_1m) {
        snprintf(r1, sizeof r1, "%.3f", rs.ret_1m_pct);
    } else {
        snprintf(r1, sizeof r1, "NA");
    }
    if (rs.valid_5m) {
        snprintf(r5, sizeof r5, "%.3f", rs.ret_5m_pct);
    } else {
        snprintf(r5, sizeof r5, "NA");
    }
    if (rs.valid_30m) {
        snprintf(r30, sizeof r30, "%.3f", rs.ret_30m_pct);
    } else {
        snprintf(r30, sizeof r30, "NA");
    }

    ESP_LOGD(TAG_AN, "closed_1m=%zu ret1m=%s ret5m=%s ret30m=%s", n, r1, r5, r30);

    if (ws.valid_1m) {
        ESP_LOGD(TAG_AN, "range1m min=%.1f max=%.1f", ws.min_1m, ws.max_1m);
    } else {
        ESP_LOGD(TAG_AN, "range1m valid=0");
    }

    if (ws.valid_5m) {
        ESP_LOGD(TAG_AN, "range5m min=%.1f max=%.1f", ws.min_5m, ws.max_5m);
    } else {
        ESP_LOGD(TAG_AN, "range5m valid=0");
    }

    if (ws.valid_30m) {
        ESP_LOGD(TAG_AN, "range30m min=%.1f max=%.1f", ws.min_30m, ws.max_30m);
    } else {
        ESP_LOGD(TAG_AN, "range30m valid=0");
    }

    const esp_err_t pe =
        esp_event_post(CRYPTO_V3_EVENTS, event_bus::EV_1M_CLOSED, nullptr, 0, pdMS_TO_TICKS(50));
    if (pe != ESP_OK) {
        ESP_LOGW(TAG_AN, "esp_event_post EV_1M_CLOSED failed: %s", esp_err_to_name(pe));
    }
}

static void emit_1m_closed(uint64_t open_ts_ms) {
    candle_engine::Candle1m c{};
    c.open_ts_ms = open_ts_ms;
    c.open = m_o;
    c.high = m_h;
    c.low = m_l;
    c.close = m_c;
    c.seconds_with_data = m_secs_with_data;
    c.valid = true;

    s_recent1m[s_recent_idx % kRingCap] = c;
    ++s_recent_idx;

    ESP_LOGD(TAG, "1m closed O=%.1f H=%.1f L=%.1f C=%.1f sec=%" PRIu32, c.open, c.high, c.low, c.close, c.seconds_with_data);

    log_analytics_after_1m_close();
}

static void on_second_closed(const candle_engine::SecondBar &sb) {
    if (!sb.valid || sb.tick_count == 0) {
        return;
    }

    const uint64_t min_id = sb.second_epoch / 60;

    if (s_min_id == UINT64_MAX) {
        s_min_id = min_id;
        s_1m_open_ts_ms = sb.second_epoch * 1000ULL;
        m_o = sb.open;
        m_h = sb.high;
        m_l = sb.low;
        m_c = sb.close;
        m_secs_with_data = 1;
        return;
    }

    if (min_id == s_min_id) {
        m_h = std::fmax(m_h, sb.high);
        m_l = std::fmin(m_l, sb.low);
        m_c = sb.close;
        ++m_secs_with_data;
        return;
    }

    emit_1m_closed(s_1m_open_ts_ms);
    s_min_id = min_id;
    s_1m_open_ts_ms = sb.second_epoch * 1000ULL;
    m_o = sb.open;
    m_h = sb.high;
    m_l = sb.low;
    m_c = sb.close;
    m_secs_with_data = 1;
}

}  // namespace

extern "C" void candle_on_market_tick(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    (void)arg;
    (void)base;
    if (id != event_bus::EV_MARKET_TICK || event_data == nullptr) {
        return;
    }
    const auto *tick = static_cast<const market_store::TickEvent *>(event_data);
    const uint64_t now_ms = tick->ts_ms ? tick->ts_ms : static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    const uint64_t sec = now_ms / 1000;
    const double px = tick->price;

    if (s_cur_sec == UINT64_MAX) {
        s_cur_sec = sec;
        s_o = s_h = s_l = s_c = px;
        s_sec_ticks = 1;
        return;
    }

    if (sec == s_cur_sec) {
        s_h = std::fmax(s_h, px);
        s_l = std::fmin(s_l, px);
        s_c = px;
        ++s_sec_ticks;
        return;
    }

    candle_engine::SecondBar sb{};
    sb.second_epoch = s_cur_sec;
    sb.open = s_o;
    sb.high = s_h;
    sb.low = s_l;
    sb.close = s_c;
    sb.tick_count = s_sec_ticks;
    sb.valid = true;

    ESP_LOGD(TAG, "1s O=%.1f H=%.1f L=%.1f C=%.1f ticks=%" PRIu32, sb.open, sb.high, sb.low, sb.close, sb.tick_count);

    on_second_closed(sb);

    s_cur_sec = sec;
    s_o = s_h = s_l = s_c = px;
    s_sec_ticks = 1;
}

namespace candle_engine {

esp_err_t init() {
    s_cur_sec = UINT64_MAX;
    s_min_id = UINT64_MAX;
    s_sec_ticks = 0;
    m_secs_with_data = 0;
    std::memset(s_recent1m, 0, sizeof(s_recent1m));
    s_recent_idx = 0;

    const esp_err_t err =
        esp_event_handler_register(CRYPTO_V3_EVENTS, event_bus::EV_MARKET_TICK, &candle_on_market_tick, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_handler_register EV_MARKET_TICK failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init (1s + 1m + analytics ring=%d)", kRingCap);
    return ESP_OK;
}

size_t get_closed_candle_count() {
    return closed_candle_count();
}

bool get_closed_candle_from_latest(size_t back, Candle1m *out) {
    return read_closed_at_back(back, out);
}

bool get_analytics_snapshot(MarketAnalyticsSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    std::memset(out, 0, sizeof(*out));
    const market_store::MarketSnapshot snap = market_store::get_snapshot();
    out->ts_ms = snap.ts_ms ? snap.ts_ms : static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    out->last_price = snap.last_price;
    out->closed_1m_count = closed_candle_count();
    fill_return_snapshot(&out->returns);
    fill_range_snapshot(&out->ranges);
    return true;
}

}  // namespace candle_engine
