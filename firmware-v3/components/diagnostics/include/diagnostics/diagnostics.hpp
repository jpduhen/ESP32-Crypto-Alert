#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace diagnostics {

/** Compacte soak/health-snapshot voor periodieke logging en export. */
struct SoakHealthSnapshot {
    uint64_t uptime_s;
    uint64_t free_heap;
    uint64_t min_free_heap;
    uint64_t ws_rx_count;
    uint64_t ws_reconnect_count;
    uint32_t ws_error_count;
    uint32_t ws_last_rx_age_ms;
    const char *ws_state_label;
    size_t closed_1m_count;
    bool analytics_valid_1m;
    bool analytics_valid_5m;
    bool analytics_valid_30m;
    bool regime_valid;
    bool levels_valid;
    bool setup_valid;
    bool trigger_valid;
    bool alert_valid;
    const char *alert_state_label;
    uint64_t market_parse_ok;
    uint64_t market_parse_fail;
};

/** Runtime-tellers (sinds boot); voor soak-evaluatie. Alleen gewijzigd door interne 1s-poll. */
struct SoakRuntimeCounters {
    uint32_t regime_changes;
    uint32_t setup_candidate_entries;
    uint32_t setup_hq_entries;
    uint32_t triggers;
    uint32_t invalidations;
    uint32_t alerts_candidate;
    uint32_t alerts_triggered;
    uint32_t alerts_invalidated;
    uint32_t alerts_resolved;
};

esp_err_t init();
esp_err_t start();

/** Eénmalige snapshot (heap/uptime); voor ad-hoc logging. */
void log_health_snapshot(const char *reason);

/** @deprecated Gebruik SOAK-poll; behouden voor oude call sites. */
void log_compact_status(const char *context);

/** @deprecated WS-transport wordt via SOAK gelogd. */
void log_mws_transport(const char *ws_state, uint64_t rx_total, uint64_t data_events, uint64_t reconnect_cycles,
                       uint32_t error_count, uint32_t last_payload_len, uint32_t idle_since_rx_ms);

/** Vult @p out met actuele waarden (kan vanuit elke context). */
void fill_soak_health_snapshot(SoakHealthSnapshot *out);

void get_soak_runtime_counters(SoakRuntimeCounters *out);

/** Compacte formatter voor uniforme regels (null-terminatie indien buf_sz > 0). */
size_t format_ws_health(char *buf, size_t buf_sz);
size_t format_analytics_health(char *buf, size_t buf_sz);
size_t format_setup_health(char *buf, size_t buf_sz);

}  // namespace diagnostics
