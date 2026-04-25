#pragma once

#include "esp_err.h"

namespace wifi_manager {

enum class phase {
    uninitialized,
    initialized,
    idle_no_credentials,
    connecting,
    connected,
    disconnected,
    error,
};

esp_err_t init();
esp_err_t start();

phase get_phase();

}  // namespace wifi_manager
