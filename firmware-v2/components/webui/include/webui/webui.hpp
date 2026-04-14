#pragma once

#include "esp_err.h"

namespace webui {

/**
 * M-013a/b/c: HTTP-status + POST mqtt/ntfy + minimaal form op / (M-013c); geen auth, geen brede settings.
 * Start `esp_http_server` op geconfigureerde poort wanneer Kconfig aan staat.
 */
esp_err_t init();

} // namespace webui
