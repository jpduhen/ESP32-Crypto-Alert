# 00 – Overzicht (UNIFIED-LVGL9 Crypto Monitor)

## Doel van het project

De **UNIFIED-LVGL9 Crypto Monitor** is een embedded firmware voor ESP32/ESP32-S3 boards (o.a. TTGO T-Display, ESP32-S3 GEEK, LCDWIKI 2.8, 4848S040) die:

- **Live prijzen** ophaalt van de Bitvavo API voor een gekozen markt (bijv. BTC-EUR).
- **Timeframe-returns** berekent (1m, 5m, 30m, 2h, 4h, 1d, 7d) via ringbuffers en optioneel warm-start (historische candles).
- **Trend en volatiliteit** classificeert (UP/DOWN/SIDEWAYS, LOW/MEDIUM/HIGH).
- **Ankerprijs (anchor)** bijhoudt met take-profit en max-loss alerts.
- **Alerts** beslist en bouwt payload (titel, bericht, kleurtag); daadwerkelijke verzending (NTFY) gebeurt in de hoofdschets (.ino); anchor-events gaan daarnaast via MQTT (`.ino`).
- **LVGL 9** gebruikt voor de UI (grafiek, kaarten, footer, anchor/trend/vol labels).

De code is modulair opgezet: netwerk, API, prijsdata, trend, volatiliteit, anchor, alerts, UI, instellingen, geheugen, warm-start en webserver zitten in aparte `src/`-modules; de hoofdlus en globale state staan in de `.ino` en worden gecoördineerd via FreeRTOS-tasks.

---

## Top-level structuur

| Pad | Rol |
|-----|-----|
| **UNIFIED-LVGL9-Crypto_Monitor.ino** | Hoofdsketch: globals, `setup()`, `loop()`, tasks (`apiTask`, `uiTask`, `webTask`, `priceRepeatTask`), `fetchPrice()`, warm-start, WiFi, MQTT, NTFY. |
| **platform_config.h** | Platformkeuze (TTGO/ESP32S3_*), versie, debug, OTA, SYMBOL_COUNT, font/chart/UI-constanten. |
| **lv_conf.h** | LVGL 9 configuratie: kleurdiepte, geheugen, DPI, FreeRTOS, rendering, fonts. |
| **PINS_*.h** | Per board: display-bus (Arduino_GFX), pinnen (TFT_*, GFX_BL), resolutie, `gfx`/`bus`, `DEV_DEVICE_INIT()`. |
| **src/Net** | `HttpFetch`: streaming HTTP body naar buffer (`httpGetToBuffer`), netwerk-mutex. |
| **src/ApiClient** | Bitvavo HTTP client: `httpGET`, `fetchBitvavoPrice`, `parseBitvavoPriceFromStream`, timeouts, foutlogging. |
| **src/PriceData** | Ringbuffers (seconden, 5m, minuten), `addPriceToSecondArray`, return-berekeningen, DataSource-tracking. |
| **src/TrendDetector** | Trend (2h/30m), medium (1d), long-term (7d), trend change + cooldown, sync met globals. |
| **src/VolatilityTracker** | 60-min abs 1m returns, auto-volatility sliding window, effective thresholds. |
| **src/AnchorSystem** | Anchor zetten/updaten, take-profit/max-loss checks, trend-adaptive thresholds; bouwt alert-payload en roept `sendNotification()` aan (transport in .ino). |
| **src/AlertEngine** | 1m/5m/30m spike & move alerts, confluence, 2h-alerts (breakout/mean/compress/trend), cooldowns & throttling; beslist en bouwt payload, roept extern `sendNotification()` aan (transport in .ino). |
| **src/UIController** | LVGL init, `buildUI()`, `updateUI()`, labels/kaarten/chart, knop. |
| **src/SettingsStore** | NVS (Preferences): laden/opslaan `CryptoMonitorSettings`, NTFY-topic, thresholds, cooldowns, anchor, warm-start, MQTT. |
| **src/Memory** | `HeapMon`: `snapHeap()`, `logHeap()` met rate limiting voor fragmentatie-audit. |
| **src/WarmStart** | `WarmStartWrapper`: status/logging/settings-binding; warm-start logica zelf in .ino (`performWarmStart`). |
| **src/WebServer** | Web-UI: instellingenpagina, save/anchor/NTFY-reset/WiFi-reset, status-JSON, OTA update. |

---

## Belangrijkste concepten

- **Ringbuffers**: seconden (60), 5m (300 sec), minuten (120), uren (168). Elke API-prijs of periodieke herhaling vult seconden + 5m; elk volle minuut vult `minuteAverages`; uur-aggregatie voor 7d.
- **Warm-start**: bij opstart (als WiFi OK en instelling aan) worden Bitvavo candles (1m, 5m, 30m, 2h) opgehaald en buffers gevuld zodat ret_2h/ret_30m e.d. snel beschikbaar zijn.
- **Cooldowns & limieten**: per alerttype (1m, 5m, 30m) een cooldown (ms) en max alerts per uur; 2h-alerts hebben een throttling-matrix en optioneel global secondary cooldown.
- **Threading**: `dataMutex` beschermt gedeelde prijs/buffer-state; `apiTask` (Core 1) doet fetch + buffer-updates; `uiTask` (Core 0) doet LVGL; `priceRepeatTask` herhaalt laatste prijs elke 2s in de ringbuffer; `webTask` doet `handleClient()`.

Geen wachtwoorden of API-keys in de documentatie; die staan in NVS of (voor NTFY-topic) worden gegenereerd uit device-ID.

Historische/legacy documentatie (o.a. CYD-verwijdering): [docs/legacy/](legacy/).

---
**[Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking)** | **[01 Architectuur →](01_ARCHITECTURE.md)**
