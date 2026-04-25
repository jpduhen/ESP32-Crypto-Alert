#pragma once

#include "esp_err.h"

namespace diagnostics {

esp_err_t init();
esp_err_t start();

/** Eénmalige snapshot (heap/uptime); voor ad-hoc logging. */
void log_health_snapshot(const char *reason);

}  // namespace diagnostics
