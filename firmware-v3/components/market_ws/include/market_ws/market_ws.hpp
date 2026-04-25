#pragma once

#include "esp_err.h"

namespace market_ws {

/** Levenscyclus markt-websocket (later: esp_websocket_client + Bitvavo wss). */
enum class connection_state {
    stopped,
    idle,
    connecting,
    connected,
    reconnecting,
    error,
};

esp_err_t init();
esp_err_t start();
void stop();

connection_state get_state();

}  // namespace market_ws
