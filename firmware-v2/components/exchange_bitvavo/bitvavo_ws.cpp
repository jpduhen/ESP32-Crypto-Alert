/**
 * WebSocket live feed — wss://ws.bitvavo.com/v2/ + ticker subscribe (TLS bundle).
 */
#include "diagnostics/diagnostics.hpp"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "market_types/types.hpp"
#include <cstdlib>
#include <cstring>

namespace exchange_bitvavo::ws {

static const char TAG[] = "bv_ws";

static esp_websocket_client_handle_t s_client;
static SemaphoreHandle_t s_metrics_mx;
static char s_market[24]{};
static market_types::MarketSnapshot *s_snap_ptr; // owned by exchange_bitvavo.cpp

/** Wandklok-seconde voor teller `ws_inbound_ticks_last_sec` (voltooide vorige seconde). */
static uint64_t s_stats_wall_sec{0};
static uint32_t s_stats_cur_sec_count{0};

static void commit_ticks_last_to_snap(uint32_t n)
{
    if (!s_metrics_mx || !s_snap_ptr) {
        return;
    }
    if (xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_snap_ptr->ws_inbound_ticks_last_sec = n;
        xSemaphoreGive(s_metrics_mx);
    }
}

void sync_inbound_tick_stats()
{
    if (!s_snap_ptr) {
        return;
    }
    const uint64_t w = esp_timer_get_time() / 1000000ULL;
    if (s_stats_wall_sec == 0ULL) {
        s_stats_wall_sec = w;
        return;
    }
    while (w > s_stats_wall_sec) {
        commit_ticks_last_to_snap(s_stats_cur_sec_count);
        s_stats_cur_sec_count = 0;
        ++s_stats_wall_sec;
    }
}

static void apply_price(double p, int64_t ts_ms)
{
    sync_inbound_tick_stats();
    if (!s_metrics_mx || !s_snap_ptr) {
        return;
    }
    if (xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    ++s_stats_cur_sec_count;
    s_snap_ptr->last_tick.price_eur = p;
    s_snap_ptr->last_tick.ts_ms = ts_ms;
    s_snap_ptr->valid = true;
    s_snap_ptr->last_tick_source = market_types::TickSource::Ws;
    s_snap_ptr->connection = market_types::ConnectionState::Connected;
    s_snap_ptr->last_error = market_types::FeedErrorCode::None;
    xSemaphoreGive(s_metrics_mx);
}

static bool parse_ticker_text(const char *buf, size_t len, double *out)
{
    if (!buf || len == 0 || !out) {
        return false;
    }
    char tmp[384];
    size_t n = len < sizeof(tmp) - 1 ? len : sizeof(tmp) - 1;
    memcpy(tmp, buf, n);
    tmp[n] = '\0';

    const char *p = strstr(tmp, "\"lastPrice\"");
    if (!p) {
        p = strstr(tmp, "\"price\"");
    }
    if (!p) {
        return false;
    }
    p = strchr(p, ':');
    if (!p) {
        return false;
    }
    ++p;
    while (*p == ' ' || *p == '\"') {
        ++p;
    }
    char *end = nullptr;
    double v = strtod(p, &end);
    if (end == p) {
        return false;
    }
    *out = v;
    return true;
}

static void send_subscribe(esp_websocket_client_handle_t h)
{
    char payload[224];
    const int n = snprintf(payload, sizeof(payload),
                           "{\"action\":\"subscribe\",\"channels\":[{\"name\":\"ticker\",\"markets\":[\"%s\"]}]}",
                           s_market);
    if (n <= 0 || n >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "subscribe buffer overflow");
        return;
    }
    if (esp_websocket_client_send_text(h, payload, static_cast<size_t>(n), pdMS_TO_TICKS(4000)) < 0) {
        ESP_LOGW(TAG, "send_text subscribe failed");
    } else {
        ESP_LOGI(DIAG_TAG_MARKET, "WS subscribe ticker %s", s_market);
    }
}

static void on_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    auto *data = static_cast<esp_websocket_event_data_t *>(event_data);

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(DIAG_TAG_BV_FEED, "WS event=CONNECTED (TLS ok, subscribe volgt)");
        ESP_LOGI(DIAG_TAG_MARKET, "WS connected");
        send_subscribe(s_client);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(DIAG_TAG_BV_FEED, "WS event=DISCONNECTED (elke reconnect = nieuwe TLS-handshake)");
        ESP_LOGW(DIAG_TAG_MARKET, "WS disconnected");
        if (s_metrics_mx && s_snap_ptr && xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_snap_ptr->ws_reconnect_count++;
            if (s_snap_ptr->connection == market_types::ConnectionState::Connected) {
                s_snap_ptr->connection = market_types::ConnectionState::Connecting;
            }
            xSemaphoreGive(s_metrics_mx);
        }
        break;
    case WEBSOCKET_EVENT_DATA: {
        const int opcode = data->op_code;
        if (opcode == 1 && data->data_ptr != nullptr && data->data_len > 0) {
            double p = 0;
            if (parse_ticker_text(data->data_ptr, static_cast<size_t>(data->data_len), &p)) {
                const int64_t ts = static_cast<int64_t>(esp_timer_get_time() / 1000);
                apply_price(p, ts);
            }
        }
        break;
    }
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(DIAG_TAG_MARKET, "WS error");
        if (s_metrics_mx && s_snap_ptr && xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_snap_ptr->last_error = market_types::FeedErrorCode::WsFailure;
            strncpy(s_snap_ptr->last_error_detail, "ws error", sizeof(s_snap_ptr->last_error_detail) - 1);
            xSemaphoreGive(s_metrics_mx);
        }
        break;
    default:
        break;
    }
}

esp_err_t start(market_types::MarketSnapshot *snap_sink, const char *market, SemaphoreHandle_t metrics_mx)
{
    if (s_client != nullptr) {
        return ESP_OK;
    }
    s_stats_wall_sec = 0;
    s_stats_cur_sec_count = 0;
    s_snap_ptr = snap_sink;
    s_metrics_mx = metrics_mx;
    strncpy(s_market, market, sizeof(s_market) - 1);

    esp_websocket_client_config_t wcfg{};
    wcfg.uri = "wss://ws.bitvavo.com/v2/";
    wcfg.crt_bundle_attach = esp_crt_bundle_attach;
    wcfg.network_timeout_ms = 15000;
    /* Iets langere pauze tussen reconnect-pogingen → minder TLS-handshakes bij wisselende WiFi */
    wcfg.reconnect_timeout_ms = 10000;
    wcfg.disable_auto_reconnect = false;

    s_client = esp_websocket_client_init(&wcfg);
    ESP_RETURN_ON_FALSE(s_client, ESP_ERR_NO_MEM, TAG, "ws init");

    ESP_RETURN_ON_ERROR(esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, on_event, nullptr),
                        TAG, "ws events");

    return esp_websocket_client_start(s_client);
}

void stop()
{
    if (s_client) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = nullptr;
    }
    s_snap_ptr = nullptr;
    s_metrics_mx = nullptr;
}

} // namespace exchange_bitvavo::ws
