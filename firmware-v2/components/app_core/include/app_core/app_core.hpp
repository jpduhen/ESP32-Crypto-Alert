#pragma once

#include "esp_err.h"

namespace app_core {

/**
 * Volledige startup + oneindige runtime-lus (skeleton).
 * M-002c: outbound service-events via `service_outbound` (stub; geen transports).
 * Geen return bij succes — alleen bij fatale fout vóór scheduler.
 */
esp_err_t run();

} // namespace app_core
