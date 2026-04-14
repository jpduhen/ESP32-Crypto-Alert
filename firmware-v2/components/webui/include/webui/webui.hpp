#pragma once

#include "esp_err.h"

namespace webui {

/**
 * M-013a/b: HTTP-status + beperkte POST voor mqtt/ntfy (M-013b); geen auth, geen brede settings.
 * Start `esp_http_server` op geconfigureerde poort wanneer Kconfig aan staat.
 */
esp_err_t init();

} // namespace webui
