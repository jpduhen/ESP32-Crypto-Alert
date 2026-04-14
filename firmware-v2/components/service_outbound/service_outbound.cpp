/**
 * M-002c: minimale outbound event-grens — stub dispatcher, geen MQTT/NTFY/WebUI.
 * Toekomst: hier registreren van echte sinks of queue-feed zonder app_core te laten
 * afhangen van protocolheaders.
 */
#include "service_outbound/service_outbound.hpp"
#include "esp_log.h"

namespace service_outbound {

namespace {

static const char TAG[] = "svc_out";

static bool s_ready{false};
static bool s_app_ready_emitted{false};

} // namespace

esp_err_t init()
{
    if (s_ready) {
        return ESP_OK;
    }
    s_ready = true;
    ESP_LOGI(TAG, "M-002c: outbound boundary active (stub — no transports)");
    return ESP_OK;
}

void emit(Event e)
{
    if (!s_ready || e == Event::None) {
        return;
    }
    switch (e) {
    case Event::ApplicationReady:
        if (!s_app_ready_emitted) {
            s_app_ready_emitted = true;
            ESP_LOGI(TAG, "emit ApplicationReady (stub sink — side-effect free)");
        }
        break;
    default:
        break;
    }
}

} // namespace service_outbound
