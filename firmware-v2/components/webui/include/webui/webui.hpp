#pragma once

#include "esp_err.h"

namespace webui {

/**
 * M-013a: read-only HTTP-status (geen POST, geen auth, geen settings).
 * Start `esp_http_server` op geconfigureerde poort wanneer Kconfig aan staat.
 */
esp_err_t init();

} // namespace webui
