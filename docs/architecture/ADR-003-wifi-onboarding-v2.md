# ADR-003 — V2 WiFi-onboarding (SoftAP CryptoAlert + browserportal)

**Status:** besloten  
**Datum:** 2026-04-11  
**Context:** V2 `firmware-v2/`, ESP32-S3 GEEK; vervanging van uitsluitend Kconfig-WiFi door provisionerbare credentials.

## Besluit

1. **Component `wifi_onboarding`** — los van `market_data` en Bitvavo: alleen WiFi SSID/wachtwoord naar **NVS** via `config_store`, SoftAP **`CryptoAlert`** (open), HTTP op poort **80**, formulier `POST /save`.
2. **`config_store::RuntimeConfig`** bevat `wifi_sta_ssid` / `wifi_sta_pass`; schema **v2**; helpers `has_wifi_credentials`, `clear_wifi_credentials`.
3. **`net_runtime`** — `early_init()` (netif + events + mutex); `start_softap` / `stop_softap`; `start_sta(ssid, pass)` met optionele **menuconfig-fallback** voor ontwikkelaars als NVS leeg blijft.
4. **Bootvolgorde (`app_core`):** `config_store` load → `net_runtime::early_init` → `wifi_onboarding::run` (blokkeert tot submit + `esp_restart` als NVS geen WiFi had) → BSP → display → UI → `start_sta` → `market_data`.
5. **Geforceerde her-onboarding:** Kconfig **`WIFI_ONBOARDING_FORCE`** of API **`wifi_onboarding::clear_credentials_for_reprovision()`** (haakje voor factory/knop).

## Waarom los van `market_data`

Exchange heeft alleen IP nodig; provisioning hoort bij **netwerk/bootstrap**, niet bij marktdata-domein. Zo blijft `market_data` een zuivere snapshot-API.

## Fallback bij mislukte STA-verbinding

**Deze stap:** geen automatische terugval naar SoftAP na een mislukte verbinding met **geldige** NVS-credentials (complexiteit, deadlock-risico’s). Wel: duidelijke logs, `WIFI_ONBOARDING_FORCE` in menuconfig, of `clear_credentials_for_reprovision()` + herstart.

**Later (M-002):** optionele watchdog (geen IP binnen X s) → event → portal.

## Bewust buiten scope

- Captive portal detectie per OS  
- BLE / app-provisioning  
- TLS op de captive portal (lokaal open AP)  
