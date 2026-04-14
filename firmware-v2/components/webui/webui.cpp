/**
 * M-013a: minimale read-only WebUI — alleen GET / en GET /api/status.json.
 * Leest `market_data::snapshot()` en STA-IP; geen exchange- of MQTT/NTFY-koppeling.
 */
#include "webui/webui.hpp"
#include "config_store/config_store.hpp"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "market_data/market_data.hpp"
#include "market_types/types.hpp"
#include "net_runtime/net_runtime.hpp"
#include "sdkconfig.h"

#if CONFIG_WEBUI_ENABLE
#include "lwip/def.h"
#include <cstdio>
#include <cstring>
#endif

namespace webui {

namespace {

static const char TAG[] = "web_ui";

#if CONFIG_WEBUI_ENABLE

static httpd_handle_t s_httpd{nullptr};

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

    char body[896]{};
    const int n = snprintf(
        body, sizeof(body),
        "{\"app\":\"CryptoAlert V2\",\"version\":\"%s\",\"has_ip\":%s,\"ip\":\"%s\","
        "\"symbol\":\"%s\",\"price_eur\":%.4f,\"valid\":%s,\"connection\":\"%s\","
        "\"tick_source\":\"%s\",\"last_tick_ms\":%lld}",
        app ? app->version : "?",
        net_runtime::has_ip() ? "true" : "false",
        ipbuf,
        snap.market_label[0] ? snap.market_label : "—",
        snap.last_tick.price_eur,
        snap.valid ? "true" : "false",
        conn_str(snap.connection),
        tick_str(snap.last_tick_source),
        (long long)snap.last_tick.ts_ms);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, body, static_cast<size_t>(n));
}

static esp_err_t handle_root_html(httpd_req_t *req)
{
    const market_data::MarketSnapshot snap = market_data::snapshot();
    char ipbuf[20]{};
    sta_ip_str(ipbuf, sizeof(ipbuf));
    const esp_app_desc_t *app = esp_app_get_description();

    char html[1400]{};
    const int n = snprintf(
        html, sizeof(html),
        "<!DOCTYPE html><html lang=\"nl\"><head><meta charset=\"utf-8\"/>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
        "<title>CryptoAlert V2</title><style>body{font-family:system-ui,sans-serif;"
        "margin:1rem;line-height:1.4}code{background:#eee;padding:2px 6px}</style></head><body>"
        "<h1>CryptoAlert V2</h1>"
        "<p><strong>Versie</strong> %s · <strong>IP</strong> %s · <strong>WiFi IP bekend</strong> %s</p>"
        "<p><strong>Symbool</strong> %s · <strong>Prijs (EUR)</strong> %.4f · <strong>Geldig</strong> %s</p>"
        "<p><strong>Verbinding feed</strong> %s · <strong>Bron tick</strong> %s</p>"
        "<p>JSON: <a href=\"/api/status.json\"><code>/api/status.json</code></a> (read-only)</p>"
        "</body></html>",
        app ? app->version : "?",
        ipbuf[0] ? ipbuf : "—",
        net_runtime::has_ip() ? "ja" : "nee",
        snap.market_label[0] ? snap.market_label : "—",
        snap.last_tick.price_eur,
        snap.valid ? "ja" : "nee",
        conn_str(snap.connection),
        tick_str(snap.last_tick_source));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(html)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "overflow");
        return ESP_FAIL;
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, static_cast<size_t>(n));
}

#endif // CONFIG_WEBUI_ENABLE

} // namespace

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
    uint16_t port = svc.webui_port;
    if (port < 1024 || port > 65535) {
        port = 8080;
    }
    cfg.server_port = port;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "httpd_start");

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

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uj), TAG, "reg json");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uh), TAG, "reg html");

    ESP_LOGI(TAG, "M-013a: read-only webui op poort %u (M-003a runtime)", static_cast<unsigned>(port));
    return ESP_OK;
#endif
}

} // namespace webui
