/**
 * M-013a: minimale WebUI — GET / en GET /api/status.json.
 * M-013b: POST /api/services.json — kleine niet-geheime mqtt/ntfy-subset naar config_store.
 */
#include "webui/webui.hpp"
#include "config_store/config_store.hpp"
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
#include "sdkconfig.h"

#if CONFIG_WEBUI_ENABLE
#include "lwip/def.h"
#include <cstdio>
#include <cstring>
#endif

namespace webui {

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

static esp_err_t handle_services_post(httpd_req_t *req)
{
    char raw[1024]{};
    const int need = req->content_len;
    if (need <= 0 || need >= static_cast<int>(sizeof(raw))) {
        ESP_LOGW(TAG, "M-013b: POST body ontbreekt of te groot (%d)", need);
        return send_json_text(req, "400 Bad Request",
                              "{\"ok\":false,\"error\":\"body ontbreekt of te groot (max 1023 bytes)\"}");
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
                              "ntfy_enabled, ntfy_topic\"}");
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
    cJSON_AddBoolToObject(out, "ntfy_enabled", s.ntfy_enabled);
    cJSON_AddStringToObject(out, "ntfy_topic", s.ntfy_topic);
    cJSON_AddStringToObject(
        out, "note",
        "NTFY: volgende push gebruikt dit topic. MQTT: nieuwe broker-URI wordt pas na herstart actief "
        "(geen hot-reload in M-013b).");
    char *printed = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!printed) {
        return send_json_text(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"geen geheugen\"}");
    }
    const esp_err_t se = send_json_text(req, "200 OK", printed);
    cJSON_free(printed);
    return se;
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
        "<p>JSON: <a href=\"/api/status.json\"><code>/api/status.json</code></a> · "
        "<code>POST /api/services.json</code> (mqtt/ntfy, M-013b)</p>"
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

    httpd_uri_t us{};
    us.uri = "/api/services.json";
    us.method = HTTP_POST;
    us.handler = handle_services_post;
    us.user_ctx = nullptr;

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uj), TAG, "reg json");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uh), TAG, "reg html");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &us), TAG, "reg services post");

    ESP_LOGI(TAG, "M-013a/b: webui op poort %u — GET status + POST services (M-013b)", static_cast<unsigned>(port));
    return ESP_OK;
#endif
}

} // namespace webui
