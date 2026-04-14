#pragma once

#include "esp_err.h"
#include "service_outbound/types.hpp"

namespace service_outbound {

/**
 * Minimale outbound-plane: FreeRTOS-queue + poll-drain naar stub-sink (M-002c).
 * Geen MQTT/NTFY/WebUI-transports; `emit` is niet-blokkerend (0-tick send).
 */
esp_err_t init();

/** Zet een event in de interne queue; bij volle queue: drop + waarschuwing. */
void emit(Event e);

/** Leegt de queue en verwerkt elk event via de interne dispatcher (stub). Aanroepen vanuit `app_core`-lus — geen extra worker-task. */
void poll();

} // namespace service_outbound
