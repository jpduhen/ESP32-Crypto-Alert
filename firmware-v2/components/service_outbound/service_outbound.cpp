/**
 * M-002c / M-011a / M-011b / M-012a / M-012b / M-010c / M-010d / M-013d: outbound queue + dispatch; sinks: ntfy + mqtt (Kconfig).
 */
#include "alert_observability/alert_observability.hpp"
#include "service_outbound/service_outbound.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_bridge/mqtt_bridge.hpp"
#include "ntfy_client/ntfy_client.hpp"
#include "sdkconfig.h"
#include "esp_timer.h"

#include <cinttypes>
#include <cstdio>

namespace service_outbound {

namespace {

static const char TAG[] = "svc_out";

/** Diepte klein houden: alleen coalescing + toekomstige bursts; geen grote backlog. */
static constexpr UBaseType_t k_queue_depth = 8;

/**
 * M-002 hardening: niet alle wachtende events in één `poll()` naar sinks sturen — elke dispatch kan
 * lang blokkeren (NTFY HTTPS onder net_mutex). Spreiding over meerdere app_core-ticks (~10 Hz).
 */
static constexpr unsigned k_max_dispatch_per_poll = 2;

/** Rate-limit backlog-waarschuwing (µs monotonic). */
static constexpr uint64_t k_backlog_log_interval_us = 5000000ULL;

static bool s_ready{false};
static uint32_t s_drop_total{0};
static uint64_t s_last_backlog_log_us{0};
static QueueHandle_t s_q{nullptr};
static bool s_app_ready_seen{false};

static void dispatch_domain_alert_ntfy(const DomainAlert1mMovePayload &p)
{
    const char *sym = p.symbol[0] != '\0' ? p.symbol : "?";
    const char *dir = p.up ? "UP" : "DOWN";
    char title[80];
    snprintf(title, sizeof(title), "CryptoAlert V2 · 1m · %s · %s", dir, sym);

    char body[256];
    snprintf(body,
             sizeof(body),
             "%s\n"
             "Soort: alert_1m\n"
             "Prijs: %.4f EUR\n"
             "1m: %+.4f %%\n"
             "ts_ms: %lld",
             sym,
             p.price_eur,
             p.pct_1m,
             (long long)p.ts_ms);

    ESP_LOGI(TAG,
             "M-011b: DomainAlert1mMove → ntfy (sym=%s %s pct=%.4f price=%.4f)",
             sym,
             dir,
             p.pct_1m,
             p.price_eur);

#if CONFIG_NTFY_CLIENT_ENABLE
    const esp_err_t n = ntfy_client::send_notification(title, body);
    if (n == ESP_OK) {
        ESP_LOGI(TAG, "M-011b: NTFY domain alert afgerond (ntfy_client ESP_OK)");
    } else {
        ESP_LOGW(TAG, "M-011b: NTFY domain alert mislukt: %s", esp_err_to_name(n));
    }
#else
    (void)title;
    (void)body;
    ESP_LOGI(TAG, "M-011b: NTFY build uit (Kconfig) — domain alert niet verstuurd");
#endif
}

static void dispatch_domain_alert_5m_ntfy(const DomainAlert5mMovePayload &p)
{
    const char *sym = p.symbol[0] != '\0' ? p.symbol : "?";
    const char *dir = p.up ? "UP" : "DOWN";
    char title[80];
    snprintf(title, sizeof(title), "CryptoAlert V2 · 5m · %s · %s", dir, sym);

    char body[256];
    snprintf(body,
             sizeof(body),
             "%s\n"
             "Soort: alert_5m\n"
             "Prijs: %.4f EUR\n"
             "5m: %+.4f %%\n"
             "ts_ms: %lld",
             sym,
             p.price_eur,
             p.pct_5m,
             (long long)p.ts_ms);

    ESP_LOGI(TAG,
             "M-010c: DomainAlert5mMove → ntfy (sym=%s %s pct=%.4f price=%.4f)",
             sym,
             dir,
             p.pct_5m,
             p.price_eur);

#if CONFIG_NTFY_CLIENT_ENABLE
    const esp_err_t n = ntfy_client::send_notification(title, body);
    if (n == ESP_OK) {
        ESP_LOGI(TAG, "M-010c: NTFY 5m domain alert afgerond (ESP_OK)");
    } else {
        ESP_LOGW(TAG, "M-010c: NTFY 5m domain alert mislukt: %s", esp_err_to_name(n));
    }
#else
    (void)title;
    (void)body;
    ESP_LOGI(TAG, "M-010c: NTFY build uit — 5m domain alert niet verstuurd");
#endif
}

static void dispatch_confluence_ntfy(const DomainConfluence1m5mPayload &p)
{
    const char *sym = p.symbol[0] != '\0' ? p.symbol : "?";
    const char *dir = p.up ? "UP" : "DOWN";
    char title[112];
    snprintf(title,
             sizeof(title),
             "CryptoAlert V2 · confluence 1m+5m · %s · %s",
             dir,
             sym);

    char body[288];
    snprintf(body,
             sizeof(body),
             "%s\n"
             "Soort: alert_confluence_1m5m\n"
             "Prijs: %.4f EUR\n"
             "1m: %+.4f %%\n"
             "5m: %+.4f %%\n"
             "ts_ms: %lld",
             sym,
             p.price_eur,
             p.pct_1m,
             p.pct_5m,
             (long long)p.ts_ms);

    ESP_LOGI(TAG,
             "M-010d: DomainAlertConfluence1m5m → ntfy (sym=%s %s 1m=%.4f 5m=%.4f)",
             sym,
             dir,
             p.pct_1m,
             p.pct_5m);

#if CONFIG_NTFY_CLIENT_ENABLE
    const esp_err_t n = ntfy_client::send_notification(title, body);
    if (n == ESP_OK) {
        ESP_LOGI(TAG, "M-010d: NTFY confluence afgerond (ESP_OK)");
    } else {
        ESP_LOGW(TAG, "M-010d: NTFY confluence mislukt: %s", esp_err_to_name(n));
    }
#else
    (void)title;
    (void)body;
    ESP_LOGI(TAG, "M-010d: NTFY build uit — confluence niet verstuurd");
#endif
}

static void dispatch_envelope(const OutboundEnvelope &env)
{
    switch (env.kind) {
    case Event::None:
        break;
    case Event::ApplicationReady:
        if (!s_app_ready_seen) {
            s_app_ready_seen = true;
            ESP_LOGI(TAG, "dispatch ApplicationReady → outbound sinks");
#if CONFIG_NTFY_CLIENT_ENABLE
            const esp_err_t n =
                ntfy_client::send_notification("CryptoAlert V2", "Application ready");
            if (n != ESP_OK) {
                ESP_LOGW(TAG, "NTFY publish: %s", esp_err_to_name(n));
            }
#else
            ESP_LOGD(TAG, "NTFY uit (Kconfig)");
#endif
#if CONFIG_MQTT_BRIDGE_ENABLE
            mqtt_bridge::request_application_ready_publish();
#else
            ESP_LOGD(TAG, "MQTT bridge uit (Kconfig)");
#endif
        }
        break;
    case Event::DomainAlert1mMove: {
        const auto &d = env.domain_1m;
        alert_observability::record_1m_alert(d.symbol, d.up, d.price_eur, d.pct_1m, d.ts_ms);
        const char *sym = d.symbol[0] != '\0' ? d.symbol : "?";
        ESP_LOGI(TAG,
                 "M-012b: DomainAlert1mMove ontvangen (sym=%s up=%d pct_1m=%.4f)",
                 sym,
                 d.up ? 1 : 0,
                 d.pct_1m);
        dispatch_domain_alert_ntfy(env.domain_1m);
#if CONFIG_MQTT_BRIDGE_ENABLE
        ESP_LOGI(TAG, "M-012b: MQTT domain alert 1m publish aanroepen");
        mqtt_bridge::publish_domain_alert_1m(d.symbol,
                                               d.up,
                                               d.price_eur,
                                               d.pct_1m,
                                               d.ts_ms);
#else
        ESP_LOGD(TAG, "M-012b: MQTT bridge uit (Kconfig) — geen domain alert MQTT");
#endif
        break;
    }
    case Event::DomainAlert5mMove: {
        const auto &d = env.domain_5m;
        alert_observability::record_5m_alert(d.symbol, d.up, d.price_eur, d.pct_5m, d.ts_ms);
        const char *sym = d.symbol[0] != '\0' ? d.symbol : "?";
        ESP_LOGI(TAG,
                 "M-010c: DomainAlert5mMove ontvangen (sym=%s up=%d pct_5m=%.4f)",
                 sym,
                 d.up ? 1 : 0,
                 d.pct_5m);
        dispatch_domain_alert_5m_ntfy(env.domain_5m);
#if CONFIG_MQTT_BRIDGE_ENABLE
        ESP_LOGI(TAG, "M-010c: MQTT domain alert 5m publish aanroepen");
        mqtt_bridge::publish_domain_alert_5m(d.symbol, d.up, d.price_eur, d.pct_5m, d.ts_ms);
#else
        ESP_LOGD(TAG, "M-010c: MQTT bridge uit — geen 5m MQTT");
#endif
        break;
    }
    case Event::DomainAlertConfluence1m5m: {
        const auto &d = env.domain_conf_1m5m;
        alert_observability::record_conf_1m5m_alert(
            d.symbol, d.up, d.price_eur, d.pct_1m, d.pct_5m, d.ts_ms);
        const char *sym = d.symbol[0] != '\0' ? d.symbol : "?";
        ESP_LOGI(TAG,
                 "M-010d: DomainAlertConfluence1m5m ontvangen (sym=%s up=%d 1m=%.4f 5m=%.4f)",
                 sym,
                 d.up ? 1 : 0,
                 d.pct_1m,
                 d.pct_5m);
        dispatch_confluence_ntfy(env.domain_conf_1m5m);
#if CONFIG_MQTT_BRIDGE_ENABLE
        ESP_LOGI(TAG, "M-010d: MQTT confluence publish aanroepen");
        mqtt_bridge::publish_domain_alert_confluence_1m5m(
            d.symbol, d.up, d.price_eur, d.pct_1m, d.pct_5m, d.ts_ms);
#else
        ESP_LOGD(TAG, "M-010d: MQTT bridge uit — geen confluence MQTT");
#endif
        break;
    }
    default:
        break;
    }
}

} // namespace

esp_err_t init()
{
    if (s_ready) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(alert_observability::init(), TAG, "alert_observability::init");
    ESP_RETURN_ON_ERROR(ntfy_client::init(), TAG, "ntfy_client::init");
    ESP_RETURN_ON_ERROR(mqtt_bridge::init(), TAG, "mqtt_bridge::init");
    s_q = xQueueCreate(k_queue_depth, sizeof(OutboundEnvelope));
    if (!s_q) {
        return ESP_ERR_NO_MEM;
    }
    s_ready = true;
    ESP_LOGI(TAG,
             "M-002c: outbound queue ready (depth=%u, max_dispatch/poll=%u)",
             static_cast<unsigned>(k_queue_depth),
             k_max_dispatch_per_poll);
    return ESP_OK;
}

void emit(Event e)
{
    if (!s_ready || !s_q || e == Event::None) {
        return;
    }
    if (e == Event::DomainAlert1mMove) {
        ESP_LOGW(TAG, "emit(DomainAlert1mMove) zonder payload — gebruik emit_domain_alert_1m");
        return;
    }
    if (e == Event::DomainAlert5mMove) {
        ESP_LOGW(TAG, "emit(DomainAlert5mMove) zonder payload — gebruik emit_domain_alert_5m");
        return;
    }
    if (e == Event::DomainAlertConfluence1m5m) {
        ESP_LOGW(TAG,
                 "emit(DomainAlertConfluence1m5m) zonder payload — gebruik emit_domain_confluence_1m5m");
        return;
    }
    OutboundEnvelope env{};
    env.kind = e;
    if (xQueueSend(s_q, &env, 0) != pdTRUE) {
        ++s_drop_total;
        ESP_LOGW(TAG,
                 "M-002: outbound queue full — drop event (kind=%u) drops_total=%" PRIu32,
                 static_cast<unsigned>(e),
                 s_drop_total);
    }
}

void emit_domain_alert_1m(const DomainAlert1mMovePayload &p)
{
    if (!s_ready || !s_q) {
        return;
    }
    OutboundEnvelope env{};
    env.kind = Event::DomainAlert1mMove;
    env.domain_1m = p;
    if (xQueueSend(s_q, &env, 0) != pdTRUE) {
        ++s_drop_total;
        ESP_LOGW(TAG,
                 "M-002: outbound queue full — drop DomainAlert1mMove drops_total=%" PRIu32,
                 s_drop_total);
    }
}

void emit_domain_alert_5m(const DomainAlert5mMovePayload &p)
{
    if (!s_ready || !s_q) {
        return;
    }
    OutboundEnvelope env{};
    env.kind = Event::DomainAlert5mMove;
    env.domain_5m = p;
    if (xQueueSend(s_q, &env, 0) != pdTRUE) {
        ++s_drop_total;
        ESP_LOGW(TAG,
                 "M-002: outbound queue full — drop DomainAlert5mMove drops_total=%" PRIu32,
                 s_drop_total);
    }
}

void emit_domain_confluence_1m5m(const DomainConfluence1m5mPayload &p)
{
    if (!s_ready || !s_q) {
        return;
    }
    OutboundEnvelope env{};
    env.kind = Event::DomainAlertConfluence1m5m;
    env.domain_conf_1m5m = p;
    if (xQueueSend(s_q, &env, 0) != pdTRUE) {
        ++s_drop_total;
        ESP_LOGW(TAG,
                 "M-002: outbound queue full — drop DomainAlertConfluence1m5m drops_total=%" PRIu32,
                 s_drop_total);
    }
}

void poll()
{
    if (!s_ready || !s_q) {
        return;
    }
    OutboundEnvelope env{};
    unsigned n = 0;
    while (n < k_max_dispatch_per_poll && xQueueReceive(s_q, &env, 0) == pdTRUE) {
        dispatch_envelope(env);
        ++n;
    }
    const UBaseType_t waiting = uxQueueMessagesWaiting(s_q);
    if (waiting == 0) {
        s_last_backlog_log_us = 0ULL;
    } else {
        const uint64_t now_us = esp_timer_get_time();
        if (s_last_backlog_log_us == 0ULL ||
            (now_us - s_last_backlog_log_us) >= k_backlog_log_interval_us) {
            s_last_backlog_log_us = now_us;
            ESP_LOGW(TAG,
                     "M-002: outbound backlog %u event(s) remain (max %u/poll; NTFY/MQTT may be slow)",
                     static_cast<unsigned>(waiting),
                     k_max_dispatch_per_poll);
        }
    }
}

unsigned queue_waiting()
{
    if (!s_ready || !s_q) {
        return 0;
    }
    return static_cast<unsigned>(uxQueueMessagesWaiting(s_q));
}

unsigned queue_capacity()
{
    return static_cast<unsigned>(k_queue_depth);
}

uint32_t drop_total()
{
    return s_drop_total;
}

} // namespace service_outbound
