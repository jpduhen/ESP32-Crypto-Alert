#pragma once

#include <cstdint>

#include "esp_err.h"

namespace setup_engine {

enum class SetupSide {
    kNone,
    kLong,
    kShort,
};

enum class SetupClass {
    kNone,
    kCandidate,
    kHighQualityCandidate,
};

/**
 * Mean-reversion setup candidate (geen entry/exit, geen alerts).
 * Drempels en regels zijn lokaal in setup_engine.cpp gedocumenteerd.
 */
struct SetupSnapshot {
    uint64_t ts_ms;
    bool valid;
    SetupSide side;
    SetupClass setup_class;
    bool regime_ok;
    bool level_ok;
    bool approach_ok;
    bool distance_valid;
    double distance_to_level_pct;
    double fast_approach_score;
    uint8_t component_score;
    uint8_t quality_score;
    char level_name[32];
    char reason[32];
};

esp_err_t init();
bool get_snapshot(SetupSnapshot *out);

}  // namespace setup_engine
