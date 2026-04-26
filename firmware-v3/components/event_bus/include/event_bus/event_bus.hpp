#pragma once

#include "esp_err.h"
#include "esp_event.h"

/** Eigen event base voor domein-events (los van WiFi/IP events). */
ESP_EVENT_DECLARE_BASE(CRYPTO_V3_EVENTS);

namespace event_bus {

enum : int32_t {
    EV_DUMMY = 0,
    /** Payload: `market_store::TickEvent` (wordt gekopieerd door esp_event). */
    EV_MARKET_TICK = 1,
    /** Geen payload: regime_engine refresht vanuit `candle_engine::get_analytics_snapshot()`. */
    EV_1M_CLOSED = 2,
};

esp_err_t init();

}  // namespace event_bus
