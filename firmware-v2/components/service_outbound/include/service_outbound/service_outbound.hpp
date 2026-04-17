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

/** M-011b: domein 1m-alert met payload — enige toegestane route voor `Event::DomainAlert1mMove`. */
void emit_domain_alert_1m(const DomainAlert1mMovePayload &p);

/** M-010c: domein 5m-alert met payload — enige toegestane route voor `Event::DomainAlert5mMove`. */
void emit_domain_alert_5m(const DomainAlert5mMovePayload &p);

/** M-010d: confluence 1m+5m — enige route voor `Event::DomainAlertConfluence1m5m`. */
void emit_domain_confluence_1m5m(const DomainConfluence1m5mPayload &p);

/** Leegt de queue en verwerkt events (o.a. NTFY bij `ApplicationReady`). Aanroepen vanuit `app_core`-lus. */
void poll();

} // namespace service_outbound
