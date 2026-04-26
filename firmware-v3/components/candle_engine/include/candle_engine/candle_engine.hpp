#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace candle_engine {

/** Gesloten 1s-bucket (UTC-agnostisch: monotoon seconde-index ~ boot-tijd). */
struct SecondBar {
    uint64_t second_epoch;
    double open;
    double high;
    double low;
    double close;
    uint32_t tick_count;
    bool valid;
};

/** Gesloten 1m-basis (gebouwd uit 1s buckets). */
struct Candle1m {
    uint64_t open_ts_ms;
    double open;
    double high;
    double low;
    double close;
    uint32_t seconds_with_data;
    bool valid;
};

/**
 * Close-to-close return (%) op gesloten 1m candles: close[0] vs close[back].
 * ret_1m: back=1, ret_5m: back=5, ret_30m: back=30.
 * Alleen geldig bij voldoende gesloten history (geen half-open minuut).
 */
struct ReturnSnapshot {
    bool valid_1m;
    bool valid_5m;
    bool valid_30m;
    double ret_1m_pct;
    double ret_5m_pct;
    double ret_30m_pct;
};

/**
 * Min/max over het venster van de laatste N **gesloten** 1m candles.
 * Per candle: window low = minimum van `low`, window high = maximum van `high`.
 * Geen open/huidige minuut. N=1,5,30; `valid_*` false zonder minimaal N candles.
 */
struct WindowRangeSnapshot {
    bool valid_1m;
    bool valid_5m;
    bool valid_30m;
    double min_1m;
    double max_1m;
    double min_5m;
    double max_5m;
    double min_30m;
    double max_30m;
};

/** Samenstelling voor strategy/regime/UI (alleen data, geen beslislogica). */
struct MarketAnalyticsSnapshot {
    uint64_t ts_ms;
    double last_price;
    ReturnSnapshot returns;
    WindowRangeSnapshot ranges;
    size_t closed_1m_count;
};

esp_err_t init();

/** Aantal geldige gesloten 1m candles in de ring (max. ringcap). */
size_t get_closed_candle_count();

/**
 * Haal gesloten 1m candle op, tellend vanaf de meest recente.
 * @param back 0 = laatst gesloten, 1 = daarvoor, enz.
 * @return false bij ongeldige @p out, onvoldoende history, of ongeldige candle in slot.
 */
bool get_closed_candle_from_latest(size_t back, Candle1m *out);

/** Vult @p out met live markt-timestamp, laatste prijs + returns/ranges op basis van gesloten 1m. */
bool get_analytics_snapshot(MarketAnalyticsSnapshot *out);

}  // namespace candle_engine
