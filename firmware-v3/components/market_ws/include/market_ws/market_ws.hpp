#pragma once

#include <cstdint>

#include "esp_err.h"

namespace market_ws {

/** Levenscyclus WebSocket transport (Bitvavo wss); geen business-/strategielogica. */
enum class MarketWsState {
    kUninitialized,
    kIdle,
    kStarting,
    kConnecting,
    kConnected,
    kSubscribed,
    kLive,
    kDisconnected,
    kReconnectBackoff,
    kStopping,
    kError,
};

esp_err_t init();
esp_err_t start();
esp_err_t stop();

/** True zodra er minstens één data-frame is ontvangen na subscribe (transport “live”). */
bool is_live();

MarketWsState get_state();

/** Huidige state als vaste string (Engels). */
const char *state_to_string();

const char *state_to_string(MarketWsState s);

/** Transportstatistieken (64-bit counters; thread-safe snapshot). */
uint64_t rx_count();
uint64_t data_events_count();
uint64_t reconnect_count();
uint32_t error_count();
/** Lengte van de laatste text-data payload (bytes), 0 als nog geen data. */
uint32_t last_payload_len();

}  // namespace market_ws
