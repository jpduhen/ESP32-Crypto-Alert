/**
 * WebSocket feed — wss://ws.bitvavo.com/v2/ + parallel **ticker** + **trades** subscribe (RWS-02; TLS bundle).
 * Officiële prijs blijft ticker→`apply_price`; trades → bounded ring + observability.
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
#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace exchange_bitvavo::ws {

static const char TAG[] = "bv_ws";

static esp_websocket_client_handle_t s_client;
static SemaphoreHandle_t s_metrics_mx;
static char s_market[24]{};
static market_types::MarketSnapshot *s_snap_ptr; // owned by exchange_bitvavo.cpp

/** Wandklok-seconde voor tellers (voltooide vorige seconde). */
static uint64_t s_stats_wall_sec{0};
static uint32_t s_stats_cur_sec_count{0};
/** RWS-02: trade-events (geparst + in ring geplaatst) in de lopende seconde. */
static uint32_t s_trade_cur_sec_count{0};
/** RWS-01: alle WS TEXT-frames (vóór parse). */
static uint32_t s_raw_cur_sec_count{0};
static uint64_t s_last_raw_wall_sec{0};
static uint64_t s_last_canonical_wall_sec{0};
static uint64_t s_last_trade_wall_sec{0};
static bool s_gap_canonical_warn_latched{false};
static bool s_gap_trade_warn_latched{false};

/** RWS-02: bounded ring (bij vol → oudste overschrijven; `s_trade_count` = cap). */
static constexpr size_t k_trade_ring_cap = 64;
static market_types::WsRawTradeSample s_trade_ring[k_trade_ring_cap];
static size_t s_trade_widx{0};
static size_t s_trade_count{0};

static void commit_ticks_last_to_snap(uint32_t n_canonical, uint32_t n_raw, uint32_t n_trade)
{
    if (!s_metrics_mx || !s_snap_ptr) {
        return;
    }
    if (xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_snap_ptr->ws_inbound_ticks_last_sec = n_canonical;
        s_snap_ptr->ws_raw_msgs_last_sec = n_raw;
        s_snap_ptr->ws_trade_events_last_sec = n_trade;
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
        commit_ticks_last_to_snap(s_stats_cur_sec_count, s_raw_cur_sec_count, s_trade_cur_sec_count);
        s_stats_cur_sec_count = 0;
        s_raw_cur_sec_count = 0;
        s_trade_cur_sec_count = 0;
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
    s_last_canonical_wall_sec = esp_timer_get_time() / 1000000ULL;
    ESP_LOGD(TAG, "[WS_AGG] price=%.6f ts_ms=%lld", p, (long long)ts_ms);
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

    /* RWS-02: trade-kanaal gebruikt ook `"price"` — nooit als ticker-canonical prijs tellen. */
    if (strstr(tmp, "\"event\":\"trade\"") != nullptr || strstr(tmp, "\"event\": \"trade\"") != nullptr) {
        return false;
    }

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

static bool parse_trade_text(const char *buf, size_t len, double *price_out, int64_t *ts_exch_out)
{
    if (!buf || len == 0 || !price_out || !ts_exch_out) {
        return false;
    }
    char tmp[384];
    size_t n = len < sizeof(tmp) - 1 ? len : sizeof(tmp) - 1;
    memcpy(tmp, buf, n);
    tmp[n] = '\0';
    if (strstr(tmp, "\"event\":\"trade\"") == nullptr && strstr(tmp, "\"event\": \"trade\"") == nullptr) {
        return false;
    }
    *ts_exch_out = 0;
    const char *tsp = strstr(tmp, "\"timestamp\":");
    if (tsp != nullptr) {
        tsp = strchr(tsp + 12, ':');
        if (tsp != nullptr) {
            ++tsp;
            while (*tsp == ' ' || *tsp == '\"') {
                ++tsp;
            }
            char *end_ts = nullptr;
            const long long tv = strtoll(tsp, &end_ts, 10);
            if (end_ts != tsp) {
                *ts_exch_out = static_cast<int64_t>(tv);
            }
        }
    }
    const char *pp = strstr(tmp, "\"price\":");
    if (pp == nullptr) {
        pp = strstr(tmp, "\"price\" :");
    }
    if (pp == nullptr) {
        return false;
    }
    pp = strchr(pp + 7, ':');
    if (pp == nullptr) {
        return false;
    }
    ++pp;
    while (*pp == ' ' || *pp == '\"') {
        ++pp;
    }
    char *end = nullptr;
    const double v = strtod(pp, &end);
    if (end == pp) {
        return false;
    }
    *price_out = v;
    return true;
}

static void trade_ring_push(const market_types::WsRawTradeSample &s, uint32_t *evict_out)
{
    s_trade_ring[s_trade_widx] = s;
    s_trade_widx = (s_trade_widx + 1) % k_trade_ring_cap;
    if (s_trade_count < k_trade_ring_cap) {
        ++s_trade_count;
    } else {
        if (evict_out != nullptr) {
            ++(*evict_out);
        }
        ESP_LOGD(TAG, "[WS_TRD_DROP] ring full, oldest overwritten (cap=%u)", static_cast<unsigned>(k_trade_ring_cap));
    }
}

static void send_subscribe(esp_websocket_client_handle_t h)
{
    char payload[320];
    const int n =
        snprintf(payload,
                 sizeof(payload),
                 "{\"action\":\"subscribe\",\"channels\":["
                 "{\"name\":\"ticker\",\"markets\":[\"%s\"]},"
                 "{\"name\":\"trades\",\"markets\":[\"%s\"]}]}",
                 s_market,
                 s_market);
    if (n <= 0 || n >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "subscribe buffer overflow");
        return;
    }
    if (esp_websocket_client_send_text(h, payload, static_cast<size_t>(n), pdMS_TO_TICKS(4000)) < 0) {
        ESP_LOGW(TAG, "send_text subscribe failed");
    } else {
        ESP_LOGI(DIAG_TAG_MARKET, "WS subscribe ticker+trades %s", s_market);
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
        s_trade_widx = 0;
        s_trade_count = 0;
        s_last_trade_wall_sec = 0;
        s_gap_trade_warn_latched = false;
        if (s_metrics_mx && s_snap_ptr && xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_snap_ptr->ws_trade_ring_capacity = static_cast<uint16_t>(k_trade_ring_cap);
            s_snap_ptr->ws_trade_ring_occupancy = 0;
            s_snap_ptr->ws_last_trade_local_ms = 0;
            xSemaphoreGive(s_metrics_mx);
        }
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
            sync_inbound_tick_stats();
            ++s_raw_cur_sec_count;
            s_last_raw_wall_sec = esp_timer_get_time() / 1000000ULL;
            ESP_LOGD(TAG, "[WS_RX] len=%d", static_cast<int>(data->data_len));
            double trade_price = 0;
            int64_t ts_exch = 0;
            if (parse_trade_text(data->data_ptr, static_cast<size_t>(data->data_len), &trade_price, &ts_exch)) {
                const int64_t loc_ms = static_cast<int64_t>(esp_timer_get_time() / 1000);
                market_types::WsRawTradeSample smp{};
                smp.price_eur = trade_price;
                smp.ts_local_ms = loc_ms;
                smp.ts_exchange_ms = ts_exch;
                uint32_t ev = 0;
                if (xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(100)) == pdTRUE) {
                    trade_ring_push(smp, &ev);
                    if (s_snap_ptr) {
                        if (ev != 0u) {
                            s_snap_ptr->ws_trade_ring_drop_total += ev;
                        }
                        ++s_trade_cur_sec_count;
                        ++s_snap_ptr->ws_trades_total_since_boot;
                        s_last_trade_wall_sec = esp_timer_get_time() / 1000000ULL;
                        s_snap_ptr->ws_trade_ring_occupancy = static_cast<uint16_t>(s_trade_count);
                        s_snap_ptr->ws_last_trade_local_ms = loc_ms;
                        s_snap_ptr->ws_trade_ring_capacity = static_cast<uint16_t>(k_trade_ring_cap);
                    }
                    xSemaphoreGive(s_metrics_mx);
                }
                ESP_LOGD(TAG, "[WS_TRD_RX] price=%.4f local_ms=%lld exch_ms=%lld", trade_price,
                         (long long)loc_ms, (long long)ts_exch);
            } else {
                double p = 0;
                if (parse_ticker_text(data->data_ptr, static_cast<size_t>(data->data_len), &p)) {
                    const int64_t ts = static_cast<int64_t>(esp_timer_get_time() / 1000);
                    apply_price(p, ts);
                }
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
    s_trade_cur_sec_count = 0;
    s_raw_cur_sec_count = 0;
    s_last_raw_wall_sec = 0;
    s_last_canonical_wall_sec = 0;
    s_last_trade_wall_sec = 0;
    s_gap_canonical_warn_latched = false;
    s_gap_trade_warn_latched = false;
    s_trade_widx = 0;
    s_trade_count = 0;
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

void publish_gap_metrics()
{
    if (!s_snap_ptr || !s_metrics_mx) {
        return;
    }
    const uint64_t now_s = esp_timer_get_time() / 1000000ULL;
    if (xSemaphoreTake(s_metrics_mx, pdMS_TO_TICKS(30)) != pdTRUE) {
        return;
    }
    uint32_t gap_raw = 0;
    uint32_t gap_can = 0;
    if (s_last_raw_wall_sec > 0 && now_s >= s_last_raw_wall_sec) {
        gap_raw = static_cast<uint32_t>(now_s - s_last_raw_wall_sec);
    }
    if (s_last_canonical_wall_sec > 0 && now_s >= s_last_canonical_wall_sec) {
        gap_can = static_cast<uint32_t>(now_s - s_last_canonical_wall_sec);
    }
    uint32_t gap_trade = 0;
    if (s_last_trade_wall_sec > 0 && now_s >= s_last_trade_wall_sec) {
        gap_trade = static_cast<uint32_t>(now_s - s_last_trade_wall_sec);
    }
    s_snap_ptr->ws_gap_sec_since_last_raw = gap_raw;
    s_snap_ptr->ws_gap_sec_since_last_canonical = gap_can;
    s_snap_ptr->ws_gap_sec_since_last_trade = gap_trade;
    xSemaphoreGive(s_metrics_mx);

    if (gap_can >= 12) {
        if (!s_gap_canonical_warn_latched) {
            s_gap_canonical_warn_latched = true;
            ESP_LOGW(TAG,
                     "[WS_GAP] no canonical tick for >=12 s (canonical_gap=%" PRIu32 " s raw_gap=%" PRIu32 " s)",
                     gap_can, gap_raw);
        }
    } else {
        s_gap_canonical_warn_latched = false;
    }

    if (s_last_trade_wall_sec > 0 && gap_trade >= 60) {
        if (!s_gap_trade_warn_latched) {
            s_gap_trade_warn_latched = true;
            ESP_LOGW(TAG, "[WS_TRD_GAP] no trade parse for >=60 s (trade_gap=%" PRIu32 " s)", gap_trade);
        }
    } else if (gap_trade < 45) {
        s_gap_trade_warn_latched = false;
    }
}

} // namespace exchange_bitvavo::ws
