#pragma once

#include <cstdint>

#include "esp_err.h"

namespace diagnostics {

esp_err_t init();
esp_err_t start();

/** Eénmalige snapshot (heap/uptime); voor ad-hoc logging. */
void log_health_snapshot(const char *reason);

/** Compacte statusregel (heap); tag DIAG — o.a. bij WiFi-state changes. */
void log_compact_status(const char *context);

/**
 * Market WebSocket transport (DIAG): state, counters, idle sinds laatste rx (ms).
 * Tag DIAG — compacte health-regel (o.a. bij WS state change).
 */
void log_mws_transport(const char *ws_state, uint64_t rx_total, uint64_t data_events, uint64_t reconnect_cycles,
                       uint32_t error_count, uint32_t last_payload_len, uint32_t idle_since_rx_ms);

}  // namespace diagnostics
