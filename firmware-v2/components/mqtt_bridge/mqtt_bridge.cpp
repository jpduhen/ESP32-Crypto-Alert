/**
 * M-012a: eerste MQTT-route achter `service_outbound` — geen Home Assistant discovery.
 * M-012b: JSON-publish voor 1m-domeinalert (topic uit Kconfig).
 * M-010c/010d: 5m- en confluence-publish.
 * S30-3: 30m-publish.
 * S2H-3: 2h-publish.
 */
#include "mqtt_bridge/mqtt_bridge.hpp"
#include "config_store/config_store.hpp"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "net_runtime/net_runtime.hpp"
#include "sdkconfig.h"

#if CONFIG_MQTT_BRIDGE_ENABLE
#include <cstdio>
#include <cstring>
#endif

namespace mqtt_bridge {

namespace {

static const char TAG[] = "mqtt_br";

#if CONFIG_MQTT_BRIDGE_ENABLE

static esp_mqtt_client_handle_t s_client{nullptr};
static bool s_connected{false};
static bool s_pending_ready{false};

static esp_err_t do_publish_ready()
{
    if (!s_client || !s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    const char *topic = CONFIG_MQTT_TOPIC_BOOT;
    const char *payload = "online";
    const int len = static_cast<int>(strlen(payload));
    const int mid = esp_mqtt_client_publish(s_client, topic, payload, len, 1, 0);
    if (mid < 0) {
        ESP_LOGW(TAG, "publish failed (mid=%d)", mid);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "published → %s", topic);
    return ESP_OK;
}

static void try_flush_pending()
{
    if (!s_pending_ready || !s_connected) {
        return;
    }
    if (do_publish_ready() == ESP_OK) {
        s_pending_ready = false;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;
    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "verbonden");
        try_flush_pending();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "verbinding weg");
        break;
    default:
        break;
    }
}

#endif // CONFIG_MQTT_BRIDGE_ENABLE

} // namespace

esp_err_t init()
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    ESP_LOGD(TAG, "mqtt_bridge uit (Kconfig build)");
    return ESP_OK;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGI(TAG, "mqtt uit (runtime M-003a)");
        return ESP_OK;
    }
    if (strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGI(TAG, "M-012a: broker-URI leeg — geen MQTT-client");
        return ESP_OK;
    }

    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri = svc.mqtt_broker_uri;
    mqtt_cfg.session.keepalive = 60;
    if (strncmp(svc.mqtt_broker_uri, "mqtts://", 8) == 0) {
        mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }
    /* NVS-runtime overschrijft Kconfig; leeg veld = fallback naar build-default. */
#if defined(CONFIG_MQTT_BRIDGE_USER)
    const char *k_user = CONFIG_MQTT_BRIDGE_USER;
#else
    const char *k_user = "";
#endif
#if defined(CONFIG_MQTT_BRIDGE_PASSWORD)
    const char *k_pass = CONFIG_MQTT_BRIDGE_PASSWORD;
#else
    const char *k_pass = "";
#endif
    const char *user_eff = svc.mqtt_username[0] != '\0' ? svc.mqtt_username : k_user;
    const char *pass_eff = svc.mqtt_password[0] != '\0' ? svc.mqtt_password : k_pass;
    if (user_eff[0] != '\0' || pass_eff[0] != '\0') {
        mqtt_cfg.credentials.username = user_eff;
        mqtt_cfg.credentials.authentication.password = pass_eff;
    }

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        return ESP_ERR_NO_MEM;
    }
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
    const esp_err_t st = esp_mqtt_client_start(s_client);
    if (st != ESP_OK) {
        ESP_LOGE(TAG, "mqtt start: %s", esp_err_to_name(st));
        esp_mqtt_client_destroy(s_client);
        s_client = nullptr;
        return st;
    }
    ESP_LOGI(TAG, "M-012a: mqtt client gestart");
    return ESP_OK;
#endif
}

void request_application_ready_publish()
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    return;
#else
    if (!config_store::service_runtime().mqtt_enabled) {
        return;
    }
    if (!s_client || strlen(config_store::service_runtime().mqtt_broker_uri) == 0) {
        return;
    }
    if (!net_runtime::has_ip()) {
        ESP_LOGD(TAG, "geen IP — ready-publish wacht op MQTT connect");
    }
    s_pending_ready = true;
    if (s_connected) {
        try_flush_pending();
    }
#endif
}

void publish_domain_alert_1m(const char *symbol,
                             bool up,
                             double price_eur,
                             double pct_1m,
                             int64_t ts_ms)
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    (void)symbol;
    (void)up;
    (void)price_eur;
    (void)pct_1m;
    (void)ts_ms;
    return;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGD(TAG, "M-012b: mqtt runtime uit — geen domain alert MQTT");
        return;
    }
    if (!s_client || strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGD(TAG, "M-012b: geen MQTT-client — skip domain alert");
        return;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "M-012b: MQTT niet verbonden — domain alert niet gepubliceerd");
        return;
    }

    const char *sym_in = (symbol && symbol[0] != '\0') ? symbol : "?";
    char sym_safe[28]{};
    std::strncpy(sym_safe, sym_in, sizeof(sym_safe) - 1);
    for (size_t i = 0; i < sizeof(sym_safe) && sym_safe[i] != '\0'; ++i) {
        if (sym_safe[i] == '"' || sym_safe[i] == '\\') {
            sym_safe[i] = '_';
        }
    }

    const char *dir = up ? "UP" : "DOWN";
    char payload[352];
    const int plen = std::snprintf(payload,
                                   sizeof(payload),
                                   "{\"kind\":\"alert_1m\",\"symbol\":\"%s\",\"dir\":\"%s\","
                                   "\"price_eur\":%.4f,\"pct_1m\":%.4f,\"ts_ms\":%lld}",
                                   sym_safe,
                                   dir,
                                   price_eur,
                                   pct_1m,
                                   (long long)ts_ms);
    if (plen <= 0 || plen >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "M-012b: domain alert JSON te lang of fout");
        return;
    }

    ESP_LOGI(TAG,
             "M-012b: publish kind=alert_1m (topic=%s sym=%s %s)",
             CONFIG_MQTT_TOPIC_DOMAIN_ALERT_1M,
             sym_safe,
             dir);

    const int mid = esp_mqtt_client_publish(s_client,
                                            CONFIG_MQTT_TOPIC_DOMAIN_ALERT_1M,
                                            payload,
                                            plen,
                                            1,
                                            0);
    if (mid < 0) {
        ESP_LOGW(TAG, "M-012b: publish domain alert mislukt (mid=%d)", mid);
        return;
    }
    ESP_LOGI(TAG, "M-012b: domain alert gepubliceerd (mid=%d)", mid);
#endif
}

void publish_domain_alert_5m(const char *symbol,
                             bool up,
                             double price_eur,
                             double pct_5m,
                             int64_t ts_ms)
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    (void)symbol;
    (void)up;
    (void)price_eur;
    (void)pct_5m;
    (void)ts_ms;
    return;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGD(TAG, "M-010c: mqtt runtime uit — geen 5m domain alert MQTT");
        return;
    }
    if (!s_client || strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGD(TAG, "M-010c: geen MQTT-client — skip 5m domain alert");
        return;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "M-010c: MQTT niet verbonden — 5m domain alert niet gepubliceerd");
        return;
    }

    const char *sym_in = (symbol && symbol[0] != '\0') ? symbol : "?";
    char sym_safe[28]{};
    std::strncpy(sym_safe, sym_in, sizeof(sym_safe) - 1);
    for (size_t i = 0; i < sizeof(sym_safe) && sym_safe[i] != '\0'; ++i) {
        if (sym_safe[i] == '"' || sym_safe[i] == '\\') {
            sym_safe[i] = '_';
        }
    }

    const char *dir = up ? "UP" : "DOWN";
    char payload[352];
    const int plen = std::snprintf(payload,
                                   sizeof(payload),
                                   "{\"kind\":\"alert_5m\",\"symbol\":\"%s\",\"dir\":\"%s\","
                                   "\"price_eur\":%.4f,\"pct_5m\":%.4f,\"ts_ms\":%lld}",
                                   sym_safe,
                                   dir,
                                   price_eur,
                                   pct_5m,
                                   (long long)ts_ms);
    if (plen <= 0 || plen >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "M-010c: 5m domain alert JSON te lang of fout");
        return;
    }

    ESP_LOGI(TAG,
             "M-010c: publish kind=alert_5m (topic=%s sym=%s %s)",
             CONFIG_MQTT_TOPIC_DOMAIN_ALERT_5M,
             sym_safe,
             dir);

    const int mid = esp_mqtt_client_publish(s_client,
                                            CONFIG_MQTT_TOPIC_DOMAIN_ALERT_5M,
                                            payload,
                                            plen,
                                            1,
                                            0);
    if (mid < 0) {
        ESP_LOGW(TAG, "M-010c: publish 5m domain alert mislukt (mid=%d)", mid);
        return;
    }
    ESP_LOGI(TAG, "M-010c: 5m domain alert gepubliceerd (mid=%d)", mid);
#endif
}

void publish_domain_alert_30m(const char *symbol,
                              bool up,
                              double price_eur,
                              double pct_30m,
                              int64_t ts_ms)
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    (void)symbol;
    (void)up;
    (void)price_eur;
    (void)pct_30m;
    (void)ts_ms;
    return;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGD(TAG, "S30-3: mqtt runtime uit — geen 30m domain alert MQTT");
        return;
    }
    if (!s_client || strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGD(TAG, "S30-3: geen MQTT-client — skip 30m domain alert");
        return;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "S30-3: MQTT niet verbonden — 30m domain alert niet gepubliceerd");
        return;
    }

    const char *sym_in = (symbol && symbol[0] != '\0') ? symbol : "?";
    char sym_safe[28]{};
    std::strncpy(sym_safe, sym_in, sizeof(sym_safe) - 1);
    for (size_t i = 0; i < sizeof(sym_safe) && sym_safe[i] != '\0'; ++i) {
        if (sym_safe[i] == '"' || sym_safe[i] == '\\') {
            sym_safe[i] = '_';
        }
    }

    const char *dir = up ? "UP" : "DOWN";
    char payload[352];
    const int plen = std::snprintf(payload,
                                   sizeof(payload),
                                   "{\"kind\":\"alert_30m\",\"symbol\":\"%s\",\"dir\":\"%s\","
                                   "\"price_eur\":%.4f,\"pct_30m\":%.4f,\"ts_ms\":%lld}",
                                   sym_safe,
                                   dir,
                                   price_eur,
                                   pct_30m,
                                   (long long)ts_ms);
    if (plen <= 0 || plen >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "S30-3: 30m domain alert JSON te lang of fout");
        return;
    }

    ESP_LOGI(TAG,
             "S30-3: publish kind=alert_30m (topic=%s sym=%s %s)",
             CONFIG_MQTT_TOPIC_DOMAIN_ALERT_30M,
             sym_safe,
             dir);

    const int mid = esp_mqtt_client_publish(s_client,
                                            CONFIG_MQTT_TOPIC_DOMAIN_ALERT_30M,
                                            payload,
                                            plen,
                                            1,
                                            0);
    if (mid < 0) {
        ESP_LOGW(TAG, "S30-3: publish 30m domain alert mislukt (mid=%d)", mid);
        return;
    }
    ESP_LOGI(TAG, "S30-3: 30m domain alert gepubliceerd (mid=%d)", mid);
#endif
}

void publish_domain_alert_2h(const char *symbol,
                             bool up,
                             double price_eur,
                             double pct_2h,
                             int64_t ts_ms)
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    (void)symbol;
    (void)up;
    (void)price_eur;
    (void)pct_2h;
    (void)ts_ms;
    return;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGD(TAG, "S2H-3: mqtt runtime uit — geen 2h domain alert MQTT");
        return;
    }
    if (!s_client || strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGD(TAG, "S2H-3: geen MQTT-client — skip 2h domain alert");
        return;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "S2H-3: MQTT niet verbonden — 2h domain alert niet gepubliceerd");
        return;
    }

    const char *sym_in = (symbol && symbol[0] != '\0') ? symbol : "?";
    char sym_safe[28]{};
    std::strncpy(sym_safe, sym_in, sizeof(sym_safe) - 1);
    for (size_t i = 0; i < sizeof(sym_safe) && sym_safe[i] != '\0'; ++i) {
        if (sym_safe[i] == '"' || sym_safe[i] == '\\') {
            sym_safe[i] = '_';
        }
    }

    const char *dir = up ? "UP" : "DOWN";
    char payload[352];
    const int plen = std::snprintf(payload,
                                   sizeof(payload),
                                   "{\"kind\":\"alert_2h\",\"symbol\":\"%s\",\"dir\":\"%s\","
                                   "\"price_eur\":%.4f,\"pct_2h\":%.4f,\"ts_ms\":%lld}",
                                   sym_safe,
                                   dir,
                                   price_eur,
                                   pct_2h,
                                   (long long)ts_ms);
    if (plen <= 0 || plen >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "S2H-3: 2h domain alert JSON te lang of fout");
        return;
    }

    ESP_LOGI(TAG,
             "S2H-3: publish kind=alert_2h (topic=%s sym=%s %s)",
             CONFIG_MQTT_TOPIC_DOMAIN_ALERT_2H,
             sym_safe,
             dir);

    const int mid = esp_mqtt_client_publish(s_client,
                                            CONFIG_MQTT_TOPIC_DOMAIN_ALERT_2H,
                                            payload,
                                            plen,
                                            1,
                                            0);
    if (mid < 0) {
        ESP_LOGW(TAG, "S2H-3: publish 2h domain alert mislukt (mid=%d)", mid);
        return;
    }
    ESP_LOGI(TAG, "S2H-3: 2h domain alert gepubliceerd (mid=%d)", mid);
#endif
}

void publish_domain_alert_confluence_1m5m(const char *symbol,
                                          bool up,
                                          double price_eur,
                                          double pct_1m,
                                          double pct_5m,
                                          int64_t ts_ms)
{
#if !CONFIG_MQTT_BRIDGE_ENABLE
    (void)symbol;
    (void)up;
    (void)price_eur;
    (void)pct_1m;
    (void)pct_5m;
    (void)ts_ms;
    return;
#else
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.mqtt_enabled) {
        ESP_LOGD(TAG, "M-010d: mqtt runtime uit — geen confluence MQTT");
        return;
    }
    if (!s_client || strlen(svc.mqtt_broker_uri) == 0) {
        ESP_LOGD(TAG, "M-010d: geen MQTT-client — skip confluence");
        return;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "M-010d: MQTT niet verbonden — confluence niet gepubliceerd");
        return;
    }

    const char *sym_in = (symbol && symbol[0] != '\0') ? symbol : "?";
    char sym_safe[28]{};
    std::strncpy(sym_safe, sym_in, sizeof(sym_safe) - 1);
    for (size_t i = 0; i < sizeof(sym_safe) && sym_safe[i] != '\0'; ++i) {
        if (sym_safe[i] == '"' || sym_safe[i] == '\\') {
            sym_safe[i] = '_';
        }
    }

    const char *dir = up ? "UP" : "DOWN";
    char payload[416];
    const int plen = std::snprintf(payload,
                                   sizeof(payload),
                                   "{\"kind\":\"alert_confluence_1m5m\",\"symbol\":\"%s\",\"dir\":\"%s\","
                                   "\"price_eur\":%.4f,\"pct_1m\":%.4f,\"pct_5m\":%.4f,"
                                   "\"ts_ms\":%lld}",
                                   sym_safe,
                                   dir,
                                   price_eur,
                                   pct_1m,
                                   pct_5m,
                                   (long long)ts_ms);
    if (plen <= 0 || plen >= static_cast<int>(sizeof(payload))) {
        ESP_LOGW(TAG, "M-010d: confluence JSON te lang of fout");
        return;
    }

    ESP_LOGI(TAG,
             "M-010d: publish kind=alert_confluence_1m5m (topic=%s sym=%s %s)",
             CONFIG_MQTT_TOPIC_DOMAIN_ALERT_CONF_1M5M,
             sym_safe,
             dir);

    const int mid = esp_mqtt_client_publish(s_client,
                                            CONFIG_MQTT_TOPIC_DOMAIN_ALERT_CONF_1M5M,
                                            payload,
                                            plen,
                                            1,
                                            0);
    if (mid < 0) {
        ESP_LOGW(TAG, "M-010d: publish confluence mislukt (mid=%d)", mid);
        return;
    }
    ESP_LOGI(TAG, "M-010d: confluence gepubliceerd (mid=%d)", mid);
#endif
}

} // namespace mqtt_bridge
