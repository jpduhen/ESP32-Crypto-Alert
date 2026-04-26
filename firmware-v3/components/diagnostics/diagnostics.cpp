#include "diagnostics/diagnostics.hpp"

#include <cinttypes>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

const char *TAG = "DIAG";

bool s_started = false;

void log_heap() {
    ESP_LOGI(TAG, "heap: free=%zu, min_free=%zu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
}

void log_uptime() {
    const int64_t us = esp_timer_get_time();
    ESP_LOGI(TAG, "uptime: %lld ms", static_cast<long long>(us / 1000));
}

}  // namespace

namespace diagnostics {

esp_err_t init() {
    ESP_LOGI(TAG, "init");
    log_heap();
    return ESP_OK;
}

esp_err_t start() {
    if (s_started) {
        return ESP_OK;
    }
    s_started = true;
    log_uptime();
    log_heap();
    ESP_LOGI(TAG, "start: periodieke health-TODO (timer) — nu alleen boot-snapshot");
    return ESP_OK;
}

void log_health_snapshot(const char *reason) {
    ESP_LOGI(TAG, "health snapshot (%s)", reason ? reason : "?");
    log_uptime();
    log_heap();
    ESP_LOGI(TAG, "TODO: stack high water marks, queue stats");
}

void log_compact_status(const char *context) {
    ESP_LOGI(TAG, "[%s] heap_free=%zu min_free=%zu", context ? context : "?",
             static_cast<size_t>(esp_get_free_heap_size()),
             static_cast<size_t>(esp_get_minimum_free_heap_size()));
}

void log_mws_transport(const char *ws_state, uint64_t rx_total, uint64_t data_events, uint64_t reconnect_cycles,
                       uint32_t error_count, uint32_t last_payload_len, uint32_t idle_since_rx_ms) {
    ESP_LOGI(
        TAG,
        "mws state=%s rx=%" PRIu64 " data_ev=%" PRIu64 " reconnect=%" PRIu64 " err=%" PRIu32 " last_len=%" PRIu32
        " idle_rx_ms=%" PRIu32 " heap=%zu",
        ws_state ? ws_state : "?", rx_total, data_events, reconnect_cycles, error_count, last_payload_len,
        idle_since_rx_ms, static_cast<size_t>(esp_get_free_heap_size()));
}

}  // namespace diagnostics
