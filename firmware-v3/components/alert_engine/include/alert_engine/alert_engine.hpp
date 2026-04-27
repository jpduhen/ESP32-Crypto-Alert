#pragma once

#include <cstdint>

#include "esp_err.h"

namespace alert_engine {

/**
 * Light alert lifecycle (log-only): state + counters, geen extern transport.
 */
enum class AlertLifecycleState {
    kNone,
    kWatch,
    kCandidate,
    kTriggered,
    kInvalidated,
    kResolved,
};

struct AlertSnapshot {
    uint64_t ts_ms;
    bool valid;
    AlertLifecycleState state;
    char source[32];
    char side[16];
    uint8_t quality_score;
    char level_name[32];
    double ref_price;
    uint64_t state_age_ms;
};

struct AlertCounters {
    uint32_t candidate_count;
    uint32_t triggered_count;
    uint32_t invalidated_count;
    uint32_t resolved_count;
};

esp_err_t init();
esp_err_t start();

bool get_snapshot(AlertSnapshot *out);
void get_counters(AlertCounters *out);
const char *state_label(AlertLifecycleState s);

}  // namespace alert_engine
