# Code Index - UNIFIED-LVGL9-Crypto_Monitor

**Versie:** 4.13  
**Platform:** ESP32 (TTGO T-Display, CYD 2.4/2.8, ESP32-S3 Super Mini)  
**Laatste update:** 2025-01-XX - Versie 4.13: UI update fix na anchor setting, 2h trend hysteresis, alert throttling & classification

---

## 1. Overzicht

Dit project is een modulaire ESP32 crypto alert systeem dat:
- Real-time prijsdata ophaalt van Binance
- Multi-timeframe analyse uitvoert (1m, 5m, 30m, 2h)
- Context-aware alerts genereert
- Web interface biedt voor configuratie
- LVGL UI toont op lokale display

**Architectuur:** Modulair met FreeRTOS multi-task systeem

---

## 2. Hoofdbestand

### `UNIFIED-LVGL9-Crypto_Monitor.ino`
**Verantwoordelijkheden:**
- Platform-selectie via `platform_config.h`
- FreeRTOS task orchestration (apiTask, uiTask, webTask)
- Globale variabelen en state management
- Module initialisatie en integratie
- WiFi/MQTT setup
- Anchor setting queue management

**Belangrijke functies:**
- `setup()` - Initialisatie van alle modules
- `loop()` - Arduino main loop (minimaal gebruikt, taken draaien in FreeRTOS tasks)
- `apiTask()` - Core 1: API calls en data processing
- `uiTask()` - Core 0: UI updates en LVGL rendering
- `webTask()` - Core 0: Web server request handling
- `queueAnchorSetting()` - Thread-safe anchor setting queue

**FreeRTOS Tasks:**
- **API Task** (Core 1): 1500ms interval, haalt prijzen op, berekent returns, detecteert alerts
- **UI Task** (Core 0): 1000ms interval, update UI, LVGL rendering, button handling
- **Web Task** (Core 0): 100ms interval, web server request handling

**Mutex:**
- `dataMutex` - Beschermt alle gedeelde data (prices, arrays, returns, trend, anchor, etc.)

---

## 3. Module Structuur

### 3.1 Core Modules

#### `src/SettingsStore/`
**Verantwoordelijkheden:** Persistent storage (Preferences/NVS)

**Bestanden:**
- `SettingsStore.h` / `SettingsStore.cpp`

**Belangrijke functies:**
- `load()` - Laad settings uit NVS
- `save()` - Sla settings op in NVS
- `reset()` - Reset naar defaults

**Dependencies:** Geen (basis module)

---

#### `src/ApiClient/`
**Verantwoordelijkheden:** Binance API communicatie

**Bestanden:**
- `ApiClient.h` / `ApiClient.cpp`

**Belangrijke functies:**
- `fetchPrice()` - Haal prijs op van Binance
- `fetchHistoricalCandles()` - Haal historische candles op (voor warm-start)

**Dependencies:** 
- `src/Net/HttpFetch` - Streaming HTTP fetch
- WiFi (ESP32 core)

---

#### `src/PriceData/`
**Verantwoordelijkheden:** Prijs data management & returns berekening

**Bestanden:**
- `PriceData.h` / `PriceData.cpp`

**Belangrijke functies:**
- `addPrice()` - Voeg nieuwe prijs toe aan buffers
- `calculateReturns()` - Bereken 1m, 5m, 30m, 2h returns
- `get2HMetrics()` - Haal 2-uur metrics op (avg, high, low, range)

**Data structuren:**
- `secondPrices[]` - 1-seconde prijs buffer
- `fiveMinutePrices[]` - 5-minuut prijs buffer
- `minuteAverages[]` - 1-minuut gemiddelde buffer

**Dependencies:**
- `src/ApiClient` - Voor prijs data

---

### 3.2 Analysis Modules

#### `src/TrendDetector/`
**Verantwoordelijkheden:** Trend detection & state management

**Bestanden:**
- `TrendDetector.h` / `TrendDetector.cpp`

**Belangrijke functies:**
- `determineTrendState()` - Bepaal trend (UP/DOWN/SIDEWAYS) met hysteresis
- `checkTrendChange()` - Detecteer trend verandering
- `getTrendState()` - Haal huidige trend state op
- `getTrendName()` - Haal trend naam op (voor display)

**FASE X.1 Features:**
- Hysteresis logica: 0.65 factor voor trend exit
- Stabiliseert trend status om flip-flop te voorkomen

**Dependencies:**
- `src/PriceData` - Voor return waarden

---

#### `src/VolatilityTracker/`
**Verantwoordelijkheden:** Volatiliteit berekeningen en state

**Bestanden:**
- `VolatilityTracker.h` / `VolatilityTracker.cpp`

**Belangrijke functies:**
- `determineVolatilityState()` - Bepaal volatiliteit (LOW/MEDIUM/HIGH)
- `calculateAutoVolatilityThreshold()` - Auto-adaptieve threshold berekening
- `getVolatilityState()` - Haal huidige volatiliteit state op

**Dependencies:**
- `src/PriceData` - Voor return waarden

---

### 3.3 Alert & Anchor Modules

#### `src/AlertEngine/`
**Verantwoordelijkheden:** Alert detection & notificaties

**Bestanden:**
- `AlertEngine.h` / `AlertEngine.cpp`
- `Alert2HThresholds.h` - 2-uur alert threshold defaults

**Belangrijke functies:**
- `checkAndNotify()` - Main alert checking functie (1m, 5m, 30m)
- `check2HAlerts()` - 2-uur context alerts (breakout, breakdown, compress, mean, anchor)
- `sendNotification()` - Stuur notificatie via NTFY
- `send2HNotification()` - Wrapper met throttling en classificatie (FASE X.2, X.3)
- `shouldThrottle2HAlert()` - Throttling matrix check (FASE X.2)
- `isPrimary2HAlert()` - Alert classificatie (FASE X.3)

**Alert Types:**
- **1m/5m/30m alerts:** Spike, Move, Momentum
- **2h alerts:** Breakout, Breakdown, Compression, Mean Reversion, Anchor Context, Trend Change

**FASE X.2 Features:**
- Throttling matrix voor 2h alerts
- Cooldown regels per alert type
- PRIMARY alerts (Breakout/Breakdown) override throttling

**FASE X.3 Features:**
- Alert classificatie: PRIMARY vs SECONDARY
- `[PRIMARY]` of `[Context]` prefix in notificaties

**Data structuren:**
- `Alert2HState` - Persistent state voor 2h alerts
- `TwoHMetrics` - 2-uur metrics struct
- `LastOneMinuteEvent` / `LastFiveMinuteEvent` - Voor confluence detection

**Dependencies:**
- `src/PriceData` - Voor return waarden
- `src/TrendDetector` - Voor trend state
- `src/VolatilityTracker` - Voor volatiliteit state
- `src/AnchorSystem` - Voor anchor data

---

#### `src/AnchorSystem/`
**Verantwoordelijkheden:** Anchor price tracking en management

**Bestanden:**
- `AnchorSystem.h` / `AnchorSystem.cpp`

**Belangrijke functies:**
- `setAnchorPrice()` - Stel anchor prijs in (met UI cache reset)
- `calcEffectiveAnchor()` - Bereken effectieve anchor (trend-adaptief)
- `checkAnchorAlerts()` - Detecteer anchor alerts (take profit, max loss)
- `begin()` - Initialisatie

**Versie 4.13 Fix:**
- Reset UI cache variabelen (`lastAnchorValue`, `lastAnchorMaxValue`, `lastAnchorMinValue`) na anchor setting
- Zorgt ervoor dat UI automatisch update na web interface anchor setting

**Dependencies:**
- `src/PriceData` - Voor prijs data
- `src/TrendDetector` - Voor trend-adaptieve thresholds

---

### 3.4 UI & Web Modules

#### `src/UIController/`
**Verantwoordelijkheden:** LVGL UI management en rendering

**Bestanden:**
- `UIController.h` / `UIController.cpp`

**Belangrijke functies:**
- `setupLVGL()` - LVGL initialisatie
- `buildUI()` - Bouw UI elementen (chart, labels, boxes)
- `updateUI()` - Main UI update functie
- `updateBTCEURCard()` - Update anchor/price card
- `updateChartSection()` - Update chart met nieuwe data
- `updateHeaderSection()` - Update header labels
- `updatePriceCardsSection()` - Update price cards
- `checkButton()` - Physical button handling

**UI Componenten:**
- Chart (LVGL chart object)
- Price boxes (per symbol)
- Anchor labels (anchor, anchorMax, anchorMin)
- Header labels (trend, volatility, date/time)
- Footer (IP address, version)

**Dependencies:**
- `src/PriceData` - Voor prijs data
- `src/TrendDetector` - Voor trend display
- `src/VolatilityTracker` - Voor volatiliteit display
- `src/AnchorSystem` - Voor anchor display
- LVGL library

---

#### `src/WebServer/`
**Verantwoordelijkheden:** Web interface voor configuratie

**Bestanden:**
- `WebServer.h` / `WebServer.cpp`

**Belangrijke functies:**
- `setupWebServer()` - Web server initialisatie
- `renderSettingsHTML()` - Genereer settings HTML pagina
- `handleRoot()` - Root handler (settings pagina)
- `handleSave()` - Save handler (POST /save)
- `handleAnchorSet()` - Anchor set handler (POST /anchor/set)
- `handleNtfyReset()` - NTFY reset handler (POST /ntfy/reset)
- `handleStatus()` - Status endpoint (GET /status) - JSON API
- `invalidatePageCache()` - Cache invalidatie
- `getOrBuildSettingsPage()` - HTML caching (WEB-PERF-3)

**Routes:**
- `GET /` - Settings pagina
- `POST /save` - Sla instellingen op
- `POST /anchor/set` - Stel anchor in
- `POST /ntfy/reset` - Reset NTFY topic
- `GET /status` - JSON status API (live data)

**WEB-PERF-3 Features:**
- HTML caching voor performance
- `/status` JSON endpoint voor client-side updates
- JavaScript voor live data updates (zonder full page reload)

**Dependencies:**
- `src/SettingsStore` - Voor settings opslag
- `src/TrendDetector` - Voor trend display
- `src/VolatilityTracker` - Voor volatiliteit display
- `src/AnchorSystem` - Voor anchor data
- WebServer (ESP32 core)

---

### 3.5 Utility Modules

#### `src/WarmStart/`
**Verantwoordelijkheden:** Warm-start functionaliteit (historische data)

**Bestanden:**
- `WarmStart.h` / `WarmStart.cpp`

**Belangrijke functies:**
- `fetchWarmStartData()` - Haal historische candles op van Binance
- `initializeBuffers()` - Initialiseer buffers met historische data

**Dependencies:**
- `src/ApiClient` - Voor API calls
- `src/PriceData` - Voor buffer initialisatie

---

#### `src/Memory/`
**Verantwoordelijkheden:** Heap telemetry en monitoring

**Bestanden:**
- `HeapMon.h` / `HeapMon.cpp`

**Belangrijke functies:**
- `logHeap()` - Log heap status
- `getHeapInfo()` - Haal heap informatie op

**Dependencies:** Geen (utility module)

---

#### `src/Net/`
**Verantwoordelijkheden:** Network utilities (streaming HTTP)

**Bestanden:**
- `HttpFetch.h` / `HttpFetch.cpp`

**Belangrijke functies:**
- `streamingHttpFetch()` - Streaming HTTP fetch zonder String allocaties

**Dependencies:**
- HTTPClient (ESP32 core)

---

## 4. Configuratiebestanden

### `platform_config.h`
**Doel:** Platform-specifieke configuratie en versie management

**Defines:**
- `PLATFORM_TTGO` - TTGO T-Display
- `PLATFORM_CYD24` - CYD 2.4 inch
- `PLATFORM_CYD28` - CYD 2.8 inch
- `PLATFORM_ESP32S3_SUPERMINI` - ESP32-S3 Super Mini
- `VERSION_MAJOR` / `VERSION_MINOR` / `VERSION_STRING`
- `DEBUG_BUTTON_ONLY` - Debug logging configuratie

### Pin Configuratiebestanden
- `PINS_TTGO_T_Display.h` - TTGO pin configuratie
- `PINS_CYD-ESP32-2432S024.h` - CYD 2.4 pin configuratie
- `PINS_CYD-ESP32-2432S028-2USB.h` - CYD 2.8 pin configuratie
- `PINS_ESP32S3_SuperMini_ST7789_154.h` - ESP32-S3 Super Mini pin configuratie

### `lv_conf.h`
**Doel:** LVGL library configuratie

### `atomic.h`
**Doel:** Atomic operations helper

---

## 5. Externe Libraries

- **WiFi** (ESP32 core) - WiFi connectiviteit
- **HTTPClient** (ESP32 core) - HTTP requests
- **WiFiManager** - WiFi configuratie via captive portal
- **WebServer** (ESP32 core) - Web interface server
- **Preferences** (ESP32 core) - NVS persistent storage
- **PubSubClient3** - MQTT client
- **LVGL** (v9.4.0) - UI library
- **FreeRTOS** (ESP32 core) - Multi-task systeem

---

## 6. Data Flow

### Prijs Data Flow
1. **API Task** → `ApiClient::fetchPrice()` → Binance API
2. **API Task** → `PriceData::addPrice()` → Update buffers
3. **API Task** → `PriceData::calculateReturns()` → Bereken returns
4. **API Task** → `TrendDetector::determineTrendState()` → Update trend
5. **API Task** → `VolatilityTracker::determineVolatilityState()` → Update volatiliteit
6. **API Task** → `AlertEngine::checkAndNotify()` → Check alerts
7. **UI Task** → `UIController::updateUI()` → Update display

### Alert Flow
1. **API Task** → `AlertEngine::checkAndNotify()` → Detecteer condities
2. **API Task** → `AlertEngine::send2HNotification()` → Throttling check
3. **API Task** → `AlertEngine::sendNotification()` → Stuur via NTFY
4. **Web Interface** → Live status updates via `/status` endpoint

### Anchor Flow
1. **Web Interface** → `POST /anchor/set` → `queueAnchorSetting()`
2. **API Task** → Verwerk queue → `AnchorSystem::setAnchorPrice()`
3. **AnchorSystem** → Reset UI cache → Force UI update
4. **UI Task** → Detecteer cache reset → Update anchor display

---

## 7. Thread Safety

### Mutex Usage
- `dataMutex` - Beschermt alle gedeelde data:
  - `prices[]` array
  - `secondPrices[]`, `fiveMinutePrices[]`, `minuteAverages[]` arrays
  - Return waarden (`ret_1m`, `ret_5m`, `ret_30m`, `ret_2h`)
  - Trend state
  - Volatility state
  - Anchor data
  - Settings

### Thread-Safe Patterns
- `safeMutexTake()` / `safeMutexGive()` - Helper functies met timeout
- Volatile flags voor cross-task communicatie
- Queue-based communicatie (anchor setting queue)

---

## 8. Memory Management

### Heap Optimization
- Minimal `String` gebruik (voorkom fragmentatie)
- `char[]` buffers i.p.v. `String` concatenatie
- PROGMEM voor static strings
- Stack-based buffers waar mogelijk
- HTML caching (WEB-PERF-3) om rebuilds te voorkomen

### Memory Monitoring
- `HeapMon::logHeap()` - Rate-limited heap telemetry
- Platform-specifieke stack sizes
- PSRAM detection en gebruik

---

## 9. Recente Wijzigingen (Versie 4.13)

### UI Update Fix
- **Probleem:** UI update niet na anchor setting via web interface
- **Oplossing:** Reset UI cache variabelen in `AnchorSystem::setAnchorPrice()`
- **Bestand:** `src/AnchorSystem/AnchorSystem.cpp`

### FASE X.1 - 2h Trend Hysteresis
- **Feature:** Hysteresis voor trend status transitions
- **Implementatie:** `TrendDetector::determineTrendState()` met 0.65 exit factor
- **Bestand:** `src/TrendDetector/TrendDetector.cpp`

### FASE X.2 - 2h Alert Throttling
- **Feature:** Throttling matrix voor 2h alerts
- **Implementatie:** `AlertEngine::shouldThrottle2HAlert()`
- **Bestand:** `src/AlertEngine/AlertEngine.cpp`

### FASE X.3 - Alert Classification
- **Feature:** PRIMARY vs SECONDARY alert classificatie
- **Implementatie:** `AlertEngine::isPrimary2HAlert()`, `send2HNotification()`
- **Bestand:** `src/AlertEngine/AlertEngine.cpp`

---

## 10. Dependency Graph

```
SettingsStore (basis)
    ↓
ApiClient → Net/HttpFetch
    ↓
PriceData
    ↓
├─→ TrendDetector
├─→ VolatilityTracker
└─→ AnchorSystem → TrendDetector
    ↓
AlertEngine → (PriceData, TrendDetector, VolatilityTracker, AnchorSystem)
    ↓
UIController → (PriceData, TrendDetector, VolatilityTracker, AnchorSystem)
WebServer → (SettingsStore, TrendDetector, VolatilityTracker, AnchorSystem)
WarmStart → (ApiClient, PriceData)
```

---

## 11. Code Conventies

### Naming
- Classes: PascalCase (`AlertEngine`, `UIController`)
- Functions: camelCase (`checkAndNotify`, `updateUI`)
- Variables: camelCase (`anchorPrice`, `ret_2h`)
- Constants: UPPER_CASE (`VERSION_STRING`, `DEBUG_BUTTON_ONLY`)

### File Structure
- Header files: `.h` met class/function declarations
- Implementation files: `.cpp` met implementations
- Module directory: `src/ModuleName/`

### Comments
- Dutch comments voor business logic
- English comments voor technical details
- FASE markers voor feature tracking

---

**Laatste update:** 2025-01-XX - Versie 4.13
