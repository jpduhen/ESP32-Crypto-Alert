# 06 – Operations

## Startup order (setup)

1. Serial and device (backlight, pins).
2. Display (Arduino_GFX, PINS).
3. LVGL (uiController.setupLVGL()).
4. Watchdog.
5. WiFi event handlers.
6. Mutexes (dataMutex, gNetMutex).
7. Allocate dynamic arrays (fiveMinutePrices, minuteAverages, hourlyAverages; PSRAM if available).
8. Optional: Bitvavo stream buffer on heap.
9. Initialise source arrays (SOURCE_LIVE).
10. Connect WiFi + first price fetch (wifiConnectionAndFetchPrice).
11. PriceData.syncStateFromGlobals().
12. Bind WarmStartWrapper (settings, logger); if WiFi + warmStartEnabled: performWarmStart(), warmWrap.endRun().
13. buildUI().
14. First render (lv_refr_now or lv_timer_handler).
15. Start FreeRTOS tasks: apiTask, priceRepeatTask, uiTask, webTask.

Then: loop() for OTA, MQTT reconnect, deferred actions; apiTask for fetch and alerts; uiTask for updateUI; webTask for handleClient.

---

## Normal operation

- **apiTask**: every UPDATE_API_INTERVAL (2 s) fetchPrice(); with that: mutex, price into buffers, updateMinuteAverage (every 60 s), compute returns, checkAnchorAlerts(), alertEngine.checkAndNotify(), mutex release; then priceRepeatTask fills the same price into the ring buffer every 2 s.
- **uiTask**: every UPDATE_UI_INTERVAL (1 s) short mutex check, then updateUI(); regularly lv_task_handler() or lv_refr_now().
- **webTask**: every 100 ms handleClient().
- **loop()**: OTA (if OTA_ENABLED), MQTT reconnect/backoff, processMqttQueue(), deferred MQTT IP publish.

---

## Error handling (high level)

- **WiFi down**: apiTask and webTask wait for WL_CONNECTED; fetch is not run; priceRepeatTask keeps repeating last price.
- **API timeout/error**: ApiClient returns false; fetchPrice() retry/backoff in .ino; no crash; buffers unchanged.
- **Mutex timeout**: apiTask logs and retries later; uiTask skips one update.
- **NTFY/MQTT error**: Transport is in .ino (sendNotification → sendNtfyNotification). Logging only; alert state (lastNotification*, alerts*ThisHour) is updated on "sent", so no duplicate attempts on top of cooldown.
- **NVS full/corrupt**: SettingsStore load() falls back to defaults; save() can fail, then old state remains in RAM.

---

## Memory and stability

- **HeapMon**: logHeap("tag") with rate limit (e.g. every 60 s in apiTask) for fragmentation audit.
- **Buffers**: secondPrices fixed; fiveMinutePrices, minuteAverages, hourlyAverages dynamic (INTERNAL or SPIRAM); Bitvavo stream buffer heap or fallback.
- (Legacy: on boards without PSRAM an array guard was previously used; after CYD removal no longer in use.)

---

## OTA (when enabled)

- Only on boards with OTA_ENABLED and partition scheme with two app partitions.
- Web endpoints: /update (GET/POST), chunk upload; on success Update.end() and reboot.
- No sensitive data in OTA payload; firmware is uploaded via browser.

---

## What is not in docs/commits

- No API keys, passwords or tokens in source or documentation.
- NTFY topic, MQTT credentials, webPassword, DuckDNS token only via NVS or secure configuration (web UI with password or local network).

---
**[← 05 Configuration](05_CONFIGURATION_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture) | **[07 Glossary →](07_GLOSSARY_EN.md)**
