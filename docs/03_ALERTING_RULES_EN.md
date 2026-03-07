# 03 – Alerting rules and cooldowns

## Purpose

This document describes **when** which alerts may be *decided* (thresholds, conditions) and **how often** (cooldowns, max per hour, 2h throttling). AlertEngine (and AnchorSystem for anchor alerts) build the payload (title, message, colorTag) and call external `sendNotification()`; **delivery** (NTFY, and for anchor also MQTT) is in the main sketch (.ino). No secrets or API keys; only logic and configuration.

---

## 1. Short timeframes (1m, 5m, 30m)

### 1m spike

- **ret_* unit**: percentage points (0.31 = 0.31%); see docs 02 and 07.
- **Condition**: `|ret_1m| >= effectiveSpike1m` (effective = base or auto-volatility adjusted).
- **Extra**: direction (up/down), color tag (e.g. blue/orange/purple/red); optional volume/range check.
- **Cooldown**: `notificationCooldowns.cooldown1MinMs` (default 120000 = 2 min).
- **Max per hour**: `MAX_1M_ALERTS_PER_HOUR` (3).
- **Debounce**: `checkAlertConditions(...)` must be `true`; then AlertEngine calls `sendNotification()` (delivery in .ino) and updates `lastNotification1Min` and `alerts1MinThisHour`.

### 5m move / 5m alert

- **Move filter**: `|ret_5m| >= move5mThreshold` (filter, not necessarily notification).
- **5m alert**: `|ret_5m| >= move5mAlertThreshold` (stronger signal).
- **Cooldown**: `notificationCooldowns.cooldown5MinMs` (default 420000 = 7 min).
- **Max per hour**: `MAX_5M_ALERTS_PER_HOUR` (3).
- Same pattern: `checkAlertConditions` with `lastNotification5Min`, `alerts5MinThisHour`.

### 30m move

- **Condition**: `|ret_30m| >= effectiveMove30m` and often combined with ret_5m direction (same direction).
- **Cooldown**: `notificationCooldowns.cooldown30MinMs` (default 900000 = 15 min).
- **Max per hour**: `MAX_30M_ALERTS_PER_HOUR` (2).
- Same pattern: `checkAlertConditions` with `lastNotification30Min`, `alerts30MinThisHour`.

### Hour reset

- `hourStartTime` is tracked; when a new hour starts, `alerts1MinThisHour`, `alerts30MinThisHour`, `alerts5MinThisHour` are reset (in AlertEngine during check).

---

## 2. Smart Confluence (1m + 5m + trend)

- **On**: `smartConfluenceEnabled`.
- **Conditions**:
  - Last 1m event and 5m event in the **same direction** (both UP or both DOWN).
  - Both events within a **time window** (CONFLUENCE_TIME_WINDOW_MS, 5 min).
  - 30m trend supports that direction.
  - Volume/range OK (evaluateVolumeRange for 5m).
  - Confluence cooldown: since `lastConfluenceAlert` at least CONFLUENCE_TIME_WINDOW_MS.
- **Debounce**: `last1mEvent.usedInConfluence` / `last5mEvent.usedInConfluence` so the same event is not used twice in a confluence alert.

---

## 3. Night mode

- **On**: `nightModeEnabled` and current time in window (e.g. 23:00–07:00).
- **Effect**: Different thresholds for 5m/spike and 30m move (`nightSpike5mThreshold`, `nightMove5mAlertThreshold`, `nightMove30mThreshold`) and longer 5m cooldown (`nightCooldown5mSec`). Auto-volatility is temporarily "on" with its own min/max multipliers.
- Cooldowns and max-per-hour still apply; only thresholds and cooldown values change.

---

## 4. 2-hour alerts (breakout, mean touch, compress, trend change, anchor context)

### Types

- **Breakout up/down**: price through 2h high/low; separate cooldowns (`breakCooldownMs`), "armed" state after breakout.
- **Mean touch**: price approaches 2h average from far (above/below); `meanMinDistancePct`, `meanTouchBandPct`, `meanCooldownMs`.
- **Compress**: 2h range below threshold; `compressThresholdPct`, `compressResetPct`, `compressCooldownMs`.
- **Trend change**: 2h trend changes (UP/DOWN/SIDEWAYS); `trendHysteresisFactor`, `throttlingTrendChangeMs`.
- **Anchor context**: anchor outside 2h range; `anchorOutsideMarginPct`, `anchorCooldownMs`.

### Throttling (2h)

- **PRIMARY** (breakout up/down): always allowed; overrides throttling.
- **SECONDARY** (mean touch, compress, trend change, anchor context):
  - **Global secondary cooldown**: `twoHSecondaryGlobalCooldownSec` (default 7200 = 120 min); after a secondary alert no other secondary for a period.
  - **Matrix cooldowns**: time since last 2h alert type; depending on (lastType, nextType) a minimum wait (e.g. trend→trend 180 min, mean→mean 60 min).
  - **Coalescing**: `twoHSecondaryCoalesceWindowSec` (e.g. 90 s) to damp bursts of secondaries (one "pending" secondary can be flushed).
- `send2HNotification()` uses `shouldThrottle2HAlert()`; PRIMARY skips throttling, SECONDARY is suppressed when throttling or global cooldown is active.

---

## 5. Anchor alerts (take profit / max loss)

- **Take profit**: price >= anchor + effective take-profit% (trend-adaptive if enabled).
- **Max loss**: price <= anchor + effective max-loss% (negative).
- **Once per event**: `anchorTakeProfitSent` / `anchorMaxLossSent` so the same boundary does not notify multiple times until anchor is set again.
- No separate "cooldown in ms"; the flags provide debounce.

---

## 6. Trend change (2h) cooldown

- When 2h trend changes (UP/DOWN/SIDEWAYS) a notification can be requested (TrendDetector/AlertEngine calls `sendNotification()`; delivery in .ino).
- **Cooldown**: `TREND_CHANGE_COOLDOWN_MS` (600000 = 10 min) via `lastTrendChangeNotification` (in TrendDetector / globals).

---

## 7. Where it lives in code

| Part | Location |
|------|----------|
| Default thresholds/cooldowns | .ino: `THRESHOLD_*`, `SPIKE_*`, `MOVE_*`, `NOTIFICATION_COOLDOWN_*`, `MAX_*_ALERTS_PER_HOUR` |
| Configurable structs | `AlertThresholds`, `NotificationCooldowns`, `Alert2HThresholds` (SettingsStore.h / .ino) |
| Check & send 1m/5m/30m | `AlertEngine::checkAndNotify()` |
| checkAlertConditions | `AlertEngine::checkAlertConditions()` |
| Confluence | `AlertEngine::checkAndSendConfluenceAlert()` |
| 2h checks | `AlertEngine::check2HNotifications()` |
| 2h throttling | `AlertEngine::shouldThrottle2HAlert()`, `send2HNotification()` |
| Anchor | `AnchorSystem::checkAnchorAlerts()` |
| Trend change | `TrendDetector::checkTrendChange()` + cooldown |
| Transport (delivery) | .ino: `sendNotification()` → `sendNtfyNotification()`; anchor MQTT: `publishMqttAnchorEvent()` |

These rules determine when an alert may be *decided* and how often (cooldowns and limits). Actual delivery (NTFY/MQTT) is in .ino; there is no separate notifier module in src/.

---
**[← 02 Dataflow](02_DATAFLOW_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture) | **[04 UI and LVGL →](04_UI_AND_LVGL_EN.md)**
