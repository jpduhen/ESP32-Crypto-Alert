/**
 * M-013a: minimale WebUI — GET / en GET /api/status.json.
 * M-013b: POST /api/services.json — mqtt/ntfy naar config_store (incl. MQTT-user/wachtwoord NVS).
 * M-013c: hoofdpagina-formulier (inline JS) naar dezelfde POST-route.
 * M-014a: POST /api/ota — ruwe firmware → ota_service.
 * M-014b: OTA-status in /api/status.json + blok op /.
 * M-013d: read-only recente 1m-alerts in status-JSON + HTML-sectie op /.
 * M-010c: 5m-alerts in JSON/HTML; M-010d: confluence 1m+5m in JSON/HTML.
 * M-013e: read-only regime/vol/effectieve drempels in status.json + compact HTML-blok.
 * M-003b: `alert_runtime_config` in status.json (typed tuning, read-only).
 * M-003c: `alert_policy_timing` in status.json (cooldown/suppress, read-only).
 * M-003d: `alert_confluence_policy` in status.json (read-only overlay).
 * M-013k: POST /api/alert-confluence-policy.json → `persist_alert_confluence_policy` (zelfde subset als M-003d).
 * M-013f: POST /api/alert-runtime.json → `persist_alert_runtime` (zelfde subset als M-003b).
 * M-013i: POST /api/alert-policy-timing.json → `persist_alert_policy_timing` (zelfde subset als M-003c).
 * M-013g: minimaal formulier op / voor diezelfde vier velden (POST naar alert-runtime.json).
 * M-013j: minimaal formulier op / voor alert-policy timing (POST naar alert-policy-timing.json).
 * M-013l: minimaal formulier op / voor confluence-policy (POST naar alert-confluence-policy.json; M-003d/M-013k).
 * M-013h: read-only alert-beslissing per pad (1m/5m/30m/2h/confluence) in status.json + compact op /.
 * C1/C2: read-only `alert_engine_runtime_stats` (emits, suppress, edge-transities) + regime `last_regime_change_epoch_ms`.
 * M-002h: read-only outbound-queue observability in status.json (geen nieuwe settings).
 * S30-1: read-only `metric_30m_observability` (domain_metrics::compute_30m_move_pct).
 * S2H-1: read-only `metric_2h_observability` (domain_metrics::compute_2h_move_pct).
 * RWS-04: read-only `metrics_input_observability` (domain_metrics::metrics_input_source_observability).
 * RWS-04b: A/B-velden + large-delta in `metrics_input_observability`.
 */
#include "webui/webui.hpp"
#include "alert_engine/alert_engine.hpp"
#include "alert_observability/alert_observability.hpp"
#include "config_store/config_store.hpp"
#include "domain_metrics/domain_metrics.hpp"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "market_data/market_data.hpp"
#include "market_types/types.hpp"
#include "net_runtime/net_runtime.hpp"
#include "service_outbound/service_outbound.hpp"
#include "sdkconfig.h"

#if CONFIG_WEBUI_ENABLE
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/def.h"
#include "ota_service/ota_service.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

namespace webui {

static const char TAG[] = "web_ui";

#if CONFIG_WEBUI_ENABLE

static httpd_handle_t s_httpd{nullptr};

/**
 * `GET /` gebruikt een vast 32 KiB-blok (geen runtime malloc) — bij ~76 KiB "vrij" heap faalde
 * `malloc(32768)` nog door fragmentatie (geen aaneengesloten blok).
 */
static constexpr size_t k_root_html_buf_bytes = 32768;
static uint8_t s_root_html_buf[k_root_html_buf_bytes];
static SemaphoreHandle_t s_root_html_mx{};

/** Bij tijdelijke heap-fragmentatie/druk faalt één `malloc`/`cJSON` soms — korte backoff-retry. */
static cJSON *cjson_create_object_retry()
{
    constexpr int k_attempts = 5;
    for (int i = 0; i < k_attempts; ++i) {
        cJSON *const o = cJSON_CreateObject();
        if (o) {
            return o;
        }
        if (i + 1 < k_attempts) {
            vTaskDelay(pdMS_TO_TICKS(10 + 10 * i));
        }
    }
    return nullptr;
}

static const char *conn_str(market_types::ConnectionState c)
{
    switch (c) {
    case market_types::ConnectionState::Disconnected:
        return "disconnected";
    case market_types::ConnectionState::Connecting:
        return "connecting";
    case market_types::ConnectionState::Connected:
        return "connected";
    case market_types::ConnectionState::Error:
        return "error";
    }
    return "?";
}

static const char *tick_str(market_types::TickSource t)
{
    switch (t) {
    case market_types::TickSource::Ws:
        return "ws";
    case market_types::TickSource::Rest:
        return "rest";
    case market_types::TickSource::None:
    default:
        return "none";
    }
}

/** M-013h: stabiele JSON-velden; `-1` = n.v.t. */
static void fmt_rem_ms(char *buf, size_t len, int64_t ms)
{
    if (!buf || len == 0) {
        return;
    }
    if (ms < 0) {
        std::snprintf(buf, len, "—");
    } else {
        std::snprintf(buf, len, "%lld ms", static_cast<long long>(ms));
    }
}

static void add_alert_decision_path_json(cJSON *parent,
                                         const char *key,
                                         const alert_engine::AlertPathDecisionSnapshot &p)
{
    cJSON *o = cJSON_CreateObject();
    if (!o || !parent || !key) {
        return;
    }
    cJSON_AddStringToObject(o, "status", p.status[0] ? p.status : "?");
    if (p.reason[0] != '\0') {
        cJSON_AddStringToObject(o, "reason", p.reason);
    }
    cJSON_AddNumberToObject(o, "remaining_cooldown_ms", static_cast<double>(p.remaining_cooldown_ms));
    cJSON_AddNumberToObject(o, "remaining_suppress_ms", static_cast<double>(p.remaining_suppress_ms));
    cJSON_AddItemToObject(parent, key, o);
}

static void add_path_edge_json(cJSON *parent,
                               const char *key,
                               const alert_engine::AlertPathEdgeStats &e)
{
    cJSON *o = cJSON_CreateObject();
    if (!o || !parent || !key) {
        return;
    }
    cJSON_AddNumberToObject(o, "enter_cooldown", static_cast<double>(e.enter_cooldown));
    cJSON_AddNumberToObject(o, "enter_suppressed", static_cast<double>(e.enter_suppressed));
    cJSON_AddNumberToObject(o, "enter_not_ready", static_cast<double>(e.enter_not_ready));
    cJSON_AddNumberToObject(o, "last_epoch_ms_enter_cooldown",
                             static_cast<double>(e.last_epoch_ms_enter_cooldown));
    cJSON_AddNumberToObject(o, "last_epoch_ms_enter_suppressed",
                             static_cast<double>(e.last_epoch_ms_enter_suppressed));
    cJSON_AddNumberToObject(o, "last_epoch_ms_enter_not_ready",
                             static_cast<double>(e.last_epoch_ms_enter_not_ready));
    cJSON_AddItemToObject(parent, key, o);
}

static void sta_ip_str(char *out, size_t out_len)
{
    out[0] = '\0';
    if (!net_runtime::has_ip()) {
        return;
    }
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        return;
    }
    esp_netif_ip_info_t ip{};
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        return;
    }
    const uint32_t h = lwip_ntohl(ip.ip.addr);
    snprintf(out, out_len, "%u.%u.%u.%u", static_cast<unsigned>((h >> 24) & 0xffu),
             static_cast<unsigned>((h >> 16) & 0xffu), static_cast<unsigned>((h >> 8) & 0xffu),
             static_cast<unsigned>(h & 0xffu));
}

static esp_err_t handle_status_json(httpd_req_t *req)
{
    const market_data::MarketSnapshot snap = market_data::snapshot();
    char ipbuf[20]{};
    sta_ip_str(ipbuf, sizeof(ipbuf));
    const esp_app_desc_t *app = esp_app_get_description();

    ota_service::OtaStatusSnapshot ota{};
    ota_service::get_status_snapshot(&ota);

    cJSON *root = cjson_create_object_retry();
    if (!root) {
        ESP_LOGW(TAG,
                 "GET /api/status.json: cJSON root alloc failed na retry (heap=%u B)",
                 static_cast<unsigned>(esp_get_free_heap_size()));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "app", "CryptoAlert V2");
    cJSON_AddStringToObject(root, "version", (app && app->version[0]) ? app->version : "?");
    cJSON_AddBoolToObject(root, "has_ip", net_runtime::has_ip() ? 1 : 0);
    cJSON_AddStringToObject(root, "ip", ipbuf);
    cJSON_AddStringToObject(root, "symbol", snap.market_label[0] ? snap.market_label : "—");
    cJSON_AddNumberToObject(root, "price_eur", snap.last_tick.price_eur);
    cJSON_AddBoolToObject(root, "valid", snap.valid ? 1 : 0);
    cJSON_AddStringToObject(root, "connection", conn_str(snap.connection));
    cJSON_AddStringToObject(root, "tick_source", tick_str(snap.last_tick_source));
    cJSON_AddNumberToObject(root, "last_tick_ms", static_cast<double>(snap.last_tick.ts_ms));
    cJSON_AddNumberToObject(root,
                           "ws_inbound_ticks_last_sec",
                           static_cast<double>(snap.ws_inbound_ticks_last_sec));
    {
        cJSON *wsf = cJSON_CreateObject();
        if (wsf) {
            cJSON_AddStringToObject(wsf, "official_price_stream",
                                     snap.ws_official_price_stream[0] ? snap.ws_official_price_stream
                                                                      : "bitvavo_ticker_ws_v1");
            cJSON_AddNumberToObject(wsf, "raw_msgs_last_sec", static_cast<double>(snap.ws_raw_msgs_last_sec));
            cJSON_AddNumberToObject(wsf, "canonical_ticks_last_sec",
                                     static_cast<double>(snap.ws_inbound_ticks_last_sec));
            cJSON_AddNumberToObject(wsf, "gap_sec_since_last_raw",
                                     static_cast<double>(snap.ws_gap_sec_since_last_raw));
            cJSON_AddNumberToObject(wsf, "gap_sec_since_last_canonical",
                                     static_cast<double>(snap.ws_gap_sec_since_last_canonical));
            cJSON_AddStringToObject(wsf,
                                    "heartbeat_reason_code",
                                    snap.ws_heartbeat_reason_code[0] ? snap.ws_heartbeat_reason_code : "ok");
            cJSON_AddStringToObject(wsf,
                                    "heartbeat_source_visibility",
                                    snap.ws_heartbeat_source_visibility[0] ? snap.ws_heartbeat_source_visibility
                                                                           : "canonical_ticker_ws");
            cJSON_AddItemToObject(root, "ws_feed_observability", wsf);
        }
    }
    {
        cJSON *wt24 = cJSON_CreateObject();
        if (wt24) {
            cJSON_AddStringToObject(wt24,
                                    "official_price_stream",
                                    snap.ws_official_price_stream[0] ? snap.ws_official_price_stream
                                                                     : "bitvavo_ticker_ws_v1");
            cJSON_AddNumberToObject(wt24, "last_ticker24h_local_ms",
                                     static_cast<double>(snap.ws_last_ticker24h_local_ms));
            cJSON_AddBoolToObject(wt24, "ticker24h_seen_recently", snap.ws_ticker24h_seen_recently ? 1 : 0);
            cJSON_AddNumberToObject(wt24, "ticker24h_msgs_total",
                                     static_cast<double>(snap.ws_ticker24h_msgs_total));
            cJSON_AddNumberToObject(wt24, "ticker24h_events_last_sec",
                                     static_cast<double>(snap.ws_ticker24h_events_last_sec));
            cJSON_AddNumberToObject(wt24, "gap_sec_since_last_ticker24h",
                                     static_cast<double>(snap.ws_gap_sec_since_last_ticker24h));
            cJSON_AddNumberToObject(wt24, "ticker24h_last_eur", snap.ws_ticker24h_last_eur);
            cJSON_AddItemToObject(root, "ws_ticker24h_observability", wt24);
        }
    }
    {
        cJSON *wst = cJSON_CreateObject();
        if (wst) {
            cJSON_AddNumberToObject(wst, "trades_last_sec", static_cast<double>(snap.ws_trade_events_last_sec));
            cJSON_AddNumberToObject(wst, "trades_total_since_boot",
                                     static_cast<double>(snap.ws_trades_total_since_boot));
            cJSON_AddNumberToObject(wst, "ring_capacity", static_cast<double>(snap.ws_trade_ring_capacity));
            cJSON_AddNumberToObject(wst, "ring_occupancy", static_cast<double>(snap.ws_trade_ring_occupancy));
            cJSON_AddNumberToObject(wst, "ring_drop_total", static_cast<double>(snap.ws_trade_ring_drop_total));
            cJSON_AddNumberToObject(wst, "gap_sec_since_last_trade",
                                     static_cast<double>(snap.ws_gap_sec_since_last_trade));
            cJSON_AddNumberToObject(wst, "last_trade_local_ms", static_cast<double>(snap.ws_last_trade_local_ms));
            cJSON_AddItemToObject(root, "ws_trades_observability", wst);
        }
    }
    {
        cJSON *wss = cJSON_CreateObject();
        if (wss) {
            cJSON_AddNumberToObject(wss, "agg_wall_sec", static_cast<double>(snap.ws_second_agg_wall_sec));
            cJSON_AddBoolToObject(wss, "has_trades", snap.ws_second_agg_has_trades ? 1 : 0);
            cJSON_AddNumberToObject(wss, "trade_count", static_cast<double>(snap.ws_second_agg_trade_count));
            cJSON_AddNumberToObject(wss, "first_eur", snap.ws_second_agg_first_eur);
            cJSON_AddNumberToObject(wss, "last_eur", snap.ws_second_agg_last_eur);
            cJSON_AddNumberToObject(wss, "min_eur", snap.ws_second_agg_min_eur);
            cJSON_AddNumberToObject(wss, "max_eur", snap.ws_second_agg_max_eur);
            cJSON_AddNumberToObject(wss, "mean_eur", snap.ws_second_agg_mean_eur);
            cJSON_AddBoolToObject(wss, "ticker_seen_in_second", snap.ws_second_agg_ticker_seen ? 1 : 0);
            cJSON_AddNumberToObject(wss, "canonical_ticks_in_second",
                                     static_cast<double>(snap.ws_second_agg_canonical_ticks));
            cJSON_AddNumberToObject(wss, "ring_capacity", static_cast<double>(snap.ws_second_ring_capacity));
            cJSON_AddNumberToObject(wss, "ring_used", static_cast<double>(snap.ws_second_ring_used));
            cJSON_AddNumberToObject(wss, "ring_writes_total", static_cast<double>(snap.ws_second_ring_writes_total));
            cJSON_AddItemToObject(root, "ws_second_observability", wss);
        }
    }
    {
        const domain_metrics::MetricsInputSourceObservability mio = domain_metrics::metrics_input_source_observability();
        cJSON *mij = cJSON_CreateObject();
        if (mij) {
            cJSON_AddBoolToObject(mij, "rws04_enabled", mio.rws04_enabled ? 1 : 0);
            cJSON_AddNumberToObject(mij, "seconds_via_trade_mean", static_cast<double>(mio.seconds_via_trade_mean));
            cJSON_AddNumberToObject(mij, "seconds_via_fallback", static_cast<double>(mio.seconds_via_fallback));
            cJSON_AddStringToObject(mij,
                                    "last_finalize_source",
                                    (mio.last_finalize_source && mio.last_finalize_source[0]) ? mio.last_finalize_source
                                                                                              : "");
            cJSON_AddStringToObject(mij,
                                    "last_fallback_reason",
                                    (mio.last_fallback_reason && mio.last_fallback_reason[0]) ? mio.last_fallback_reason
                                                                                              : "");
            cJSON_AddNumberToObject(mij,
                                    "ab_compare_seconds_total",
                                    static_cast<double>(mio.ab_compare_seconds_total));
            cJSON_AddNumberToObject(mij,
                                    "ab_compare_trade_seconds_total",
                                    static_cast<double>(mio.ab_compare_trade_seconds_total));
            cJSON_AddNumberToObject(mij,
                                    "ab_compare_large_delta_total",
                                    static_cast<double>(mio.ab_compare_large_delta_total));
            cJSON_AddNumberToObject(mij, "last_compare_wall_sec", static_cast<double>(mio.last_compare_wall_sec));
            cJSON_AddNumberToObject(mij, "last_ticker_canonical_eur", mio.last_ticker_canonical_eur);
            cJSON_AddNumberToObject(mij, "last_trade_mean_eur", mio.last_trade_mean_eur);
            cJSON_AddNumberToObject(mij, "last_delta_abs_eur", mio.last_delta_abs_eur);
            cJSON_AddNumberToObject(mij, "last_delta_bps", mio.last_delta_bps);
            cJSON_AddNumberToObject(mij, "last_delta_pct", mio.last_delta_pct);
            cJSON_AddNumberToObject(mij, "last_trade_count", static_cast<double>(mio.last_trade_count));
            cJSON_AddBoolToObject(mij, "last_large_delta", mio.last_large_delta ? 1 : 0);
            cJSON_AddStringToObject(mij,
                                    "last_ab_class",
                                    (mio.last_ab_class && mio.last_ab_class[0]) ? mio.last_ab_class : "");
            cJSON_AddNumberToObject(mij,
                                    "large_delta_threshold_bps",
                                    static_cast<double>(mio.large_delta_threshold_bps));
            cJSON_AddItemToObject(root, "metrics_input_observability", mij);
        }
    }
    {
        const domain_metrics::Metric30mMovePct m30 = domain_metrics::compute_30m_move_pct();
        cJSON *m30j = cJSON_CreateObject();
        if (m30j) {
            cJSON_AddBoolToObject(m30j, "ready", m30.ready ? 1 : 0);
            cJSON_AddNumberToObject(m30j, "pct", m30.pct);
            cJSON_AddNumberToObject(m30j, "ref_ts_ms", static_cast<double>(m30.ref_ts_ms));
            cJSON_AddNumberToObject(m30j, "now_ts_ms", static_cast<double>(m30.now_ts_ms));
            cJSON_AddNumberToObject(m30j, "ref_price_eur", m30.ref_price_eur);
            cJSON_AddNumberToObject(m30j, "now_price_eur", m30.now_price_eur);
            cJSON_AddNumberToObject(m30j, "ref_span_ms", static_cast<double>(m30.ref_span_ms));
            cJSON_AddNumberToObject(m30j,
                                    "canonical_ring_samples",
                                    static_cast<double>(domain_metrics::canonical_ring_count()));
            cJSON_AddNumberToObject(m30j,
                                    "ring_cap",
                                    static_cast<double>(CONFIG_DOMAIN_METRICS_30M_WINDOW_S +
                                                        CONFIG_DOMAIN_METRICS_CANONICAL_RING_EXTRA_S));
            cJSON_AddNumberToObject(m30j,
                                    "window_s",
                                    static_cast<double>(CONFIG_DOMAIN_METRICS_30M_WINDOW_S));
            cJSON_AddItemToObject(root, "metric_30m_observability", m30j);
        }
    }
    {
        const domain_metrics::Metric2hMovePct m2 = domain_metrics::compute_2h_move_pct();
        cJSON *m2j = cJSON_CreateObject();
        if (m2j) {
            cJSON_AddBoolToObject(m2j, "ready", m2.ready ? 1 : 0);
            cJSON_AddNumberToObject(m2j, "pct", m2.pct);
            cJSON_AddNumberToObject(m2j, "ref_ts_ms", static_cast<double>(m2.ref_ts_ms));
            cJSON_AddNumberToObject(m2j, "now_ts_ms", static_cast<double>(m2.now_ts_ms));
            cJSON_AddNumberToObject(m2j, "ref_price_eur", m2.ref_price_eur);
            cJSON_AddNumberToObject(m2j, "now_price_eur", m2.now_price_eur);
            cJSON_AddNumberToObject(m2j, "ref_span_ms", static_cast<double>(m2.ref_span_ms));
            cJSON_AddNumberToObject(m2j, "minute_ring_used", static_cast<double>(m2.minute_ring_used));
            cJSON_AddNumberToObject(m2j,
                                    "window_s",
                                    static_cast<double>(CONFIG_DOMAIN_METRICS_2H_WINDOW_S));
            cJSON_AddNumberToObject(
                m2j,
                "minute_ring_cap",
                static_cast<double>((CONFIG_DOMAIN_METRICS_2H_WINDOW_S + 59) / 60 +
                                    CONFIG_DOMAIN_METRICS_2H_MINUTE_RING_EXTRA_SLOTS + 2));
            cJSON_AddItemToObject(root, "metric_2h_observability", m2j);
        }
    }
    cJSON_AddNumberToObject(root,
                           "outbound_queue_waiting",
                           static_cast<double>(service_outbound::queue_waiting()));
    cJSON_AddNumberToObject(root,
                           "outbound_queue_capacity",
                           static_cast<double>(service_outbound::queue_capacity()));
    cJSON_AddNumberToObject(root,
                           "outbound_drop_total",
                           static_cast<double>(service_outbound::drop_total()));

    cJSON *ota_j = cJSON_CreateObject();
    if (ota_j) {
        cJSON_AddStringToObject(ota_j,
                                "running_partition",
                                ota.running_label[0] ? ota.running_label : "?");
        cJSON_AddStringToObject(ota_j,
                                "next_update_partition",
                                ota.next_update_label[0] ? ota.next_update_label : "?");
        cJSON_AddNumberToObject(ota_j, "running_address", static_cast<double>(ota.running_address));
        cJSON_AddNumberToObject(ota_j, "running_size_bytes", static_cast<double>(ota.running_size_bytes));
        cJSON_AddStringToObject(ota_j, "img_state", ota.img_state[0] ? ota.img_state : "?");
        cJSON_AddStringToObject(ota_j, "boot_confirm", ota.boot_confirm[0] ? ota.boot_confirm : "?");
        cJSON_AddStringToObject(ota_j, "reset_reason", ota.reset_reason[0] ? ota.reset_reason : "?");
        cJSON_AddItemToObject(root, "ota", ota_j);
    }

    alert_observability::add_alerts_to_cjson(root);

    alert_engine::RegimeObservabilitySnapshot rob{};
    alert_engine::get_regime_observability_snapshot(&rob);
    cJSON *reg_j = cJSON_CreateObject();
    if (reg_j) {
        cJSON_AddStringToObject(reg_j, "regime", rob.regime[0] ? rob.regime : "normal");
        cJSON_AddBoolToObject(reg_j, "vol_metric_ready", rob.vol_metric_ready ? 1 : 0);
        cJSON_AddNumberToObject(reg_j, "vol_mean_abs_step_bps", rob.vol_mean_abs_step_bps);
        cJSON_AddNumberToObject(reg_j, "vol_pairs_used", static_cast<double>(rob.vol_pairs_used));
        cJSON_AddBoolToObject(reg_j, "vol_unavailable_fallback", rob.vol_unavailable_fallback ? 1 : 0);
        cJSON_AddNumberToObject(reg_j, "threshold_scale_permille", static_cast<double>(rob.threshold_scale_permille));
        cJSON_AddNumberToObject(reg_j, "threshold_scale_permille_raw", static_cast<double>(rob.threshold_scale_permille_raw));
        cJSON_AddBoolToObject(reg_j, "threshold_scale_clamped", rob.threshold_scale_clamped ? 1 : 0);
        cJSON_AddNumberToObject(reg_j, "regime_calm_max_step_bps", static_cast<double>(rob.regime_calm_max_step_bps));
        cJSON_AddNumberToObject(reg_j, "regime_hot_min_step_bps", static_cast<double>(rob.regime_hot_min_step_bps));
        cJSON *base_thr = cJSON_CreateObject();
        if (base_thr) {
            cJSON_AddNumberToObject(base_thr, "move_pct_1m", rob.base_threshold_move_pct_1m);
            cJSON_AddNumberToObject(base_thr, "move_pct_5m", rob.base_threshold_move_pct_5m);
            cJSON_AddNumberToObject(base_thr, "move_pct_30m", rob.base_threshold_move_pct_30m);
            cJSON_AddNumberToObject(base_thr, "move_pct_2h", rob.base_threshold_move_pct_2h);
            cJSON_AddItemToObject(reg_j, "base_threshold_move_pct", base_thr);
        }
        cJSON *eff_thr = cJSON_CreateObject();
        if (eff_thr) {
            cJSON_AddNumberToObject(eff_thr, "move_pct_1m", rob.effective_threshold_move_pct_1m);
            cJSON_AddNumberToObject(eff_thr, "move_pct_5m", rob.effective_threshold_move_pct_5m);
            cJSON_AddNumberToObject(eff_thr, "move_pct_30m", rob.effective_threshold_move_pct_30m);
            cJSON_AddNumberToObject(eff_thr, "move_pct_2h", rob.effective_threshold_move_pct_2h);
            cJSON_AddItemToObject(reg_j, "effective_threshold_move_pct", eff_thr);
        }
        cJSON *conf_g = cJSON_CreateObject();
        if (conf_g) {
            cJSON_AddNumberToObject(conf_g, "requires_abs_move_pct_1m", rob.effective_threshold_move_pct_1m);
            cJSON_AddNumberToObject(conf_g, "requires_abs_move_pct_5m", rob.effective_threshold_move_pct_5m);
            cJSON_AddItemToObject(reg_j, "confluence_effective_gate_pct", conf_g);
        }
        cJSON_AddNumberToObject(reg_j, "last_regime_change_epoch_ms",
                                 static_cast<double>(rob.last_regime_change_epoch_ms));
        cJSON_AddItemToObject(root, "regime_observability", reg_j);
    }

    {
        alert_engine::AlertDecisionObservabilitySnapshot ado{};
        alert_engine::get_alert_decision_observability_snapshot(&ado);
        cJSON *ado_j = cJSON_CreateObject();
        if (ado_j) {
            add_alert_decision_path_json(ado_j, "1m", ado.tf_1m);
            add_alert_decision_path_json(ado_j, "5m", ado.tf_5m);
            add_alert_decision_path_json(ado_j, "30m", ado.tf_30m);
            add_alert_decision_path_json(ado_j, "2h", ado.tf_2h);
            add_alert_decision_path_json(ado_j, "confluence_1m5m", ado.confluence_1m5m);
            cJSON_AddItemToObject(root, "alert_decision_observability", ado_j);
        }
    }

    {
        alert_engine::AlertEngineRuntimeStatsSnapshot ars{};
        alert_engine::get_alert_runtime_stats_snapshot(&ars);
        cJSON *ars_j = cJSON_CreateObject();
        if (ars_j) {
            cJSON_AddNumberToObject(ars_j, "emit_total_1m", static_cast<double>(ars.emit_total_1m));
            cJSON_AddNumberToObject(ars_j, "emit_total_5m", static_cast<double>(ars.emit_total_5m));
            cJSON_AddNumberToObject(ars_j, "emit_total_30m", static_cast<double>(ars.emit_total_30m));
            cJSON_AddNumberToObject(ars_j, "emit_total_2h", static_cast<double>(ars.emit_total_2h));
            cJSON_AddNumberToObject(ars_j, "emit_total_confluence_1m5m",
                                     static_cast<double>(ars.emit_total_conf));
            cJSON_AddNumberToObject(ars_j, "last_emit_epoch_ms_1m", static_cast<double>(ars.last_emit_epoch_ms_1m));
            cJSON_AddNumberToObject(ars_j, "last_emit_epoch_ms_5m", static_cast<double>(ars.last_emit_epoch_ms_5m));
            cJSON_AddNumberToObject(ars_j, "last_emit_epoch_ms_30m", static_cast<double>(ars.last_emit_epoch_ms_30m));
            cJSON_AddNumberToObject(ars_j, "last_emit_epoch_ms_2h", static_cast<double>(ars.last_emit_epoch_ms_2h));
            cJSON_AddNumberToObject(ars_j, "last_emit_epoch_ms_confluence_1m5m",
                                     static_cast<double>(ars.last_emit_epoch_ms_conf));
            cJSON_AddNumberToObject(ars_j, "suppress_after_conf_window_episodes_1m",
                                     static_cast<double>(ars.suppress_after_conf_window_1m));
            cJSON_AddNumberToObject(ars_j, "suppress_after_conf_window_episodes_5m",
                                     static_cast<double>(ars.suppress_after_conf_window_5m));
            add_path_edge_json(ars_j, "edge_1m", ars.edge_1m);
            add_path_edge_json(ars_j, "edge_5m", ars.edge_5m);
            add_path_edge_json(ars_j, "edge_30m", ars.edge_30m);
            add_path_edge_json(ars_j, "edge_2h", ars.edge_2h);
            add_path_edge_json(ars_j, "edge_confluence_1m5m", ars.edge_confluence);
            cJSON_AddItemToObject(root, "alert_engine_runtime_stats", ars_j);
        }
    }

    {
        const config_store::AlertRuntimeConfig &ar = config_store::alert_runtime();
        cJSON *arc_j = cJSON_CreateObject();
        if (arc_j) {
            cJSON_AddNumberToObject(arc_j, "schema_version", static_cast<double>(config_store::kSchemaVersion));
            cJSON_AddNumberToObject(arc_j, "threshold_1m_bps", static_cast<double>(ar.threshold_1m_bps));
            cJSON_AddNumberToObject(arc_j, "threshold_5m_bps", static_cast<double>(ar.threshold_5m_bps));
            cJSON_AddNumberToObject(arc_j, "regime_calm_scale_permille", static_cast<double>(ar.regime_calm_scale_permille));
            cJSON_AddNumberToObject(arc_j, "regime_hot_scale_permille", static_cast<double>(ar.regime_hot_scale_permille));
            cJSON_AddItemToObject(root, "alert_runtime_config", arc_j);
        }
    }

    {
        const config_store::AlertPolicyTimingConfig &apt = config_store::alert_policy_timing();
        cJSON *apt_j = cJSON_CreateObject();
        if (apt_j) {
            cJSON_AddNumberToObject(apt_j, "schema_version", static_cast<double>(config_store::kSchemaVersion));
            cJSON_AddNumberToObject(apt_j, "cooldown_1m_s", static_cast<double>(apt.cooldown_1m_s));
            cJSON_AddNumberToObject(apt_j, "cooldown_5m_s", static_cast<double>(apt.cooldown_5m_s));
            cJSON_AddNumberToObject(apt_j, "cooldown_conf_1m5m_s", static_cast<double>(apt.cooldown_conf_1m5m_s));
            cJSON_AddNumberToObject(apt_j, "suppress_loose_after_conf_s",
                                     static_cast<double>(apt.suppress_loose_after_conf_s));
            cJSON_AddItemToObject(root, "alert_policy_timing", apt_j);
        }
    }

    {
        const config_store::AlertConfluencePolicyConfig &acfp = config_store::alert_confluence_policy();
        cJSON *acf_j = cJSON_CreateObject();
        if (acf_j) {
            cJSON_AddNumberToObject(acf_j, "schema_version", static_cast<double>(config_store::kSchemaVersion));
            cJSON_AddBoolToObject(acf_j, "confluence_enabled", acfp.confluence_enabled ? 1 : 0);
            cJSON_AddBoolToObject(acf_j,
                                  "confluence_require_same_direction",
                                  acfp.confluence_require_same_direction ? 1 : 0);
            cJSON_AddBoolToObject(acf_j,
                                  "confluence_require_both_thresholds",
                                  acfp.confluence_require_both_thresholds ? 1 : 0);
            cJSON_AddBoolToObject(acf_j,
                                  "confluence_emit_loose_alerts_when_conf_fails",
                                  acfp.confluence_emit_loose_alerts_when_conf_fails ? 1 : 0);
            cJSON_AddItemToObject(root, "alert_confluence_policy", acf_j);
        }
    }

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        ESP_LOGW(TAG, "GET /api/status.json: cJSON_PrintUnformatted failed (geen heap)");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    const esp_err_t se = httpd_resp_send(req, printed, HTTPD_RESP_USE_STRLEN);
    cJSON_free(printed);
    return se;
}

static bool cjson_to_bool(const cJSON *j, bool *out)
{
    if (cJSON_IsBool(j)) {
        *out = cJSON_IsTrue(j) != 0;
        return true;
    }
    if (cJSON_IsNumber(j)) {
        *out = (j->valuedouble != 0.0);
        return true;
    }
    return false;
}

static esp_err_t send_json_text(httpd_req_t *req, const char *status_line, const char *json)
{
    httpd_resp_set_status(req, status_line);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

/** Alleen positieve integers die in uint16 passen, geen breuken. */
static bool json_u16_whole(const cJSON *j, uint16_t *out)
{
    if (!cJSON_IsNumber(j) || j == nullptr) {
        return false;
    }
    const double d = j->valuedouble;
    if (d < 0.0 || d > 65535.0) {
        return false;
    }
    if (d != std::floor(d)) {
        return false;
    }
    *out = static_cast<uint16_t>(d);
    return true;
}

static esp_err_t handle_alert_runtime_post(httpd_req_t *req)
{
    char raw[512]{};
    const int need = req->content_len;
    if (need <= 0 || need >= static_cast<int>(sizeof(raw))) {
        ESP_LOGW(TAG, "M-013f: POST body ontbreekt of te groot (%d)", need);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"body ontbreekt of te groot (max 511 bytes)\"}");
    }
    int got = 0;
    while (got < need) {
        const int r = httpd_req_recv(req, raw + got, (size_t)(need - got));
        if (r < 0) {
            ESP_LOGW(TAG, "M-013f: recv: %d", r);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"recv mislukt\"}");
        }
        if (r == 0) {
            break;
        }
        got += r;
    }
    if (got != need) {
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"body incompleet\"}");
    }
    raw[need] = '\0';

    cJSON *root = cJSON_Parse(raw);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGW(TAG, "M-013f: JSON parse/object");
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"JSON moet een object zijn\"}");
    }

    int nchild = 0;
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        ++nchild;
    }
    if (nchild != 4) {
        cJSON_Delete(root);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"exact vier velden vereist (M-003b subset)\"}");
    }

    config_store::AlertRuntimeConfig next{};
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        if (c->string == nullptr) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"ongeldige sleutel\"}");
        }
        uint16_t v = 0;
        if (!json_u16_whole(c, &v)) {
            cJSON_Delete(root);
            return send_json_text(
                req, "400 Bad Request",
                "{\"ok\":false,\"error\":\"alle waarden moeten niet-negatieve gehele getallen zijn\"}");
        }
        if (std::strcmp(c->string, "threshold_1m_bps") == 0) {
            next.threshold_1m_bps = v;
        } else if (std::strcmp(c->string, "threshold_5m_bps") == 0) {
            next.threshold_5m_bps = v;
        } else if (std::strcmp(c->string, "regime_calm_scale_permille") == 0) {
            next.regime_calm_scale_permille = v;
        } else if (std::strcmp(c->string, "regime_hot_scale_permille") == 0) {
            next.regime_hot_scale_permille = v;
        } else {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"onbekende sleutel (alleen M-003b-velden)\"}");
        }
    }
    cJSON_Delete(root);

    const esp_err_t pe = config_store::persist_alert_runtime(next);
    if (pe == ESP_ERR_INVALID_ARG) {
        return send_json_text(
            req, "400 Bad Request",
            "{\"ok\":false,\"error\":\"waarde buiten toegestaan bereik (zie config_store constanten)\"}");
    }
    if (pe != ESP_OK) {
        ESP_LOGW(TAG, "M-013f: persist_alert_runtime: %s", esp_err_to_name(pe));
        return send_json_text(req, "500 Internal Server Error",
                              "{\"ok\":false,\"error\":\"opslaan mislukt\"}");
    }

    const config_store::AlertRuntimeConfig &ar = config_store::alert_runtime();
    cJSON *out = cJSON_CreateObject();
    if (!out) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    cJSON_AddBoolToObject(out, "ok", true);
    cJSON *cfg = cJSON_CreateObject();
    if (cfg) {
        cJSON_AddNumberToObject(cfg, "threshold_1m_bps", static_cast<double>(ar.threshold_1m_bps));
        cJSON_AddNumberToObject(cfg, "threshold_5m_bps", static_cast<double>(ar.threshold_5m_bps));
        cJSON_AddNumberToObject(cfg, "regime_calm_scale_permille",
                                static_cast<double>(ar.regime_calm_scale_permille));
        cJSON_AddNumberToObject(cfg, "regime_hot_scale_permille",
                                static_cast<double>(ar.regime_hot_scale_permille));
        cJSON_AddItemToObject(out, "alert_runtime_config", cfg);
    }
    cJSON_AddStringToObject(
        out, "note",
        "Opgeslagen in NVS — alert_engine gebruikt dit vanaf de volgende tick (geen herstart).");
    char *printed = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!printed) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    ESP_LOGI(TAG,
             "M-013f: alert runtime opgeslagen via POST (1m=%u 5m=%u bps calm=%u‰ hot=%u‰)",
             (unsigned)ar.threshold_1m_bps, (unsigned)ar.threshold_5m_bps,
             (unsigned)ar.regime_calm_scale_permille, (unsigned)ar.regime_hot_scale_permille);
    const esp_err_t se = send_json_text(req, "200 OK", printed);
    cJSON_free(printed);
    return se;
}

static esp_err_t handle_alert_policy_timing_post(httpd_req_t *req)
{
    char raw[512]{};
    const int need = req->content_len;
    if (need <= 0 || need >= static_cast<int>(sizeof(raw))) {
        ESP_LOGW(TAG, "M-013i: POST body ontbreekt of te groot (%d)", need);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"body ontbreekt of te groot (max 511 bytes)\"}");
    }
    int got = 0;
    while (got < need) {
        const int r = httpd_req_recv(req, raw + got, (size_t)(need - got));
        if (r < 0) {
            ESP_LOGW(TAG, "M-013i: recv: %d", r);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"recv mislukt\"}");
        }
        if (r == 0) {
            break;
        }
        got += r;
    }
    if (got != need) {
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"body incompleet\"}");
    }
    raw[need] = '\0';

    cJSON *root = cJSON_Parse(raw);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGW(TAG, "M-013i: JSON parse/object");
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"JSON moet een object zijn\"}");
    }

    int nchild = 0;
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        ++nchild;
    }
    if (nchild != 4) {
        cJSON_Delete(root);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"exact vier velden vereist (M-003c subset)\"}");
    }

    config_store::AlertPolicyTimingConfig next{};
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        if (c->string == nullptr) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"ongeldige sleutel\"}");
        }
        uint16_t v = 0;
        if (!json_u16_whole(c, &v)) {
            cJSON_Delete(root);
            return send_json_text(
                req, "400 Bad Request",
                "{\"ok\":false,\"error\":\"alle waarden moeten niet-negatieve gehele getallen zijn\"}");
        }
        if (std::strcmp(c->string, "cooldown_1m_s") == 0) {
            next.cooldown_1m_s = v;
        } else if (std::strcmp(c->string, "cooldown_5m_s") == 0) {
            next.cooldown_5m_s = v;
        } else if (std::strcmp(c->string, "cooldown_conf_1m5m_s") == 0) {
            next.cooldown_conf_1m5m_s = v;
        } else if (std::strcmp(c->string, "suppress_loose_after_conf_s") == 0) {
            next.suppress_loose_after_conf_s = v;
        } else {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"onbekende sleutel (alleen M-003c-velden)\"}");
        }
    }
    cJSON_Delete(root);

    const esp_err_t pe = config_store::persist_alert_policy_timing(next);
    if (pe == ESP_ERR_INVALID_ARG) {
        return send_json_text(
            req, "400 Bad Request",
            "{\"ok\":false,\"error\":\"waarde buiten toegestaan bereik (zie config_store / Kconfig-ranges)\"}");
    }
    if (pe != ESP_OK) {
        ESP_LOGW(TAG, "M-013i: persist_alert_policy_timing: %s", esp_err_to_name(pe));
        return send_json_text(req, "500 Internal Server Error",
                              "{\"ok\":false,\"error\":\"opslaan mislukt\"}");
    }

    const config_store::AlertPolicyTimingConfig &apt = config_store::alert_policy_timing();
    cJSON *out = cJSON_CreateObject();
    if (!out) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    cJSON_AddBoolToObject(out, "ok", true);
    cJSON *cfg = cJSON_CreateObject();
    if (cfg) {
        cJSON_AddNumberToObject(cfg, "schema_version", static_cast<double>(config_store::kSchemaVersion));
        cJSON_AddNumberToObject(cfg, "cooldown_1m_s", static_cast<double>(apt.cooldown_1m_s));
        cJSON_AddNumberToObject(cfg, "cooldown_5m_s", static_cast<double>(apt.cooldown_5m_s));
        cJSON_AddNumberToObject(cfg, "cooldown_conf_1m5m_s", static_cast<double>(apt.cooldown_conf_1m5m_s));
        cJSON_AddNumberToObject(cfg, "suppress_loose_after_conf_s",
                                 static_cast<double>(apt.suppress_loose_after_conf_s));
        cJSON_AddItemToObject(out, "alert_policy_timing", cfg);
    }
    cJSON_AddStringToObject(
        out, "note",
        "Opgeslagen in NVS — alert_engine gebruikt dit vanaf de volgende tick (geen herstart).");
    char *printed = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!printed) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    ESP_LOGI(TAG,
             "M-013i: alert policy timing opgeslagen via POST (1m_cd=%us 5m_cd=%us conf_cd=%us sup_loose=%us)",
             (unsigned)apt.cooldown_1m_s, (unsigned)apt.cooldown_5m_s,
             (unsigned)apt.cooldown_conf_1m5m_s, (unsigned)apt.suppress_loose_after_conf_s);
    const esp_err_t se = send_json_text(req, "200 OK", printed);
    cJSON_free(printed);
    return se;
}

static esp_err_t handle_alert_confluence_policy_post(httpd_req_t *req)
{
    char raw[512]{};
    const int need = req->content_len;
    if (need <= 0 || need >= static_cast<int>(sizeof(raw))) {
        ESP_LOGW(TAG, "M-013k: POST body ontbreekt of te groot (%d)", need);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"body ontbreekt of te groot (max 511 bytes)\"}");
    }
    int got = 0;
    while (got < need) {
        const int r = httpd_req_recv(req, raw + got, (size_t)(need - got));
        if (r < 0) {
            ESP_LOGW(TAG, "M-013k: recv: %d", r);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"recv mislukt\"}");
        }
        if (r == 0) {
            break;
        }
        got += r;
    }
    if (got != need) {
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"body incompleet\"}");
    }
    raw[need] = '\0';

    cJSON *root = cJSON_Parse(raw);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGW(TAG, "M-013k: JSON parse/object");
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"JSON moet een object zijn\"}");
    }

    int nchild = 0;
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        ++nchild;
    }
    if (nchild != 4) {
        cJSON_Delete(root);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"exact vier velden vereist (M-003d subset)\"}");
    }

    config_store::AlertConfluencePolicyConfig next{};
    for (cJSON *c = root->child; c != nullptr; c = c->next) {
        if (c->string == nullptr) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"ongeldige sleutel\"}");
        }
        bool v = false;
        if (!cjson_to_bool(c, &v)) {
            cJSON_Delete(root);
            return send_json_text(
                req, "400 Bad Request",
                "{\"ok\":false,\"error\":\"alle waarden moeten boolean zijn of getal (0=onwaar, anders waar)\"}");
        }
        if (std::strcmp(c->string, "confluence_enabled") == 0) {
            next.confluence_enabled = v;
        } else if (std::strcmp(c->string, "confluence_require_same_direction") == 0) {
            next.confluence_require_same_direction = v;
        } else if (std::strcmp(c->string, "confluence_require_both_thresholds") == 0) {
            next.confluence_require_both_thresholds = v;
        } else if (std::strcmp(c->string, "confluence_emit_loose_alerts_when_conf_fails") == 0) {
            next.confluence_emit_loose_alerts_when_conf_fails = v;
        } else {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"onbekende sleutel (alleen M-003d-velden)\"}");
        }
    }
    cJSON_Delete(root);

    const esp_err_t pe = config_store::persist_alert_confluence_policy(next);
    if (pe != ESP_OK) {
        ESP_LOGW(TAG, "M-013k: persist_alert_confluence_policy: %s", esp_err_to_name(pe));
        return send_json_text(req, "500 Internal Server Error",
                              "{\"ok\":false,\"error\":\"opslaan mislukt\"}");
    }

    const config_store::AlertConfluencePolicyConfig &acfp = config_store::alert_confluence_policy();
    cJSON *out = cJSON_CreateObject();
    if (!out) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    cJSON_AddBoolToObject(out, "ok", true);
    cJSON *cfg = cJSON_CreateObject();
    if (cfg) {
        cJSON_AddNumberToObject(cfg, "schema_version", static_cast<double>(config_store::kSchemaVersion));
        cJSON_AddBoolToObject(cfg, "confluence_enabled", acfp.confluence_enabled ? 1 : 0);
        cJSON_AddBoolToObject(cfg, "confluence_require_same_direction", acfp.confluence_require_same_direction ? 1 : 0);
        cJSON_AddBoolToObject(cfg, "confluence_require_both_thresholds", acfp.confluence_require_both_thresholds ? 1 : 0);
        cJSON_AddBoolToObject(cfg,
                              "confluence_emit_loose_alerts_when_conf_fails",
                              acfp.confluence_emit_loose_alerts_when_conf_fails ? 1 : 0);
        cJSON_AddItemToObject(out, "alert_confluence_policy", cfg);
    }
    cJSON_AddStringToObject(
        out, "note",
        "Opgeslagen in NVS — alert_engine gebruikt dit vanaf de volgende tick (geen herstart).");
    char *printed = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!printed) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    ESP_LOGI(TAG,
             "M-013k: confluence policy opgeslagen via POST (en=%d same_dir=%d both_thr=%d emit_loose=%d)",
             acfp.confluence_enabled ? 1 : 0,
             acfp.confluence_require_same_direction ? 1 : 0,
             acfp.confluence_require_both_thresholds ? 1 : 0,
             acfp.confluence_emit_loose_alerts_when_conf_fails ? 1 : 0);
    const esp_err_t se = send_json_text(req, "200 OK", printed);
    cJSON_free(printed);
    return se;
}

static esp_err_t handle_services_post(httpd_req_t *req)
{
    char raw[2048]{};
    const int need = req->content_len;
    if (need <= 0 || need >= static_cast<int>(sizeof(raw))) {
        ESP_LOGW(TAG, "M-013b: POST body ontbreekt of te groot (%d)", need);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"body ontbreekt of te groot (max 2047 bytes)\"}");
    }
    int got = 0;
    while (got < need) {
        const int r = httpd_req_recv(req, raw + got, (size_t)(need - got));
        if (r < 0) {
            ESP_LOGW(TAG, "M-013b: recv: %d", r);
            return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"recv mislukt\"}");
        }
        if (r == 0) {
            break;
        }
        got += r;
    }
    if (got != need) {
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"body incompleet\"}");
    }
    raw[need] = '\0';

    cJSON *root = cJSON_Parse(raw);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGW(TAG, "M-013b: JSON parse mislukt");
        return send_json_text(req, "400 Bad Request", "{\"ok\":false,\"error\":\"ongeldige JSON\"}");
    }

    config_store::ServiceRuntimeConfig merged = config_store::service_runtime();
    bool any = false;

    const cJSON *jm = cJSON_GetObjectItem(root, "mqtt_enabled");
    if (jm != nullptr) {
        bool v = false;
        if (!cjson_to_bool(jm, &v)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_enabled: verwacht boolean of getal\"}");
        }
        merged.mqtt_enabled = v;
        any = true;
    }

    const cJSON *juri = cJSON_GetObjectItem(root, "mqtt_broker_uri");
    if (juri != nullptr) {
        if (!cJSON_IsString(juri)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_broker_uri: verwacht string\"}");
        }
        const char *s = cJSON_GetStringValue(juri);
        if (!s) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_broker_uri: leeg\"}");
        }
        if (strlen(s) >= config_store::kMqttBrokerUriMax) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_broker_uri: te lang\"}");
        }
        strncpy(merged.mqtt_broker_uri, s, sizeof(merged.mqtt_broker_uri) - 1);
        merged.mqtt_broker_uri[sizeof(merged.mqtt_broker_uri) - 1] = '\0';
        any = true;
    }

    const cJSON *jmu = cJSON_GetObjectItem(root, "mqtt_username");
    if (jmu != nullptr) {
        if (!cJSON_IsString(jmu)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_username: verwacht string\"}");
        }
        const char *s = cJSON_GetStringValue(jmu);
        if (!s) {
            s = "";
        }
        if (strlen(s) >= config_store::kMqttUserMax) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_username: te lang\"}");
        }
        strncpy(merged.mqtt_username, s, sizeof(merged.mqtt_username) - 1);
        merged.mqtt_username[sizeof(merged.mqtt_username) - 1] = '\0';
        any = true;
    }

    const cJSON *jmp = cJSON_GetObjectItem(root, "mqtt_password");
    if (jmp != nullptr) {
        if (!cJSON_IsString(jmp)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_password: verwacht string\"}");
        }
        const char *s = cJSON_GetStringValue(jmp);
        if (!s) {
            s = "";
        }
        if (strlen(s) >= config_store::kMqttPassMax) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"mqtt_password: te lang\"}");
        }
        strncpy(merged.mqtt_password, s, sizeof(merged.mqtt_password) - 1);
        merged.mqtt_password[sizeof(merged.mqtt_password) - 1] = '\0';
        any = true;
    }

    const cJSON *jn = cJSON_GetObjectItem(root, "ntfy_enabled");
    if (jn != nullptr) {
        bool v = false;
        if (!cjson_to_bool(jn, &v)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"ntfy_enabled: verwacht boolean of getal\"}");
        }
        merged.ntfy_enabled = v;
        any = true;
    }

    const cJSON *jtp = cJSON_GetObjectItem(root, "ntfy_topic");
    if (jtp != nullptr) {
        if (!cJSON_IsString(jtp)) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"ntfy_topic: verwacht string\"}");
        }
        const char *s = cJSON_GetStringValue(jtp);
        if (!s) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"ntfy_topic: ongeldig\"}");
        }
        if (strlen(s) >= config_store::kNtfyTopicMax) {
            cJSON_Delete(root);
            return send_json_text(req, "400 Bad Request",
                                  "{\"ok\":false,\"error\":\"ntfy_topic: te lang\"}");
        }
        strncpy(merged.ntfy_topic, s, sizeof(merged.ntfy_topic) - 1);
        merged.ntfy_topic[sizeof(merged.ntfy_topic) - 1] = '\0';
        any = true;
    }

    cJSON_Delete(root);

    if (!any) {
        ESP_LOGW(TAG, "M-013b: geen herkende velden");
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"minstens één veld: mqtt_enabled, mqtt_broker_uri, "
                              "mqtt_username, mqtt_password, ntfy_enabled, ntfy_topic\"}");
    }

    const esp_err_t pe = config_store::persist_service_connectivity(merged);
    if (pe == ESP_ERR_INVALID_ARG) {
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"validatie mislukt (bijv. mqtt_enabled zonder URI)\"}");
    }
    if (pe != ESP_OK) {
        ESP_LOGW(TAG, "M-013b: persist: %s", esp_err_to_name(pe));
        return send_json_text(req, "500 Internal Server Error",
                              "{\"ok\":false,\"error\":\"opslaan mislukt\"}");
    }

    const config_store::ServiceRuntimeConfig &s = config_store::service_runtime();
    cJSON *out = cJSON_CreateObject();
    cJSON_AddBoolToObject(out, "ok", true);
    cJSON_AddBoolToObject(out, "mqtt_enabled", s.mqtt_enabled);
    cJSON_AddStringToObject(out, "mqtt_broker_uri", s.mqtt_broker_uri);
    cJSON_AddStringToObject(out, "mqtt_username", s.mqtt_username);
    cJSON_AddBoolToObject(out, "mqtt_password_set", s.mqtt_password[0] != '\0');
    cJSON_AddBoolToObject(out, "ntfy_enabled", s.ntfy_enabled);
    cJSON_AddStringToObject(out, "ntfy_topic", s.ntfy_topic);
    cJSON_AddStringToObject(
        out, "note",
        "NTFY: volgende push gebruikt dit topic. MQTT: URI en inlog (user/wachtwoord NVS) worden pas na "
        "herstart gebruikt door de client (geen hot-reload).");
    char *printed = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!printed) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    const esp_err_t se = send_json_text(req, "200 OK", printed);
    cJSON_free(printed);
    return se;
}

/** HTML attribute escaping for value="…" (minimal subset). */
static const char *boot_confirm_nl(const char *bc)
{
    if (!bc || !bc[0]) {
        return "—";
    }
    if (std::strcmp(bc, "marked_valid") == 0) {
        return "Image bevestigd (rollback uitgeschakeld voor dit slot).";
    }
    if (std::strcmp(bc, "rollback_disabled") == 0) {
        return "IDF rollback staat uit — geen aparte bevestigingsplicht.";
    }
    return bc;
}

static void html_attr_escape(const char *src, char *dst, size_t dst_sz)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 8 < dst_sz; ++i) {
        switch (src[i]) {
        case '&':
            std::memcpy(dst + j, "&amp;", 5);
            j += 5;
            break;
        case '"':
            std::memcpy(dst + j, "&quot;", 6);
            j += 6;
            break;
        case '<':
            std::memcpy(dst + j, "&lt;", 4);
            j += 4;
            break;
        default:
            dst[j++] = src[i];
            break;
        }
    }
    dst[j] = '\0';
}

static esp_err_t handle_root_html_impl(httpd_req_t *req)
{
    const market_data::MarketSnapshot snap = market_data::snapshot();
    char ipbuf[20]{};
    sta_ip_str(ipbuf, sizeof(ipbuf));
    const esp_app_desc_t *app = esp_app_get_description();
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    ota_service::OtaStatusSnapshot ota{};
    ota_service::get_status_snapshot(&ota);
    const domain_metrics::MetricsInputSourceObservability mio = domain_metrics::metrics_input_source_observability();

    char esc_uri[config_store::kMqttBrokerUriMax * 6]{};
    char esc_muser[config_store::kMqttUserMax * 6]{};
    char esc_topic[config_store::kNtfyTopicMax * 6]{};
    html_attr_escape(svc.mqtt_broker_uri, esc_uri, sizeof(esc_uri));
    html_attr_escape(svc.mqtt_username, esc_muser, sizeof(esc_muser));
    html_attr_escape(svc.ntfy_topic, esc_topic, sizeof(esc_topic));

    const char *mq_chk = svc.mqtt_enabled ? " checked" : "";
    const char *nt_chk = svc.ntfy_enabled ? " checked" : "";

    /* Homepage: vaste buffer `s_root_html_buf` (geen malloc — zie PR-02/fragmentatie). */
    char *const html = reinterpret_cast<char *>(s_root_html_buf);
    constexpr size_t k_html_alloc = k_root_html_buf_bytes;

    int n = std::snprintf(
        html, k_html_alloc,
        "<!DOCTYPE html><html lang=\"nl\"><head><meta charset=\"utf-8\"/>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
        "<title>CryptoAlert V2</title>"
        "<style>body{font-family:system-ui,sans-serif;margin:1rem;line-height:1.4}"
        "code{background:#eee;padding:2px 6px}fieldset{border:1px solid #ccc;border-radius:6px;padding:1rem;margin-top:1rem}"
        "#svc-msg,#alert-runtime-msg,#alert-policy-msg,#alert-conf-msg{min-height:1.5em;margin-top:.75em;font-size:.95rem}"
        "</style></head><body>"
        "<h1>CryptoAlert V2</h1>"
        "<p><strong>Versie</strong> %s · <strong>IP</strong> %s · <strong>WiFi IP bekend</strong> %s</p>"
        "<p><strong>Symbool</strong> %s · <strong>Prijs (EUR)</strong> %.4f · <strong>Geldig</strong> %s</p>"
        "<p><strong>Verbinding feed</strong> %s · <strong>Bron tick</strong> %s · "
        "<strong>WS raw/canonical (vorige s)</strong> %u/%u · "
        "<strong>GAP raw/canonical (s)</strong> %u/%u · <code>%s</code></p>"
        "<p><strong>WS trades (RWS-02)</strong> %u/s · tot %u · ring %u/%u · drops %u · gap %us</p>"
        "<p><strong>RWS-03</strong> laatste sec=%llu · trades=%u · mean=%.4f · ticker=%s · sec-ring %u/%u</p>"
        "<p><strong>RWS-04</strong> metrics-ingang %s · trade_mean=%u · fallback=%u · laatste=%s</p>"
        "<p><strong>RWS-04b</strong> sec=%llu · Δbps=%.2f · class=%s · large_events=%u (drempel=%d) · trades=%u</p>"
        "<p><strong>RWS-05</strong> ticker24h: msgs %u · gap %us · recent %s · hb <code>%s</code> / <code>%s</code></p>",
        app ? app->version : "?",
        ipbuf[0] ? ipbuf : "—",
        net_runtime::has_ip() ? "ja" : "nee",
        snap.market_label[0] ? snap.market_label : "—",
        snap.last_tick.price_eur,
        snap.valid ? "ja" : "nee",
        conn_str(snap.connection),
        tick_str(snap.last_tick_source),
        static_cast<unsigned>(snap.ws_raw_msgs_last_sec),
        static_cast<unsigned>(snap.ws_inbound_ticks_last_sec),
        static_cast<unsigned>(snap.ws_gap_sec_since_last_raw),
        static_cast<unsigned>(snap.ws_gap_sec_since_last_canonical),
        snap.ws_official_price_stream[0] ? snap.ws_official_price_stream : "bitvavo_ticker_ws_v1",
        static_cast<unsigned>(snap.ws_trade_events_last_sec),
        static_cast<unsigned>(snap.ws_trades_total_since_boot),
        static_cast<unsigned>(snap.ws_trade_ring_occupancy),
        snap.ws_trade_ring_capacity != 0u ? static_cast<unsigned>(snap.ws_trade_ring_capacity)
                                          : static_cast<unsigned>(64),
        static_cast<unsigned>(snap.ws_trade_ring_drop_total),
        static_cast<unsigned>(snap.ws_gap_sec_since_last_trade),
        static_cast<unsigned long long>(snap.ws_second_agg_wall_sec),
        static_cast<unsigned>(snap.ws_second_agg_trade_count),
        snap.ws_second_agg_mean_eur,
        snap.ws_second_agg_ticker_seen ? "ja" : "nee",
        static_cast<unsigned>(snap.ws_second_ring_used),
        static_cast<unsigned>(snap.ws_second_ring_capacity != 0u ? snap.ws_second_ring_capacity : 32u),
        mio.rws04_enabled ? "aan" : "uit",
        static_cast<unsigned>(mio.seconds_via_trade_mean),
        static_cast<unsigned>(mio.seconds_via_fallback),
        (mio.last_finalize_source && mio.last_finalize_source[0]) ? mio.last_finalize_source : "—",
        static_cast<unsigned long long>(mio.last_compare_wall_sec),
        mio.last_delta_bps,
        (mio.last_ab_class && mio.last_ab_class[0]) ? mio.last_ab_class : "—",
        static_cast<unsigned>(mio.ab_compare_large_delta_total),
        mio.large_delta_threshold_bps,
        static_cast<unsigned>(mio.last_trade_count),
        static_cast<unsigned>(snap.ws_ticker24h_msgs_total),
        static_cast<unsigned>(snap.ws_gap_sec_since_last_ticker24h),
        snap.ws_ticker24h_seen_recently ? "ja" : "nee",
        snap.ws_heartbeat_reason_code[0] ? snap.ws_heartbeat_reason_code : "—",
        snap.ws_heartbeat_source_visibility[0] ? snap.ws_heartbeat_source_visibility : "—");
    if (n <= 0 || static_cast<size_t>(n) >= k_html_alloc) {
        const bool trunc = (n > 0 && static_cast<size_t>(n) >= k_html_alloc);
        ESP_LOGW(TAG,
                 "M-013c/d: HTML deel1 overflow GET / (buf=%u n=%d truncated=%s)",
                 static_cast<unsigned>(k_html_alloc),
                 n,
                 trunc ? "ja" : "nee");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    size_t w = static_cast<size_t>(n);
    const int na = alert_observability::append_alerts_html_section(html + w, k_html_alloc - w);
    if (na < 0 || w + static_cast<size_t>(na) >= k_html_alloc) {
        ESP_LOGW(TAG, "M-013d: HTML alerts-sectie overflow (na=%d)", na);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(na);
    const int na5 = alert_observability::append_alerts_5m_html_section(html + w, k_html_alloc - w);
    if (na5 < 0 || w + static_cast<size_t>(na5) >= k_html_alloc) {
        ESP_LOGW(TAG, "M-010c: HTML 5m alerts-sectie overflow (na5=%d)", na5);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(na5);
    const int na30 = alert_observability::append_alerts_30m_html_section(html + w, k_html_alloc - w);
    if (na30 < 0 || w + static_cast<size_t>(na30) >= k_html_alloc) {
        ESP_LOGW(TAG, "S30-3: HTML 30m alerts-sectie overflow (na30=%d)", na30);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(na30);
    const int na2h = alert_observability::append_alerts_2h_html_section(html + w, k_html_alloc - w);
    if (na2h < 0 || w + static_cast<size_t>(na2h) >= k_html_alloc) {
        ESP_LOGW(TAG, "S2H-3: HTML 2h alerts-sectie overflow (na2h=%d)", na2h);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(na2h);
    const int nac = alert_observability::append_alerts_conf_1m5m_html_section(html + w, k_html_alloc - w);
    if (nac < 0 || w + static_cast<size_t>(nac) >= k_html_alloc) {
        ESP_LOGW(TAG, "M-010d: HTML confluence-sectie overflow (nac=%d)", nac);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(nac);
    {
        alert_engine::RegimeObservabilitySnapshot rob{};
        alert_engine::get_regime_observability_snapshot(&rob);
        const char *fb = rob.vol_unavailable_fallback ? " — <em>vol nog niet klaar: drempels tijdelijk basis (normal ‰)</em>" : "";
        const int nreg = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Regime / drempels (M-013e, read-only)</h2>"
            "<p><strong>Regime</strong> <code>%s</code> · <strong>Vol-metric</strong> %s · "
            "<strong>gem. stap</strong> %.2f bps · <strong>paren</strong> %u%s<br/>"
            "<strong>Grenzen (bps)</strong> calm &lt; %d · hot ≥ %d · "
            "<strong>‰</strong> eff %d (raw %d%s)<br/>"
            "<strong>Laatste regime-wissel (epoch ms)</strong> %lld (C2)<br/>"
            "<strong>Basis 1m / 5m / 30m / 2h</strong> %.3f %% / %.3f %% / %.3f %% / %.3f %% · "
            "<strong>Effectief 1m / 5m / 30m / 2h</strong> %.3f %% / %.3f %% / %.3f %% / %.3f %%<br/>"
            "<small>Confluence: |1m| en |5m| elk ≥ effectieve drempel (zelfde schaal als M-010f). 30m/2h: Kconfig-drempels "
            "(S30-2, S2H-2).</small></p>",
            rob.regime[0] ? rob.regime : "?",
            rob.vol_metric_ready ? "klaar" : "niet klaar",
            rob.vol_mean_abs_step_bps,
            static_cast<unsigned>(rob.vol_pairs_used),
            fb,
            rob.regime_calm_max_step_bps,
            rob.regime_hot_min_step_bps,
            rob.threshold_scale_permille,
            rob.threshold_scale_permille_raw,
            rob.threshold_scale_clamped ? ", clamp" : "",
            (long long)rob.last_regime_change_epoch_ms,
            rob.base_threshold_move_pct_1m,
            rob.base_threshold_move_pct_5m,
            rob.base_threshold_move_pct_30m,
            rob.base_threshold_move_pct_2h,
            rob.effective_threshold_move_pct_1m,
            rob.effective_threshold_move_pct_5m,
            rob.effective_threshold_move_pct_30m,
            rob.effective_threshold_move_pct_2h);
        if (nreg <= 0 || w + static_cast<size_t>(nreg) >= k_html_alloc) {
            ESP_LOGW(TAG, "M-013e: HTML regime-sectie overflow (nreg=%d)", nreg);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nreg);
    }
    {
        alert_engine::AlertDecisionObservabilitySnapshot ado{};
        alert_engine::get_alert_decision_observability_snapshot(&ado);
        char sfx1[88]{};
        char sfx5[88]{};
        char sfx30[88]{};
        char sfx2h[88]{};
        char sfxc[88]{};
        if (ado.tf_1m.reason[0] != '\0') {
            std::snprintf(sfx1, sizeof(sfx1), " · <code>%s</code>", ado.tf_1m.reason);
        }
        if (ado.tf_5m.reason[0] != '\0') {
            std::snprintf(sfx5, sizeof(sfx5), " · <code>%s</code>", ado.tf_5m.reason);
        }
        if (ado.tf_30m.reason[0] != '\0') {
            std::snprintf(sfx30, sizeof(sfx30), " · <code>%s</code>", ado.tf_30m.reason);
        }
        if (ado.tf_2h.reason[0] != '\0') {
            std::snprintf(sfx2h, sizeof(sfx2h), " · <code>%s</code>", ado.tf_2h.reason);
        }
        if (ado.confluence_1m5m.reason[0] != '\0') {
            std::snprintf(sfxc, sizeof(sfxc), " · <code>%s</code>", ado.confluence_1m5m.reason);
        }
        char cd1[28]{};
        char sup1[28]{};
        char cd5[28]{};
        char sup5[28]{};
        char cd30[28]{};
        char sup30[28]{};
        char cd2h[28]{};
        char sup2h[28]{};
        char cdc[28]{};
        char supc[28]{};
        fmt_rem_ms(cd1, sizeof(cd1), ado.tf_1m.remaining_cooldown_ms);
        fmt_rem_ms(sup1, sizeof(sup1), ado.tf_1m.remaining_suppress_ms);
        fmt_rem_ms(cd5, sizeof(cd5), ado.tf_5m.remaining_cooldown_ms);
        fmt_rem_ms(sup5, sizeof(sup5), ado.tf_5m.remaining_suppress_ms);
        fmt_rem_ms(cd30, sizeof(cd30), ado.tf_30m.remaining_cooldown_ms);
        fmt_rem_ms(sup30, sizeof(sup30), ado.tf_30m.remaining_suppress_ms);
        fmt_rem_ms(cd2h, sizeof(cd2h), ado.tf_2h.remaining_cooldown_ms);
        fmt_rem_ms(sup2h, sizeof(sup2h), ado.tf_2h.remaining_suppress_ms);
        fmt_rem_ms(cdc, sizeof(cdc), ado.confluence_1m5m.remaining_cooldown_ms);
        fmt_rem_ms(supc, sizeof(supc), ado.confluence_1m5m.remaining_suppress_ms);
        const int nd = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Alert-beslissing (M-013h, read-only)</h2>"
            "<p class=\"hint\">Snapshot per tick uit <code>alert_engine</code> — geen aparte WebUI-logica.</p>"
            "<p><strong>1m</strong> <code>%s</code>%s<br/>"
            "<small>Cooldown rest · %s · suppress rest · %s</small></p>"
            "<p><strong>5m</strong> <code>%s</code>%s<br/>"
            "<small>Cooldown rest · %s · suppress rest · %s</small></p>"
            "<p><strong>30m</strong> <code>%s</code>%s<br/>"
            "<small>Cooldown rest · %s · suppress rest · %s</small></p>"
            "<p><strong>2h</strong> <code>%s</code>%s<br/>"
            "<small>Cooldown rest · %s · suppress rest · %s</small></p>"
            "<p><strong>Confluence 1m+5m</strong> <code>%s</code>%s<br/>"
            "<small>Cooldown rest · %s · suppress rest · %s</small></p>",
            ado.tf_1m.status[0] ? ado.tf_1m.status : "?",
            sfx1,
            cd1,
            sup1,
            ado.tf_5m.status[0] ? ado.tf_5m.status : "?",
            sfx5,
            cd5,
            sup5,
            ado.tf_30m.status[0] ? ado.tf_30m.status : "?",
            sfx30,
            cd30,
            sup30,
            ado.tf_2h.status[0] ? ado.tf_2h.status : "?",
            sfx2h,
            cd2h,
            sup2h,
            ado.confluence_1m5m.status[0] ? ado.confluence_1m5m.status : "?",
            sfxc,
            cdc,
            supc);
        if (nd <= 0 || w + static_cast<size_t>(nd) >= k_html_alloc) {
            ESP_LOGW(TAG, "M-013h: HTML alert-beslissing overflow (nd=%d)", nd);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nd);
    }
    {
        alert_engine::AlertEngineRuntimeStatsSnapshot ars{};
        alert_engine::get_alert_runtime_stats_snapshot(&ars);
        const int nst = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Alert-engine runtime-statistieken (C1/C2, read-only)</h2>"
            "<p class=\"hint\">Sinds boot: emits, suppress-episodes (M-010e), edge-transities (cooldown/suppressed/not_ready). "
            "Zie <code>GET /api/status.json</code> → <code>alert_engine_runtime_stats</code>.</p>"
            "<p><strong>Emits</strong> 1m=%u · 5m=%u · 30m=%u · 2h=%u · confluence=%u<br/>"
            "<strong>Laatste emit (epoch ms)</strong> 1m=%lld · 5m=%lld · 30m=%lld · 2h=%lld · conf=%lld<br/>"
            "<strong>Suppress-episodes (na conf)</strong> 1m=%u · 5m=%u<br/>"
            "<strong>Edge-transities</strong> 1m cd/sup/nr=%u/%u/%u · 5m=%u/%u/%u · 30m=%u/%u/%u · 2h=%u/%u/%u · conf=%u/%u/%u</p>",
            static_cast<unsigned>(ars.emit_total_1m),
            static_cast<unsigned>(ars.emit_total_5m),
            static_cast<unsigned>(ars.emit_total_30m),
            static_cast<unsigned>(ars.emit_total_2h),
            static_cast<unsigned>(ars.emit_total_conf),
            (long long)ars.last_emit_epoch_ms_1m,
            (long long)ars.last_emit_epoch_ms_5m,
            (long long)ars.last_emit_epoch_ms_30m,
            (long long)ars.last_emit_epoch_ms_2h,
            (long long)ars.last_emit_epoch_ms_conf,
            static_cast<unsigned>(ars.suppress_after_conf_window_1m),
            static_cast<unsigned>(ars.suppress_after_conf_window_5m),
            static_cast<unsigned>(ars.edge_1m.enter_cooldown),
            static_cast<unsigned>(ars.edge_1m.enter_suppressed),
            static_cast<unsigned>(ars.edge_1m.enter_not_ready),
            static_cast<unsigned>(ars.edge_5m.enter_cooldown),
            static_cast<unsigned>(ars.edge_5m.enter_suppressed),
            static_cast<unsigned>(ars.edge_5m.enter_not_ready),
            static_cast<unsigned>(ars.edge_30m.enter_cooldown),
            static_cast<unsigned>(ars.edge_30m.enter_suppressed),
            static_cast<unsigned>(ars.edge_30m.enter_not_ready),
            static_cast<unsigned>(ars.edge_2h.enter_cooldown),
            static_cast<unsigned>(ars.edge_2h.enter_suppressed),
            static_cast<unsigned>(ars.edge_2h.enter_not_ready),
            static_cast<unsigned>(ars.edge_confluence.enter_cooldown),
            static_cast<unsigned>(ars.edge_confluence.enter_suppressed),
            static_cast<unsigned>(ars.edge_confluence.enter_not_ready));
        if (nst <= 0 || w + static_cast<size_t>(nst) >= k_html_alloc) {
            ESP_LOGW(TAG, "C1: HTML runtime-stats overflow (nst=%d)", nst);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nst);
    }
    {
        const config_store::AlertRuntimeConfig &ar = config_store::alert_runtime();
        const int nar = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Alert-runtime (M-013g)</h2>"
            "<p class=\"hint\">Zelfde subset als M-003b/M-013f. <strong>Opslaan</strong> schrijft naar NVS; "
            "<strong>alert_engine</strong> gebruikt de waarden vanaf de <strong>volgende tick</strong> "
            "(geen firmware-herstart).</p>"
            "<form id=\"alert-runtime-form\">"
            "<fieldset><legend>Drempels (bps) en regime-schaal (‰)</legend>"
            "<p><label>threshold_1m_bps<br/><input type=\"number\" id=\"ar_1m\" required min=\"%u\" max=\"%u\" "
            "step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>threshold_5m_bps<br/><input type=\"number\" id=\"ar_5m\" required min=\"%u\" max=\"%u\" "
            "step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>regime_calm_scale_permille<br/><input type=\"number\" id=\"ar_calm\" required "
            "min=\"%u\" max=\"%u\" step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>regime_hot_scale_permille<br/><input type=\"number\" id=\"ar_hot\" required "
            "min=\"%u\" max=\"%u\" step=\"1\" value=\"%u\"/></label></p>"
            "<p><button type=\"submit\">Alert-runtime opslaan</button></p>"
            "</fieldset></form>"
            "<p id=\"alert-runtime-msg\"></p>"
            "<script>"
            "(function(){var f=document.getElementById('alert-runtime-form');"
            "var m=document.getElementById('alert-runtime-msg');if(!f||!m)return;"
            "f.addEventListener('submit',function(e){e.preventDefault();m.textContent='Bezig…';m.style.color='#333';"
            "var body=JSON.stringify({"
            "threshold_1m_bps:parseInt(document.getElementById('ar_1m').value,10),"
            "threshold_5m_bps:parseInt(document.getElementById('ar_5m').value,10),"
            "regime_calm_scale_permille:parseInt(document.getElementById('ar_calm').value,10),"
            "regime_hot_scale_permille:parseInt(document.getElementById('ar_hot').value,10)"
            "});"
            "fetch('/api/alert-runtime.json',{method:'POST',headers:{'Content-Type':'application/json'},body:body})"
            ".then(function(r){return r.text().then(function(t){return {ok:r.ok,status:r.status,text:t};});})"
            ".then(function(x){try{var j=JSON.parse(x.text);if(x.ok){m.style.color='#063';"
            "m.textContent='Opgeslagen. '+(j.note||'');}else{m.style.color='#800';"
            "m.textContent='Fout '+x.status+': '+(j.error||x.text);}}catch(err){"
            "m.style.color=x.ok?'#063':'#800';m.textContent=x.ok?x.text:('Fout '+x.status+': '+x.text);}})"
            ".catch(function(err){m.style.color='#800';m.textContent='Netwerkfout: '+err;});});})();"
            "</script>",
            static_cast<unsigned>(config_store::kAlertThreshold1mBpsMin),
            static_cast<unsigned>(config_store::kAlertThreshold1mBpsMax),
            static_cast<unsigned>(ar.threshold_1m_bps),
            static_cast<unsigned>(config_store::kAlertThreshold5mBpsMin),
            static_cast<unsigned>(config_store::kAlertThreshold5mBpsMax),
            static_cast<unsigned>(ar.threshold_5m_bps),
            static_cast<unsigned>(config_store::kAlertRegimeCalmScalePermilleMin),
            static_cast<unsigned>(config_store::kAlertRegimeCalmScalePermilleMax),
            static_cast<unsigned>(ar.regime_calm_scale_permille),
            static_cast<unsigned>(config_store::kAlertRegimeHotScalePermilleMin),
            static_cast<unsigned>(config_store::kAlertRegimeHotScalePermilleMax),
            static_cast<unsigned>(ar.regime_hot_scale_permille));
        if (nar <= 0 || w + static_cast<size_t>(nar) >= k_html_alloc) {
            ESP_LOGW(TAG, "M-013g: HTML alert-runtime-form overflow (nar=%d)", nar);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nar);
    }
    {
        const config_store::AlertPolicyTimingConfig &apt = config_store::alert_policy_timing();
        const int nap = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Alert-policy timing (M-013j)</h2>"
            "<p class=\"hint\">Zelfde subset als M-003c/M-013i (seconden). <strong>Opslaan</strong> schrijft naar NVS; "
            "<strong>alert_engine</strong> gebruikt de waarden vanaf de <strong>volgende tick</strong> "
            "(geen firmware-herstart).</p>"
            "<form id=\"alert-policy-form\">"
            "<fieldset><legend>Cooldowns en suppressie-venster (s)</legend>"
            "<p><label>cooldown_1m_s<br/><input type=\"number\" id=\"ap_cd1\" required min=\"%u\" max=\"%u\" "
            "step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>cooldown_5m_s<br/><input type=\"number\" id=\"ap_cd5\" required min=\"%u\" max=\"%u\" "
            "step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>cooldown_conf_1m5m_s<br/><input type=\"number\" id=\"ap_cf\" required min=\"%u\" max=\"%u\" "
            "step=\"1\" value=\"%u\"/></label></p>"
            "<p><label>suppress_loose_after_conf_s<br/><input type=\"number\" id=\"ap_sup\" required "
            "min=\"%u\" max=\"%u\" step=\"1\" value=\"%u\"/></label></p>"
            "<p><button type=\"submit\">Policy-timing opslaan</button></p>"
            "</fieldset></form>"
            "<p id=\"alert-policy-msg\"></p>"
            "<script>"
            "(function(){var f=document.getElementById('alert-policy-form');"
            "var m=document.getElementById('alert-policy-msg');if(!f||!m)return;"
            "f.addEventListener('submit',function(e){e.preventDefault();m.textContent='Bezig…';m.style.color='#333';"
            "var body=JSON.stringify({"
            "cooldown_1m_s:parseInt(document.getElementById('ap_cd1').value,10),"
            "cooldown_5m_s:parseInt(document.getElementById('ap_cd5').value,10),"
            "cooldown_conf_1m5m_s:parseInt(document.getElementById('ap_cf').value,10),"
            "suppress_loose_after_conf_s:parseInt(document.getElementById('ap_sup').value,10)"
            "});"
            "fetch('/api/alert-policy-timing.json',{method:'POST',headers:{'Content-Type':'application/json'},"
            "body:body})"
            ".then(function(r){return r.text().then(function(t){return {ok:r.ok,status:r.status,text:t};});})"
            ".then(function(x){try{var j=JSON.parse(x.text);if(x.ok){m.style.color='#063';"
            "m.textContent='Opgeslagen. '+(j.note||'');}else{m.style.color='#800';"
            "m.textContent='Fout '+x.status+': '+(j.error||x.text);}}catch(err){"
            "m.style.color=x.ok?'#063':'#800';m.textContent=x.ok?x.text:('Fout '+x.status+': '+x.text);}})"
            ".catch(function(err){m.style.color='#800';m.textContent='Netwerkfout: '+err;});});})();"
            "</script>",
            static_cast<unsigned>(config_store::kAlertPolicyCooldown1mSMin),
            static_cast<unsigned>(config_store::kAlertPolicyCooldown1mSMax),
            static_cast<unsigned>(apt.cooldown_1m_s),
            static_cast<unsigned>(config_store::kAlertPolicyCooldown5mSMin),
            static_cast<unsigned>(config_store::kAlertPolicyCooldown5mSMax),
            static_cast<unsigned>(apt.cooldown_5m_s),
            static_cast<unsigned>(config_store::kAlertPolicyCooldownConfSMin),
            static_cast<unsigned>(config_store::kAlertPolicyCooldownConfSMax),
            static_cast<unsigned>(apt.cooldown_conf_1m5m_s),
            static_cast<unsigned>(config_store::kAlertPolicySuppressLooseSMin),
            static_cast<unsigned>(config_store::kAlertPolicySuppressLooseSMax),
            static_cast<unsigned>(apt.suppress_loose_after_conf_s));
        if (nap <= 0 || w + static_cast<size_t>(nap) >= k_html_alloc) {
            ESP_LOGW(TAG, "M-013j: HTML alert-policy-form overflow (nap=%d)", nap);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nap);
    }
    {
        const config_store::AlertConfluencePolicyConfig &acf = config_store::alert_confluence_policy();
        const char *cf_en = acf.confluence_enabled ? " checked" : "";
        const char *cf_sd = acf.confluence_require_same_direction ? " checked" : "";
        const char *cf_bt = acf.confluence_require_both_thresholds ? " checked" : "";
        const char *cf_lo = acf.confluence_emit_loose_alerts_when_conf_fails ? " checked" : "";
        const int nacf = std::snprintf(
            html + w,
            k_html_alloc - w,
            "<h2>Confluence-policy (M-013l)</h2>"
            "<p class=\"hint\">Zelfde subset als M-003d/M-013k (booleans). <strong>Opslaan</strong> schrijft naar NVS; "
            "<strong>alert_engine</strong> gebruikt de waarden vanaf de <strong>volgende tick</strong> "
            "(geen firmware-herstart).</p>"
            "<form id=\"alert-confluence-form\">"
            "<fieldset><legend>Confluence-gedrag</legend>"
            "<p><label><input type=\"checkbox\" id=\"acf_en\"%s/> confluence_enabled</label></p>"
            "<p><label><input type=\"checkbox\" id=\"acf_sd\"%s/> confluence_require_same_direction</label></p>"
            "<p><label><input type=\"checkbox\" id=\"acf_bt\"%s/> confluence_require_both_thresholds</label></p>"
            "<p><label><input type=\"checkbox\" id=\"acf_lo\"%s/> confluence_emit_loose_alerts_when_conf_fails</label></p>"
            "<p><button type=\"submit\">Confluence-policy opslaan</button></p>"
            "</fieldset></form>"
            "<p id=\"alert-conf-msg\"></p>"
            "<script>"
            "(function(){var f=document.getElementById('alert-confluence-form');"
            "var m=document.getElementById('alert-conf-msg');if(!f||!m)return;"
            "f.addEventListener('submit',function(e){e.preventDefault();m.textContent='Bezig…';m.style.color='#333';"
            "var body=JSON.stringify({"
            "confluence_enabled:document.getElementById('acf_en').checked,"
            "confluence_require_same_direction:document.getElementById('acf_sd').checked,"
            "confluence_require_both_thresholds:document.getElementById('acf_bt').checked,"
            "confluence_emit_loose_alerts_when_conf_fails:document.getElementById('acf_lo').checked"
            "});"
            "fetch('/api/alert-confluence-policy.json',{method:'POST',headers:{'Content-Type':'application/json'},"
            "body:body})"
            ".then(function(r){return r.text().then(function(t){return {ok:r.ok,status:r.status,text:t};});})"
            ".then(function(x){try{var j=JSON.parse(x.text);if(x.ok){m.style.color='#063';"
            "m.textContent='Opgeslagen. '+(j.note||'');}else{m.style.color='#800';"
            "m.textContent='Fout '+x.status+': '+(j.error||x.text);}}catch(err){"
            "m.style.color=x.ok?'#063':'#800';m.textContent=x.ok?x.text:('Fout '+x.status+': '+x.text);}})"
            ".catch(function(err){m.style.color='#800';m.textContent='Netwerkfout: '+err;});});})();"
            "</script>",
            cf_en,
            cf_sd,
            cf_bt,
            cf_lo);
        if (nacf <= 0 || w + static_cast<size_t>(nacf) >= k_html_alloc) {
            ESP_LOGW(TAG, "M-013l: HTML confluence-policy-form overflow (nacf=%d)", nacf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
            return ESP_FAIL;
        }
        w += static_cast<size_t>(nacf);
    }
    n = std::snprintf(
        html + w, k_html_alloc - w,
        "<p><a href=\"/api/status.json\"><code>/api/status.json</code></a> "
        "(incl. OTA, alerts_1m/5m/conf, <code>regime_observability</code>, "
        "<code>alert_decision_observability</code> M-013h, "
        "<code>alert_runtime_config</code> M-003b, <code>alert_policy_timing</code> M-003c, "
        "<code>alert_confluence_policy</code> M-003d; "
        "POST <code>/api/alert-runtime.json</code> M-013f/g, <code>/api/alert-policy-timing.json</code> M-013i/j, "
        "<code>/api/alert-confluence-policy.json</code> M-013k/l; "
        "M-014b)</p>"
        "<h2>OTA / boot</h2>"
        "<p><strong>Partitie</strong> %s @ 0x%08X · <strong>img-state</strong> %s<br/>"
        "<strong>Volgende update-slot</strong> %s · <strong>Reset</strong> %s<br/>"
        "<strong>Post-boot</strong> %s</p>"
        "<h2>Services (mqtt / ntfy)</h2>"
        "<p class=\"hint\">MQTT: na wijziging URI of inlog <strong>herstart</strong> — client start bij boot. "
        "Wachtwoord: leeg laten om het opgeslagen wachtwoord niet te wijzigen. "
        "NTFY: topic geldt bij de volgende push.</p>"
        "<form id=\"svc-form\">"
        "<fieldset><legend>Instellingen</legend>"
        "<p><label><input type=\"checkbox\" id=\"mqtt_enabled\"%s/> MQTT aan</label></p>"
        "<p><label>Broker-URI<br/><input type=\"text\" id=\"mqtt_broker_uri\" value=\"%s\" "
        "maxlength=\"%u\" size=\"56\" placeholder=\"mqtt://… of mqtts://…\"/></label></p>"
        "<p><label>MQTT-gebruiker (optioneel; leeg = build-default uit Kconfig)<br/>"
        "<input type=\"text\" id=\"mqtt_username\" value=\"%s\" maxlength=\"%u\" size=\"40\" "
        "autocomplete=\"username\"/></label></p>"
        "<p><label>MQTT-wachtwoord (optioneel; leeg = niet wijzigen)<br/>"
        "<input type=\"password\" id=\"mqtt_password\" maxlength=\"%u\" size=\"28\" "
        "autocomplete=\"new-password\" placeholder=\"alleen bij nieuw wachtwoord\"/></label></p>"
        "<p><label><input type=\"checkbox\" id=\"mqtt_pw_clear\"/> Opgeslagen MQTT-wachtwoord wissen (NVS; daarna "
        "fallback Kconfig)</label></p>"
        "<p><label><input type=\"checkbox\" id=\"ntfy_enabled\"%s/> NTFY aan</label></p>"
        "<p><label>NTFY-topic<br/><input type=\"text\" id=\"ntfy_topic\" value=\"%s\" "
        "maxlength=\"%u\" size=\"40\"/></label></p>"
        "<p><button type=\"submit\">Opslaan</button></p>"
        "</fieldset></form>"
        "<p id=\"svc-msg\"></p>"
        "<script>"
        "(function(){var f=document.getElementById('svc-form');var m=document.getElementById('svc-msg');"
        "f.addEventListener('submit',function(e){e.preventDefault();m.textContent='Bezig…';m.style.color='#333';"
        "var p={mqtt_enabled:document.getElementById('mqtt_enabled').checked,"
        "mqtt_broker_uri:document.getElementById('mqtt_broker_uri').value,"
        "mqtt_username:document.getElementById('mqtt_username').value,"
        "ntfy_enabled:document.getElementById('ntfy_enabled').checked,"
        "ntfy_topic:document.getElementById('ntfy_topic').value};"
        "var mpw=document.getElementById('mqtt_password').value;"
        "var cl=document.getElementById('mqtt_pw_clear').checked;"
        "if(cl){p.mqtt_password='';}else if(mpw.length>0){p.mqtt_password=mpw;}"
        "var body=JSON.stringify(p);"
        "fetch('/api/services.json',{method:'POST',headers:{'Content-Type':'application/json'},body:body})"
        ".then(function(r){return r.text().then(function(t){return {ok:r.ok,status:r.status,text:t};});})"
        ".then(function(x){try{var j=JSON.parse(x.text);if(x.ok){m.style.color='#063';"
        "m.textContent='Opgeslagen. '+(j.note||'');}else{m.style.color='#800';"
        "m.textContent='Fout '+x.status+': '+(j.error||x.text);}}catch(err){"
        "m.style.color=x.ok?'#063':'#800';m.textContent=x.ok?x.text:('Fout '+x.status+': '+x.text);}})"
        ".catch(function(err){m.style.color='#800';m.textContent='Netwerkfout: '+err;});});})();"
        "</script>"
        "<h2>OTA-firmware</h2>"
        "<p style=\"color:#666;font-size:.9rem\">Geen login — alleen op een vertrouwd netwerk. Gebruik het "
        "<code>build/*.bin</code> van <code>idf.py build</code>. Stroom stabiel houden.</p>"
        "<p><input type=\"file\" id=\"ota-file\" accept=\".bin,.BIN\"/></p>"
        "<p><button type=\"button\" id=\"ota-btn\">Firmware uploaden</button></p>"
        "<p id=\"ota-msg\"></p>"
        "<script>(function(){var b=document.getElementById('ota-btn');if(!b)return;"
        "b.addEventListener('click',function(){var f=document.getElementById('ota-file');"
        "var o=document.getElementById('ota-msg');if(!f||!f.files.length){o.textContent='Kies een .bin';"
        "o.style.color='#800';return;}o.textContent='Upload…';o.style.color='#333';"
        "fetch('/api/ota',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f.files[0]})"
        ".then(function(r){return r.text().then(function(t){return {ok:r.ok,s:r.status,t:t};});})"
        ".then(function(x){o.style.color=x.ok?'#063':'#800';o.textContent=x.t;})"
        ".catch(function(e){o.style.color='#800';o.textContent=''+e;});});})();</script>"
        "</body></html>",
        ota.running_label[0] ? ota.running_label : "?",
        (unsigned)ota.running_address,
        ota.img_state[0] ? ota.img_state : "?",
        ota.next_update_label[0] ? ota.next_update_label : "?",
        ota.reset_reason[0] ? ota.reset_reason : "?",
        boot_confirm_nl(ota.boot_confirm),
        mq_chk,
        esc_uri,
        static_cast<unsigned>(config_store::kMqttBrokerUriMax - 1),
        esc_muser,
        static_cast<unsigned>(config_store::kMqttUserMax - 1),
        static_cast<unsigned>(config_store::kMqttPassMax - 1),
        nt_chk,
        esc_topic,
        static_cast<unsigned>(config_store::kNtfyTopicMax - 1));
    if (n <= 0 || w + static_cast<size_t>(n) >= k_html_alloc) {
        const size_t cap_left = (w < k_html_alloc) ? (k_html_alloc - w) : 0U;
        const bool trunc = (n > 0 && static_cast<size_t>(n) >= cap_left);
        ESP_LOGW(TAG,
                 "M-013c/d: HTML deel2 overflow GET / (buf=%u w=%u cap_left=%u n=%d truncated=%s)",
                 static_cast<unsigned>(k_html_alloc),
                 static_cast<unsigned>(w),
                 static_cast<unsigned>(cap_left),
                 n,
                 trunc ? "ja" : "nee");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    w += static_cast<size_t>(n);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const esp_err_t send_err = httpd_resp_send(req, html, w);
    return send_err;
}

static esp_err_t handle_root_html(httpd_req_t *req)
{
    if (!s_root_html_mx) {
        s_root_html_mx = xSemaphoreCreateMutex();
    }
    if (!s_root_html_mx) {
        ESP_LOGW(TAG,
                 "GET /: mutex create failed (heap=%u B)",
                 static_cast<unsigned>(esp_get_free_heap_size()));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_ERR_NO_MEM;
    }
    if (xSemaphoreTake(s_root_html_mx, pdMS_TO_TICKS(15000)) != pdTRUE) {
        ESP_LOGW(TAG,
                 "GET /: homepage buffer bezet — timeout (heap=%u B)",
                 static_cast<unsigned>(esp_get_free_heap_size()));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "busy");
        return ESP_FAIL;
    }
    const esp_err_t err = handle_root_html_impl(req);
    xSemaphoreGive(s_root_html_mx);
    return err;
}

static esp_err_t handle_ota_post(httpd_req_t *req)
{
    return ota_service::handle_firmware_upload(req);
}

#endif // CONFIG_WEBUI_ENABLE

esp_err_t init()
{
#if !CONFIG_WEBUI_ENABLE
    ESP_LOGD(TAG, "webui uit (Kconfig build)");
    return ESP_OK;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.webui_enabled) {
        ESP_LOGI(TAG, "webui uit (runtime M-003a)");
        return ESP_OK;
    }
    if (s_httpd) {
        return ESP_OK;
    }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    const int port_cfg = static_cast<int>(svc.webui_port);
    uint16_t port = svc.webui_port;
    if (port_cfg < 1024 || port_cfg > 65535) {
        port = 8080;
    }
    cfg.server_port = port;
    cfg.max_uri_handlers = 14;
    cfg.lru_purge_enable = true;
    /** M-014a: OTA-handler gebruikt ~1 KiB recv-buffer op httpd-taskstack (default 4096 is krap). */
    cfg.stack_size = 8192;

    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "httpd_start");

    if (!s_root_html_mx) {
        s_root_html_mx = xSemaphoreCreateMutex();
    }

    httpd_uri_t uj{};
    uj.uri = "/api/status.json";
    uj.method = HTTP_GET;
    uj.handler = handle_status_json;
    uj.user_ctx = nullptr;

    httpd_uri_t uh{};
    uh.uri = "/";
    uh.method = HTTP_GET;
    uh.handler = handle_root_html;
    uh.user_ctx = nullptr;

    httpd_uri_t us{};
    us.uri = "/api/services.json";
    us.method = HTTP_POST;
    us.handler = handle_services_post;
    us.user_ctx = nullptr;

    httpd_uri_t ua{};
    ua.uri = "/api/alert-runtime.json";
    ua.method = HTTP_POST;
    ua.handler = handle_alert_runtime_post;
    ua.user_ctx = nullptr;

    httpd_uri_t uap{};
    uap.uri = "/api/alert-policy-timing.json";
    uap.method = HTTP_POST;
    uap.handler = handle_alert_policy_timing_post;
    uap.user_ctx = nullptr;

    httpd_uri_t uacf{};
    uacf.uri = "/api/alert-confluence-policy.json";
    uacf.method = HTTP_POST;
    uacf.handler = handle_alert_confluence_policy_post;
    uacf.user_ctx = nullptr;

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uj), TAG, "reg json");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uh), TAG, "reg html");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &us), TAG, "reg services post");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &ua), TAG, "reg alert-runtime post");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uap), TAG, "reg alert-policy-timing post");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uacf), TAG, "reg alert-confluence-policy post");

    httpd_uri_t uo{};
    uo.uri = "/api/ota";
    uo.method = HTTP_POST;
    uo.handler = handle_ota_post;
    uo.user_ctx = nullptr;
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uo), TAG, "reg ota post");

    ESP_LOGI(TAG,
             "M-013a–l + M-014a/b: webui poort %u — status+alerts+OTA, services, alert-runtime + "
             "alert-policy + confluence-policy POST+forms, OTA-upload",
             static_cast<unsigned>(port));
    return ESP_OK;
#endif
}

} // namespace webui
