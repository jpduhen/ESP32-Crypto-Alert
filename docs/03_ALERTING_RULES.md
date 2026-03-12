# 03 – Alertregels en cooldowns

## Doel

Dit document beschrijft **wanneer** welke alerts mogen worden *beslist* (drempels, voorwaarden) en **hoe vaak** (cooldowns, max per uur, 2h-throttling). AlertEngine (en AnchorSystem voor anchor-alerts) bouwen de payload (title, message, colorTag) en roepen extern `sendNotification()` aan; de **verzending** (NTFY, en voor anchor ook MQTT) gebeurt in de hoofdschets (.ino). Geen secrets of API-keys; alleen logica en configuratie.

---

## 1. Korte timeframes (1m, 5m, 30m)

### 1m spike

- **Eenheid ret_***: percentagepunten (0,31 = 0,31%); zie docs 02 en 07.
- **Basisdrempel**: `spike1m` (default `SPIKE_1M_THRESHOLD_DEFAULT`); na auto-volatility ontstaat `effectiveSpike1m`.
- **Voorwaarde**: `|ret_1m| >= effectiveSpike1m` (effective = basis of auto-volatility aangepast).
- **Extra**: richting (up/down), kleurtag (bijv. blauw/oranje/paars/rood); optioneel volume/range-check.
- **Cooldown**: `notificationCooldowns.cooldown1MinMs` (default 120000 = 2 min).
- **Max per uur**: `MAX_1M_ALERTS_PER_HOUR` (3).
- **Debounce**: `checkAlertConditions(...)` moet `true` zijn; pas dan roept AlertEngine `sendNotification()` aan (verzending in .ino) en worden `lastNotification1Min` en `alerts1MinThisHour` bijgewerkt.

### 5m move / 5m alert

- **5m-confirmatie voor 30m move**: `move5m` → `move5mThreshold` (default `MOVE_5M_THRESHOLD_DEFAULT`):  
  `|ret_5m| >= move5mThreshold` wordt gebruikt als **filter / bevestiging** bij 30m move alerts (geen zelfstandige 5m-alert).
- **Zelfstandige 5m move-alert**: `move5mAlert` → `move5mAlertThreshold` (default `MOVE_5M_ALERT_THRESHOLD_DEFAULT`):  
  `|ret_5m| >= move5mAlertThreshold` triggert een **losse 5m move-alert**, mits cooldown/volumenormen gehaald worden.
- **Cooldown**: `notificationCooldowns.cooldown5MinMs` (default 420000 = 7 min).
- **Max per uur**: `MAX_5M_ALERTS_PER_HOUR` (3).
- Zelfde patroon: `checkAlertConditions` met `lastNotification5Min`, `alerts5MinThisHour`.

### 30m move

- **Basisdrempel**: `move30m` (default `MOVE_30M_THRESHOLD_DEFAULT`); na auto-volatility ontstaat `effectiveMove30m`.
- **5m-confirmatie**: `move5m` wordt als filter gebruikt: voldoende 5m-move in **dezelfde richting** als 30m.
- **Hard override**: `move30mHardOverride` (default `MOVE_30M_HARD_OVERRIDE_DEFAULT`): bij extreem sterke 30m-move mag de alert **nooit** door 2h-context worden onderdrukt.
- **Voorwaarde** (normaal): `|ret_30m| >= effectiveMove30m` **én** `|ret_5m| >= move5mThreshold` in dezelfde richting, tenzij hard/priority override actief is.
- **Cooldown**: `notificationCooldowns.cooldown30MinMs` (default 900000 = 15 min).
- **Max per uur**: `MAX_30M_ALERTS_PER_HOUR` (2).
- Zelfde patroon: `checkAlertConditions` met `lastNotification30Min`, `alerts30MinThisHour`.

### Uur-reset

- `hourStartTime` wordt bijgehouden; wanneer een nieuw uur begint worden `alerts1MinThisHour`, `alerts30MinThisHour`, `alerts5MinThisHour` gereset (in AlertEngine bij check).

---

## 2. Smart Confluence (1m + 5m + trend)

- **Aan**: `smartConfluenceEnabled`.
- **Voorwaarden**:
  - Laatste 1m-event en 5m-event in **dezelfde richting** (beide UP of beide DOWN).
  - Beide events binnen een **tijdvenster** (CONFLUENCE_TIME_WINDOW_MS, 5 min).
  - 30m-trend ondersteunt die richting.
  - Volume/range OK (evaluateVolumeRange voor 5m).
  - Confluence-cooldown: sinds `lastConfluenceAlert` minstens CONFLUENCE_TIME_WINDOW_MS.
- **Debounce**: `last1mEvent.usedInConfluence` / `last5mEvent.usedInConfluence` zodat hetzelfde event niet twee keer in een confluence-alert zit.

---

## 3. Nachtstand (night mode)

- **Aan**: `nightModeEnabled` en huidige tijd in window (bijv. 23:00–07:00).
- **Effect**: Andere thresholds voor 5m/spike en 30m move (`nightSpike5mThreshold`, `nightMove5mAlertThreshold`, `nightMove30mThreshold`) en langere 5m-cooldown (`nightCooldown5mSec`). Auto-volatility wordt tijdelijk “aan” gezet met eigen min/max multipliers.
- Cooldowns en max-per-uur blijven van toepassing; alleen drempels en cooldown-waarden veranderen.

---

## 4. 2-uur alerts (breakout, mean touch, compress, trend change, anchor context)

### Typen

- **Breakout up/down**: prijs door 2h high/low; aparte cooldowns (`breakCooldownMs`), “armed” state na breakout.
- **Mean touch**: prijs nadert 2h-gemiddelde vanuit ver (boven/onder); `meanMinDistancePct`, `meanTouchBandPct`, `meanCooldownMs`.
- **Compress**: 2h-range onder drempel; `compressThresholdPct`, `compressResetPct`, `compressCooldownMs`.
- **Trend change**: 2h-trend wisselt (UP/DOWN/SIDEWAYS); `trendHysteresisFactor`, `throttlingTrendChangeMs`.
- **Anchor context**: anchor buiten 2h-range; `anchorOutsideMarginPct`, `anchorCooldownMs`.

### Throttling (2h)

- **PRIMARY** (breakout up/down): altijd toegestaan; override throttling.
- **SECONDARY** (mean touch, compress, trend change, anchor context):
  - **Global secondary cooldown**: `twoHSecondaryGlobalCooldownSec` (default 7200 = 120 min); na een secondary alert mag er een tijd geen andere secondary.
  - **Matrix cooldowns**: tijd sinds laatste 2h-alert bepalend; afhankelijk van (lastType, nextType) een minimale wachttijd (bijv. trend→trend 180 min, mean→mean 60 min).
  - **Coalescing**: `twoHSecondaryCoalesceWindowSec` (bijv. 90 s) om burst van secondaries te dempen (één “pending” secondary kan worden geflushed).
- `send2HNotification()` gebruikt `shouldThrottle2HAlert()`; PRIMARY slaat throttling over, SECONDARY wordt gesuppresseerd als throttling of global cooldown actief is.

---

## 5. Anchor alerts (take profit / max loss)

- **Take profit**: prijs >= anchor + effectieve take-profit% (trend-adaptive indien aan).
- **Max loss**: prijs <= anchor + effectieve max-loss% (negatief).
- **Eenmalig per event**: `anchorTakeProfitSent` / `anchorMaxLossSent` zodat dezelfde grens niet meerdere keren notificatie geeft tot anchor opnieuw wordt gezet.
- Geen aparte “cooldown in ms”; de vlaggen zorgen voor debounce.

---

## 6. Trend change (2h) cooldown

- Bij wisseling van 2h-trend (UP/DOWN/SIDEWAYS) kan een notificatie worden aangevraagd (TrendDetector/AlertEngine roept `sendNotification()` aan; verzending in .ino).
- **Cooldown**: `TREND_CHANGE_COOLDOWN_MS` (600000 = 10 min) via `lastTrendChangeNotification` (in TrendDetector / globals).

---

## 7. Waar staat het in de code

| Onderdeel | Locatie |
|-----------|---------|
| Default thresholds/cooldowns | .ino: `SPIKE_*`, `MOVE_*`, `NOTIFICATION_COOLDOWN_*`, `MAX_*_ALERTS_PER_HOUR` |
| Instelbare structs | `AlertThresholds`, `NotificationCooldowns`, `Alert2HThresholds` (SettingsStore.h / .ino) |
| Check & send 1m/5m/30m | `AlertEngine::checkAndNotify()` |
| checkAlertConditions | `AlertEngine::checkAlertConditions()` |
| Confluence | `AlertEngine::checkAndSendConfluenceAlert()` |
| 2h checks | `AlertEngine::check2HNotifications()` |
| 2h throttling | `AlertEngine::shouldThrottle2HAlert()`, `send2HNotification()` |
| Anchor | `AnchorSystem::checkAnchorAlerts()` |
| Trend change | `TrendDetector::checkTrendChange()` + cooldown |
| Transport (verzending) | .ino: `sendNotification()` → `sendNtfyNotification()`; anchor MQTT: `publishMqttAnchorEvent()` |

Deze regels bepalen wanneer een alert mag worden *beslist* en hoe vaak (cooldowns en limieten). De daadwerkelijke verzending (NTFY/MQTT) zit in .ino; er is geen aparte notifier-module in src/.

---
**[← 02 Dataflow](02_DATAFLOW.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking) | **[04 UI en LVGL →](04_UI_AND_LVGL.md)**
