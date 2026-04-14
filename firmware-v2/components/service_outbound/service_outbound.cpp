/**
 * M-002c / M-011a / M-012a: outbound queue + dispatch; sinks: ntfy + mqtt (Kconfig).
 */
#include "service_outbound/service_outbound.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_bridge/mqtt_bridge.hpp"
#include "ntfy_client/ntfy_client.hpp"
#include "sdkconfig.h"

namespace service_outbound {

namespace {

static const char TAG[] = "svc_out";

/** Diepte klein houden: alleen coalescing + toekomstige bursts; geen grote backlog. */
static constexpr UBaseType_t k_queue_depth = 8;

static bool s_ready{false};
static QueueHandle_t s_q{nullptr};
static bool s_app_ready_seen{false};

static void dispatch_event(Event e)
{
    switch (e) {
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
    ESP_RETURN_ON_ERROR(ntfy_client::init(), TAG, "ntfy_client::init");
    ESP_RETURN_ON_ERROR(mqtt_bridge::init(), TAG, "mqtt_bridge::init");
    s_q = xQueueCreate(k_queue_depth, sizeof(Event));
    if (!s_q) {
        return ESP_ERR_NO_MEM;
    }
    s_ready = true;
    ESP_LOGI(TAG, "M-002c: outbound queue ready (depth=%u)", static_cast<unsigned>(k_queue_depth));
    return ESP_OK;
}

void emit(Event e)
{
    if (!s_ready || !s_q || e == Event::None) {
        return;
    }
    if (xQueueSend(s_q, &e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "outbound queue full — drop event (kind=%u)", static_cast<unsigned>(e));
    }
}

void poll()
{
    if (!s_ready || !s_q) {
        return;
    }
    Event e{};
    while (xQueueReceive(s_q, &e, 0) == pdTRUE) {
        dispatch_event(e);
    }
}

} // namespace service_outbound
