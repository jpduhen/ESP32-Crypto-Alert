#pragma once

#include "esp_err.h"
#include "service_outbound/types.hpp"

namespace service_outbound {

/**
 * Minimale dispatcher voor toekomstige outbound services (MQTT/NTFY/WebUI).
 * Huidige implementatie: stub — geen transports, geen queues (M-002c).
 */
esp_err_t init();

/** Stuurt een event naar de geregistreerde sink(s); default is no-op + beperkte diag. */
void emit(Event e);

} // namespace service_outbound
