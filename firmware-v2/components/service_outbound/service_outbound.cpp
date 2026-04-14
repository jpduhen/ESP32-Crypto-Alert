/**
 * M-002c: outbound event-plane — FreeRTOS-queue + poll-dispatcher, stub consumer.
 * Toekomstige transports (MQTT/NTFY/…) kunnen achter dezelfde dispatch-hook of
 * een tweede consumer-stage; `emit` blijft producer-zijde dun.
 */
#include "service_outbound/service_outbound.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace service_outbound {

namespace {

static const char TAG[] = "svc_out";

/** Diepte klein houden: alleen coalescing + toekomstige bursts; geen grote backlog. */
static constexpr UBaseType_t k_queue_depth = 8;

static bool s_ready{false};
static QueueHandle_t s_q{nullptr};
static bool s_app_ready_seen{false};

static void dispatch_stub(Event e)
{
    switch (e) {
    case Event::None:
        break;
    case Event::ApplicationReady:
        if (!s_app_ready_seen) {
            s_app_ready_seen = true;
            ESP_LOGI(TAG, "dispatch ApplicationReady (stub sink — side-effect free)");
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
    s_q = xQueueCreate(k_queue_depth, sizeof(Event));
    if (!s_q) {
        return ESP_ERR_NO_MEM;
    }
    s_ready = true;
    ESP_LOGI(TAG, "M-002c: outbound queue ready (depth=%u, stub consumer)", static_cast<unsigned>(k_queue_depth));
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
        dispatch_stub(e);
    }
}

} // namespace service_outbound
