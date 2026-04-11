# Functionele inventarisatie V1 (kort)

**V2-traject:** het leidende werkdocument is [../architecture/V2_WORKDOCUMENT_MASTER.md](../architecture/V2_WORKDOCUMENT_MASTER.md).

Gebaseerd op `README.md`, `ESP32-Crypto-Alert.ino`, `platform_config.h`, `docs/CODE_INDEX.md`, `docs/CODE_ANALYSIS.md` en module-mappen onder `src/`.  
**Let op:** `CODE_INDEX.md` vermeldt nog op sommige plekken “Binance”; de actieve integratie is **Bitvavo** (`BITVAVO_API_BASE` in de `.ino`). Dat is een **documentatie-inconsistentie** in V1.

## Hoofdfunctionaliteit

| Gebied | Korte beschrijving |
|--------|--------------------|
| **Data & API** | Periodiek prijzen en historie via HTTPS (Bitvavo); optioneel WebSocket (`WS_ENABLED`). |
| **Analyse** | Multi-timeframe returns/metrics; trend- en volatiliteitsdetectie; regime-snapshot (`RegimeEngine`). |
| **Alerts** | Contextuele alerts (o.a. 1m/5m/30m en 2h-context); throttling en classificatie in `AlertEngine`. |
| **Anchor** | Gebruikersanker, zones en gerelateerde logica (`AnchorSystem`). |
| **Display** | LVGL UI (`UIController`); board-specifieke backends (`src/display/`, o.a. Arduino_GFX, `esp_lcd` AXS15231B). |
| **Web** | Embedded webserver: HTML-instellingen, status/API (`src/WebServer/`). |
| **Notificaties** | NTFY (HTTPS); MQTT (`PubSubClient`). |
| **OTA** | Web-gebaseerde firmware-update (`src/OtaWebUpdater/`). |
| **Persistentie** | NVS / Preferences (`SettingsStore`). |
| **Warm start** | Vullen buffers vanuit historische data waar mogelijk (`WarmStart`). |
| **Platform** | Eén `.ino` + veel `#ifdef` in `platform_config.h` en pin-headers `PINS_*.h`. |

## Taak-/concurrencymodel (V1)

- FreeRTOS-taken o.a. `apiTask`, `uiTask`, (optioneel/configurable) `webTask`; gedeelde state o.a. via `dataMutex` en netwerk-mutex (`gNetMutex` in nieuwere code).
- **Sterke koppeling:** timings, boot-orchestratie en diagnose-flags staan verspreid over `platform_config.h` en het hoofd-`.ino`-bestand (groot bestand).

## Documentatie in repo

- Gebruikershandleiding: `docs/01-` t/m `docs/10-` (EN/NL).
- Developer: o.a. `docs/METRICS_CONTRACT.md`, `docs/OTA_UPDATE.md`, NTFY-trackers, diverse risico-/verificatiedocumenten.
