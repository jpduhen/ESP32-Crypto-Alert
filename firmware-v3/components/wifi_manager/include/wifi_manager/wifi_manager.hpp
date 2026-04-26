#pragma once

#include "esp_err.h"

namespace wifi_manager {

enum class WifiState {
    kUninitialized,
    kIdle,
    kStarting,
    kConnecting,
    kConnected,
    kGotIp,
    kDisconnected,
    kError,
    kNotConfigured,
};

esp_err_t init();
esp_err_t start();

WifiState get_state();

/** Leesbare naam voor logging (Engels, stabiel). */
const char *state_label(WifiState s);

}  // namespace wifi_manager
