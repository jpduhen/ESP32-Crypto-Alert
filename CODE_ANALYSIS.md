# Code Analysis - UNIFIED-LVGL9-Crypto_Monitor

**Versie:** 4.13  
**Laatste update:** 2025-01-XX

---

## 1. Architectuur Overzicht

### 1.1 Systeem Architectuur

Het systeem gebruikt een **modulaire FreeRTOS multi-task architectuur**:

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Platform                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │  Core 1      │  │  Core 0      │  │  Core 0      │ │
│  │  API Task    │  │  UI Task     │  │  Web Task    │ │
│  │  (1500ms)    │  │  (1000ms)    │  │  (100ms)     │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
│         │                 │                 │          │
│         └─────────────────┼─────────────────┘          │
│                           │                             │
│                    ┌──────▼──────┐                      │
│                    │  dataMutex  │                      │
│                    │  (Shared)   │                      │
│                    └─────────────┘                      │
└─────────────────────────────────────────────────────────┘
```

### 1.2 Module Dependencies

**Onafhankelijke Modules:**
- `SettingsStore` - Geen dependencies
- `Memory/HeapMon` - Geen dependencies
- `Net/HttpFetch` - Alleen ESP32 core

**Core Data Modules:**
- `ApiClient` → `Net/HttpFetch`
- `PriceData` → `ApiClient`
- `TrendDetector` → `PriceData`
- `VolatilityTracker` → `PriceData`

**Business Logic Modules:**
- `AnchorSystem` → `PriceData`, `TrendDetector`
- `AlertEngine` → `PriceData`, `TrendDetector`, `VolatilityTracker`, `AnchorSystem`

**UI Modules:**
- `UIController` → `PriceData`, `TrendDetector`, `VolatilityTracker`, `AnchorSystem`
- `WebServer` → `SettingsStore`, `TrendDetector`, `VolatilityTracker`, `AnchorSystem`

**Utility Modules:**
- `WarmStart` → `ApiClient`, `PriceData`

---

## 2. FreeRTOS Task Analyse

### 2.1 API Task (Core 1)

**Functie:** `apiTask(void *parameter)`

**Verantwoordelijkheden:**
1. Haal prijs op van Binance (`fetchPrice()`)
2. Update price buffers (`PriceData::addPrice()`)
3. Bereken returns (`PriceData::calculateReturns()`)
4. Update trend (`TrendDetector::determineTrendState()`)
5. Update volatiliteit (`VolatilityTracker::determineVolatilityState()`)
6. Check alerts (`AlertEngine::checkAndNotify()`, `check2HAlerts()`)
7. Verwerk anchor setting queue (`pendingAnchorSetting`)

**Interval:** 1500ms (configurable via `API_CALL_INTERVAL`)

**Stack Size:**
- ESP32: 8192 bytes
- ESP32-S3: 10240 bytes

**Mutex Usage:**
- Neemt `dataMutex` voor alle data operaties
- Timeout: 500ms (configurable)

**Performance:**
- Meest CPU-intensieve task
- Network I/O (Binance API calls)
- JSON parsing
- Alert detection logic

---

### 2.2 UI Task (Core 0)

**Functie:** `uiTask(void *parameter)`

**Verantwoordelijkheden:**
1. Update UI (`UIController::updateUI()`)
2. LVGL rendering (`lv_timer_handler()`)
3. Button handling (`UIController::checkButton()`)
4. Chart updates
5. Label updates (trend, volatility, price, anchor)

**Interval:** 1000ms (configurable via `UPDATE_UI_INTERVAL`)

**LVGL Handler:** Elke 3-5ms (platform-specifiek)

**Stack Size:**
- ESP32: 8192 bytes
- ESP32-S3: 10240 bytes

**Mutex Usage:**
- Neemt `dataMutex` voor data lezen
- Timeout: 100ms (korter voor UI responsiveness)

**Performance:**
- Rendering overhead (LVGL)
- Display updates
- Cache-based updates (minimaliseert rendering)

---

### 2.3 Web Task (Core 0)

**Functie:** `webTask(void *parameter)`

**Verantwoordelijkheden:**
1. Web server request handling (`WebServerModule::handleClient()`)
2. HTML generation (`WebServerModule::renderSettingsHTML()`)
3. Settings save/load
4. Status API (`/status` endpoint)

**Interval:** 100ms (server.handleClient() continu)

**Stack Size:**
- ESP32: 4096 bytes
- ESP32-S3: 6144 bytes

**Mutex Usage:**
- Neemt `dataMutex` voor data lezen (read-only)
- Timeout: 100ms

**Performance:**
- HTML generation (WEB-PERF-3: cached)
- JSON status endpoint (lightweight)
- Network I/O (HTTP responses)

---

## 3. Data Structure Analyse

### 3.1 Price Buffers

**`secondPrices[]`** - 1-seconde prijs buffer
- Size: 60 elements (1 minuut data)
- Type: `float`
- Usage: 1m return berekening

**`fiveMinutePrices[]`** - 5-minuut prijs buffer
- Size: 12 elements (1 uur data)
- Type: `float`
- Usage: 5m return berekening

**`minuteAverages[]`** - 1-minuut gemiddelde buffer
- Size: 120 elements (2 uur data)
- Type: `float`
- Usage: 30m, 2h return berekening, 2h metrics

**Memory Footprint:**
- `secondPrices`: 60 × 4 bytes = 240 bytes
- `fiveMinutePrices`: 12 × 4 bytes = 48 bytes
- `minuteAverages`: 120 × 4 bytes = 480 bytes
- **Total:** ~768 bytes

---

### 3.2 Alert State Structures

**`Alert2HState`** - Persistent state voor 2h alerts
- Size: 24 bytes (geoptimaliseerd met bitfields)
- Fields:
  - `lastBreakoutUpMs` (4 bytes)
  - `lastBreakoutDownMs` (4 bytes)
  - `lastCompressMs` (4 bytes)
  - `lastMeanMs` (4 bytes)
  - `lastAnchorCtxMs` (4 bytes)
  - `flags` (1 byte) - Bitfield voor armed states

**`TwoHMetrics`** - 2-uur metrics struct
- Size: 20 bytes
- Fields:
  - `avg2h` (4 bytes)
  - `high2h` (4 bytes)
  - `low2h` (4 bytes)
  - `rangePct` (4 bytes)
  - `valid` (1 byte)

---

### 3.3 UI Cache Variables

**Cache voor performance:**
- `lastPriceLblValue` - Cache voor price label updates
- `lastAnchorValue` - Cache voor anchor label updates
- `lastAnchorMaxValue` - Cache voor anchor max label updates
- `lastAnchorMinValue` - Cache voor anchor min label updates

**Reset Logic:**
- Reset naar `-1.0f` forceert UI update
- Gebeurt in `AnchorSystem::setAnchorPrice()` (versie 4.13 fix)

---

## 4. Alert System Analyse

### 4.1 Alert Types & Timeframes

**Short-term Alerts (1m, 5m, 30m):**
- **Spike Alert:** Plotselinge beweging (> threshold)
- **Move Alert:** Bevestigde directionele move
- **Momentum Alert:** Aanhoudende beweging

**2-hour Context Alerts:**
- **Breakout:** Prijs verlaat 2h range (UP)
- **Breakdown:** Prijs verlaat 2h range (DOWN)
- **Compression:** Volatiliteit instorting (range verstrakt)
- **Mean Reversion:** Prijs keert terug naar 2h gemiddelde
- **Anchor Context:** Anchor buiten 2h range
- **Trend Change:** 2h trend verschuiving

### 4.2 Alert Classification (FASE X.3)

**PRIMARY Alerts:**
- `ALERT2H_BREAKOUT_UP`
- `ALERT2H_BREAKOUT_DOWN`
- **Gedrag:** Altijd notificeren (override throttling)
- **Prefix:** `[PRIMARY]`

**SECONDARY Alerts:**
- `ALERT2H_TREND_CHANGE`
- `ALERT2H_MEAN_TOUCH`
- `ALERT2H_COMPRESS`
- `ALERT2H_ANCHOR_CTX`
- **Gedrag:** Onderhevig aan throttling matrix
- **Prefix:** `[Context]`

### 4.3 Throttling Matrix (FASE X.2)

**Suppressie Regels:**
- Trend Change → Trend Change: 180 min cooldown
- Trend Change → Mean Touch: 60 min cooldown
- Mean Touch → Mean Touch: 60 min cooldown
- Compress → Compress: 120 min cooldown

**Implementatie:**
- `AlertEngine::shouldThrottle2HAlert()` - Check throttling
- `last2HAlertType` - Laatste alert type
- `last2HAlertTimestamp` - Laatste alert timestamp

---

## 5. Trend Detection Analyse

### 5.1 Trend State Logic

**Trend States:**
- `TREND_UP` - Opwaartse trend
- `TREND_DOWN` - Neerwaartse trend
- `TREND_SIDEWAYS` - Zijwaartse trend

**Bepaling:**
- Gebaseerd op `ret_2h` (2-uur return)
- `ret_30m` (30-minuut return) voor confirmatie
- `trendThreshold` (instelbaar via web interface)

### 5.2 Hysteresis (FASE X.1)

**Probleem:** Trend status flip-flop bij kleine schommelingen

**Oplossing:** Hysteresis met 0.65 factor

**Logica:**
```
SIDEWAYS → UP:   ret_2h >= +trendThreshold && ret_30m >= 0
SIDEWAYS → DOWN: ret_2h <= -trendThreshold && ret_30m <= 0

UP → SIDEWAYS:   ret_2h < +(trendThreshold * 0.65) || ret_30m < 0
DOWN → SIDEWAYS: ret_2h > -(trendThreshold * 0.65) || ret_30m > 0
```

**Resultaat:** Stabilere trend status, minder Trend Change alerts

---

## 6. Memory Management Analyse

### 6.1 Heap Fragmentation Prevention

**Strategieën:**
1. **Minimal String Usage:**
   - `char[]` buffers i.p.v. `String` concatenatie
   - Stack-based buffers waar mogelijk
   - PROGMEM voor static strings

2. **HTML Caching (WEB-PERF-3):**
   - `sPageCache` - Cached HTML pagina
   - `sPageCacheValid` - Cache validatie flag
   - Invalidate na settings save/anchor set

3. **Streaming Parsing:**
   - `HttpFetch::streamingHttpFetch()` - Geen String allocaties
   - JSON parsing zonder volledige document in geheugen

### 6.2 Memory Monitoring

**Heap Telemetry:**
- `HeapMon::logHeap()` - Rate-limited logging
- Platform-specifieke heap info:
  - `heap_caps_get_free_size(MALLOC_CAP_8BIT)` - DRAM
  - `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` - PSRAM (indien beschikbaar)

**Logging Points:**
- `TASKS_START_PRE` - Voor task start
- `WEB_ROOT` - Bij web interface access
- Rate-limited om spam te voorkomen

---

## 7. Performance Optimalisaties

### 7.1 Web Interface Performance (WEB-PERF-3)

**Probleem:** Langzame web interface op ESP32-S2/S3

**Oplossingen:**
1. **HTML Caching:**
   - Cache settings pagina in `sPageCache`
   - Rebuild alleen na invalidatie

2. **Status Endpoint:**
   - `/status` JSON endpoint voor live data
   - Client-side updates via JavaScript
   - Geen full page reload nodig

3. **Minimal sendContent() Calls:**
   - Combineer HTML chunks waar mogelijk
   - Gebruik `client.write_P()` voor PROGMEM strings

### 7.2 UI Update Optimalisaties

**Cache-based Updates:**
- Check waarde verandering voor rendering
- Update alleen bij wijziging
- Reset cache na anchor setting (versie 4.13)

**LVGL Rendering:**
- Double buffering (indien PSRAM beschikbaar)
- Platform-specifieke buffer sizes
- Efficient chart updates

---

## 8. Thread Safety Analyse

### 8.1 Mutex Patterns

**dataMutex Usage:**
- **API Task:** Neemt mutex voor alle data writes
- **UI Task:** Neemt mutex voor data reads
- **Web Task:** Neemt mutex voor data reads (read-only)

**Timeout Handling:**
- `safeMutexTake()` - Helper met timeout
- Timeout counters voor monitoring
- Graceful degradation bij timeout

### 8.2 Cross-Task Communication

**Queue-based:**
- `pendingAnchorSetting` - Volatile struct voor anchor setting queue
- Thread-safe flags (`pending`, `useCurrentPrice`)

**Volatile Variables:**
- `lastAnchorSetTime` - Cooldown tracking
- Cross-task state flags

---

## 9. Error Handling

### 9.1 Network Errors

**API Call Failures:**
- Retry logic in `ApiClient::fetchPrice()`
- Exponential backoff
- Graceful degradation (gebruik laatste prijs)

**WiFi Reconnection:**
- Automatic reconnection
- Exponential backoff
- State tracking

### 9.2 Data Validation

**Price Validation:**
- `isValidPrice()` - Check voor NaN, Inf, negatieve waarden
- Range checks
- Fallback naar laatste geldige prijs

**Settings Validation:**
- Range checks voor thresholds
- Default values bij invalid input
- Sanitization van string inputs

---

## 10. Code Quality Metrics

### 10.1 Modulariteit

**Score:** ⭐⭐⭐⭐⭐ (Excellent)
- Duidelijke module scheiding
- Minimale dependencies
- Herbruikbare componenten

### 10.2 Thread Safety

**Score:** ⭐⭐⭐⭐ (Good)
- Mutex voor gedeelde data
- Queue-based communicatie
- Volatile flags waar nodig

### 10.3 Memory Management

**Score:** ⭐⭐⭐⭐ (Good)
- Minimal heap allocations
- Cache-based optimizations
- Heap monitoring

### 10.4 Error Handling

**Score:** ⭐⭐⭐ (Moderate)
- Network error handling
- Data validation
- Graceful degradation

---

## 11. Known Issues & Limitations

### 11.1 Current Limitations

1. **Single Symbol Support:**
   - Ondersteunt één Binance symbol tegelijk
   - Multi-symbol support vereist architectuur wijziging

2. **No Historical Data Storage:**
   - Geen persistent historische data
   - Warm-start haalt data op, maar slaat niet op

3. **Limited Alert History:**
   - Geen alert geschiedenis
   - Alleen laatste alert state

### 11.2 Platform-Specific Considerations

**ESP32-S3:**
- Meer stack ruimte nodig
- PSRAM beschikbaar (double buffering)
- Betere performance dan ESP32

**CYD (ESP32):**
- Geen PSRAM
- Single buffer rendering
- Memory constraints

---

## 12. Future Improvements

### 12.1 Potential Enhancements

1. **Multi-Symbol Support:**
   - Parallel symbol monitoring
   - Symbol switching via web interface

2. **Alert History:**
   - Persistent alert log
   - Alert statistics

3. **Advanced Filtering:**
   - Machine learning voor noise filtering
   - Adaptive thresholds

4. **Data Export:**
   - CSV export van historische data
   - API voor externe tools

---

## 13. Testing Recommendations

### 13.1 Unit Testing

**Modules te testen:**
- `TrendDetector::determineTrendState()` - Hysteresis logica
- `AlertEngine::shouldThrottle2HAlert()` - Throttling matrix
- `PriceData::calculateReturns()` - Return berekeningen

### 13.2 Integration Testing

**Scenarios:**
- Anchor setting via web interface → UI update
- Alert throttling → Suppressie regels
- Trend change → Hysteresis gedrag

### 13.3 Performance Testing

**Metrics:**
- Web interface load time
- API call latency
- UI update frequency
- Memory usage over time

---

**Laatste update:** 2025-01-XX - Versie 4.13
