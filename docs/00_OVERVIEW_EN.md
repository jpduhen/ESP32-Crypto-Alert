# 00 – Overview (UNIFIED-LVGL9 Crypto Monitor)

## Project purpose

The **UNIFIED-LVGL9 Crypto Monitor** is embedded firmware for ESP32/ESP32-S3 boards (e.g. TTGO T-Display, ESP32-S3 GEEK, LCDWIKI 2.8, 4848S040) that:

- **Fetches live prices** from the Bitvavo API for a chosen market (e.g. BTC-EUR).
- **Computes timeframe returns** (1m, 5m, 30m, 2h, 4h, 1d, 7d) via ring buffers and optional warm-start (historical candles).
- **Classifies trend and volatility** (UP/DOWN/SIDEWAYS, LOW/MEDIUM/HIGH).
- **Maintains an anchor price** with take-profit and max-loss alerts.
- **Decides on alerts** and builds payload (title, message, color tag); actual delivery (NTFY) is done in the main sketch (.ino); anchor events are also sent via MQTT (`.ino`).
- **Uses LVGL 9** for the UI (chart, cards, footer, anchor/trend/vol labels).

The code is modular: network, API, price data, trend, volatility, anchor, alerts, UI, settings, memory, warm-start and web server live in separate `src/` modules; the main loop and global state are in the `.ino` and coordinated via FreeRTOS tasks.

---

## Top-level structure

| Path | Role |
|------|------|
| **UNIFIED-LVGL9-Crypto_Monitor.ino** | Main sketch: globals, `setup()`, `loop()`, tasks (`apiTask`, `uiTask`, `webTask`, `priceRepeatTask`), `fetchPrice()`, warm-start, WiFi, MQTT, NTFY. |
| **platform_config.h** | Platform choice (TTGO/ESP32S3_*), version, debug, OTA, SYMBOL_COUNT, font/chart/UI constants. |
| **lv_conf.h** | LVGL 9 config: colour depth, memory, DPI, FreeRTOS, rendering, fonts. |
| **PINS_*.h** | Per board: display bus (Arduino_GFX), pins (TFT_*, GFX_BL), resolution, `gfx`/`bus`, `DEV_DEVICE_INIT()`. |
| **src/Net** | `HttpFetch`: streaming HTTP body to buffer (`httpGetToBuffer`), network mutex. |
| **src/ApiClient** | Bitvavo HTTP client: `httpGET`, `fetchBitvavoPrice`, `parseBitvavoPriceFromStream`, timeouts, error logging. |
| **src/PriceData** | Ring buffers (seconds, 5m, minutes), `addPriceToSecondArray`, return calculations, DataSource tracking. |
| **src/TrendDetector** | Trend (2h/30m), medium (1d), long-term (7d), trend change + cooldown, sync with globals. |
| **src/VolatilityTracker** | 60-min abs 1m returns, auto-volatility sliding window, effective thresholds. |
| **src/AnchorSystem** | Set/update anchor, take-profit/max-loss checks, trend-adaptive thresholds; builds alert payload and calls `sendNotification()` (transport in .ino). |
| **src/AlertEngine** | 1m/5m/30m spike & move alerts, confluence, 2h alerts (breakout/mean/compress/trend), cooldowns & throttling; decides and builds payload, calls external `sendNotification()` (transport in .ino). |
| **src/UIController** | LVGL init, `buildUI()`, `updateUI()`, labels/cards/chart, button. |
| **src/SettingsStore** | NVS (Preferences): load/save `CryptoMonitorSettings`, NTFY topic, thresholds, cooldowns, anchor, warm-start, MQTT. |
| **src/Memory** | `HeapMon`: `snapHeap()`, `logHeap()` with rate limiting for fragmentation audit. |
| **src/WarmStart** | `WarmStartWrapper`: status/logging/settings binding; warm-start logic itself in .ino (`performWarmStart`). |
| **src/WebServer** | Web UI: settings page, save/anchor/NTFY-reset/WiFi-reset, status JSON, OTA update. |

---

## Main concepts

- **Ring buffers**: seconds (60), 5m (300 sec), minutes (120), hours (168). Each API price or periodic repeat fills seconds + 5m; each full minute fills `minuteAverages`; hour aggregation for 7d.
- **Warm-start**: on boot (when WiFi OK and setting enabled) Bitvavo candles (1m, 5m, 30m, 2h) are fetched and buffers filled so ret_2h/ret_30m etc. are available quickly.
- **Cooldowns & limits**: per alert type (1m, 5m, 30m) a cooldown (ms) and max alerts per hour; 2h alerts use a throttling matrix and optional global secondary cooldown.
- **Threading**: `dataMutex` protects shared price/buffer state; `apiTask` (Core 1) does fetch + buffer updates; `uiTask` (Core 0) does LVGL; `priceRepeatTask` repeats last price every 2s into the ring buffer; `webTask` does `handleClient()`.

No passwords or API keys in the documentation; they live in NVS or (for NTFY topic) are generated from device ID.

Legacy/historical documentation (e.g. CYD removal): [docs/legacy/](legacy/).

---
**[Technical docs overview](../README.md#technical-documentation-code--architecture)** | **[01 Architecture →](01_ARCHITECTURE_EN.md)**
