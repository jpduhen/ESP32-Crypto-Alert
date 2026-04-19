#pragma once

#include "esp_err.h"

namespace ntfy_client {

/**
 * M-011a: minimale HTTPS-POST naar ntfy (topic + optionele Bearer).
 * Geen event-types hier — alleen titel + platte body; mapping zit in `service_outbound`.
 */
esp_err_t init();

/**
 * Publiceert één notificatie. Bij uitgeschakelde Kconfig of leeg topic: `ESP_OK`, geen netwerk.
 * Onder `net_runtime::net_mutex` (zelfde coördinatie als REST/WS).
 */
esp_err_t send_notification(const char *title, const char *body);

} // namespace ntfy_client
