# Code Index - UNIFIED-LVGL9-Crypto_Monitor

**Versie:** 3.82  
**Platform:** ESP32 (TTGO T-Display, CYD 2.4/2.8, ESP32-S3 Super Mini)  
**Laatste update:** Warm-start optimalisaties, mutex/API timing verbeteringen, trend kleuren

---

## 1. Modules & Bestanden

### Hoofdbestand
- **`UNIFIED-LVGL9-Crypto_Monitor.ino`** (8169 regels)
  - Monolithische structuur met alle functionaliteit
  - Platform-selectie via `platform_config.h`
  - FreeRTOS multi-task architectuur

### Configuratiebestanden
- **`platform_config.h`** - Platform-specifieke defines (TTGO, CYD24, CYD28, ESP32-S3 Super Mini)
- **`PINS_TTGO_T_Display.h`** - TTGO pin configuratie
- **`PINS_CYD-ESP32-2432S024.h`** - CYD 2.4 pin configuratie
- **`PINS_CYD-ESP32-2432S028-2USB.h`** - CYD 2.8 pin configuratie
- **`PINS_ESP32S3_SuperMini_ST7789_154.h`** - ESP32-S3 Super Mini pin configuratie
- **`lv_conf.h`** - LVGL library configuratie
- **`atomic.h`** - Atomic operations helper

### Externe Libraries
- **WiFi** (ESP32 core) - WiFi connectiviteit
- **HTTPClient** (ESP32 core) - HTTP requests naar Binance API
- **WiFiManager** - WiFi configuratie via captive portal
- **WebServer** (ESP32 core) - Web interface server
- **Preferences** (ESP32 core) - Persistent storage (NVS)
- **PubSubClient3** - MQTT client
- **LVGL v9.2.2+** - Graphics library voor display
- **FreeRTOS** - Task management en synchronisatie

---

## 2. Globale Structs & Flags

### Data Source Tracking
```cpp
enum DataSource {
    SOURCE_BINANCE,  // Data van Binance historische klines
    SOURCE_LIVE      // Data van live API calls
};
```

### Warm-Start System
```cpp
enum WarmStartStatus {
    WARMING_UP,   // Buffers bevatten nog Binance data
    LIVE,         // Volledig op live data
    LIVE_COLD     // Live data maar warm-start gefaald
};

enum WarmStartMode {
    WS_MODE_FULL,     // Alle timeframes succesvol geladen
    WS_MODE_PARTIAL,  // Gedeeltelijk geladen
    WS_MODE_FAILED,   // Alle timeframes gefaald
    WS_MODE_DISABLED  // Warm-start uitgeschakeld
};

struct WarmStartStats {
    uint16_t loaded1m, loaded5m, loaded30m, loaded2h;
    bool warmStartOk1m, warmStartOk5m, warmStartOk30m, warmStartOk2h;
    WarmStartMode mode;
    uint8_t warmUpProgress;  // 0-100%
};
```

### Trend Detection
```cpp
enum TrendState {
    TREND_UP,
    TREND_DOWN,
    TREND_SIDEWAYS
};

// Availability flags (geen ret != 0.0f checks)
static bool hasRet2hWarm;   // Warm-start beschikbaar
static bool hasRet30mWarm;  // Warm-start beschikbaar
static bool hasRet2hLive;   // Live data beschikbaar (minuteIndex >= 120)
static bool hasRet30mLive;  // Live data beschikbaar (minuteIndex >= 30)
static bool hasRet2h;       // Combined: hasRet2hWarm || hasRet2hLive
static bool hasRet30m;      // Combined: hasRet30mWarm || hasRet30mLive
```

### Volatiliteit
```cpp
enum VolatilityState {
    VOLATILITY_LOW,      // < 0.05%
    VOLATILITY_MEDIUM,   // 0.05% - 0.15%
    VOLATILITY_HIGH      // >= 0.15%
};
```

### Smart Confluence Mode
```cpp
enum EventDirection {
    EVENT_UP,
    EVENT_DOWN,
    EVENT_NONE
};

struct LastOneMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_1m|
    bool usedInConfluence;
};

struct LastFiveMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_5m|
    bool usedInConfluence;
};
```

### Auto-Volatility Mode
```cpp
struct EffectiveThresholds {
    float spike1m;
    float move5m;
    float move30m;
    float volFactor;
    float stdDev;
};
```

### Anchor System
```cpp
struct AnchorConfigEffective {
    float maxLossPct;      // Effectieve max loss (negatief)
    float takeProfitPct;    // Effectieve take profit (positief)
};
```

### Alert Thresholds
```cpp
struct AlertThresholds {
    float spike1m, spike5m;
    float move30m, move5m, move5mAlert;
    float threshold1MinUp, threshold1MinDown;
    float threshold30MinUp, threshold30MinDown;
};

struct NotificationCooldowns {
    unsigned long cooldown1MinMs;
    unsigned long cooldown30MinMs;
    unsigned long cooldown5MinMs;
};
```

### MQTT Queue
```cpp
struct MqttMessage {
    char topic[128];
    char payload[128];
    bool retained;
    bool valid;
};
```

### Thread-Safe Anchor Setting
```cpp
struct AnchorSetting {
    volatile float value;
    volatile bool pending;
    volatile bool useCurrentPrice;
};
```

---

## 3. Hoofd Dataflow

### Data Acquisition Flow
```
[Boot] → [WiFi Connect] → [Warm-Start (optioneel)] → [API Task Start]
    ↓
[API Task (Core 1)] - Elke 1500ms:
    ├─ fetchPrice() → Binance API
    ├─ addPriceToSecondArray() → secondPrices[60]
    ├─ updateMinuteAverage() → minuteAverages[120]
    ├─ calculateReturn1Minute() → ret_1m
    ├─ calculateReturn5Minutes() → ret_5m
    ├─ calculateReturn30Minutes() → ret_30m
    ├─ calculateReturn2Hours() → ret_2h
    ├─ determineTrendState() → trendState
    ├─ updateVolatilityWindow() → volatility1mReturns[]
    ├─ calculateEffectiveThresholds() → EffectiveThresholds
    └─ checkAndNotify() → Alert Engine
```

### Price Buffer Management
```
Live API (1500ms interval)
    ↓
secondPrices[60]          → 1 minuut geschiedenis (circulair)
    ↓ (elke minuut)
minuteAverages[120]       → 2 uur geschiedenis (circulair)
    ↓
fiveMinutePrices[300]     → 5 minuten geschiedenis (circulair)
```

### Return Calculations
```
ret_1m  = (priceNow - price60sAgo) / price60sAgo * 100
ret_5m  = (priceNow - price5mAgo) / price5mAgo * 100
ret_30m = (avgLast30min - avgPrev30min) / avgPrev30min * 100
ret_2h  = (priceNow - price120mAgo) / price120mAgo * 100
```

### Trend Detection Flow
```
ret_2h + ret_30m
    ↓
determineTrendState():
    - ret_2h >= +1.30% AND ret_30m >= 0 → TREND_UP
    - ret_2h <= -1.30% AND ret_30m <= 0 → TREND_DOWN
    - else → TREND_SIDEWAYS
    ↓
trendState (global)
    ↓
checkTrendChange() → Notificatie bij wijziging
```

### Alert Engine Flow
```
[checkAndNotify()]
    ↓
[Auto-Volatility Mode?]
    ├─ Ja: calculateEffectiveThresholds() → effSpike1m, effMove5m, effMove30m
    └─ Nee: gebruik basis thresholds
    ↓
[Trend-Adaptive Anchors?]
    ├─ Ja: calculateEffectiveAnchorThresholds() → effAnchorMaxLoss, effAnchorTakeProfit
    └─ Nee: gebruik basis anchor thresholds
    ↓
[Event Detection]
    ├─ 1m Spike: |ret_1m| >= effSpike1m AND |ret_5m| >= spike5mThreshold AND zelfde richting
    ├─ 5m Move: |ret_5m| >= effMove5m
    └─ 30m Move: |ret_30m| >= effMove30m AND |ret_5m| >= move5mThreshold AND zelfde richting
    ↓
[Smart Confluence Mode?]
    ├─ Ja: checkAndSendConfluenceAlert() → combineer 1m+5m+trend
    └─ Nee: verstuur losse alerts
    ↓
[Anchor Monitoring]
    └─ checkAnchorAlerts() → Max Loss / Take Profit
    ↓
[Cooldowns & Rate Limiting]
    └─ checkAlertConditions() → max alerts/uur, cooldown checks
```

---

## 4. Netwerk / UI / Alert Engine / Warm-Start

### Netwerk Module

#### WiFi Management
- **`WiFiManager`** - Captive portal voor configuratie
- **Reconnect logica** - Exponential backoff bij failures
- **Status tracking** - `wifiInitialized`, `wifiReconnectEnabled`

#### HTTP Client
- **`httpGET()`** - Generic HTTP GET met retry logic (max 1 retry)
- **Timeout handling** - `HTTP_TIMEOUT_MS` (1000ms), `HTTP_CONNECT_TIMEOUT_MS` (800ms)
- **Retry delay** - `RETRY_DELAY_MS` (100ms)
- **Connection reuse** - Uitgeschakeld voor betrouwbaarheid

#### Binance API Integration
- **`fetchPrice()`** - Live price fetch (elke 1500ms)
  - Endpoint: `/api/v3/ticker/price?symbol=BTCEUR`
  - JSON parsing: manual string parsing (geen ArduinoJson)
- **`fetchBinanceKlines()`** - Historische candle data (warm-start)
  - Endpoint: `/api/v3/klines?symbol=BTCEUR&interval=X&limit=Y`
  - Memory-efficient: streaming parsing, circulaire buffer
  - Heap allocation: 8-16KB response buffer (malloc/free)

#### MQTT Integration
- **`PubSubClient`** - MQTT client met queue systeem
- **`mqttQueue[MQTT_QUEUE_SIZE]`** - Message queue (max 10 berichten)
- **`enqueueMqttMessage()`** - Thread-safe queueing
- **`processMqttQueue()`** - Batch processing (max 3 per call)
- **Topics:**
  - `{prefix}/config/*` - Configuratie updates
  - `{prefix}/button/reset/set` - Anchor reset
  - `{prefix}/anchor/set` - Anchor waarde instellen
  - `{prefix}/values` - Live data publishing
  - `{prefix}/anchor/event` - Anchor events (set/take_profit/max_loss)

#### NTFY.sh Notifications
- **`sendNtfyNotification()`** - Push notifications via NTFY.sh
- **Topic format:** `{ESP32-ID}-alert` (unieke device ID)
- **Color tags:** green_square, red_square, blue_square, etc.

### UI Module (LVGL)

#### Display Initialization
- **Platform-specifieke setup:**
  - TTGO: 135x240, ST7789 driver
  - CYD 2.4: 320x240, ILI9341 driver
  - CYD 2.8: 320x240, ILI9341 driver
  - ESP32-S3 Super Mini: 240x240, ST7789 driver

#### UI Components
- **Chart:** `lv_chart` met 60 data points
- **Price boxes:** 3 symbolen (BTCEUR, 1m, 30m)
- **Labels:**
  - `trendLabel` - Trend weergave (UP/DOWN/SIDEWAYS) met kleur-codering:
    - Grijs: data uit warm-start
    - Blauw: live data + SIDEWAYS
    - Groen: live data + UP
    - Rood: live data + DOWN
  - `volatilityLabel` - Volatiliteit status
  - `warmStartStatusLabel` - Warm-start status (WARMXX%/LIVE/COLD)
  - `anchorLabel` - Anchor price info
  - `chartDateLabel`, `chartTimeLabel` - Datum/tijd

#### UI Update Flow
```
[UI Task (Core 0)] - Elke 1000ms:
    ├─ lv_task_handler() / lv_refr_now() - LVGL rendering
    ├─ updateUI() - Update alle labels en chart
    │   ├─ updateTrendLabel() - Trend met kleur-codering (grijs/blauw/groen/rood)
    │   ├─ updateVolatilityLabel() - Volatiliteit status
    │   ├─ updateWarmStartStatusLabel() - WARMXX%/LIVE/COLD
    │   └─ updateChart() - Prijs grafiek
    └─ checkButton() - Fysieke knop (GPIO 0)
```

### Alert Engine

#### Alert Types
1. **1m Spike Alert**
   - Voorwaarde: `|ret_1m| >= effSpike1m AND |ret_5m| >= spike5mThreshold AND zelfde richting`
   - Cooldown: `notificationCooldown1MinMs`
   - Max per uur: `MAX_ALERTS_1MIN_PER_HOUR`

2. **5m Move Alert**
   - Voorwaarde: `|ret_5m| >= effMove5m`
   - Cooldown: `notificationCooldown5MinMs`
   - Max per uur: `MAX_ALERTS_5MIN_PER_HOUR`

3. **30m Move Alert**
   - Voorwaarde: `|ret_30m| >= effMove30m AND |ret_5m| >= move5mThreshold AND zelfde richting`
   - Cooldown: `notificationCooldown30MinMs`
   - Max per uur: `MAX_ALERTS_30MIN_PER_HOUR`

4. **Confluence Alert** (Smart Confluence Mode)
   - Voorwaarde: 1m + 5m events binnen ±5 min, zelfde richting, trend ondersteunt
   - Cooldown: `CONFLUENCE_TIME_WINDOW_MS` (300000ms)
   - Onderdrukt losse 1m/5m alerts

5. **Anchor Alerts**
   - Take Profit: `deltaPct >= effAnchorTakeProfit`
   - Max Loss: `deltaPct <= effAnchorMaxLoss`
   - Trend-adaptive: multipliers per trend state

6. **Trend Change Alert**
   - Voorwaarde: `trendState != previousTrendState`
   - Cooldown: `TREND_CHANGE_COOLDOWN_MS` (600000ms)

#### Alert Decision Tree
```
[checkAndNotify()]
    ↓
[Auto-Volatility?] → Effective thresholds
    ↓
[Trend-Adaptive Anchors?] → Effective anchor thresholds
    ↓
[Event Detection]
    ├─ 1m Spike → update1mEvent() → [Confluence check]
    ├─ 5m Move → update5mEvent() → [Confluence check]
    └─ 30m Move → Direct alert
    ↓
[Smart Confluence?]
    ├─ Ja: checkAndSendConfluenceAlert() → Combineer events
    └─ Nee: Verstuur losse alerts
    ↓
[Anchor Monitoring] → checkAnchorAlerts()
    ↓
[Cooldowns & Rate Limiting] → checkAlertConditions()
```

### Warm-Start Module

#### Warm-Start Flow
```
[performWarmStart()]
    ├─ Heap check (minimaal 20KB)
    ├─ PSRAM detectie → Clamp limits
    ├─ Fetch 1m candles → secondPrices[60]
    ├─ Fetch 5m candles → fiveMinutePrices[300]
    ├─ Fetch 30m candles → minuteAverages[120] + ret_30m
    ├─ Fetch 2h candles → ret_2h
    ├─ Bepaal trendState (als hasRet2h && hasRet30m)
    └─ Update combined flags
```

#### Memory-Efficient Parsing
- **Streaming JSON parsing** - Geen volledige response in RAM
- **Fixed-size buffer** - `binanceStreamBuffer[1024]` voor chunked reading
- **Iteratieve parsing** - Parse direct van stream, geen grote allocaties
- **Circulaire buffer** - Alleen laatste N candles bewaren
- **Returns-only** - Alleen closing prices, geen volledige OHLC
- **Minimale logging** - Alleen essentiële logs, geen debug output

#### PSRAM-Aware Clamps
```cpp
Zonder PSRAM:  1m≤80,  5m≤12, 30m≤6, 2h≤4
Met PSRAM:     1m≤150, 5m≤24, 30m≤12, 2h≤8
```

#### Status Updates
- **`updateWarmStartStatus()`** - Bereken warm-up progress (elke 10s)
- **Progress calculation:** Gemiddelde van volatility% en trend% live data
- **Transition:** WARMING_UP → LIVE bij ≥80% live data

---

## 5. Memory Hotspots

### Grote Arrays (Stack)
```cpp
// Prijs geschiedenis buffers
static float secondPrices[60];              // 240 bytes
static float fiveMinutePrices[300];         // 1200 bytes
static float minuteAverages[120];          // 480 bytes

// Source tracking (parallel arrays)
static DataSource secondPricesSource[60];   // 60 bytes
static DataSource fiveMinutePricesSource[300]; // 300 bytes
static DataSource minuteAveragesSource[120];   // 120 bytes

// Volatiliteit buffers
static float abs1mReturns[60];              // 240 bytes (VOLATILITY_LOOKBACK_MINUTES)
static float volatility1mReturns[60];       // 240 bytes (MAX_VOLATILITY_WINDOW_SIZE)

// LVGL UI objects (pointers, niet de data zelf)
static lv_obj_t *priceBox[3];
static lv_obj_t *priceTitle[3];
static lv_obj_t *priceLbl[3];
```

### Heap Allocations (Runtime)
```cpp
// Warm-start response buffers
char* response = malloc(8192-16384);        // 8-16KB (tijdelijk, freed na gebruik)

// HTTPClient internals
HTTPClient http;                            // ~2-4KB (tijdelijk)

// Web server buffers
WebServer server(80);                       // ~4-8KB (persistent)

// MQTT queue
static MqttMessage mqttQueue[10];          // ~2.5KB (persistent)
```

### String Buffers (Stack)
```cpp
// Configuratie strings
static char ntfyTopic[64];                  // 64 bytes
static char binanceSymbol[16];              // 16 bytes
static char mqttHost[64];                   // 64 bytes
static char mqttUser[64];                  // 64 bytes
static char mqttPass[64];                  // 64 bytes

// Temporary buffers
char url[256];                              // 256 bytes (fetchBinanceKlines)
char response[8192-16384];                 // 8-16KB (heap, niet stack)
char fieldBuf[64];                         // 64 bytes (JSON parsing)
```

### Memory-Efficient Strategies
1. **Circulaire buffers** - Overschrijf oudste data
2. **Streaming parsing** - Geen volledige JSON in RAM
3. **Returns-only storage** - Alleen closing prices, geen OHLC
4. **Heap voor grote buffers** - Response buffers op heap (warm-start)
5. **Char arrays i.p.v. String** - Voorkom fragmentatie
6. **PSRAM-aware clamps** - Schaal limits met beschikbaar geheugen

### Memory Footprint Schatting
```
Stack (per task):
- API Task: ~4-6KB
- UI Task: ~6-8KB
- Web Task: ~4-6KB
- Main Loop: ~2-4KB

Heap:
- Static arrays: ~3KB
- Runtime allocations: ~10-20KB (warm-start)
- FreeRTOS overhead: ~2-4KB
- LVGL buffers: ~4-8KB

Totaal RAM gebruik: ~30-50KB (zonder PSRAM)
```

---

## 6. FreeRTOS Task Architectuur

### Tasks
1. **`apiTask`** (Core 1)
   - Frequentie: 1500ms (UPDATE_API_INTERVAL)
   - Functie: `fetchPrice()`, return calculations, alert detection
   - Mutex: `dataMutex` voor data access (timeout: 400-500ms)
   - Timing: Reset bij lange calls, voorkomt opstapeling

2. **`uiTask`** (Core 0)
   - Frequentie: 1000ms (UPDATE_UI_INTERVAL)
   - Functie: `updateUI()`, LVGL rendering, button check
   - Mutex: `dataMutex` voor data access (timeout: 30-50ms, lagere prioriteit)
   - LVGL handler: 3-5ms interval

3. **`webTask`** (Core 0)
   - Frequentie: 100ms (server.handleClient())
   - Functie: Web server request handling
   - Geen mutex (read-only data access)

### Synchronisatie
- **`dataMutex`** - Beschermt gedeelde data (prices, arrays, returns)
- **`safeMutexTake()` / `safeMutexGive()`** - Thread-safe mutex operaties met deadlock detection
- **`pendingAnchorSetting`** - Thread-safe anchor queue (volatile flags)

---

## 7. Belangrijke Functies

### Price Management
- `addPriceToSecondArray()` - Voeg prijs toe aan 1m buffer
- `updateMinuteAverage()` - Bereken minuut gemiddelde
- `calculateReturn1Minute()` - 1m return berekening
- `calculateReturn5Minutes()` - 5m return berekening
- `calculateReturn30Minutes()` - 30m return berekening
- `calculateReturn2Hours()` - 2h return berekening

### Trend & Volatiliteit
- `determineTrendState()` - Bepaal trend (UP/DOWN/SIDEWAYS)
- `checkTrendChange()` - Trend change notificatie
- `calculateStdDev1mReturns()` - Volatiliteit berekening
- `calculateEffectiveThresholds()` - Auto-Volatility thresholds

### Alert System
- `checkAndNotify()` - Hoofd alert detection functie
- `checkAndSendConfluenceAlert()` - Smart Confluence detection
- `checkAnchorAlerts()` - Anchor monitoring
- `update1mEvent()` / `update5mEvent()` - Event state tracking

### Warm-Start
- `performWarmStart()` - Hoofd warm-start functie
- `fetchBinanceKlines()` - Binance API klines fetch
- `updateWarmStartStatus()` - Warm-up progress berekening
- `hasPSRAM()` - PSRAM detectie

### UI Updates
- `updateUI()` - Hoofd UI update functie
- `updateTrendLabel()` - Trend label met kleur-codering (grijs/blauw/groen/rood)
- `updateVolatilityLabel()` - Volatiliteit status
- `updateChart()` - Prijs grafiek update

### Network
- `httpGET()` - Generic HTTP GET met retry
- `sendNtfyNotification()` - NTFY.sh push notification
- `connectMQTT()` - MQTT connectie
- `mqttCallback()` - MQTT message handler

### Settings
- `loadSettings()` - Laad configuratie uit Preferences
- `saveSettings()` - Sla configuratie op in Preferences
- `getSettingsHTML()` - Genereer web interface HTML

---

## 8. Configuratie Parameters

### Warm-Start (PSRAM-aware)
- `warmStartEnabled` - Enable/disable warm-start
- `warmStart1mExtraCandles` - Extra 1m candles (default: 15)
- `warmStart5mCandles` - 5m candles (default: 12, clamp: 12/24)
- `warmStart30mCandles` - 30m candles (default: 8, clamp: 6/12)
- `warmStart2hCandles` - 2h candles (default: 6, clamp: 4/8)

### Alert Thresholds
- `spike1mThreshold` - 1m spike threshold (default: 0.31%)
- `spike5mThreshold` - 5m spike filter (default: 0.65%)
- `move30mThreshold` - 30m move threshold (default: 1.3%)
- `move5mThreshold` - 5m move filter (default: 0.40%)
- `move5mAlertThreshold` - 5m alert threshold (default: 0.8%)

### Trend & Volatiliteit
- `trendThreshold` - Trend detection threshold (default: 1.30%)
- `autoVolatilityWindowMinutes` - Volatiliteit window (default: 60)
- `autoVolatilityBaseline1mStdPct` - Baseline std dev (default: 0.10%)
- `autoVolatilityMinMultiplier` - Min vol factor (default: 0.5)
- `autoVolatilityMaxMultiplier` - Max vol factor (default: 2.0)

### Anchor
- `anchorTakeProfit` - Take profit threshold (default: 5.0%)
- `anchorMaxLoss` - Max loss threshold (default: -3.0%)
- `trendAdaptiveAnchorsEnabled` - Trend-adaptive mode
- `uptrendMaxLossMultiplier` - UP trend max loss multiplier (default: 1.15)
- `uptrendTakeProfitMultiplier` - UP trend take profit multiplier (default: 1.2)

### Smart Confluence
- `smartConfluenceEnabled` - Enable/disable confluence mode
- `CONFLUENCE_TIME_WINDOW_MS` - Tijdshorizon voor confluence (300000ms = 5 min)

---

## 9. Thread Safety & Synchronisatie

### Mutex Usage
- **`dataMutex`** - Beschermt alle gedeelde data:
  - Price arrays (`secondPrices`, `fiveMinutePrices`, `minuteAverages`)
  - Returns (`ret_1m`, `ret_5m`, `ret_30m`, `ret_2h`)
  - Trend state (`trendState`, `hasRet2h`, `hasRet30m`)
  - Anchor data (`anchorPrice`, `anchorActive`)
  - Volatiliteit buffers

### Thread-Safe Patterns
- **Volatile flags** - `pendingAnchorSetting` (web/MQTT → UI task)
- **Atomic reads** - ESP32 garandeert atomic 32-bit reads
- **Memory barriers** - Pending flag als laatste write
- **Cooldown mechanisme** - Voorkomt race conditions bij anchor sets

### Deadlock Prevention
- **Timeout handling** - Mutex timeouts met retry logic
  - API task: 400-500ms timeout (hogere prioriteit)
  - UI task: 30-50ms timeout (lagere prioriteit, kan skip)
- **Deadlock detection** - Track mutex hold times
- **Context tracking** - Log welke functie mutex heeft
- **Timeout counters** - Log alleen bij eerste of periodieke timeouts

---

## 10. Error Handling & Robustness

### Fail-Safe Mechanisms
- **Warm-start failures** → Cold start (LIVE_COLD)
- **Memory allocation failures** → Skip warm-start, continue
- **Network failures** → Retry logic, exponential backoff
- **Mutex timeouts** → Log warning, continue zonder data update
- **Invalid price data** → Validation checks, skip updates

### Validation
- **`isValidPrice()`** - Check NaN, Inf, <= 0
- **`areValidPrices()`** - Check twee prijzen
- **`safeAtof()`** - Safe float conversion met validation
- **Bounds checking** - Array index validation

### Logging
- **Conditional logging** - `DEBUG_BUTTON_ONLY` flag
- **Structured logging** - Prefixes: `[WarmStart]`, `[API Task]`, `[Mutex]`, etc.
- **Error tracking** - Mutex timeout counters, missed API calls

---

## 11. Performance Optimizations

### Memory Efficiency
- Streaming JSON parsing (geen volledige response in RAM)
- Circulaire buffers (overschrijf oudste data)
- Returns-only storage (geen volledige OHLC)
- Heap voor grote buffers (warm-start)
- Char arrays i.p.v. String (voorkom fragmentatie)

### CPU Efficiency
- FreeRTOS task scheduling (multi-core)
- Mutex timeouts (voorkom blocking, prioriteit voor API task)
- Watchdog feeding (yield/delay tijdens warm-start)
- LVGL rendering optimalisatie (platform-specifieke handlers)
- API timing optimalisatie (reset bij lange calls, voorkomt opstapeling)

### Network Efficiency
- Connection reuse uitgeschakeld (betrouwbaarheid)
- Retry logic met korte delays (100ms, max 1 retry)
- Snellere timeouts (1000ms HTTP, 800ms connect)
- Exponential backoff (reconnect attempts)
- MQTT message queue (voorkom message loss)

---

## 12. Platform-Specifieke Aanpassingen

### Display Resolutie
- **TTGO:** 135x240 (compact formaat)
- **CYD 2.4/2.8:** 320x240 (volledig formaat)
- **ESP32-S3 Super Mini:** 240x240 (vierkant)

### Font Sizes
- Platform-specifieke font definities via `platform_config.h`
- TTGO: Kleinere fonts voor lagere resolutie
- CYD/ESP32-S3: Grotere fonts voor betere leesbaarheid

### LVGL Rendering
- **CYD 2.4:** `lv_refr_now()` i.p.v. `lv_task_handler()` (work-around)
- **TTGO/CYD 2.8/ESP32-S3:** Normale `lv_task_handler()`

### Datum/Tijd Format
- **TTGO:** `dd-mm-yy` (compact)
- **CYD/ESP32-S3:** `dd-mm-yyyy` (volledig)

---

---

## 13. Recente Wijzigingen (v3.82)

### Warm-Start Optimalisaties
- **Debug logging verwijderd** - Alleen essentiële logs behouden
- **Code gerefactored** - Schonere structuur, minder geheugengebruik
- **Streaming parsing verbeterd** - Fixed-size buffer, iteratieve parsing
- **Error logging toegevoegd** - Minimale logging voor failed fetches

### Mutex & API Timing Verbeteringen
- **Mutex timeouts aangepast** - API task krijgt hogere prioriteit (400-500ms), UI task lagere (30-50ms)
- **HTTP timeouts verlaagd** - 1000ms (was 1200ms), connect 800ms (was 1000ms)
- **Retry delay verlaagd** - 100ms (was 200ms)
- **API timing logica** - Betere handling van lange calls, voorkomt opstapeling

### UI Verbeteringen
- **Trend kleuren aangepast**:
  - Grijs: data uit warm-start
  - Blauw: live data + SIDEWAYS
  - Groen: live data + UP
  - Rood: live data + DOWN
- **"-warm" tekst verwijderd** - Status nu alleen via kleur

---

*Laatste update: Warm-start optimalisaties, mutex/API timing verbeteringen, trend kleuren (v3.82)*
