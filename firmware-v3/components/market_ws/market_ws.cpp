#include "market_ws/market_ws.hpp"

#include "esp_log.h"

namespace {

const char *TAG = "market_ws";

struct market_ws_state {
    market_ws::connection_state state{market_ws::connection_state::stopped};
    bool inited = false;
};

market_ws_state s_ws;

void set_state(market_ws::connection_state s) {
    s_ws.state = s;
    switch (s) {
        case market_ws::connection_state::stopped:
            ESP_LOGI(TAG, "WS: stopped");
            break;
        case market_ws::connection_state::idle:
            ESP_LOGI(TAG, "WS: idle");
            break;
        case market_ws::connection_state::connecting:
            ESP_LOGI(TAG, "WS: connecting (stub)");
            break;
        case market_ws::connection_state::connected:
            ESP_LOGI(TAG, "WS: connected (stub)");
            break;
        case market_ws::connection_state::reconnecting:
            ESP_LOGI(TAG, "WS: reconnecting (stub)");
            break;
        case market_ws::connection_state::error:
            ESP_LOGW(TAG, "WS: error (stub)");
            break;
    }
}

}  // namespace

namespace market_ws {

esp_err_t init() {
    if (s_ws.inited) {
        return ESP_OK;
    }
    s_ws.inited = true;
    ESP_LOGI(TAG, "init: voorbereid op esp_websocket_client (nog geen URI/sessie)");
    // TODO: config (URI, subscriptions); TLS gebruikt certificate bundle (sdkconfig).
    set_state(connection_state::idle);
    return ESP_OK;
}

esp_err_t start() {
    ESP_LOGW(TAG, "start: stub — geen echte wss tot WiFi + endpoint-config");
    set_state(connection_state::idle);
    return ESP_OK;
}

void stop() { set_state(connection_state::stopped); }

connection_state get_state() { return s_ws.state; }

}  // namespace market_ws
