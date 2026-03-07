# NLM Key Points – UNIFIED-LVGL9 Crypto Monitor

Max one page of bullets; based on /docs. No code changes.

---

- **ret_* unit:** All returns (ret_1m, ret_5m, ret_30m, ret_2h, …) are **percentage points**: `(priceNow - priceXAgo) / priceXAgo * 100`. Example: 0.12 = 0.12%. Thresholds (spike1m, move5m, etc.) use the same unit.
- **Cooldowns and max-per-hour:** Per alert type (1m, 5m, 30m) a cooldown in ms (e.g. 1m default 2 min, 30m 15 min) and a max number of alerts per hour (e.g. 1m: 3, 30m: 2). Only when cooldown has passed and the hour limit is not reached may a notification be *requested* (AlertEngine calls `sendNotification`; delivery in .ino).
- **2h throttling:** **PRIMARY** (breakout up/down) skips throttling. **SECONDARY** (mean touch, compress, trend change, anchor context) is subject to: (1) **global secondary cooldown** (default 7200 s = 120 min), (2) matrix cooldowns between last and next 2h alert type, (3) **coalescing** window (e.g. 90 s) to damp bursts. `shouldThrottle2HAlert` suppresses secondaries when any of these is active.
- **PriceRepeat warning:** With API failure or slow responses priceRepeatTask puts the *last* price into the ring buffer every 2 s; 1m/5m returns can be temporarily flattened (volatility underestimated) until new API prices arrive.
- **WS feature-flag, HTTP primary:** WebSocket is feature-flagged (WS_ENABLED in platform_config.h). HTTP polling via ApiClient is the **primary** price path; WS is used step by step in .ino.
- **Decision vs delivery:** AlertEngine and AnchorSystem **decide** and build the payload (title, message, colorTag); they call external `sendNotification()`. **Delivery** (NTFY via `sendNtfyNotification()` in .ino; anchor also MQTT via `publishMqttAnchorEvent()` in .ino) is in the main sketch; there is no separate notifier module in src/.
- **DataSource:** SOURCE_BINANCE = legacy enum name; functionally warm-start/historical candles (Bitvavo). SOURCE_LIVE = live API. See PriceData.h.
- **State:** Volatile (RAM): prices, ring buffers, anchor, trend/vol state, cooldown timestamps. Persistent: NVS via SettingsStore (CryptoMonitorSettings). Derived: ret_*, trend/vol states, hasRet* flags.

**Trade-offs / Known limitations**

- **UI snapshot not implemented:** Snapshot under mutex is recommended but not in code; uiTask reads globals without mutex during updateUI(), with risk of an inconsistent frame (mix of old/new values).

---
**[← Story Script](NLM_Story_Script_EN.md)** | [Technical docs overview (README)](../README.md#technical-documentation-code--architecture) | **[Examples →](NLM_Examples_EN.md)**
