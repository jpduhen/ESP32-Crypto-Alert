# NLM Examples – UNIFIED-LVGL9 Crypto Monitor

Two worked examples with numbers. Terminology consistent with /docs. Values not exactly in docs are marked as *(example)*.

---

## Example A: 1m spike — calculation, threshold, cooldown, payload, transport

**Situation:** We choose price numbers so that ret_1m exactly triggers a "real" alert: 0.35 percentage points (0.35%). Then come threshold check (0.31), cooldown, max-per-hour, payload and transport path.

**1. ret_1m in percentage points**

- Formula: `(priceNow - priceXAgo) / priceXAgo * 100`.
- *(Example)* Price 60 seconds ago: 50 000 EUR. Price now: 50 175 EUR.  
  ret_1m = (50 175 − 50 000) / 50 000 * 100 = 175 / 50 000 * 100 = **0.35** (i.e. 0.35%).
- Unit: percentage points; 0.35 means 0.35%.

**2. Threshold check**

- 1m spike condition: `|ret_1m| >= effectiveSpike1m`. Default spike1m (docs) is 0.31 (0.31%).
- *(Example)* effectiveSpike1m = 0.31. |0.35| = 0.35 ≥ 0.31 → **threshold exceeded** → spike alert may fire (if cooldown and hour limit OK).
- Contrast: if ret_1m &lt; 0.31 (e.g. 0.20), no 1m spike alert is triggered regardless of cooldown.

**3. Cooldown and hour-limit check**

- `checkAlertConditions(now, lastNotification1Min, cooldown1MinMs, alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR)` must be true.
- Default cooldown1MinMs = 120 000 (2 min). MAX_1M_ALERTS_PER_HOUR = 3.
- *(Example)* Last 1m notification was 3 minutes ago; this hour only 1 of 3 alerts so far → cooldown passed and hour limit not reached → **conditions OK**.

**4. Payload**

- AlertEngine builds title, message and colorTag (direction and optionally strength). No delivery in the module; only call `sendNotification(title, msg, colorTag)`.

**5. Transport path**

- In .ino: `sendNotification()` is called → calls `sendNtfyNotification(title, message, colorTag)` → NTFY HTTPS request to ntfy.sh. Anchor events also go via `publishMqttAnchorEvent()` to MQTT; for a normal 1m spike only NTFY.

**Summary A:** ret_1m in percentage points → threshold check (same unit) → cooldown + max-per-hour → payload in AlertEngine → delivery in .ino (sendNotification → sendNtfyNotification).

---

## Example B: 2h secondary (mean touch or compress) suppressed by global secondary cooldown/throttling

**Situation:** A 2h secondary alert would normally fire — e.g. "mean touch" (price approaches 2h average) or "compress" (2h range below threshold). Yet **no** notification is sent. We show why.

**1. Type: SECONDARY**

- Mean touch and compress are **SECONDARY** 2h alerts. Only breakout up/down are PRIMARY (and skip throttling).
- Because this is a secondary, `shouldThrottle2HAlert(alertType, now)` is evaluated before delivery.

**2. Global secondary cooldown**

- `twoHSecondaryGlobalCooldownSec` has default 7200 (120 minutes).
- *(Example)* Another secondary alert was sent 45 minutes ago (e.g. trend change). Time since last secondary = 45 * 60 = 2700 s. 2700 < 7200 → **suppressed** by global secondary cooldown; no notification is requested.
- Even if the mean-touch or compress conditions are met, there is no sendNotification call for this secondary now.

**3. Matrix cooldown (if global cooldown had passed)**

- Suppose the global cooldown had passed. Then the throttling matrix looks at the *last* 2h alert type and the *next* (e.g. mean touch). There is a minimum wait between certain type combinations (e.g. mean→mean 60 min, trend→mean 60 min; exact matrix in code/docs).
- *(Example)* Last alert was 30 min ago a "mean touch"; now mean touch again. Matrix cooldown mean→mean = 60 min. 30 min < 60 min → **suppressed** by matrix cooldown; again no notification.

**4. No notification**

- As soon as `shouldThrottle2HAlert` returns true, `send2HNotification` / `sendNotification` for this secondary is **not** called. So the user sees no second secondary alert until the relevant cooldowns have passed.

**Summary B:** 2h secondary (mean touch or compress) → shouldThrottle2HAlert: first global secondary cooldown (e.g. 120 min), else matrix cooldown → on suppress no sendNotification → no notification. PRIMARY (breakout) would have gone through.

---
**[← Story Script](NLM_Story_Script_EN.md)** | **[← Key Points](NLM_Key_Points_EN.md)** | [Technical docs overview (README)](../README.md#technical-documentation-code--architecture)
