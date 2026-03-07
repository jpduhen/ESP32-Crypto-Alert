# 08 – FAQ

## General

**What is this project?**  
A crypto price display for ESP32/ESP32-S3 with Bitvavo prices, timeframe returns (1m through 7d), trend/volatility, anchor (take-profit/max-loss) and alerts via NTFY and MQTT. The UI runs on LVGL 9.

**Which boards are supported?**  
TTGO T-Display, CYD 2.4/2.8, ESP32-S3 Super Mini, GEEK, LCDwiki 2.8, 4848S040, AMOLED 2.06. Choice in `platform_config.h`.

**Where is the main logic?**  
In `UNIFIED-LVGL9-Crypto_Monitor.ino`: globals, setup/loop, tasks, fetchPrice, warm-start, WiFi/MQTT/NTFY. Modules in `src/` for Net, ApiClient, PriceData, TrendDetector, VolatilityTracker, AnchorSystem, AlertEngine, UIController, SettingsStore, Memory, WarmStart, WebServer.

---

## Data and timeframes

**How are 1m and 5m returns calculated?**  
Via ring buffers: `secondPrices` (60) and `fiveMinutePrices` (300). Each API call and the priceRepeatTask (every 2 s) fill the buffers via `priceData.addPriceToSecondArray(price)`. ret_1m = (current price − price 60 s ago) / price 60 s ago × 100; ret_5m analogously over 300 s. **Unit:** percentage points (0.12 = 0.12%); thresholds the same (see doc 02, 07).

**What is warm-start?**  
On boot Bitvavo candles (1m, 5m, 30m, 2h) are fetched and buffers filled. So ret_2h and ret_30m are available immediately (or quickly) instead of after 30–120 minutes of live data.

**What is minuteAverages?**  
Array of 120 values: each full minute the average of the 60 seconds is written into one element. Used for ret_30m and ret_2h.

---

## Alerts

**Why do I not get a notification even though the move is large?**  
Check: (1) cooldown – enough time must have passed since the last notification of that type; (2) max per hour – e.g. max 3 for 1m; (3) threshold – effective threshold (including night mode/auto-vol) must be exceeded.

**How does cooldown work exactly?**  
Per type (1m, 5m, 30m) `lastNotification*` is tracked. AlertEngine decides if there is an alert; if "yes" it builds the payload and calls `sendNotification()`. Actual delivery (NTFY) is in .ino (`sendNotification()` → `sendNtfyNotification()`). A notification is only *requested* when `(now - lastNotification*) >= cooldown*Ms` and the hour limit is not reached. See doc 03_ALERTING_RULES.

**What is Smart Confluence?**  
When within a short time window both a 1m and a 5m event occur in the same direction and the 30m trend supports that direction, one combined "Confluence" alert is sent. Both events are then marked as "used".

**What are 2h PRIMARY and SECONDARY alerts?**  
PRIMARY = breakout up/down (regime change); they skip throttling. SECONDARY = mean touch, compress, trend change, anchor context; they are subject to throttling and global secondary cooldown.

---

## UI and display

**Where is the screen built?**  
In `UIController::buildUI()`: chart, header/footer, price boxes, anchor/trend/volatility labels. `updateUI()` updates everything periodically (every 1 s in uiTask).

**Why does the price not flicker?**  
Caches are used (lastPriceLblValue, lastAnchorValue, …); labels are only updated when the value has changed.

**Which display driver do I use?**  
Per board in `PINS_*.h`: Arduino_GFX (ST7789, ILI9341, ST7701, …) with SPI (or I2C) bus. Resolution and pins are there.

---

## Configuration and memory

**Where do I set MQTT password or NTFY topic?**  
Via the web interface (settings page) or MQTT commands; stored in NVS. NTFY topic can be generated automatically from device ID. No secrets in code or docs.

**How much RAM does it use?**  
Depends on platform: secondPrices + 5m + minute + hourly arrays; LVGL draw buffer(s); global buffers. HeapMon (`logHeap("tag")`) gives insight; on CYD without PSRAM arrays are on INTERNAL heap, on S3 with PSRAM part can be on SPIRAM.

**What if NVS is full?**  
Load falls back to defaults; save can fail. Old keys can be cleaned up or namespace wiped (note: loss of settings).

---

## Errors and debugging

**API does not fetch price.**  
Check WiFi, Bitvavo reachability, symbol (e.g. BTC-EUR). Timeouts and retries are in .ino; ApiClient logs errors.

**Mutex timeout in apiTask.**  
Another task held dataMutex for a long time. Usually temporary; next cycle retries. If structural: reduce stack or length of critical section.

**Display stays black.**  
Check PINS (backlight pin, DC/CS/RST), DEV_DEVICE_INIT(), LVGL init and flush callback. Board-specific notes in PINS_*.h.

**No alerts despite set thresholds.**  
See "Why do I not get a notification" above. Also check that NTFY topic and (for MQTT) broker/credentials are correct; errors are logged but do not crash.

---

## Open questions (documentation)

- **Notifier/transport:** There is no separate notifier module in `src/`. AlertEngine and AnchorSystem call external `sendNotification(title, message, colorTag)`. The implementation is in the main sketch: `sendNotification()` (.ino, around line 3348) calls `sendNtfyNotification()` (NTFY HTTPS). Anchor events also go via `publishMqttAnchorEvent()` in .ino. For an explicit "transport layer" module: see .ino as reference.
- **DataSource name:** In `src/PriceData/PriceData.h` the enum is SOURCE_BINANCE (warm-start) and SOURCE_LIVE (live). Warm-start data in the repo comes from the **Bitvavo** candles endpoint, not Binance. Whether SOURCE_BINANCE should be renamed to e.g. SOURCE_WARMSTART: open; definition is in PriceData.h.
- **WebSocket:** HTTP polling is the primary price path. WS is feature-flagged (`WS_ENABLED`, platform_config.h) and used in .ino (maybeInitWebSocketAfterWarmStart, processWsTextMessage). Exact role (candles only, or also ticker) and whether WS is active in your build: see .ino and platform_config.h.

---
**[← 07 Glossary](07_GLOSSARY_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture)
