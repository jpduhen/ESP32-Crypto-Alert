#pragma once

#include <cstdint>

#include "esp_err.h"

namespace ui_model {

struct UiStatusModel {
    uint64_t ts_ms;
    double last_price;
    bool wifi_ready;
    bool ws_live;
    char wifi_state[16];
    char ws_state[20];
    uint64_t ws_last_rx_age_ms;
    bool ret_1m_valid;
    bool ret_5m_valid;
    bool ret_30m_valid;
    double ret_1m_pct;
    double ret_5m_pct;
    double ret_30m_pct;
    char regime[16];
    bool support_1_valid;
    bool resistance_1_valid;
    double support_1;
    double resistance_1;
    char setup_state[24];
    char trigger_state[24];
    char alert_state[24];
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint64_t uptime_s;
    char version[16];
};

esp_err_t init();
bool get_status_model(UiStatusModel *out);

}  // namespace ui_model
