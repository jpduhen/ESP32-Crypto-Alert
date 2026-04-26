#pragma once

#include <cstdint>

#include "esp_err.h"

namespace level_engine {

enum class LevelType {
    kUnknown,
    kSupport,
    kResistance,
};

struct PriceLevel {
    bool valid;
    LevelType type;
    double price;
    uint64_t source_ts_ms;
    uint32_t candle_index_back;
    uint32_t minutes_away;
    /**
     * Signed distance in % tov laatste prijs:
     * ((level_price - current_price) / current_price) * 100.
     */
    double distance_pct;
};

struct LevelSnapshot {
    uint64_t ts_ms;
    bool valid;
    PriceLevel nearest_support_1;
    PriceLevel nearest_support_2;
    PriceLevel nearest_resistance_1;
    PriceLevel nearest_resistance_2;
};

esp_err_t init();
void update();
bool get_snapshot(LevelSnapshot *out);

}  // namespace level_engine
