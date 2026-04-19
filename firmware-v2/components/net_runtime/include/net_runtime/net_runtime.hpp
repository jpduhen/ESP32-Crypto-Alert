#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace net_runtime {

/**
 * Stap 1: netif + event loop + mutex (eenmalig per boot).
 * NVS: `config_store::init()` moet eerder draaien.
 */
esp_err_t early_init();

/**
 * Stap 2a: SoftAP voor WiFi-onboarding (SSID bijv. CryptoAlert).
 * Alleen als nog geen STA actief is in deze sessie.
 */
esp_err_t start_softap(const char *ap_ssid);

/** SoftAP stoppen (vóór esp_restart na provisioning). */
esp_err_t stop_softap();

/**
 * Stap 2b: STA met credentials uit NVS/onboarding.
 * Optioneel fallback naar menuconfig als beide strings leeg (ontwikkelaars).
 */
esp_err_t start_sta(const char *sta_ssid, const char *sta_pass);

/**
 * STA heeft een geldig IP (gate voor TLS naar buiten). Enige plek voor «WiFi klaar?» in app-laag;
 * feedstatus blijft via `market_data::snapshot()`.
 */
bool has_ip();

bool net_mutex_take(TickType_t timeout_ticks = pdMS_TO_TICKS(15000));
void net_mutex_give();

} // namespace net_runtime
