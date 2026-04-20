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
    /**
     * RWS-01: ruwe Bitvavo WS TEXT-frames (opcode text) in de **afgelopen voltooide** wandklok-seconde —
     * vóór parse; kan hoger zijn dan canonical als er non-ticker of parse-fail is.
     */
    uint32_t ws_raw_msgs_last_sec{0};
    /**
     * Aantal **geslaagde ticker-parses** → officiële prijs-update (`apply_price`) in de **afgelopen voltooide**
     * wandklok-seconde (= canonical pad voor 1 Hz-analyse); zelfde als historische `ws_inbound_ticks_last_sec`-semantiek.
     */
    uint32_t ws_inbound_ticks_last_sec{0};
    /** RWS-01: seconden sinds laatste ruwe WS TEXT-frame (0 = nog geen frame sinds boot). */
    uint32_t ws_gap_sec_since_last_raw{0};
    /** RWS-01: seconden sinds laatste geslaagde canonical tick (parse→prijs). */
    uint32_t ws_gap_sec_since_last_canonical{0};
    /**
     * RWS-01: vast label huidige officiële seconde-prijsketen (geen runtime-switch in RWS-01).
     * Waarde `"bitvavo_ticker_ws_v1"`.
     */
    char ws_official_price_stream[28]{};

    /** RWS-02: geparste trade-events in de **afgelopen voltooide** wandklok-seconde (parallel; niet de officiële prijs). */
    uint32_t ws_trade_events_last_sec{0};
    /** RWS-02: cumulatief aantal geparste trade-events sinds boot (WebSocket-pad). */
    uint32_t ws_trades_total_since_boot{0};
    /** RWS-02: capaciteit van de interne trade-ring (vast). */
    uint16_t ws_trade_ring_capacity{0};
    /** RWS-02: huidige bezetting van de trade-ring. */
    uint16_t ws_trade_ring_occupancy{0};
    /** RWS-02: trade-ring vol → oudste vervangen (backpressure-teller). */
    uint32_t ws_trade_ring_drop_total{0};
    /** RWS-02: seconden sinds laatste geparste trade (0 = nog geen trade sinds boot). */
    uint32_t ws_gap_sec_since_last_trade{0};
    /** RWS-02: lokale monotoon tijdstip laatste trade (ms, esp_timer_get_time/1000); 0 = nog geen trade. */
    int64_t ws_last_trade_local_ms{0};

    /**
     * RWS-03: laatste **voltooide** wandklok-seconde — aggregate uit **trade**-events (parallel; niet de officiële ticker-prijs).
     * `wall_sec` = monotoon seconde-id (zelfde basis als interne WS-stats).
     */
    uint64_t ws_second_agg_wall_sec{0};
    bool ws_second_agg_has_trades{false};
    uint32_t ws_second_agg_trade_count{0};
    double ws_second_agg_first_eur{0.0};
    double ws_second_agg_last_eur{0.0};
    double ws_second_agg_min_eur{0.0};
    double ws_second_agg_max_eur{0.0};
    /** Arithmetic mean trade price wanneer `has_trades`; anders 0. */
    double ws_second_agg_mean_eur{0.0};
    /** Minstens één canonical ticker-tick in die seconde (aanvullende context). */
    bool ws_second_agg_ticker_seen{false};
    uint32_t ws_second_agg_canonical_ticks{0};
    uint16_t ws_second_ring_capacity{0};
    uint16_t ws_second_ring_used{0};
    uint32_t ws_second_ring_writes_total{0};

    /**
     * RWS-05: parallel **ticker24h**-kanaal (heartbeat/bronzichtbaarheid) — **niet** de officiële prijsbron;
     * `last_tick` / `apply_price` blijft **ticker** (`bitvavo_ticker_ws_v1`).
     */
    int64_t ws_last_ticker24h_local_ms{0};
    uint32_t ws_ticker24h_msgs_total{0};
    /** RWS-05: events in de **afgelopen voltooide** wandklok-seconde (zoals andere WS-tellers). */
    uint32_t ws_ticker24h_events_last_sec{0};
    uint32_t ws_gap_sec_since_last_ticker24h{0};
    bool ws_ticker24h_seen_recently{false};
    /** Laatst geparste `"last"` uit ticker24h (alleen observability; niet `last_tick`). */
    double ws_ticker24h_last_eur{0.0};
    /**
     * RWS-05: feed-health / heartbeat-redencode — o.a. `ok`, `canonical_gap_24h_alive`, `feed_stale`
     * (canonical-gap ≥12 s logica; geen alert-policy).
     */
    char ws_heartbeat_reason_code[24]{};
    /**
     * RWS-05: leesbare bron/heartbeat — o.a. `canonical_ticker_ws`, `ticker24h_secondary_heartbeat`, `stale`.
     */
    char ws_heartbeat_source_visibility[40]{};
};

/**
 * RWS-02: één vastgelegde trade uit WS (parallel capture; geen doorrekening naar `last_tick` / alerts in deze fase).
 */
struct WsRawTradeSample {
    double price_eur{0.0};
    int64_t ts_local_ms{0};
    int64_t ts_exchange_ms{0};
};

} // namespace market_types
