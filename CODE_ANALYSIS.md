# Code Analyse - Crypto Monitor

**Datum:** 2025-12-17 22:30  
**Status:** In uitvoering

---

## Overzicht

- **Totaal regels:** 8169
- **Statische variabelen:** ~319
- **Functies:** ~353

---

## Globale Variabelen Categorisatie

### 1. Display & UI (LVGL)
**Module:** `UIController`
- `lv_display_t *disp`
- `static lv_color_t *disp_draw_buf`
- `static lv_obj_t *chart`
- `static lv_chart_series_t *dataSeries`
- `static lv_obj_t *priceBox[SYMBOL_COUNT]`
- `static lv_obj_t *priceTitle[SYMBOL_COUNT]`
- `static lv_obj_t *priceLbl[SYMBOL_COUNT]`
- `static lv_obj_t *chartTitle`
- `static lv_obj_t *trendLabel`
- `static lv_obj_t *warmStartStatusLabel`
- `static lv_obj_t *volatilityLabel`
- `static lv_obj_t *anchorLabel`
- `static lv_obj_t *anchorMaxLabel`
- `static lv_obj_t *anchorMinLabel`
- ... (veel meer UI labels)

**UI Buffers:**
- `static char priceLblBuffer[32]`
- `static char anchorMaxLabelBuffer[32]`
- `static char priceTitleBuffer[3][64]`
- ... (veel meer buffers)

### 2. Price Data & History
**Module:** `PriceData`
- `static float secondPrices[60]` - 1 minuut geschiedenis
- `static float fiveMinutePrices[300]` - 5 minuten geschiedenis
- `static float minuteAverages[120]` - 2 uur geschiedenis
- `static DataSource secondPricesSource[60]` - Source tracking
- `static DataSource fiveMinutePricesSource[300]`
- `static DataSource minuteAveragesSource[120]`
- `static uint8_t secondIndex`
- `static uint16_t fiveMinuteIndex`
- `static uint8_t minuteIndex`
- `static bool secondArrayFilled`
- `static bool fiveMinuteArrayFilled`
- `static bool minuteArrayFilled`
- `static float firstMinuteAverage`
- `static unsigned long lastMinuteUpdate`

**Returns:**
- `static float ret_2h`
- `static float ret_30m`
- `static bool hasRet2hWarm`
- `static bool hasRet30mWarm`
- `static bool hasRet2hLive`
- `static bool hasRet30mLive`
- `static bool hasRet2h`
- `static bool hasRet30m`

**Current Prices:**
- `static float prices[SYMBOL_COUNT]`
- `static float openPrices[SYMBOL_COUNT]`
- `static float averagePrices[SYMBOL_COUNT]`
- `static char symbolsArray[SYMBOL_COUNT][16]`
- `static const char *symbols[SYMBOL_COUNT]`

### 3. Anchor System
**Module:** `AnchorSystem`
- `static float anchorPrice`
- `static float anchorMax`
- `static float anchorMin`
- `static unsigned long anchorTime`
- `static bool anchorActive`
- `static bool anchorNotificationPending`
- `static float anchorTakeProfit`
- `static float anchorMaxLoss`
- `static bool anchorTakeProfitSent`
- `static bool anchorMaxLossSent`
- `static bool trendAdaptiveAnchorsEnabled`
- `static float uptrendMaxLossMultiplier`
- `static float uptrendTakeProfitMultiplier`
- `static float downtrendMaxLossMultiplier`
- `static float downtrendTakeProfitMultiplier`

### 4. Trend Detection
**Module:** `TrendDetector`
- `static TrendState trendState`
- `static TrendState previousTrendState`
- `static float trendThreshold`
- `static unsigned long lastTrendChangeNotification`

### 5. Volatility Tracking
**Module:** `VolatilityTracker`
- `static float abs1mReturns[60]`
- `static uint8_t volatilityIndex`
- `static bool volatilityArrayFilled`
- `static VolatilityState volatilityState`
- `static float volatilityLowThreshold`
- `static float volatilityHighThreshold`
- `static bool autoVolatilityEnabled`
- `static uint8_t autoVolatilityWindowMinutes`
- `static float autoVolatilityBaseline1mStdPct`
- `static float autoVolatilityMinMultiplier`
- `static float autoVolatilityMaxMultiplier`
- `static float volatility1mReturns[MAX_VOLATILITY_WINDOW_SIZE]`
- `static uint8_t volatility1mIndex`
- `static bool volatility1mArrayFilled`
- `static float currentVolFactor`
- `static unsigned long lastVolatilityLog`

### 6. Alert System
**Module:** `AlertEngine`
- `static AlertThresholds alertThresholds`
- `static NotificationCooldowns notificationCooldowns`
- `static bool smartConfluenceEnabled`
- `static LastOneMinuteEvent last1mEvent`
- `static LastFiveMinuteEvent last5mEvent`
- `static unsigned long lastConfluenceAlert`

### 7. Warm-Start
**Module:** `WarmStart`
- `static bool warmStartEnabled`
- `static uint8_t warmStart1mExtraCandles`
- `static uint8_t warmStart5mCandles`
- `static uint8_t warmStart30mCandles`
- `static uint8_t warmStart2hCandles`
- `static WarmStartStatus warmStartStatus`
- `static unsigned long warmStartCompleteTime`
- `static WarmStartStats warmStartStats`

### 8. Network & Settings
**Module:** `NetworkManager` / `SettingsStore`
- `static char ntfyTopic[64]`
- `static char binanceSymbol[16]`
- `static uint8_t language`
- `static char mqttHost[64]`
- `static char mqttUser[64]`
- `static char mqttPass[64]`
- `static uint16_t mqttPort`
- `static bool mqttEnabled`
- `static bool wifiInitialized`
- `static bool wifiReconnectEnabled`

### 9. FreeRTOS & Synchronisatie
**Module:** (blijft in hoofdbestand of aparte module)
- `SemaphoreHandle_t dataMutex`
- `static uint32_t lastApiMs`
- Task handles (worden in setup aangemaakt)

### 10. Web Server
**Module:** `WebServer`
- `WebServer server(80)`
- `static float cpuUsagePercent`
- `static unsigned long loopTimeSum`
- `static uint16_t loopCount`
- `static uint32_t heapLowWatermark`
- `static unsigned long lastHeapTelemetryLog`

### 11. MQTT
**Module:** `NetworkManager`
- `PubSubClient mqttClient`
- `static MqttMessage mqttQueue[MQTT_QUEUE_SIZE]`
- `static uint8_t mqttQueueHead`
- `static uint8_t mqttQueueTail`
- `static bool mqttConnected`

### 12. Chart & Display State
**Module:** `UIController`
- `static uint8_t symbolIndexToChart`
- `static uint32_t maxRange`
- `static uint32_t minRange`
- `static bool newPriceDataAvailable`
- `static bool graph_data_ready` (als dit bestaat)

---

## Functie Categorisatie

### Price Data Functions
**Module:** `PriceData`
- `addPriceToSecondArray()`
- `updateMinuteAverage()`
- `calculateReturn1Minute()`
- `calculateReturn5Minutes()`
- `calculateReturn30Minutes()`
- `calculateReturn2Hours()`

### Trend Functions
**Module:** `TrendDetector`
- `determineTrendState()`
- `checkTrendChange()`

### Volatility Functions
**Module:** `VolatilityTracker`
- `calculateStdDev1mReturns()`
- `calculateEffectiveThresholds()`
- `updateVolatilityWindow()`

### Alert Functions
**Module:** `AlertEngine`
- `checkAndNotify()`
- `checkAndSendConfluenceAlert()`
- `checkAnchorAlerts()`
- `update1mEvent()`
- `update5mEvent()`
- `checkAlertConditions()`

### Anchor Functions
**Module:** `AnchorSystem`
- `calculateEffectiveAnchorThresholds()`
- `checkAnchorAlerts()`
- `setAnchorPrice()`

### Warm-Start Functions
**Module:** `WarmStart`
- `performWarmStart()`
- `fetchBinanceKlines()`
- `updateWarmStartStatus()`
- `hasPSRAM()`

### UI Functions
**Module:** `UIController`
- `buildUI()`
- `updateUI()`
- `updateTrendLabel()`
- `updateVolatilityLabel()`
- `updateChart()`
- `updateDateTimeLabels()`
- `checkButton()`

### Network Functions
**Module:** `NetworkManager`
- `httpGET()`
- `fetchPrice()`
- `connectMQTT()`
- `mqttCallback()`
- `enqueueMqttMessage()`
- `processMqttQueue()`

### Settings Functions
**Module:** `SettingsStore`
- `loadSettings()`
- `saveSettings()`

### Web Server Functions
**Module:** `WebServer`
- `getSettingsHTML()`
- `handleAnchorSet()`
- `handleNtfyReset()`
- `setupWebServer()`

### Notification Functions
**Module:** `NtfyNotifier` (herbruikbaar)
- `sendNtfyNotification()`

---

## FreeRTOS Task Structuur

### Tasks:
1. **API Task** (Core 1)
   - Functie: `apiTask()`
   - Interval: 1500ms
   - Functies: `fetchPrice()`, return calculations, alert detection

2. **UI Task** (Core 0)
   - Functie: `uiTask()`
   - Interval: 1000ms
   - Functies: `updateUI()`, LVGL rendering, button check

3. **Web Task** (Core 0)
   - Functie: `webTask()`
   - Interval: 100ms (server.handleClient())
   - Functies: Web server request handling

### Mutex:
- `dataMutex` - Beschermt alle gedeelde data (prices, arrays, returns, trend, anchor)

---

## Dependency Analyse

### Onafhankelijke Modules (kunnen eerst):
1. **SettingsStore** - Geen dependencies
2. **NtfyNotifier** - Alleen WiFi (herbruikbaar)

### Modules met minimale dependencies:
3. **NetworkManager** - WiFi, SettingsStore
4. **ApiClient** - NetworkManager, HTTPClient

### Modules met meer dependencies:
5. **PriceData** - ApiClient
6. **TrendDetector** - PriceData
7. **VolatilityTracker** - PriceData
8. **AlertEngine** - PriceData, TrendDetector, VolatilityTracker, AnchorSystem, NetworkManager
9. **AnchorSystem** - PriceData, TrendDetector
10. **WarmStart** - NetworkManager, PriceData
11. **UIController** - PriceData, TrendDetector, VolatilityTracker, AnchorSystem, WarmStart
12. **WebServer** - Alle modules (read-only access)

---

## Ongebruikte Code (te verifiëren)

- [ ] Controleren op ongebruikte functies
- [ ] Controleren op ongebruikte variabelen
- [ ] Controleren op oude/commented code

---

## Volgende Stappen

1. ✅ Code analyse gestart
2. ⏭️ Verifieer compileerbaarheid (Stap 1.2)
3. ⏭️ Begin met SettingsStore module (Fase 2.1)

---

**Laatste update:** 2025-12-17 22:30



