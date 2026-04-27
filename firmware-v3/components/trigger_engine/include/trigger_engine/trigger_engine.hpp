#pragma once

#include <cstdint>

#include "esp_err.h"

namespace trigger_engine {

enum class TriggerState {
    kNone,
    kCandidate,
    kTriggered,
    kInvalidated,
};

enum class TriggerSide {
    kNone,
    kLong,
    kShort,
};

/**
 * Actionable reversal trigger (geen NTFY, geen SL/TP, geen alert-lifecycle).
 * State wordt bijgewerkt op gesloten 1m (`EV_1M_CLOSED`), na setup/levels/regime.
 */
struct TriggerSnapshot {
    uint64_t ts_ms;
    bool valid;
    TriggerState state;
    TriggerSide side;
    bool level_touched;
    bool close_back_confirmed;
    double trigger_level_price;
    double close_price;
    uint8_t inherited_quality_score;
    char source_level_name[32];
    char reason[32];
};

esp_err_t init();
bool get_snapshot(TriggerSnapshot *out);

}  // namespace trigger_engine
