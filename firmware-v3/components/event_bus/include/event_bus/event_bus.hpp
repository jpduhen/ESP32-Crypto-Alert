#pragma once

#include "esp_err.h"
#include "esp_event.h"

/** Eigen event base voor domein-events (los van WiFi/IP events). */
ESP_EVENT_DECLARE_BASE(CRYPTO_V3_EVENTS);

namespace event_bus {

enum : int32_t {
    EV_DUMMY = 0,  // placeholder; echte codes volgen per subsystem
};

esp_err_t init();

}  // namespace event_bus
