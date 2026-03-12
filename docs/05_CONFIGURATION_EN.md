# 05 – Configuration

## Where is what configured?

Configuration lives in three places:

1. **Compile-time**: `platform_config.h`, `lv_conf.h`, `.ino` defines (constants).
2. **Runtime, persistent**: NVS via **SettingsStore** (web UI, MQTT, or code after load).
3. **Runtime, non-persistent**: global variables that only live in RAM until reboot.

**No** passwords or API keys are set in code or documentation. NTFY topic is e.g. generated from device ID; MQTT/passwords are in NVS and set via web/MQTT.

---

## platform_config.h

- **Platform**: exactly one of: `PLATFORM_TTGO`, `PLATFORM_ESP32S3_SUPERMINI`, `PLATFORM_ESP32S3_GEEK`, `PLATFORM_ESP32S3_LCDWIKI_28`, `PLATFORM_ESP32S3_4848S040`, `PLATFORM_ESP32S3_AMOLED_206`.
- **Version**: `VERSION_STRING` (e.g. "5.15").
- **Debug**: `DEBUG_BUTTON_ONLY` (1 = button log only), `DEBUG_CALCULATIONS` (0 = off on TTGO due to DRAM).
- **WebSocket**: `WS_ENABLED` (0/1).
- **Default language**: `DEFAULT_LANGUAGE` (0 = NL, 1 = EN).
- **OTA**: `OTA_ENABLED`, `OTA_HOSTNAME` (only for boards with suitable flash partitions).

Per platform follow e.g.: MQTT_TOPIC_PREFIX, DEVICE_NAME, SYMBOL_COUNT, CHART_WIDTH/HEIGHT, PRICE_BOX_Y_START, FONT_SIZE_*, BUTTON_PIN, HAS_PHYSICAL_BUTTON.

---

## lv_conf.h

- LVGL 9 parameters: colour depth, heap size, refresh period, DPI, FreeRTOS, draw buffer, fonts.
- No application logic; only LVGL behaviour.

---

## Root .ino – fixed constants

Some important ones (defaults; many are overridable via NVS/settings):

- **API**: `BITVAVO_API_BASE`, `HTTP_CONNECT_TIMEOUT_MS`, `HTTP_READ_TIMEOUT_MS`.
- **Intervals**: `UPDATE_UI_INTERVAL` (1000), `UPDATE_API_INTERVAL` (2000), `UPDATE_WEB_INTERVAL` (5000).
- **Anchor default**: `ANCHOR_TAKE_PROFIT_DEFAULT`, `ANCHOR_MAX_LOSS_DEFAULT`.
- **Trend**: `TREND_THRESHOLD_DEFAULT`, `TREND_CHANGE_COOLDOWN_MS`.
- **Volatility**: `VOLATILITY_LOW_THRESHOLD_DEFAULT`, `VOLATILITY_HIGH_THRESHOLD_DEFAULT`, `VOLATILITY_LOOKBACK_MINUTES`.
- **Warm-start**: `WARM_START_*_DEFAULT`, `WARM_START_SKIP_1M_DEFAULT`, `WARM_START_SKIP_5M_DEFAULT`.
- **Alerts**: `SPIKE_1M_THRESHOLD_DEFAULT`, `SPIKE_5M_THRESHOLD_DEFAULT`, `MOVE_30M_THRESHOLD_DEFAULT`, `MOVE_30M_HARD_OVERRIDE_DEFAULT`, `MOVE_5M_THRESHOLD_DEFAULT`, `MOVE_5M_ALERT_THRESHOLD_DEFAULT`, `NOTIFICATION_COOLDOWN_*_MS_DEFAULT`, `MAX_*_ALERTS_PER_HOUR`.
- **Array sizes**: `SECONDS_PER_MINUTE`, `SECONDS_PER_5MINUTES`, `MINUTES_FOR_30MIN_CALC`, etc.

---

## SettingsStore and NVS (CryptoMonitorSettings)

All fields below can be set via the **web UI** or **MQTT** and are stored in NVS. Only logical groups are listed; not the full struct definition.

- **Basic**: ntfyTopic, bitvavoSymbol, duckdnsEnabled, duckdnsToken, webPassword, language, displayRotation.
- **Alert thresholds**: spike1m, spike5m, move30m, move5m, move5mAlert.
- **Notification cooldowns**: cooldown1MinMs, cooldown30MinMs, cooldown5MinMs.
- **2h alerts**: breakMarginPct, breakCooldownMs, meanMinDistancePct, meanTouchBandPct, meanCooldownMs, compressThresholdPct, compressResetPct, compressCooldownMs, anchorOutsideMarginPct, anchorCooldownMs; trend hysteresis and throttling (trendChange, mean touch, compress, secondary global/coalesce).
- **Anchor**: anchorTakeProfit, anchorMaxLoss, anchorStrategy; trendAdaptiveAnchorsEnabled, uptrend/downtrend multipliers.
- **Smart Confluence / Night**: smartConfluenceEnabled; nightModeEnabled, nightModeStartHour/EndHour, nightSpike5mThreshold, nightMove5mAlertThreshold, nightMove30mThreshold, nightCooldown5mSec, nightAutoVolMin/MaxMultiplier.
- **Warm-start**: warmStartEnabled, warmStart1mExtraCandles, warmStart5m/30m/2hCandles, warmStartSkip1m, warmStartSkip5m.
- **Auto-volatility**: autoVolatilityEnabled, autoVolatilityWindowMinutes, autoVolatilityBaseline1mStdPct, autoVolatilityMin/MaxMultiplier.
- **Trend/vol**: trendThreshold, volatilityLowThreshold, volatilityHighThreshold.
- **MQTT**: mqttHost, mqttPort, mqttUser, mqttPass.

Load on boot: `settingsStore.load()` and copy values to globals. Save after change (web/MQTT): `settingsStore.save(settings)` and update globals.

---

## PINS_*.h

Per board: only display and bus configuration (pins, bus, gfx, DEV_DEVICE_INIT). No API or network data.

---

## No secrets in documentation

- NTFY topic: generated (e.g. device ID + "-alert"); not hardcoded.
- MQTT password, webPassword, duckdnsToken: only via NVS/web/MQTT; never in source or docs.

---
**[← 04 UI and LVGL](04_UI_AND_LVGL_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture) | **[06 Operations →](06_OPERATIONS_EN.md)**
