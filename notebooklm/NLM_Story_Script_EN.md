# NLM Story Script – UNIFIED-LVGL9 Crypto Monitor

English video script, approx. 6–10 minutes. Based on existing /docs; no code changes.

"Spelling: ESP32, Bitvavo, NTFY, MQTT."

"Use the exact formula for ret_*."

"Mention core pinning as 'config-dependent'."

"Use only sendNotification/sendNtfyNotification/publishMqttAnchorEvent."
---

## (1) What is it?

The UNIFIED-LVGL9 Crypto Monitor is embedded firmware for ESP32 and ESP32-S3 boards: a crypto price display that fetches live prices from the Bitvavo API, computes timeframe returns, classifies trend and volatility, maintains an anchor price with take-profit and max-loss, and *decides* on alerts and delivers the payload — actual delivery is in the main sketch. The UI runs on LVGL 9. Everything is modular: network, API, price data, trend, volatility, anchor, alerts, UI, settings and web server live in separate src modules; the .ino coordinates via FreeRTOS tasks.

---

## (2) Platform, LVGL and tasks

You choose one platform in platform_config.h — e.g. CYD 2.4, CYD 2.8, TTGO T-Display or an ESP32-S3 variant. Per board there is a PINS_* file for display and bus; LVGL is configured via lv_conf.h. The app runs on two cores: apiTask on Core 1 for fetch and buffer updates; uiTask and webTask on Core 0 for the UI and web interface. There is also a priceRepeatTask that every two seconds puts the last price back into the ring buffer for stable 1m and 5m calculations. Shared data is protected by dataMutex; network calls by gNetMutex.

---

## (3) Net and ApiClient

The Net module provides streaming HTTP: `httpGetToBuffer` reads the body into a buffer without String allocations. ApiClient uses that layer and does the Bitvavo calls: `fetchBitvavoPrice` gets the current price, with stream parsing where possible. Timeouts and error logging are in ApiClient; retries and backoff in the .ino. There is no separate notifier module: AlertEngine and AnchorSystem only build the payload and call external `sendNotification`; the transport layer is in the .ino.

---

## (4) PriceData, buffers and returns (ret_* in percentage points)

PriceData manages the ring buffers: 60 seconds, 300 seconds for 5 minutes, 120 minutes for 30m and 2h, and an hour buffer for long term. Each new price goes via `addPriceToSecondArray` into the seconds and 5m buffer; each full minute the average of 60 seconds is written into `minuteAverages`. The returns — ret_1m, ret_5m, ret_30m, ret_2h — are computed as **percentage points**: (price_now minus price_old) divided by price_old times 100. So 0.12 means 0.12 per cent move; all thresholds (spike, move, etc.) use the same unit. Warm-start optionally fills the buffers with Bitvavo candles on boot so ret_2h and ret_30m are available quickly.

---

## (5) Trend and Volatility

TrendDetector determines the 2h trend (UP, DOWN, SIDEWAYS) from ret_2h and ret_30m and thresholds; there is also medium trend (1d) and long-term (7d). On a trend change a notification can be requested, with a ten-minute cooldown. VolatilityTracker keeps the absolute 1m returns over a lookback; from that comes a volatility state (LOW, MEDIUM, HIGH). In auto-volatility mode the effective thresholds for spikes and moves are adjusted accordingly.

---

## (6) AnchorSystem

AnchorSystem manages the anchor price: set, update min and max on each price, and the take-profit and max-loss checks. The effective thresholds can be trend-adaptive. On breach AnchorSystem builds the notification payload and calls `sendNotification`; for anchor events the .ino also does an MQTT publish via `publishMqttAnchorEvent`. Each boundary (take-profit or max-loss) sends one notification until the anchor is set again.

---

## (7) AlertEngine: confluence and 2h primary/secondary throttling

AlertEngine decides on 1m, 5m and 30m alerts: spikes and moves with cooldowns and a maximum per hour. Smart Confluence combines a 1m and 5m event in the same direction within a time window, supported by the 30m trend. For 2h alerts there is a clear distinction: **PRIMARY** — breakout up and down — may always go through and skips throttling. **SECONDARY** — mean touch, compress, trend change, anchor context — is subject to a throttling matrix and a **global secondary cooldown** (default 120 minutes). Within secondaries there is also coalescing to damp short bursts. AlertEngine builds the payload and calls `sendNotification` or `send2HNotification`; actual delivery is in the .ino.

---

## (8) Transport in .ino: sendNotification → sendNtfyNotification + publishMqttAnchorEvent

So far we have determined when there is an alert; now we look at how that payload actually leaves the device (NTFY/MQTT). There is no separate notifier module in src. In the main sketch there is `sendNotification(title, message, colorTag)`; it calls `sendNtfyNotification` — so the NTFY HTTP delivery is in the .ino. Anchor events also go via `publishMqttAnchorEvent` in the .ino to MQTT. So: decision and payload in AlertEngine or AnchorSystem; transport (NTFY and MQTT for anchor) in the .ino.

---

## (9) WebServer, SettingsStore/NVS and WarmStart

The WebServer module provides the settings page, save handlers, anchor set, NTFY reset, WiFi reset, status JSON and OTA upload. All configurable values — thresholds, cooldowns, anchor, warm-start, MQTT, night mode, etc. — are stored via SettingsStore in NVS (Preferences). WarmStart is a wrapper for status and logging; the actual warm-start (fetch candles and fill buffers) is in the .ino. WebSocket is feature-flagged; HTTP polling is the primary price path.

---

## (10) Failure modes

- **API down:** ApiClient returns false; fetchPrice does retry/backoff in .ino; buffers stay unchanged; no crash.
- **WiFi down:** apiTask and webTask wait for WL_CONNECTED; priceRepeatTask keeps putting the last price into the buffer every 2s.
- **PriceRepeat flattening:** With API failure or slow responses priceRepeat keeps repeating the same price; 1m and 5m returns can be temporarily flattened and volatility underestimated until new API prices arrive.
- **Mutex timeouts:** apiTask logs and retries later; uiTask skips one update if the mutex is not available.
- **UI snapshot not implemented:** uiTask takes dataMutex very briefly and then reads all globals *without* mutex during updateUI. So one frame can show a mix of old and new values. The recommended pattern — snapshot under mutex: copy required fields under mutex, then render only from that snapshot — is not implemented in the current code.

End of script.

---
[Technical docs overview (README)](../README.md#technical-documentation-code--architecture) | **[Key Points →](NLM_Key_Points_EN.md)** | **[Examples →](NLM_Examples_EN.md)**
