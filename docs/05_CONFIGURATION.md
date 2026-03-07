# 05 – Configuratie

## Waar wordt wat geconfigureerd?

Configuratie zit op drie plekken:

1. **Compile-time**: `platform_config.h`, `lv_conf.h`, `.ino`-defines (constanten).
2. **Runtime, persistent**: NVS via **SettingsStore** (web UI, MQTT, of code na load).
3. **Runtime, niet-persistent**: globale variabelen die alleen in RAM staan tot reboot.

Er worden **geen** wachtwoorden of API-keys in code of documentatie gezet. NTFY-topic wordt o.a. gegenereerd uit device-ID; MQTT/wachtwoorden staan in NVS en worden via web/MQTT ingesteld.

---

## platform_config.h

- **Platform**: precies één van: `PLATFORM_CYD24`, `PLATFORM_CYD28_1USB`, `PLATFORM_CYD28_2USB`, `PLATFORM_TTGO`, `PLATFORM_ESP32S3_SUPERMINI`, `PLATFORM_ESP32S3_GEEK`, `PLATFORM_ESP32S3_LCDWIKI_28`, `PLATFORM_ESP32S3_4848S040`, `PLATFORM_ESP32S3_AMOLED_206`.
- **Versie**: `VERSION_STRING` (bijv. "5.15").
- **Debug**: `DEBUG_BUTTON_ONLY` (1 = alleen knop-log), `DEBUG_CALCULATIONS` (0 = uit op CYD/TTGO i.v.m. DRAM).
- **WebSocket**: `WS_ENABLED` (0/1).
- **Taal default**: `DEFAULT_LANGUAGE` (0 = NL, 1 = EN).
- **OTA**: `OTA_ENABLED`, `OTA_HOSTNAME` (alleen voor boards met voldoende flash-partities).

Per platform volgen o.a.: MQTT_TOPIC_PREFIX, DEVICE_NAME, SYMBOL_COUNT, CHART_WIDTH/HEIGHT, PRICE_BOX_Y_START, FONT_SIZE_*, BUTTON_PIN, HAS_PHYSICAL_BUTTON.

---

## lv_conf.h

- LVGL 9-parameters: kleurdiepte, heap size, refresh period, DPI, FreeRTOS, draw buffer, fonts.
- Geen applicatielogica; alleen LVGL-gedrag.

---

## Root .ino – vaste constanten

Enkele belangrijke (defaults; veel zijn overschrijfbaar via NVS/settings):

- **API**: `BITVAVO_API_BASE`, `HTTP_CONNECT_TIMEOUT_MS`, `HTTP_READ_TIMEOUT_MS`.
- **Intervals**: `UPDATE_UI_INTERVAL` (1000), `UPDATE_API_INTERVAL` (2000), `UPDATE_WEB_INTERVAL` (5000).
- **Anchor default**: `ANCHOR_TAKE_PROFIT_DEFAULT`, `ANCHOR_MAX_LOSS_DEFAULT`.
- **Trend**: `TREND_THRESHOLD_DEFAULT`, `TREND_CHANGE_COOLDOWN_MS`.
- **Volatiliteit**: `VOLATILITY_LOW_THRESHOLD_DEFAULT`, `VOLATILITY_HIGH_THRESHOLD_DEFAULT`, `VOLATILITY_LOOKBACK_MINUTES`.
- **Warm-start**: `WARM_START_*_DEFAULT`, `WARM_START_SKIP_1M_DEFAULT`, `WARM_START_SKIP_5M_DEFAULT`.
- **Alerts**: `SPIKE_1M_THRESHOLD_DEFAULT`, `MOVE_5M_ALERT_THRESHOLD_DEFAULT`, `NOTIFICATION_COOLDOWN_*_MS_DEFAULT`, `MAX_*_ALERTS_PER_HOUR`.
- **Arraygroottes**: `SECONDS_PER_MINUTE`, `SECONDS_PER_5MINUTES`, `MINUTES_FOR_30MIN_CALC`, enz.

---

## SettingsStore en NVS (CryptoMonitorSettings)

Alle onderstaande velden kunnen via **web UI** of **MQTT** worden ingesteld en worden in NVS opgeslagen. Alleen de logische groepen worden genoemd; geen volledige struct-definitie.

- **Basis**: ntfyTopic, bitvavoSymbol, duckdnsEnabled, duckdnsToken, webPassword, language, displayRotation.
- **Alert thresholds**: spike1m, spike5m, move30m, move5m, move5mAlert, threshold1MinUp/Down, threshold30MinUp/Down.
- **Notification cooldowns**: cooldown1MinMs, cooldown30MinMs, cooldown5MinMs.
- **2h-alerts**: breakMarginPct, breakCooldownMs, meanMinDistancePct, meanTouchBandPct, meanCooldownMs, compressThresholdPct, compressResetPct, compressCooldownMs, anchorOutsideMarginPct, anchorCooldownMs; trend hysteresis en throttling (trendChange, mean touch, compress, secondary global/coalesce).
- **Anchor**: anchorTakeProfit, anchorMaxLoss, anchorStrategy; trendAdaptiveAnchorsEnabled, uptrend/downtrend multipliers.
- **Smart Confluence / Nacht**: smartConfluenceEnabled; nightModeEnabled, nightModeStartHour/EndHour, nightSpike5mThreshold, nightMove5mAlertThreshold, nightMove30mThreshold, nightCooldown5mSec, nightAutoVolMin/MaxMultiplier.
- **Warm-start**: warmStartEnabled, warmStart1mExtraCandles, warmStart5m/30m/2hCandles, warmStartSkip1m, warmStartSkip5m.
- **Auto-volatility**: autoVolatilityEnabled, autoVolatilityWindowMinutes, autoVolatilityBaseline1mStdPct, autoVolatilityMin/MaxMultiplier.
- **Trend/vol**: trendThreshold, volatilityLowThreshold, volatilityHighThreshold.
- **MQTT**: mqttHost, mqttPort, mqttUser, mqttPass.

Load bij opstart: `settingsStore.load()` en waarden naar globals kopiëren. Save na wijziging (web/MQTT): `settingsStore.save(settings)` en globals bijwerken.

---

## PINS_*.h

Per board: alleen display- en busconfiguratie (pinnen, bus, gfx, DEV_DEVICE_INIT). Geen API- of netwerkgegevens.

---

## Geen secrets in documentatie

- NTFY-topic: gegenereerd (bijv. device-ID + "-alert"); niet hardcoded.
- MQTT-wachtwoord, webPassword, duckdnsToken: alleen via NVS/web/MQTT; nooit in broncode of docs.

---
**[← 04 UI en LVGL](04_UI_AND_LVGL.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking) | **[06 Operations →](06_OPERATIONS.md)**
