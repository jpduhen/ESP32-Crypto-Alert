#pragma once

#include "esp_err.h"

namespace alert_engine {

/**
 * Alert lifecycle (single writer): dedupe, cooldown, dispatch naar ntfy_client.
 * Geen marktdata-parser hier.
 */
esp_err_t init();
esp_err_t start();

}  // namespace alert_engine
