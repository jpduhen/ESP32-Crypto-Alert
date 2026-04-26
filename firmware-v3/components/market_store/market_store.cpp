#include "market_store/market_store.hpp"

#include <atomic>
#include <cinttypes>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_bus/event_bus.hpp"
#include "freertos/portmacro.h"

namespace {

const char *TAG = "MSTORE";

constexpr size_t kParseBuf = 768;

portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;
market_store::MarketSnapshot s_snap{};

std::atomic<uint64_t> s_frames{0};
std::atomic<uint64_t> s_ticker_ok{0};
std::atomic<uint64_t> s_ignored{0};
std::atomic<uint64_t> s_failed{0};
std::atomic<uint64_t> s_log_tick{0};

bool copy_bounded(const char *data, size_t len, char *out, size_t out_sz) {
    if (data == nullptr || out_sz == 0) {
        return false;
    }
    const size_t n = len < out_sz - 1 ? len : out_sz - 1;
    memcpy(out, data, n);
    out[n] = '\0';
    return true;
}

bool is_subscribed_ack(const char *buf) {
    return strstr(buf, "\"event\":\"subscribed\"") != nullptr || strstr(buf, "\"event\": \"subscribed\"") != nullptr;
}

bool is_btc_ticker(const char *buf) {
    if (strstr(buf, "\"event\":\"ticker\"") == nullptr && strstr(buf, "\"event\": \"ticker\"") == nullptr) {
        return false;
    }
    return strstr(buf, "BTC-EUR") != nullptr;
}

/** JSON `"key":"123.45"` (string getal). */
bool read_json_string_double(const char *buf, const char *quoted_key, double *out) {
    if (buf == nullptr || quoted_key == nullptr || out == nullptr) {
        return false;
    }
    const char *k = strstr(buf, quoted_key);
    if (k == nullptr) {
        return false;
    }
    const char *colon = strchr(k, ':');
    if (colon == nullptr) {
        return false;
    }
    const char *q = strchr(colon + 1, '"');
    if (q == nullptr) {
        return false;
    }
    ++q;
    char *end = nullptr;
    const double v = strtod(q, &end);
    if (end == q) {
        return false;
    }
    *out = v;
    return true;
}

bool build_tick_from_ticker_json(const char *buf, market_store::TickEvent *out) {
    std::memset(out, 0, sizeof(*out));
    out->ts_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);

    double bid = 0;
    double ask = 0;
    double last = 0;
    double lts = 0;
    const bool hb = read_json_string_double(buf, "\"bestBid\"", &bid);
    const bool ha = read_json_string_double(buf, "\"bestAsk\"", &ask);
    const bool hl = read_json_string_double(buf, "\"lastPrice\"", &last);
    (void)read_json_string_double(buf, "\"lastTradeSize\"", &lts);

    out->has_bid = hb;
    out->has_ask = ha;
    out->has_trade_price = hl;
    out->bid = bid;
    out->ask = ask;
    out->price = last;
    out->last_size = lts;

    if (!hl && hb && ha) {
        out->price = (bid + ask) * 0.5;
        out->has_trade_price = true;
    } else if (hl) {
        out->has_trade_price = true;
    } else if (hb) {
        out->price = bid;
        out->has_trade_price = true;
    } else if (ha) {
        out->price = ask;
        out->has_trade_price = true;
    } else {
        return false;
    }
    return true;
}

void update_snapshot_locked(const market_store::TickEvent &t, uint64_t transport_rx) {
    s_snap.ts_ms = t.ts_ms;
    s_snap.last_price = t.price;
    if (t.has_bid) {
        s_snap.best_bid = t.bid;
    }
    if (t.has_ask) {
        s_snap.best_ask = t.ask;
    }
    s_snap.ws_live = true;
    s_snap.transport_rx = transport_rx;
}

}  // namespace

namespace market_store {

esp_err_t init() {
    portENTER_CRITICAL(&s_snap_mux);
    std::memset(&s_snap, 0, sizeof(s_snap));
    portEXIT_CRITICAL(&s_snap_mux);
    s_frames.store(0);
    s_ticker_ok.store(0);
    s_ignored.store(0);
    s_failed.store(0);
    s_log_tick.store(0);
    ESP_LOGI(TAG, "init (snapshot + Bitvavo ticker parse)");
    return ESP_OK;
}

void ingest_ws_text(const char *data, size_t len, uint64_t transport_rx) {
    if (data == nullptr || len == 0) {
        return;
    }

    char buf[kParseBuf];
    if (!copy_bounded(data, len, buf, sizeof(buf))) {
        return;
    }

    s_frames.fetch_add(1, std::memory_order_relaxed);

    if (is_subscribed_ack(buf)) {
        s_ignored.fetch_add(1, std::memory_order_relaxed);
        portENTER_CRITICAL(&s_snap_mux);
        s_snap.transport_rx = transport_rx;
        portEXIT_CRITICAL(&s_snap_mux);
        return;
    }

    if (!is_btc_ticker(buf)) {
        s_ignored.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    TickEvent tick{};
    if (!build_tick_from_ticker_json(buf, &tick)) {
        s_failed.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    s_ticker_ok.fetch_add(1, std::memory_order_relaxed);

    portENTER_CRITICAL(&s_snap_mux);
    update_snapshot_locked(tick, transport_rx);
    portEXIT_CRITICAL(&s_snap_mux);

    const esp_err_t pe =
        esp_event_post(CRYPTO_V3_EVENTS, event_bus::EV_MARKET_TICK, &tick, sizeof(tick), pdMS_TO_TICKS(50));
    if (pe != ESP_OK) {
        ESP_LOGW(TAG, "esp_event_post EV_MARKET_TICK failed: %s", esp_err_to_name(pe));
    }

    const uint64_t n = s_log_tick.fetch_add(1, std::memory_order_relaxed) + 1;
    if ((n % 50ULL) == 0ULL) {
        ESP_LOGI(TAG, "last=%.2f bid=%.2f ask=%.2f parsed=%" PRIu64 " fail=%" PRIu64, tick.price, tick.bid, tick.ask,
                 static_cast<unsigned long long>(s_ticker_ok.load(std::memory_order_relaxed)),
                 static_cast<unsigned long long>(s_failed.load(std::memory_order_relaxed)));
    }
}

MarketSnapshot get_snapshot() {
    MarketSnapshot out{};
    portENTER_CRITICAL(&s_snap_mux);
    out = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);
    return out;
}

IngestStats get_ingest_stats() {
    IngestStats o{};
    o.frames = s_frames.load(std::memory_order_relaxed);
    o.ticker_ok = s_ticker_ok.load(std::memory_order_relaxed);
    o.ignored = s_ignored.load(std::memory_order_relaxed);
    o.failed = s_failed.load(std::memory_order_relaxed);
    return o;
}

}  // namespace market_store
