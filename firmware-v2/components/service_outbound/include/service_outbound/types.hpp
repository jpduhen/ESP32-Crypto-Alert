#pragma once

#include <cstdint>

namespace service_outbound {

/**
 * Neutrale outbound-signalen (M-002c) — geen MQTT/NTFY/WebUI-protocoltypes.
 * Producer → interne FreeRTOS-queue → `poll`/`dispatch`; payloads later per event-type.
 */
enum class Event : uint8_t {
    None = 0,
    /** Eénmalige haak na succesvolle app-start (demo/stub — geen netwerk). */
    ApplicationReady,
    /** M-010a / M-011b: 1m %-move drempel — alleen via `emit_domain_alert_1m` (met payload). */
    DomainAlert1mMove,
    /** M-010c: 5m %-move drempel — alleen via `emit_domain_alert_5m` (met payload). */
    DomainAlert5mMove,
    /** M-010d: 1m+5m confluence — alleen via `emit_domain_confluence_1m5m` (met payload). */
    DomainAlertConfluence1m5m,
};

/** M-011b: compacte payload voor NTFY — vullen in `alert_engine`, alleen transport in `ntfy_client`. */
struct DomainAlert1mMovePayload {
    char symbol[24]{};
    bool up{true};
    double price_eur{0.0};
    double pct_1m{0.0};
    int64_t ts_ms{0};
};

/** M-010c: zelfde vorm als 1m, aparte event-route (pct_5m). */
struct DomainAlert5mMovePayload {
    char symbol[24]{};
    bool up{true};
    double price_eur{0.0};
    double pct_5m{0.0};
    int64_t ts_ms{0};
};

/** M-010d: beide timeframes boven drempel + zelfde richting. */
struct DomainConfluence1m5mPayload {
    char symbol[24]{};
    bool up{true};
    double price_eur{0.0};
    double pct_1m{0.0};
    double pct_5m{0.0};
    int64_t ts_ms{0};
};

/** Queue-element: één kind + optionele domeinpayload. */
struct OutboundEnvelope {
    Event kind{Event::None};
    DomainAlert1mMovePayload domain_1m{};
    DomainAlert5mMovePayload domain_5m{};
    DomainConfluence1m5mPayload domain_conf_1m5m{};
};

} // namespace service_outbound
