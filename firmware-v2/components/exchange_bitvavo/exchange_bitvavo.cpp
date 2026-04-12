#include "exchange_bitvavo/exchange_bitvavo.hpp"
#include "exchange_bitvavo/detail/rest_api.hpp"
#include "exchange_bitvavo/detail/ws_api.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "net_runtime/net_runtime.hpp"
#include <cstring>

namespace exchange_bitvavo {

static const char TAG[] = "exchange_bv";

static SemaphoreHandle_t s_mx;
static market_types::MarketSnapshot s_snap;
static char s_symbol[24]{};
static uint64_t s_next_rest_ms{0};
static bool s_ws_started{false};

esp_err_t init(const char *market_symbol)
{
    if (!market_symbol || !market_symbol[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    s_mx = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_mx, ESP_ERR_NO_MEM, TAG, "mutex");

    strncpy(s_symbol, market_symbol, sizeof(s_symbol) - 1);
    s_snap = {};
    strncpy(s_snap.market_label, market_symbol, sizeof(s_snap.market_label) - 1);
    s_snap.connection = market_types::ConnectionState::Disconnected;
    s_next_rest_ms = 0;
    s_ws_started = false;

    ESP_LOGI(DIAG_TAG_MARKET, "exchange_bitvavo init market=%s", s_symbol);
    return ESP_OK;
}

void tick()
{
    const uint64_t now = esp_timer_get_time() / 1000ULL;

    /* M-002: geen WiFi-reconnect hier — alleen gate op IP; STA-backoff leeft in net_runtime. */
    if (!net_runtime::has_ip()) {
        if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_snap.connection = market_types::ConnectionState::Error;
            s_snap.last_error = market_types::FeedErrorCode::NetworkDown;
            strncpy(s_snap.last_error_detail, "geen IP (WiFi?)", sizeof(s_snap.last_error_detail) - 1);
            xSemaphoreGive(s_mx);
        }
        return;
    }

    if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (s_snap.connection == market_types::ConnectionState::Disconnected ||
            s_snap.connection == market_types::ConnectionState::Error) {
            s_snap.connection = market_types::ConnectionState::Connecting;
        }
        xSemaphoreGive(s_mx);
    }

    if (now >= s_next_rest_ms) {
        bool do_rest = true;
        if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(20)) == pdTRUE) {
            /* M-002 / T-103b: dubbele TLS (REST+WS) elke 45s is overbodig als WS al live prijs levert. */
            if (s_ws_started && s_snap.valid &&
                s_snap.connection == market_types::ConnectionState::Connected) {
                do_rest = false;
            }
            xSemaphoreGive(s_mx);
        }
        if (!do_rest) {
            ESP_LOGD(DIAG_TAG_BV_FEED, "REST skip (WS live); volgende venster over 300s");
            s_next_rest_ms = now + 300000ULL;
        } else {
            double p = 0;
            char err[48]{};
            const esp_err_t er = rest::fetch_ticker_price(s_symbol, &p, err, sizeof(err));
            if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (er == ESP_OK) {
                    s_snap.last_tick.price_eur = p;
                    s_snap.last_tick.ts_ms = static_cast<int64_t>(now);
                    s_snap.valid = true;
                    s_snap.last_tick_source = market_types::TickSource::Rest;
                    s_snap.rest_bootstrap_ok++;
                    s_snap.last_error = market_types::FeedErrorCode::None;
                    s_snap.last_error_detail[0] = '\0';
                    if (s_snap.connection != market_types::ConnectionState::Connected) {
                        s_snap.connection = market_types::ConnectionState::Connected;
                    }
                } else {
                    s_snap.last_error = market_types::FeedErrorCode::RestFailure;
                    strncpy(s_snap.last_error_detail, err, sizeof(s_snap.last_error_detail) - 1);
                }
                xSemaphoreGive(s_mx);
            }
            s_next_rest_ms = now + 45000ULL;
        }
    }

    if (!s_ws_started) {
        const esp_err_t wse = ws::start(&s_snap, s_symbol, s_mx);
        s_ws_started = true; /* één poging; herstart later expliciet (M-002 / backoff) */
        if (wse != ESP_OK) {
            ESP_LOGW(TAG, "WS start: %s", esp_err_to_name(wse));
            if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
                s_snap.last_error = market_types::FeedErrorCode::WsFailure;
                strncpy(s_snap.last_error_detail, "ws start", sizeof(s_snap.last_error_detail) - 1);
                xSemaphoreGive(s_mx);
            }
        }
    }
}

market_types::MarketSnapshot snapshot()
{
    market_types::MarketSnapshot o{};
    if (s_mx && xSemaphoreTake(s_mx, pdMS_TO_TICKS(200)) == pdTRUE) {
        o = s_snap;
        xSemaphoreGive(s_mx);
    }
    return o;
}

} // namespace exchange_bitvavo
