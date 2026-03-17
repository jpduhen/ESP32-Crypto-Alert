# v5.09 vs Mainline — Notification Reliability Migration Risk Analysis

**Baseline:** `docs/V509_NOTIFICATION_BASELINE.md` (v5.09, commit 82d22b4)  
**Comparison target:** Sibling project `../UNIFIED-LVGL9-Crypto_Monitor` (mainline)  
**Focus:** Notification reliability; no code changes in this document.

---

## 1. NTFY transport

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Retry count** | 2 attempts (MAX_RETRIES=1) | 3 attempts (MAX_RETRIES=2) | Low. More attempts can prolong hold of gNetMutex under flaky network. | Keep v5.09 (2 attempts) initially. |
| **Retry delays** | 250 ms, 750 ms | 1500 ms, 2500 ms | Medium. Longer delays = gNetMutex held longer; other HTTP (API, warm-start) blocks. | Keep v5.09 delays; reintroduce longer delays only after transport is stable. |
| **Cleanup order** | `http.end()` then `stream->stop()` | `stream->stop()` then `http.end()` then `ntfyClient.stop()` | Low. Mainline order is more defensive (stream first, explicit client stop). | Safe to adopt mainline cleanup order early. |
| **Error context logging** | None on code &lt; 0 | Extra Serial dump (WiFi.status, RSSI, IPs, sinceLastOk) on failure | Low. Purely diagnostic; can increase Serial load during outages. | Reintroduce only after core send path is verified. |
| **appendNotificationLog** | Not present | Called after every sendNotification (ring buffer + s_notifLogMutex) | Medium. Extra mutex and buffer writes on every send; if log mutex fails to create, append is skipped but send still runs. | Do **not** reintroduce until NTFY send is rock-solid; then add as optional feature. |

**Summary:** Mainline added more retries, longer delays, and notification log. For reliability-first rebuild: keep v5.09 attempt count and delays; adopt mainline cleanup order only. Defer notification log and heavy error logging.

---

## 2. Runtime / task structure

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **apiTask → notify** | dataMutex released, then checkAndNotify / checkAnchorAlerts / check2HNotifications | Same pattern | None. | Keep as-is. |
| **Anchor set from Web** | Direct call to setAnchorPrice from webTask (after mutex release inside setAnchorPrice) | **Deferred:** Web sets `pendingAnchorSetting`; apiTask consumes it and calls setAnchorPrice in apiTask context | Medium. Mainline avoids webTask doing HTTPS (setAnchorPrice → sendNotification) by moving anchor set to apiTask. Reduces risk of nested mutex or different-task HTTP. | **Reintroduce early:** pending-anchor deferral to apiTask is a clear reliability win; no extra notification logic. |
| **Notification log mutex** | N/A | s_notifLogMutex created in setup(); append uses try-lock only | Low if mutex created; append is best-effort. If creation fails, log disabled. | Defer until after notification log feature is desired. |
| **Stack sizes** | 8192 / 8192 / 5120 (API/UI/Web) or platform-specific | 10240 / 10240 / 6144 (ESP32-S3), 8192 / 8192 / 5120 else | Low. Larger stacks reduce overflow risk under heavy use. | Safe to align with mainline stack sizes when rebuilding. |

**Summary:** Keep v5.09 call order; adopt **pending-anchor deferral to apiTask** early. Defer notification-log mutex and log feature.

---

## 3. WebSocket / REST interaction

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Price source** | REST or WebSocket; apiTask uses same fetchPrice() and dataMutex pattern | Same; usedWs and apiMutexTimeout for WS path | None for notification path. Notifications are driven by data written under dataMutex; send happens after release. | No change needed for notification reliability. |
| **Auto-anchor from WS** | maybeUpdateAutoAnchor() in apiTask (REST/WS EMA candles); can trigger NTFY | Mainline has **no** auto-anchor; getActiveAnchorPrice(manual) only | High if reintroducing auto-anchor later: EMA fetch and anchor-update NTFY add code paths and timing that can race or block. | Keep v5.09 auto-anchor behaviour out of scope until NTFY and manual anchor are stable; then reintroduce as a separate phase. |

**Summary:** WebSocket/REST mix does not itself increase notification risk. The main difference is **auto-anchor**: mainline removed it (manual-only anchor). For rebuild: keep v5.09’s manual + auto-anchor design in the spec but **implement manual-only first**; add auto-anchor in a later phase.

---

## 4. MQTT interaction

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Anchor events** | publishMqttAnchorEvent(price, "anchor_set" \| "take_profit" \| "max_loss"); topic `{prefix}/anchor/event` | Same | None. | Keep as in baseline. |
| **ntfyTopic over MQTT** | config/ntfyTopic/set → handleMqttStringSetting; publishMqttString("ntfyTopic", …) | Same; discovery uses table entry for ntfyTopic | Low. Mainline may have more discovery entities. | Keep v5.09 MQTT topic handling; align discovery later if needed. |
| **Settings publish** | publishMqttSettings() with many keys (cooldowns, 2h, anchor, etc.) | Same idea; possible extra keys (e.g. webPassword, move30mHardOverride) | Low. More keys = more surface for MQTT callback and save logic. | Reintroduce MQTT settings publish in line with baseline; add mainline-only keys only when corresponding settings exist. |

**Summary:** MQTT behaviour is aligned; mainline has no fundamental change that hurts notification reliability. Rebuild: keep baseline anchor events and ntfyTopic; add extra MQTT keys only when the matching settings are reintroduced.

---

## 5. Anchor / auto-anchor behaviour

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Auto-anchor** | Full: AutoAnchorPersist blob, maybeUpdateAutoAnchor(), EMA 4h/1d, notify on update, anchorSourceMode 0/1/2/3 | **Removed.** anchorSourceMode 0 (manual) or 3 (OFF); getActiveAnchorPrice(manual) returns manual or 0 | High if porting mainline → v5.09: v5.09 has many more code paths (EMA fetch, NVS blob, 2h anchor source). | **Do not reintroduce** auto-anchor from mainline (it’s not there). Rebuild from v5.09: implement **manual anchor first** (and 2h using manual/OFF); add auto-anchor in a later phase with tests. |
| **2h anchor source** | getActiveAnchorPrice() uses manual or auto value by anchorSourceMode | Manual only or OFF | Lower complexity in mainline = fewer ways for 2h to misbehave. | Keep 2h using manual (or OFF) until auto-anchor is back. |
| **Anchor set NTFY** | setAnchorPrice(..., skipNotifications=false) sends once after dataMutex release | Same; plus mainline defers set to apiTask (see §2) | Deferral reduces risk of webTask doing HTTP. | Keep deferral; keep single “Anchor Set” NTFY. |
| **TP/ML alerts** | AnchorSystem.checkAnchorAlerts(); sendNotification + publishMqttAnchorEvent | Same | None. | Keep as baseline. |

**Summary:** Mainline simplified by dropping auto-anchor. For reliability-focused rebuild: **keep v5.09 design on paper** but **implement manual anchor + 2h (manual/OFF) first**; reintroduce auto-anchor and related settings (NVS blob, EMA, notify on update) **late**, with clear tests.

---

## 6. Settings complexity

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **AlertThresholds** | spike1m, spike5m, move30m, move5m, move5mAlert, **threshold1MinUp/Down, threshold30MinUp/Down** | **move30mHardOverride** instead of threshold1Min/30Min; no threshold1Min/30Min | Medium. Different semantics: v5.09 uses 1m/30Min thresholds; mainline uses 30m “hard override” and forceAllow/priority logic. Migration between the two is easy to get wrong. | Keep v5.09 struct and NVS keys first. Reintroduce move30mHardOverride (and shouldForce30mPriorityAlert) only after short-term alerts are stable. |
| **Alert2HThresholds** | Full 2h + **auto-anchor fields** (update intervals, candles, weights, flags, blob) | 2h only; **anchorSourceMode** only (no auto-anchor in struct) | High if mixing: v5.09 has many auto-anchor fields and helpers; mainline has a slim 2h block. | Rebuild with v5.09 2h + auto-anchor struct but **don’t load/save auto-anchor** until auto-anchor feature is reintroduced; or keep mainline-sized struct and add auto-anchor later. |
| **CryptoMonitorSettings** | No webPassword | **webPassword[64]** | Low. Adds one string and auth checks; not tied to NTFY send. | Reintroduce when Web UI auth is required; not needed for notification reliability. |
| **NVS / load/save** | Many keys; AutoAnchorPersist blob; migration helpers | Fewer keys (no auto-anchor blob); possible extra keys (MOVE30M_HARD, WEB_PASSWORD) | Medium. More keys and blobs = more load/save paths and migration cases. | Keep v5.09 key set for notification-related settings; add mainline-only keys only when features are added. |

**Summary:** Mainline reduced settings (no auto-anchor, different 1m/30m semantics). Rebuild: **keep v5.09 notification-related settings and defaults**; add move30mHardOverride and webPassword only when their features are reintroduced.

---

## 7. Web UI complexity

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Notification log in UI** | None | Section that reads getNotificationLogCount / getNotificationLogEntry and displays last N notifications | Low for send path; adds Web UI code and mutex reads. | **Reintroduce late**; after NTFY and cooldowns are stable. |
| **/status** | GET /status JSON (price, returns, trend, etc.) | Same idea; may have more fields or polling (e.g. fetch('/status')) | Low. Status is read-only; does not trigger notifications. | Safe to align with mainline /status when convenient. |
| **Settings form** | Single big form POST /save | Same; mainline may have extra fields (move30mHardOverride, webPassword, export) | Low. Extra fields only matter if settings struct and handleSave grow. | Keep v5.09 form and handleSave for notification-related fields; add mainline-only fields when features exist. |
| **Anchor set** | POST /anchor/set | Same; mainline may use deferred anchor (form posts, apiTask sets) | Deferral improves reliability (see §2). | Use deferred anchor set from mainline when reintroducing Web UI. |
| **Export / config dump** | Not in baseline | Mainline may have SEND_LINE_C / export of config (e.g. for backup) | Low. No impact on send path. | Reintroduce when needed for ops; not required for notification reliability. |

**Summary:** Mainline adds notification log display and possibly export/auth. Rebuild: **keep v5.09 Web UI for notifications** (NTFY topic, anchor, cooldowns, 2h); add **deferred anchor set** early; add **notification log section and export** later.

---

## 8. Alert frequency / throttling behaviour

| Aspect | v5.09 baseline | Mainline delta | Regression risk | Rebuild advice |
|--------|----------------|----------------|-----------------|----------------|
| **Short-term (1m/5m/30m)** | checkAlertConditions(cooldown + hourly cap); volume-event cooldown 120 s; night 5m cooldown | Same plus: **last30mAlertMs** “extra guard”; **shouldForce30mPriorityAlert** and **move30mHardOverride**; **debugLogAdd** and **debugVerbose30mWebLog** for 30m | Medium. Extra 30m logic (force allow, hard override, extra guard) and debug logging add branches and state; bugs could over-send or under-send. | Keep v5.09 short-term logic first. Reintroduce move30mHardOverride and forceAllow logic only after baseline 1m/5m/30m and 2h are stable. |
| **2h PRIMARY/SECONDARY** | PRIMARY = breakout up/down; SECONDARY = rest; throttling matrix + global secondary cooldown + coalescing | Same; mainline **send2HNotification** has extra params (meanTouchFromAbove, trendChangeUp) and **arrow-only title prefix** (stripTrailingArrows, get2HArrowOnlyPrefix) | Low for reliability; more code paths for title formatting. | Keep v5.09 send2HNotification signature and title format first; add mainline arrow-only prefix and params later if desired. |
| **2h coalescing** | Pending secondary; flush on PRIMARY or when window expires | Same | None. | Keep as baseline. |
| **Hourly reset** | hourStartTime; reset alerts*ThisHour every 3600 s | Same | None. | Keep as baseline. |

**Summary:** Mainline added 30m “hard override”, extra 30m guard, and 2h title formatting. Rebuild: **keep v5.09 throttling and hourly limits**; reintroduce 30m override/guard and 2h title tweaks **after** core alert and NTFY behaviour are validated.

---

## Prioritized rebuild advice

### Phase 1 — Core NTFY and tasks (do first)

1. **Keep v5.09 NTFY transport:** 2 attempts, delays 250/750 ms, same validation and global backoff. Optionally adopt mainline **cleanup order** (stream→stop, http.end, ntfyClient.stop) only.
2. **Adopt pending-anchor deferral:** Web/MQTT/UI set a “pending” flag; apiTask performs setAnchorPrice (and thus NTFY + MQTT) in apiTask context. No new notification types.
3. **Keep v5.09 task and mutex pattern:** dataMutex released before checkAndNotify / checkAnchorAlerts / check2HNotifications; no send under dataMutex.
4. **Manual anchor only initially:** Implement getActiveAnchorPrice(manual) with mode 0/3 (manual/OFF) only; no auto-anchor yet.
5. **Keep baseline settings for notifications:** Cooldowns, 2h thresholds, anchor TP/ML, trend-adaptive; no webPassword, no move30mHardOverride in Phase 1.

### Phase 2 — After Phase 1 is stable

6. **Optional:** Align stack sizes with mainline if needed.
7. **Optional:** Add mainline-style **error context logging** (WiFi/RSSI/IPs) on NTFY failure only if diagnostics are needed.
8. **Optional:** Add **notification log** (ring buffer + try-lock append) and **Web UI section** for last N notifications; ensure s_notifLogMutex creation does not affect send path.

### Phase 3 — Features and polish (late)

9. **Reintroduce auto-anchor:** NVS blob, maybeUpdateAutoAnchor, EMA fetch, notify-on-update, anchorSourceMode 1/2; full tests and 2h integration.
10. **Reintroduce move30mHardOverride and 30m priority/force logic** if desired; keep cooldown and hourly caps.
11. **Reintroduce send2HNotification** mainline signature (meanTouchFromAbove, trendChangeUp) and arrow-only title formatting if UI polish is desired.
12. **Reintroduce webPassword and Web auth** when needed; not required for notification reliability.
13. **Reintroduce config export** (SEND_LINE_C, etc.) when needed for ops.

### Do not reintroduce until late

- **Three NTFY attempts** and 1500/2500 ms retry delays (increases mutex hold time).
- **Auto-anchor** (many code paths; reintroduce in Phase 3 with tests).
- **move30mHardOverride / shouldForce30mPriorityAlert** and extra 30m guards (add after baseline alerts are stable).
- **Notification log** (add in Phase 2 at earliest, after send path is verified).
- **Heavy error-context logging** on every failure (add only if diagnostics are needed).

---

## Document and scope

- **Baseline:** `docs/V509_NOTIFICATION_BASELINE.md`  
- **Comparison:** `../UNIFIED-LVGL9-Crypto_Monitor` (mainline)  
- **Purpose:** Categorized migration risk for notification reliability; prioritized rebuild order.  
- **No code changes** in this repository; this is a reference only.
