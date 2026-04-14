#pragma once

#include "esp_err.h"

namespace webui {

/**
 * M-013a/b/c + M-014a: HTTP-status, POST services, form, POST /api/ota; geen auth, geen brede settings.
 * Start `esp_http_server` op geconfigureerde poort wanneer Kconfig aan staat.
 */
esp_err_t init();

} // namespace webui
