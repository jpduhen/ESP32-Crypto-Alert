# v5.09 Notification System — Baseline Specification

This document is the **baseline reference** for the notification system as implemented in v5.09 (commit 82d22b4). Use it when rebuilding or evolving the notification stack to avoid importing regressions from later versions.

**Scope:** Notification transport, alert decision flow, anchor behaviour, related settings, Web UI mapping, retry/backoff, cooldowns, and MQTT interactions. No code changes are specified here.

---

## 1. Notification transport

### 1.1 Entry point

- **Public API:** `bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr)`
- **Definition:** `ESP32-Crypto-Alert-v509.ino` (around line 3334). It is a thin wrapper that calls `sendNtfyNotification(title, message, colorTag)` and returns its result.
- **Implementation:** `static bool sendNtfyNotification(...)` in the same .ino file (around lines 2488–2702). All NTFY sends go through this function.

### 1.2 Transport details

- **Endpoint:** `https://ntfy.sh/<ntfyTopic>`. Topic comes from `CryptoMonitorSettings::ntfyTopic` (max 63 chars), synced to global `ntfyTopic[64]`.
- **Method:** HTTPS POST; body = `message`; headers: `Title`, `Priority: high`, optional `Tags` (color/emoji).
- **TLS:** `WiFiClientSecure` with `setInsecure()` (no certificate verification).
- **Timeouts (in .ino):** `HTTP_CONNECT_TIMEOUT_MS` = 2000 ms, `HTTP_READ_TIMEOUT_MS` = 2500 ms.
- **Client lifecycle:** Per send attempt, stack-allocated `HTTPClient http` and `WiFiClientSecure ntfyClient`; `http.begin(ntfyClient, url)`, then after request `http.end()` and `stream->stop()` (no reuse).

### 1.3 Concurrency

- **Network mutex:** All NTFY send logic runs under `gNetMutex`: `netMutexLock("sendNtfyNotification")` before the retry loop, `netMutexUnlock("sendNtfyNotification")` after. Same mutex is used by other HTTP usage (e.g. ApiClient, HttpFetch).
- **Data mutex:** No `dataMutex` is held during `sendNtfyNotification`. In apiTask, `dataMutex` is released before calling AlertEngine/AnchorSystem; in AnchorSystem::setAnchorPrice, the “Anchor Set” NTFY is sent after `dataMutex` has been released.

### 1.4 Input validation (before send)

- WiFi connected; `strlen(ntfyTopic) > 0`; `title` and `message` non-null; `strlen(title) <= 64`, `strlen(message) <= 512`; URL buffer fit; optional `colorTag` length <= 64 when present.

---

## 2. Retry and backoff behaviour

### 2.1 Per-call retry

- **Max attempts:** 2 (MAX_RETRIES = 1, loop `attempt = 0..MAX_RETRIES`).
- **Delays between attempts:** `RETRY_DELAYS[] = {250, 750}` ms; for 429, delay is at least 1000 ms.
- **Retry only when:** `code == HTTPC_ERROR_CONNECTION_REFUSED`, `HTTPC_ERROR_CONNECTION_LOST`, `HTTPC_ERROR_READ_TIMEOUT`, `HTTPC_ERROR_SEND_HEADER_FAILED`, `HTTPC_ERROR_SEND_PAYLOAD_FAILED`, or HTTP 429, or 5xx. Otherwise no retry.

### 2.2 Global backoff (after a send fails after all retries)

- **State:** `ntfyFailStreak` (0..5, incremented on failure), `ntfyNextAllowedMs` (next time a send is allowed).
- **Rule:** If `nowMs < ntfyNextAllowedMs`, `sendNtfyNotification` returns false without attempting HTTP (rate-limited).
- **Backoff formula:** `backoffMs = 5000UL << (ntfyFailStreak - 1)`, capped at 300000 ms (5 min). So: 5s, 10s, 20s, 40s, 80s, 160s, then cap 300s.
- **Reset:** On successful send, `ntfyFailStreak = 0` and `ntfyNextAllowedMs = 0`.

---

## 3. Alert decision flow

### 3.1 Invocation context

- **apiTask** (in .ino): After fetching price and updating state, it releases `dataMutex`, then calls:
  - `alertEngine.checkAndNotify(ret1mLocal, ret5mLocal, ret30mLocal)`
  - `anchorSystem.checkAnchorAlerts()`
  - `AlertEngine::check2HNotifications(fetchedLocal, manualAnchorLocal)`
  - Periodically `AlertEngine::maybeUpdateAutoAnchor(...)` (first run after 5s, then every 5 min; also on WebSocket candle rollover).
- **TrendDetector** (called from apiTask under dataMutex): `checkTrendChange`, `checkMediumTrendChange`, `checkLongTermTrendChange` can call `AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, ...)` when conditions and cooldowns allow.
- **AnchorSystem::setAnchorPrice** (from UI, Web, or MQTT): Can send “Anchor Set” NTFY and MQTT after releasing dataMutex if `!skipNotifications`.

### 3.2 Short-term alerts (1m, 5m, 30m)

- **Module:** `AlertEngine` (`src/AlertEngine/AlertEngine.cpp`).
- **Entry:** `AlertEngine::checkAndNotify(ret_1m, ret_5m, ret_30m)`.
- **Hourly reset:** If `now - hourStartTime >= 3600000` ms, `alerts1MinThisHour`, `alerts30MinThisHour`, `alerts5MinThisHour` are set to 0 and `hourStartTime = now`.
- **Gating:** For each alert type, `checkAlertConditions(now, lastNotification*, cooldownMs, alerts*ThisHour, maxAlertsPerHour, alertType)` must pass (cooldown elapsed and under hourly cap). Optional extra gate: volume-event cooldown (120 s) and night-mode / 5m cooldown where applicable.
- **Limits (constants in AlertEngine.cpp):** `MAX_1M_ALERTS_PER_HOUR = 3`, `MAX_30M_ALERTS_PER_HOUR = 2`, `MAX_5M_ALERTS_PER_HOUR = 3`.
- **Types and send path:**
  - **1m spike:** Thresholds from settings (spike1m, spike5m, etc.), 5m same direction, volume/range checks, night filter; on pass → `sendNotification(...)`.
  - **30m move:** ret_30m vs move30m threshold, 5m confirmation, volume/range and night filters; on pass → `sendNotification(...)`.
  - **5m move:** ret_5m vs (night-adjusted) move5mAlert, volume/range and night filters; on pass → `sendNotification(...)`.
- **Smart Confluence:** If enabled, significant 1m + 5m events in same direction can be combined into one Confluence alert via `checkAndSendConfluenceAlert` (subject to volume-event cooldown), which calls `sendNotification(...)`.

### 3.3 2-hour (2h) alerts

- **Entry:** `AlertEngine::check2HNotifications(lastPrice, anchorPrice)` and (for trend changes) `TrendDetector::checkTrendChange` / `checkMediumTrendChange` / `checkLongTermTrendChange` → `AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, ...)`.
- **Types (Alert2HType):** `ALERT2H_BREAKOUT_UP`, `ALERT2H_BREAKOUT_DOWN`, `ALERT2H_COMPRESS`, `ALERT2H_MEAN_TOUCH`, `ALERT2H_ANCHOR_CTX`, `ALERT2H_TREND_CHANGE`.
- **PRIMARY vs SECONDARY:**
  - **PRIMARY:** `ALERT2H_BREAKOUT_UP`, `ALERT2H_BREAKOUT_DOWN` — not throttled by 2h matrix or secondary global cooldown; sent immediately (after flushing any pending secondary).
  - **SECONDARY:** All others — subject to `shouldThrottle2HAlert` (global secondary cooldown + matrix cooldowns) and coalescing.
- **Send path:** All 2h notifications go through `AlertEngine::send2HNotification(alertType, title, msg, colorTag)`, which eventually calls `sendNotification(...)` (with optional “[PRIMAIR]” / “[Context]” prefix in title).
- **Coalescing (SECONDARY):** Within `twoHSecondaryCoalesceWindowSec` seconds, multiple secondary candidates are merged; the one with highest priority (Trend Change > Anchor Ctx > Mean Touch > Compress) is sent when the window is flushed (e.g. by a PRIMARY or timer).

### 3.4 Anchor-related notifications

- **Take profit / max loss:** `AnchorSystem::checkAnchorAlerts()` compares current price vs anchor and effective TP/ML thresholds (trend-adaptive if enabled); on breach it calls `sendAnchorAlert(...)`, which calls `sendNotification(...)` and `publishMqttAnchorEvent(...)`.
- **Anchor set:** `AnchorSystem::setAnchorPrice(..., skipNotifications = false)` sends one NTFY “Anchor Set” via `sendNotification(...)` and `publishMqttAnchorEvent(..., "anchor_set")` (after releasing dataMutex).
- **Auto-anchor update:** `AlertEngine::maybeUpdateAutoAnchor` can send an “Auto Anchor” NTFY when `getAutoAnchorNotifyEnabled()` is true and the anchor was updated; it calls `sendNotification(...)`.

---

## 4. Anchor behaviour (notification-relevant)

- **Module:** `AnchorSystem` (`src/AnchorSystem/AnchorSystem.{h,cpp}`).
- **Effective thresholds:** From settings: `anchorTakeProfit`, `anchorMaxLoss`. If `trendAdaptiveAnchorsEnabled`, multipliers (uptrend/downtrend) from settings are applied and result is clamped (e.g. max loss between -6% and -1%, take profit between +2% and +10%).
- **Alerts:** When `anchorPct >= effectiveTakeProfitPct` or `anchorPct <= effectiveMaxLossPct`, the corresponding alert is sent once (flags `anchorTakeProfitSent` / `anchorMaxLossSent` prevent repeats).
- **Anchor source for 2h:** `AlertEngine::getActiveAnchorPrice()` uses manual anchor or auto-anchor value depending on `anchorSourceMode` (from Alert2HThresholds / Auto Anchor settings).

---

## 5. Cooldown behaviour

### 5.1 Short-term (1m / 5m / 30m)

- **Struct:** `NotificationCooldowns`: `cooldown1MinMs`, `cooldown30MinMs`, `cooldown5MinMs` (stored in NVS; defaults below).
- **Usage:** `checkAlertConditions(now, lastNotification, cooldownMs, alertsThisHour, maxAlertsPerHour, alertType)`: send only if `(lastNotification == 0 || (now - lastNotification >= cooldownMs))` and `alertsThisHour < maxAlertsPerHour`.
- **Defaults (SettingsStore.cpp):**  
  `NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT` 120000 (2 min),  
  `NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT` 900000 (15 min),  
  `NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT` 420000 (7 min).
- **Night mode:** 5m uses extended cooldown when active: `nightCooldown5mSec` (seconds) → converted to ms for the 5m cooldown check. Default `NIGHT_MODE_COOLDOWN_5M_SEC_DEFAULT` 900 (15 min).
- **Volume-event cooldown:** Fixed 120 s (`VOLUME_EVENT_COOLDOWN_MS`) between Confluence-style volume events and used to suppress 1m/5m/30m when recently sent.

### 5.2 2-hour alerts

- **Per-type cooldowns (Alert2HThresholds):** `breakCooldownMs`, `meanCooldownMs`, `compressCooldownMs`, `anchorCooldownMs` — used to gate when each 2h type can fire again (and for “armed” state).
- **Defaults (SettingsStore):** break 30 min, mean 60 min, compress 2 h, anchor context 3 h.
- **Secondary global cooldown:** `twoHSecondaryGlobalCooldownSec` — no SECONDARY alert is sent if less than this many seconds since last secondary. Default 7200 (120 min).
- **Secondary matrix:** `getSecondaryCooldownSec(lastType, nextType)` returns cooldown in seconds between last and next SECONDARY type (hardcoded matrix in AlertEngine.cpp). Also used: settings `throttlingTrendChangeMs`, `throttlingTrendToMeanMs`, `throttlingMeanTouchMs`, `throttlingCompressMs` (stored in ms, used in matrix/checks).
- **Coalescing window:** `twoHSecondaryCoalesceWindowSec` — secondary alerts within this window are coalesced; one notification sent with highest priority. Default 90 s.

### 5.3 Trend change (TrendDetector)

- **Constant:** `TREND_CHANGE_COOLDOWN_MS` 600000 (10 min) for 2h, 1d, and 7d trend-change notifications. Applied in TrendDetector before calling `AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, ...)`.

---

## 6. Settings involved in notifications

All under `CryptoMonitorSettings` / `SettingsStore` (NVS namespace `"crypto"`).

### 6.1 Transport and basic

- `ntfyTopic[64]` — NTFY topic (max 63 chars).

### 6.2 Alert thresholds (AlertThresholds)

- `spike1m`, `spike5m`, `move30m`, `move5m`, `move5mAlert`
- `threshold1MinUp`, `threshold1MinDown`, `threshold30MinUp`, `threshold30MinDown`

### 6.3 Notification cooldowns (NotificationCooldowns)

- `cooldown1MinMs`, `cooldown30MinMs`, `cooldown5MinMs`

### 6.4 2-hour (Alert2HThresholds)

- Breakout/breakdown: `breakMarginPct`, `breakResetMarginPct`, `breakCooldownMs`
- Mean reversion: `meanMinDistancePct`, `meanTouchBandPct`, `meanCooldownMs`
- Compress: `compressThresholdPct`, `compressResetPct`, `compressCooldownMs`
- Anchor context: `anchorOutsideMarginPct`, `anchorCooldownMs`
- Trend/throttling: `trendHysteresisFactor`, `throttlingTrendChangeMs`, `throttlingTrendToMeanMs`, `throttlingMeanTouchMs`, `throttlingCompressMs`
- Secondary: `twoHSecondaryGlobalCooldownSec`, `twoHSecondaryCoalesceWindowSec`
- Auto-anchor (in same struct): `anchorSourceMode`, `autoAnchorLastValue`, `autoAnchorLastUpdateEpoch`, `autoAnchorUpdateMinutes`, `autoAnchorForceUpdateMinutes`, `autoAnchor4hCandles`, `autoAnchor1dCandles`, `autoAnchorMinUpdatePct_x100`, `autoAnchorTrendPivotPct_x100`, `autoAnchorW4hBase_x100`, `autoAnchorW4hTrendBoost_x100`, `autoAnchorFlags` (e.g. notify on update)

### 6.5 Anchor

- `anchorTakeProfit`, `anchorMaxLoss`, `anchorStrategy`
- `trendAdaptiveAnchorsEnabled`, `uptrendMaxLossMultiplier`, `uptrendTakeProfitMultiplier`, `downtrendMaxLossMultiplier`, `downtrendTakeProfitMultiplier`

### 6.6 Filters and logic

- `smartConfluenceEnabled`
- Night: `nightModeEnabled`, `nightModeStartHour`, `nightModeEndHour`, `nightSpike5mThreshold`, `nightMove5mAlertThreshold`, `nightMove30mThreshold`, `nightCooldown5mSec`, `nightAutoVolMinMultiplier`, `nightAutoVolMaxMultiplier`
- Auto-volatility: `autoVolatilityEnabled`, `autoVolatilityWindowMinutes`, `autoVolatilityBaseline1mStdPct`, `autoVolatilityMinMultiplier`, `autoVolatilityMaxMultiplier`
- Trend/volatility: `trendThreshold`, `volatilityLowThreshold`, `volatilityHighThreshold`

---

## 7. Web UI settings related to notifications

Form names and sections (from `WebServer.cpp`). POST `/save` maps these to globals and `SettingsStore::save`.

### 7.1 Basic & connectivity

- **ntfytopic** — NTFY topic (text, maxlength 63). Button “Default unique NTFY topic” calls POST `/ntfy/reset` to regenerate.

### 7.2 Anchor & risk

- **anchorStrategy** — 0=Manual, 1=Conservative, 2=Active
- **anchorTP** — Take profit (%)
- **anchorML** — Max loss (%)

### 7.3 Signal generation (thresholds)

- **spike1m**, **spike5m**, **move5mAlert**, **move5m**, **move30m**, **trendTh**, **volLow**, **volHigh**

### 7.4 2-hour alert thresholds

- **2hBreakMargin**, **2hBreakReset**, **2hBreakCD**
- **2hMeanMinDist**, **2hMeanTouch**, **2hMeanCD**
- **2hCompressTh**, **2hCompressReset**, **2hCompressCD**
- **2hAnchorMargin**, **2hAnchorCD**
- **2hTrendHyst**, **2hThrottleTC**, **2hThrottleTM**, **2hThrottleMT**, **2hThrottleComp**
- **2hSecGlobalCD** (min), **2hSecCoalesce** (sec)

### 7.5 Auto anchor

- **anchorSourceMode** — MANUAL / AUTO / AUTO_FALLBACK / OFF
- **autoAnchorUpdateMinutes**, **autoAnchorForceUpdateMinutes**, **autoAnchor4hCandles**, **autoAnchor1dCandles**
- **autoAnchorMinUpdatePct**, **autoAnchorTrendPivotPct**, **autoAnchorW4hBase**, **autoAnchorW4hTrendBoost**
- **autoAnchorNotifyEnabled** — notify on auto-anchor update

### 7.6 Smart logic & filters

- **trendAdapt** — Trend-adaptive anchors
- **upMLMult**, **upTPMult**, **downMLMult**, **downTPMult** (when trend-adaptive on)
- **smartConf** — Smart Confluence Mode
- **nightMode**, **nightStartHour**, **nightEndHour**
- **nightSpike5m**, **nightMove5m**, **nightMove30m**, **nightCd5m**, **nightAvMin**, **nightAvMax**
- **autoVol**, **autoVolWin**, **autoVolBase**, **autoVolMin**, **autoVolMax**

### 7.7 Cooldowns

- **cd1min**, **cd5min**, **cd30min** (seconds; stored as ms in settings)

### 7.8 Routes

- GET `/` — settings page (includes all above)
- POST `/save` — parse form and save (including `ntfytopic` when present)
- POST `/ntfy/reset` — set `ntfyTopic` to default (e.g. device-based) and save

---

## 8. MQTT interactions related to notifications

### 8.1 Anchor events (notification-related)

- **Function:** `publishMqttAnchorEvent(float anchor_price, const char* event_type)` (.ino).
- **Topic:** `{mqttPrefix}/anchor/event`. `mqttPrefix` is derived from NTFY topic (e.g. topic without "-alert" suffix).
- **Payload (JSON):** `{"time":"<ISO8601 or millis>","price":<rounded>,"event":"<event_type>"}`.
- **event_type values:** `"anchor_set"`, `"take_profit"`, `"max_loss"`.
- **When:** From `AnchorSystem::setAnchorPrice` (`anchor_set`) and from `AnchorSystem::sendAnchorAlert` (`take_profit` / `max_loss`). If publish fails, message is queued (`enqueueMqttMessage`).

### 8.2 NTFY topic via MQTT

- **Config command topic:** `{prefix}/config/ntfyTopic/set` — receiving a message updates `ntfyTopic` and is persisted (handleMqttStringSetting).
- **State/discovery:** `publishMqttString("ntfyTopic", ntfyTopic)` in `publishMqttSettings()`; Home Assistant discovery for NTFY Topic uses `state_topic` / `command_topic` derived from same prefix.

### 8.3 Other MQTT (for reference)

- `publishMqttSettings()` publishes many notification-related settings (cooldowns, thresholds, 2h params, anchor, night, auto-vol, etc.) to `{prefix}/config/<key>` (or similar). Used for state and optional external control.
- Discovery: `homeassistant/text/<deviceId>_ntfyTopic/config` and anchor event entity with `state_topic` / `json_attributes_topic` `{prefix}/anchor/event`.

---

## 9. Call sites that can trigger NTFY

Every NTFY is sent via `sendNotification(...)` → `sendNtfyNotification(...)`. Call sites:

| # | File | Function / context | Alert type |
|---|------|--------------------|------------|
| 1 | AlertEngine.cpp | sendShortTermAlert() | 1m/5m/30m short-term |
| 2 | AlertEngine.cpp | checkAndSendConfluenceAlert() | Smart Confluence |
| 3 | AlertEngine.cpp | checkAndNotify() — 1m spike block | 1m spike |
| 4 | AlertEngine.cpp | checkAndNotify() — 5m move block | 5m move |
| 5 | AlertEngine.cpp | checkAndNotify() — 30m move block | 30m move |
| 6 | AlertEngine.cpp | flushPendingSecondaryAlertInternal() | 2h secondary (coalesced) |
| 7 | AlertEngine.cpp | send2HNotification() | 2h primary/secondary |
| 8 | AlertEngine.cpp | maybeUpdateAutoAnchor() | Auto-anchor update |
| 9 | AnchorSystem.cpp | sendAnchorAlert() | Take profit / max loss |
| 10 | AnchorSystem.cpp | setAnchorPrice() (when !skipNotifications) | Anchor Set |

TrendDetector does not call `sendNotification` directly; it calls `AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, ...)` (counted under row 7).

---

## 10. Design traits to preserve (reliability)

- Single network mutex for all NTFY (and other HTTP) to avoid concurrent WiFi/HTTP use.
- NTFY send never runs under dataMutex (avoids blocking UI/API task).
- Explicit client cleanup per attempt (`http.end()` + `stream->stop()`), no connection reuse for NTFY.
- Input validation and length limits before building request.
- Global backoff after repeated failures to avoid storming the service.
- Short retry (2 attempts, small delays) only on retryable errors.
- Fixed timeouts for connect and read.

---

## 11. Version and reference

- **Codebase:** v5.09 parallel rebuild worktree, branch `rebuild-from-v509`, clean rebuild base from commit **82d22b4**.
- **Do not** use the current main working folder unless explicitly required.
- This baseline describes behaviour as implemented in the v5.09 codebase only; later changes are out of scope.
