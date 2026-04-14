#pragma once

#include "esp_err.h"
#include "service_outbound/types.hpp"

namespace service_outbound {

/**
 * Outbound-plane: queue + poll (M-002c); sinks `ntfy_client` (M-011a), `mqtt_bridge` (M-012a), Kconfig.
 * `emit` is niet-blokkerend (0-tick send).
 */
esp_err_t init();

/** Zet een event in de interne queue; bij volle queue: drop + waarschuwing. */
void emit(Event e);

/** Leegt de queue en verwerkt events (o.a. NTFY bij `ApplicationReady`). Aanroepen vanuit `app_core`-lus. */
void poll();

} // namespace service_outbound
