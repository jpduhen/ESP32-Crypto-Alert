/**
 * M-012a: eerste MQTT-route achter `service_outbound` — geen Home Assistant discovery.
 */
#include "mqtt_bridge/mqtt_bridge.hpp"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "net_runtime/net_runtime.hpp"
#include "sdkconfig.h"

#if CONFIG_MQTT_BRIDGE_ENABLE
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
    ESP_LOGD(TAG, "mqtt_bridge uit (Kconfig)");
    return ESP_OK;
#else
    if (strlen(CONFIG_MQTT_BROKER_URI) == 0) {
        ESP_LOGI(TAG, "M-012a: broker-URI leeg — geen MQTT-client");
        return ESP_OK;
    }

    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri = CONFIG_MQTT_BROKER_URI;
    mqtt_cfg.session.keepalive = 60;
    if (strncmp(CONFIG_MQTT_BROKER_URI, "mqtts://", 8) == 0) {
        mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }
    if (strlen(CONFIG_MQTT_BRIDGE_USER) > 0) {
        mqtt_cfg.credentials.username = CONFIG_MQTT_BRIDGE_USER;
        mqtt_cfg.credentials.authentication.password = CONFIG_MQTT_BRIDGE_PASSWORD;
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
    if (!s_client || strlen(CONFIG_MQTT_BROKER_URI) == 0) {
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

} // namespace mqtt_bridge
