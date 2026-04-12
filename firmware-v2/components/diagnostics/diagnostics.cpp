#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"

namespace diagnostics {

static const char TAG[] = DIAG_TAG_HEALTH;

esp_err_t init_early()
{
    /* I-tag "Certificate validated" komt bij elke TLS-handshake (REST/WS/reconnect).
     * Standaard INFO → spam; bij problemen tijdelijk DEBUG voor deze tag via menuconfig/runtime. */
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);
    /* Conventie: vaste tags per subsysteem (zie diagnostics.hpp). */
    ESP_LOGI(DIAG_TAG_BOOT, "diagnostics: init_early (log categories active)");
    return ESP_OK;
}

void tick_heartbeat()
{
    ESP_LOGD(TAG, "heartbeat");
}

} // namespace diagnostics
