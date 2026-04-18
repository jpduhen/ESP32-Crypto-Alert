#pragma once

#include <cstdint>

/**
 * Gedeelde domeintypes voor marktdata — gebruikt door `market_data` en `exchange_bitvavo`
 * zonder onderlinge CMake-circulariteit.
 */
namespace market_types {

enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
    Error,
};

/** Fouten in de exchange-/feedlaag (niet TLS-stackcodes; detail in last_error_detail). */
enum class FeedErrorCode : uint8_t {
    None = 0,
    NotConfigured,
    NetworkDown,
    RestFailure,
    RestParse,
    WsFailure,
    WsParse,
};

/** Welk pad de laatste `last_tick` heeft gezet (T-103 field-test / diagnostiek). */
enum class TickSource : uint8_t {
    None = 0,
    Rest,
    Ws,
};

struct PriceTick {
    double price_eur{0.0};
    int64_t ts_ms{0};
};

struct MarketSnapshot {
    ConnectionState connection{ConnectionState::Disconnected};
    PriceTick last_tick{};
    bool valid{false};
    FeedErrorCode last_error{FeedErrorCode::None};
    uint32_t rest_bootstrap_ok{0};
    uint32_t ws_reconnect_count{0};
    char last_error_detail[48]{};
    /** Marktlabel zoals aan exchange doorgegeven (bv. BTC-EUR). */
    char market_label[24]{};
    TickSource last_tick_source{TickSource::None};
    /** Aantal binnenkomende WS-ticker-updates in de **afgelopen voltooide** wandklok-seconde (0 = geen of nog niet gesynchroniseerd). */
    uint32_t ws_inbound_ticks_last_sec{0};
};

} // namespace market_types
