# Phase 1 Implementation Summary

**Plan:** `docs/V509_MAINLINE_MIGRATION_RISK_ANALYSIS.md` — Phase 1 only  
**Date:** Implemented in v5.09 worktree (rebuild-from-v509).

---

## Before Changing Code

### Exact files edited

| File | Goal(s) |
|------|--------|
| `ESP32-Crypto-Alert-v509.ino` | Goal 2 (NTFY cleanup order), Goals 4/5 (remove auto-anchor calls), Goal 3 (button → queue) |
| `src/AlertEngine/AlertEngine.cpp` | Goal 4 (getActiveAnchorPrice manual-only) |
| `src/UIController/UIController.cpp` | Goal 3 (long-press anchor → queue) |

### Mapping edits to Phase 1 goals

- **Goal 1 (Keep v5.09 NTFY transport):** No change; baseline kept.
- **Goal 2 (Optionally adopt mainline cleanup order):** `.ino` — in `sendNtfyNotification`, cleanup order is now: get `stream`, `stream->stop()`, `http.end()`, `ntfyClient.stop()`.
- **Goal 3 (Pending-anchor deferral):** Web and MQTT already used `queueAnchorSetting` in v5.09. `.ino` checkButton and `UIController.cpp` long-press now call `queueAnchorSetting(0.0f, true)` instead of `anchorSystem.setAnchorPrice(0.0f)` so all anchor sets run in apiTask.
- **Goal 4 (Manual anchor only):** `AlertEngine.cpp` — `getActiveAnchorPrice()` returns `0.0f` for mode 3 (OFF), else `manualAnchorPrice`; AUTO/AUTO_FALLBACK no longer use auto value.
- **Goal 5 (Do not implement auto-anchor):** `.ino` — all `maybeUpdateAutoAnchor` calls and the block that scheduled them (doAutoAnchorForce, doAutoAnchorPeriodic, WS-triggered) removed from apiTask.
- **Goals 6–9:** No code added (no notification log, no move30mHardOverride, no webPassword, no config export).

### What was NOT touched

- **WebServer:** `handleAnchorSet` already uses `queueAnchorSetting`; unchanged.
- **MQTT callback:** Already uses `queueAnchorSetting` for anchor; unchanged.
- **SettingsStore, SettingsStore.h, AlertThresholds, Alert2HThresholds, NotificationCooldowns:** Unchanged.
- **AnchorSystem:** No changes.
- **NTFY:** Retry count (2), delays (250/750 ms), validation, backoff, input limits — unchanged.
- **Notification log:** Not added.
- **move30mHardOverride / force-allow logic:** Not added.
- **webPassword, config export:** Not added.
- **Auto-anchor settings in struct/UI:** Left in place; only runtime behavior is manual-only (no `maybeUpdateAutoAnchor`, `getActiveAnchorPrice` ignores AUTO/AUTO_FALLBACK).

---

## After Changing Code

### Exact code changes

1. **ESP32-Crypto-Alert-v509.ino**
   - **sendNtfyNotification (cleanup):** Replaced `http.end();` then `stream = http.getStreamPtr(); if (stream) stream->stop();` with: `stream = http.getStreamPtr(); if (stream) stream->stop(); http.end(); ntfyClient.stop();`.
   - **apiTask:** Removed the block that set `doAutoAnchorForce` / `doAutoAnchorPeriodic` and called `maybeUpdateAutoAnchor` (first-run, periodic, WS-triggered). Replaced the “Auto anchor timing flags” comment with “Phase 1: Auto-anchor uitgeschakeld (alleen manual anchor)”.
   - **checkButton (anchor set):** Replaced `anchorSystem.setAnchorPrice(0.0f)` with `queueAnchorSetting(0.0f, true)` and adjusted the failure message.

2. **src/AlertEngine/AlertEngine.cpp**
   - **getActiveAnchorPrice:** Replaced full mode switch with: if `anchorSourceMode == 3` return `0.0f`; else return `manualAnchorPrice`. Removed use of `autoAnchorLastValue` and AUTO/AUTO_FALLBACK branches.

3. **src/UIController/UIController.cpp**
   - **Extern:** Added `extern bool queueAnchorSetting(float value, bool useCurrentPrice);`.
   - **Long-press anchor:** Replaced `anchorSystem.setAnchorPrice(0.0f)` with `queueAnchorSetting(0.0f, true)` and adjusted the failure message.

### Confirmations

- **Auto-anchor is absent:** No code path calls `maybeUpdateAutoAnchor`. apiTask no longer schedules it. `getActiveAnchorPrice` does not read `autoAnchorLastValue`; only manual anchor (or OFF) is used.
- **Manual anchor + OFF remain:** `getActiveAnchorPrice` returns `manualAnchorPrice` for mode 0 (MANUAL) and for legacy modes 1/2; returns `0.0f` for mode 3 (OFF). Anchor set is still performed by `AnchorSystem::setAnchorPrice` when apiTask processes `pendingAnchorSetting` (from Web, MQTT, or button/UI queue).
- **NTFY transport remains baseline v5.09 except cleanup order:** Retry count (2), RETRY_DELAYS {250, 750}, same validation, same global backoff, same mutex usage. Only change: cleanup order is stream→stop, http.end(), ntfyClient.stop().

### Manual tests to run after build

1. **NTFY send:** Trigger a short-term alert (e.g. 1m spike or 5m/30m move) and confirm one NTFY notification is received; trigger again after cooldown and confirm no duplicate storm; disconnect WiFi, trigger condition, reconnect and confirm backoff/retry behavior.
2. **Anchor set from Web:** Open Web UI, set anchor (explicit value and “use current price”), submit; confirm “Anchor Set” NTFY and MQTT `anchor_set`; confirm anchor value on device/UI after next apiTask run.
3. **Anchor set from MQTT:** Publish to `.../config/anchorValue/set` with a value and with `current`/empty; confirm NTFY and MQTT and correct anchor on device.
4. **Anchor set from button (if present):** Long-press to set anchor; confirm NTFY and that anchor updates after a short delay (apiTask cycle).
5. **Anchor set from UIController long-press (if present):** Same as button — confirm queue path and NTFY.
6. **2h alerts with anchor:** Set anchor, wait for 2h breakout or other 2h alert; confirm PRIMARY/SECONDARY behavior and that 2h context uses manual anchor (or no anchor when mode OFF).
7. **Anchor source mode OFF:** Set anchor source to OFF (mode 3); confirm 2h alerts do not use anchor for context (getActiveAnchorPrice returns 0).
8. **TP/ML:** Set anchor, move price (or wait) so take profit or max loss triggers; confirm NTFY and MQTT `take_profit` / `max_loss`.
9. **No auto-anchor:** Leave device running; confirm no “Auto Anchor” NTFY and no automatic anchor value changes without user/Web/MQTT/button set.
10. **Build/regression:** Clean build, flash, run; confirm no crashes, no mutex timeouts in Serial, and that Web UI settings page and /save still work.
