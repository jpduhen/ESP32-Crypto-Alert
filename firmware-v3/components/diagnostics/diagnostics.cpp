#include "diagnostics/diagnostics.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

const char *TAG = "diagnostics";

bool s_started = false;

void log_heap() {
    ESP_LOGI(TAG, "heap: free=%zu, min_free=%zu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
}

void log_uptime() {
    const int64_t us = esp_timer_get_time();
    ESP_LOGI(TAG, "uptime: %lld ms", static_cast<long long>(us / 1000));
}

// TODO: queue depth / stack watermarks per bekende task handles registreren.

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

}  // namespace diagnostics
