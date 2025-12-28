# Module Optimalisatie Richtlijnen

## Overzicht
Dit document beschrijft de randvoorwaarden, geleerde lessen en best practices voor het optimaliseren van modules in het UNIFIED-LVGL9-Crypto_Monitor project. Deze richtlijnen zijn gebaseerd op de optimalisatie van de AlertEngine module.

---

## Randvoorwaarden

### 1. Geheugen Optimalisaties

#### DRAM Constraints
- **TTGO board**: Zeer beperkte DRAM beschikbaar
- **Overflow threshold**: Elke byte telt (72 bytes overflow was kritiek)
- **Static vs Instance members**: 
  - Static functies kunnen **geen** instance members gebruiken
  - Instance members gebruiken DRAM (`.dram0.bss` sectie)
  - Lokale stack variabelen gebruiken stack geheugen (niet DRAM)

#### Buffer Optimalisatie Strategie
```cpp
// ❌ FOUT: Static functie met instance member
static void formatNotificationMessage(...) {
    getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer)); // ERROR!
}

// ✅ CORRECT: Static functie met lokale buffer
static void formatNotificationMessage(...) {
    char timestamp[32];  // Lokale stack variabele
    getFormattedTimestamp(timestamp, sizeof(timestamp));
}

// ✅ CORRECT: Non-static functie met instance member
void checkAndNotify(...) {
    getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer)); // OK!
}
```

#### Buffer Sizing
- **Minimaliseer buffer groottes** waar mogelijk
- **Hergebruik buffers** in plaats van meerdere lokale allocaties
- **Verklein buffers** tot minimum benodigde grootte:
  - `msgBuffer`: 256 → 200 bytes (bespaart 56 bytes)
  - `titleBuffer`: 64 → 48 bytes (bespaart 16 bytes)
  - `timestampBuffer`: 32 bytes (voldoende)

#### Buffer Strategie: Instance vs Lokale Buffers
- **Instance buffers** gebruiken DRAM (`.dram0.bss` sectie) - **gebruik alleen als nodig**
- **Lokale stack buffers** gebruiken stack geheugen - **gebruik wanneer mogelijk**
- **Wanneer instance buffers gebruiken?**
  - Functie wordt zeer frequent aangeroepen (elke seconde of vaker)
  - Meerdere functies delen dezelfde buffers
  - Stack geheugen is beperkt in die context
- **Wanneer lokale buffers gebruiken?**
  - Functie wordt minder frequent aangeroepen (minder dan elke seconde)
  - Functie is niet recursief
  - Buffers zijn kort-levend (alleen tijdens functie-uitvoering)
  - **Bespaart DRAM** - kritiek bij DRAM overflow!

**Voorbeeld (AnchorSystem):**
```cpp
// ❌ FOUT: Instance buffers gebruiken DRAM (208 bytes)
class AnchorSystem {
private:
    char msgBuffer[140];      // DRAM!
    char titleBuffer[40];     // DRAM!
    char timestampBuffer[32]; // DRAM!
};

// ✅ CORRECT: Lokale buffers gebruiken stack (geen DRAM)
void AnchorSystem::checkAnchorAlerts() {
    char timestamp[32];  // Stack geheugen
    char title[40];      // Stack geheugen
    char msg[140];      // Stack geheugen
    // ... gebruik buffers ...
}  // Buffers worden automatisch vrijgegeven
```

### 2. CPU Optimalisaties

#### Caching Strategie
- **Cache berekende waarden** om herhaalde berekeningen te voorkomen:
  ```cpp
  // Cache absolute waarden (voorkomt herhaalde fabsf calls)
  void cacheAbsoluteValues(float ret_1m, float ret_5m, float ret_30m) {
      cachedAbsRet1m = (ret_1m != 0.0f) ? fabsf(ret_1m) : 0.0f;
      cachedAbsRet5m = (ret_5m != 0.0f) ? fabsf(ret_5m) : 0.0f;
      cachedAbsRet30m = (ret_30m != 0.0f) ? fabsf(ret_30m) : 0.0f;
      valuesCached = true;
  }
  ```

#### Early Returns
- **Check voorwaarden eerst** (sneller dan complexe berekeningen):
  ```cpp
  // ❌ FOUT: Complexe berekening eerst
  bool spikeDetected = (absRet1m >= effThresh.spike1m) && 
                       (absRet5m >= alertThresholds.spike5m) && 
                       sameDirection;
  if (spikeDetected) { ... }

  // ✅ CORRECT: Early return bij threshold check
  if (absRet1m < effThresh.spike1m || absRet5m < alertThresholds.spike5m) {
      return;  // Skip rest
  }
  // ... verder met checks
  ```

#### Inline Helpers
- **Mark kleine helper functies als `inline`**:
  ```cpp
  static inline bool eventsWithinTimeWindow(...) { ... }
  static inline bool trendSupportsDirection(...) { ... }
  ```

#### Enum i.p.v. String Vergelijkingen
- **Gebruik enums in plaats van string vergelijkingen** voor betere performance:
  ```cpp
  // ❌ FOUT: String vergelijking (langzaam)
  if (strcmp(eventType, "take_profit") == 0) { ... }
  else if (strcmp(eventType, "max_loss") == 0) { ... }
  
  // ✅ CORRECT: Enum switch (snel)
  enum AnchorEventType {
      ANCHOR_EVENT_TAKE_PROFIT,
      ANCHOR_EVENT_MAX_LOSS
  };
  if (eventType == ANCHOR_EVENT_TAKE_PROFIT) { ... }
  else { ... }  // ANCHOR_EVENT_MAX_LOSS
  ```

#### Code Duplicatie Eliminatie
- **Consolideer duplicatie met helper functies**:
  ```cpp
  // ❌ FOUT: Code duplicatie
  if (takeProfit) {
      char title[40]; char msg[140];
      formatNotification(...);
      sendNotification(title, msg, "green");
      // ... update flags ...
  }
  if (maxLoss) {
      char title[40]; char msg[140];  // Duplicatie!
      formatNotification(...);
      sendNotification(title, msg, "red");
      // ... update flags ...
  }
  
  // ✅ CORRECT: Helper functie
  void sendAnchorAlert(AnchorEventType type, ...) {
      char title[40]; char msg[140];
      formatNotification(type, ...);
      const char* color = (type == TAKE_PROFIT) ? "green" : "red";
      sendNotification(title, msg, color);
      // ... update flags ...
  }
  if (takeProfit) sendAnchorAlert(TAKE_PROFIT, ...);
  if (maxLoss) sendAnchorAlert(MAX_LOSS, ...);
  ```

#### Geconsolideerde Validatie
- **Combineer meerdere validatie checks in één statement**:
  ```cpp
  // ❌ FOUT: Meerdere early returns
  if (!active) return;
  if (!isValid(price)) return;
  if (isnan(price)) return;
  if (price <= 0) return;
  
  // ✅ CORRECT: Geconsolideerde check
  if (!active || !isValid(price) || isnan(price) || price <= 0) {
      return;  // Eén check, sneller
  }
  ```

#### Check Volgorde Optimalisatie
- **Snelle checks eerst**, langzame checks later:
  ```cpp
  // ✅ CORRECT: Snelle checks eerst
  if (!smartConfluenceEnabled) return false;
  if (last1mEvent.direction == EVENT_NONE) return false;
  if (last1mEvent.direction != last5mEvent.direction) return false;  // Sneller dan time window check
  if (!eventsWithinTimeWindow(...)) return false;  // Langzamere check later
  ```

### 3. Stabiliteit

#### Input Validatie
- **Check voor NaN/Inf waarden**:
  ```cpp
  if (isnan(ret_1m) || isinf(ret_1m) || isnan(ret_5m) || isinf(ret_5m)) {
      return;  // Skip checks bij ongeldige waarden
  }
  ```

#### Null Pointer Checks
- **Altijd checken op null pointers**:
  ```cpp
  const float* fiveMinPrices = priceData.getFiveMinutePrices();
  if (fiveMinPrices == nullptr) {
      // Fallback naar veilige waarde
      minVal = prices[0];
      maxVal = prices[0];
      return false;
  }
  ```

#### Array Bounds Checks
- **Valideer array indices en sizes**:
  ```cpp
  if (elementsToCheck == 0 || elementsToCheck > SECONDS_PER_5MINUTES) {
      // Fallback
      return false;
  }
  ```

#### Fallback Waarden
- **Altijd fallback waarden** voor kritieke berekeningen:
  ```cpp
  if (!foundValid || minVal <= 0.0f || maxVal <= 0.0f) {
      // Fallback naar huidige prijs
      if (prices[0] > 0.0f) {
          minVal = prices[0];
          maxVal = prices[0];
      }
      return false;
  }
  ```

### 4. Code Opschoning

#### Debug Logging
- **Conditional debug logging** om performance impact te minimaliseren:
  ```cpp
  #if !DEBUG_BUTTON_ONLY
  Serial_printf(F("[Notify] Alert verzonden\n"));
  #endif
  ```

#### Duplicatie Eliminatie
- **Hergebruik helper functies** in plaats van code duplicatie
- **Consolideer vergelijkbare logica** in één functie

#### Commentaar
- **Documenteer optimalisaties** met korte comments:
  ```cpp
  // Geoptimaliseerd: single-pass min/max berekening
  // Static functie: gebruik lokale buffers (kan geen instance members gebruiken)
  ```

---

## Geleerde Lessen

### 1. Static vs Non-Static Functies

**Probleem**: Static member functies kunnen geen instance members gebruiken.

**Oplossing**: 
- Gebruik lokale buffers in static functies
- Gebruik instance members alleen in non-static functies
- Documenteer duidelijk welke functies static zijn

**Voorbeeld**:
```cpp
class AlertEngine {
private:
    char msgBuffer[200];  // Instance member
    
public:
    // Static functie: gebruikt lokale buffer
    static void formatNotificationMessage(char* msg, ...) {
        char timestamp[32];  // Lokale buffer
        // ...
    }
    
    // Non-static functie: gebruikt instance member
    void checkAndNotify(...) {
        getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer));  // Instance member
        // ...
    }
};
```

### 2. Buffer Strategie: Instance vs Lokale Buffers

**Probleem**: Instance buffers gebruiken DRAM, wat bij grote overflow (>200 bytes) problematisch is.

**Oplossing**:
- **Eerst proberen**: Verklein instance buffers
- **Als dat niet genoeg is**: Verwijder instance buffers en gebruik lokale stack buffers
- Analyseer functie frequentie: als functie niet zeer frequent wordt aangeroepen, gebruik lokale buffers

**Voorbeeld (AnchorSystem)**:
- **Fase 1**: Verklein buffers (200→160, 48→40, 32→32) = 78 bytes bespaard
- **Fase 2**: Verwijder cache struct = 6 bytes bespaard
- **Fase 3**: Verwijder alle instance buffers, gebruik lokale buffers = 208 bytes bespaard
- **Totaal: 286 bytes DRAM bespaard** (meer dan genoeg voor 296 bytes overflow!)

**Belangrijk**: Lokale buffers gebruiken stack geheugen, wat acceptabel is voor niet-recursieve functies die niet zeer frequent worden aangeroepen.

### 3. Caching Strategie

**Probleem**: Herhaalde berekeningen in meerdere functies.

**Oplossing**:
- Cache absolute waarden één keer
- Hergebruik gecachte waarden in meerdere functies
- Reset cache flag wanneer nodig
- **Maar**: Overweeg of cache nodig is - soms is direct berekenen beter voor geheugen

**Voorbeeld (AlertEngine - cache gebruikt)**:
```cpp
void checkAndNotify(...) {
    cacheAbsoluteValues(ret_1m, ret_5m, ret_30m);  // Cache één keer
    
    // Gebruik gecachte waarden
    float absRet1m = cachedAbsRet1m;
    float absRet5m = cachedAbsRet5m;
    
    // Update functies kunnen ook gecachte waarden gebruiken
    update1mEvent(ret_1m, now, effThresh.spike1m);  // Gebruikt cachedAbsRet1m intern
}
```

**Voorbeeld (AnchorSystem - cache verwijderd)**:
```cpp
// Cache verwijderd om DRAM te besparen (6 bytes)
// Direct berekenen kost iets meer CPU maar bespaart kritieke DRAM bytes
float anchorPct = ((prices[0] - this->anchorPrice) / this->anchorPrice) * 100.0f;
TrendState currentTrend = trendDetector.getTrendState();  // Direct ophalen
```

**Beslissing**: Gebruik cache alleen als:
- Functie wordt zeer frequent aangeroepen (elke seconde of vaker)
- Berekening is duur (complexe wiskunde)
- DRAM is niet kritiek beperkt

### 4. Early Returns

**Probleem**: Onnodige berekeningen werden uitgevoerd zelfs wanneer niet nodig.

**Oplossing**:
- Check voorwaarden eerst (snelle checks)
- Return early wanneer mogelijk
- Voorkom onnodige berekeningen

**Voorbeeld**:
```cpp
// ❌ FOUT: Berekent alles, zelfs als threshold niet gehaald wordt
bool spikeDetected = (absRet1m >= effThresh.spike1m) && 
                     (absRet5m >= alertThresholds.spike5m) && 
                     sameDirection;

// ✅ CORRECT: Early return bij threshold check
if (absRet1m < effThresh.spike1m || absRet5m < alertThresholds.spike5m) {
    return;  // Skip rest
}
// Alleen verder als thresholds gehaald zijn
```

### 5. Single-Pass Algoritmes

**Probleem**: Dubbele loops voor min/max berekening.

**Oplossing**:
- Combineer berekeningen in één loop
- Gebruik single-pass algoritmes waar mogelijk

**Voorbeeld**:
```cpp
// ❌ FOUT: Dubbele loop
for (uint16_t i = 0; i < elementsToCheck; i++) {
    if (valid) { minVal = val; maxVal = val; break; }
}
for (uint16_t i = 0; i < elementsToCheck; i++) {
    if (valid) { if (val < minVal) minVal = val; if (val > maxVal) maxVal = val; }
}

// ✅ CORRECT: Single-pass
bool foundValid = false;
for (uint16_t i = 0; i < elementsToCheck; i++) {
    if (valid) {
        if (!foundValid) {
            minVal = val; maxVal = val; foundValid = true;
        } else {
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
        }
    }
}
```

### 6. Enum i.p.v. String Vergelijkingen

**Probleem**: `strcmp()` calls zijn langzaam en gebruiken meer geheugen.

**Oplossing**:
- Gebruik enums in plaats van string constanten
- Switch statements zijn sneller dan string vergelijkingen
- Compiler kan betere optimalisaties doen

**Voorbeeld (AnchorSystem)**:
```cpp
// ❌ FOUT: String vergelijkingen
if (strcmp(eventType, "take_profit") == 0) { ... }
else if (strcmp(eventType, "max_loss") == 0) { ... }

// ✅ CORRECT: Enum switch
enum AnchorEventType {
    ANCHOR_EVENT_TAKE_PROFIT,
    ANCHOR_EVENT_MAX_LOSS
};
switch (eventType) {
    case ANCHOR_EVENT_TAKE_PROFIT: ... break;
    case ANCHOR_EVENT_MAX_LOSS: ... break;
}
```

### 7. Code Duplicatie Eliminatie met Helper Functies

**Probleem**: Take profit en max loss checks hadden veel duplicatie.

**Oplossing**:
- Maak helper functie die gedeelde logica consolideert
- Gebruik parameters om verschillen te hanteren
- Eén plek voor wijzigingen

**Voorbeeld (AnchorSystem)**:
```cpp
// ❌ FOUT: Duplicatie tussen take profit en max loss
if (!anchorTakeProfitSent && anchorPct >= effAnchor.takeProfitPct) {
    char title[40]; char msg[140];
    formatNotification("take_profit", ...);
    sendNotification(title, msg, "green");
    anchorTakeProfitSent = true;
    // ... update globals, logging, MQTT ...
}
if (!anchorMaxLossSent && anchorPct <= effAnchor.maxLossPct) {
    char title[40]; char msg[140];  // Duplicatie!
    formatNotification("max_loss", ...);
    sendNotification(title, msg, "red");
    anchorMaxLossSent = true;
    // ... update globals, logging, MQTT ...
}

// ✅ CORRECT: Helper functie
void sendAnchorAlert(AnchorEventType type, ...) {
    char title[40]; char msg[140];
    formatNotification(type, ...);
    const char* color = (type == TAKE_PROFIT) ? "green" : "red";
    sendNotification(title, msg, color);
    // ... update flags, globals, logging, MQTT ...
}
if (!anchorTakeProfitSent && anchorPct >= effAnchor.takeProfitPct) {
    sendAnchorAlert(ANCHOR_EVENT_TAKE_PROFIT, ...);
}
if (!anchorMaxLossSent && anchorPct <= effAnchor.maxLossPct) {
    sendAnchorAlert(ANCHOR_EVENT_MAX_LOSS, ...);
}
```

### 8. Geconsolideerde Validatie Checks

**Probleem**: Meerdere early returns maken code minder efficiënt.

**Oplossing**:
- Combineer gerelateerde checks in één if-statement
- Minder branches = snellere code
- Betere leesbaarheid

**Voorbeeld (AnchorSystem)**:
```cpp
// ❌ FOUT: Meerdere early returns
if (!this->anchorActive) return;
if (!isValidPrice(this->anchorPrice)) return;
if (!isValidPrice(prices[0])) return;
if (isnan(prices[0])) return;
if (isinf(prices[0])) return;
if (isnan(this->anchorPrice)) return;
if (isinf(this->anchorPrice)) return;
if (this->anchorPrice <= 0.0f) return;

// ✅ CORRECT: Geconsolideerde check
if (!this->anchorActive || 
    !isValidPrice(this->anchorPrice) || !isValidPrice(prices[0]) ||
    isnan(prices[0]) || isinf(prices[0]) || 
    isnan(this->anchorPrice) || isinf(this->anchorPrice) ||
    this->anchorPrice <= 0.0f) {
    return;  // Eén check, sneller
}
```

### 9. Geconsolideerde Operaties

**Probleem**: Herhaalde operaties of duplicatie van logica.

**Oplossing**:
- Consolideer vergelijkbare operaties
- Gebruik één keer berekenen en hergebruiken
- Elimineer onnodige herhaling

**Voorbeeld (AnchorSystem - calculateEffectiveAnchorThresholds)**:
```cpp
// ❌ FOUT: Clamp operaties op twee plaatsen
if (!trendAdaptiveEnabled) {
    eff.maxLossPct = baseMaxLoss;
    eff.takeProfitPct = baseTakeProfit;
    // Clamp hier
    if (eff.maxLossPct < -6.0f) eff.maxLossPct = -6.0f;
    // ...
    return eff;
}
// ... switch statement ...
// Clamp hier weer
if (eff.maxLossPct < -6.0f) eff.maxLossPct = -6.0f;
// ...

// ✅ CORRECT: Clamp één keer aan het einde
if (!trendAdaptiveEnabled) {
    eff.maxLossPct = baseMaxLoss;
    eff.takeProfitPct = baseTakeProfit;
} else {
    // ... switch statement ...
}
// Clamp één keer voor beide paden
if (eff.maxLossPct < -6.0f) eff.maxLossPct = -6.0f;
// ...
return eff;
```

---

## Checklist voor Module Optimalisatie

### Geheugen
- [ ] Analyseer DRAM gebruik (check `.dram0.bss` sectie)
- [ ] Identificeer grote buffers en arrays
- [ ] **Fase 1**: Verklein buffers tot minimum benodigde grootte
- [ ] **Fase 2**: Als nog steeds overflow, verwijder instance buffers en gebruik lokale stack buffers
- [ ] Gebruik lokale buffers in static functies
- [ ] Hergebruik buffers waar mogelijk (maar alleen als DRAM niet kritiek is)
- [ ] Overweeg dynamische allocatie voor grote arrays (alleen indien nodig)
- [ ] **Beslissing**: Instance buffers vs lokale buffers op basis van functie frequentie

### CPU
- [ ] Identificeer herhaalde berekeningen
- [ ] Cache berekende waarden (alleen indien nodig)
- [ ] Voeg early returns toe
- [ ] **Consolideer early returns** in één gecombineerde check
- [ ] Optimaliseer check volgorde (snelle checks eerst)
- [ ] Mark kleine helpers als `inline`
- [ ] Gebruik single-pass algoritmes
- [ ] **Gebruik enums i.p.v. string vergelijkingen** (strcmp)
- [ ] **Elimineer code duplicatie** met helper functies
- [ ] **Consolideer vergelijkbare operaties** (clamp, validatie, etc.)

### Stabiliteit
- [ ] Voeg input validatie toe (NaN/Inf checks)
- [ ] Check null pointers
- [ ] Valideer array bounds
- [ ] Implementeer fallback waarden
- [ ] Test edge cases

### Code Kwaliteit
- [ ] Elimineer code duplicatie met helper functies
- [ ] **Gebruik enums i.p.v. string constanten** waar mogelijk
- [ ] **Consolideer vergelijkbare logica** in helper functies
- [ ] Conditional debug logging
- [ ] Documenteer optimalisaties
- [ ] Test alle functionaliteit na optimalisatie

---

## Template voor Module Review

```markdown
## Module: [Module Naam]

### Geheugen Analyse
- **Huidige DRAM gebruik**: [X] bytes
- **Grote buffers**: [lijst]
- **Optimalisatie mogelijkheden**: [lijst]

### CPU Analyse
- **Herhaalde berekeningen**: [lijst]
- **Check volgorde**: [analyse]
- **Optimalisatie mogelijkheden**: [lijst]

### Stabiliteit Analyse
- **Input validatie**: [status]
- **Error handling**: [status]
- **Edge cases**: [lijst]

### Uitgevoerde Optimalisaties
1. [Beschrijving]
2. [Beschrijving]
3. [Beschrijving]

### Resultaat
- **DRAM bespaard**: [X] bytes
- **CPU verbetering**: [beschrijving]
- **Stabiliteit**: [verbeteringen]
```

---

## Best Practices

1. **Meet eerst, optimaliseer daarna**: Gebruik tools om geheugen/CPU gebruik te meten
2. **Test na elke optimalisatie**: Zorg dat functionaliteit behouden blijft
3. **Documenteer wijzigingen**: Maak duidelijk waarom optimalisaties zijn toegepast
4. **Balanceer optimalisatie vs leesbaarheid**: Code moet onderhoudbaar blijven
5. **Platform-specifieke optimalisaties**: Overweeg verschillende optimalisaties per platform

---

## Referenties

- **AlertEngine optimalisatie**: `src/AlertEngine/AlertEngine.cpp` en `AlertEngine.h`
  - Buffer verkleining (72 bytes bespaard)
  - Instance buffers behouden (functie wordt zeer frequent aangeroepen)
  - Caching geïmplementeerd (CPU optimalisatie)
  
- **AnchorSystem optimalisatie**: `src/AnchorSystem/AnchorSystem.cpp` en `AnchorSystem.h`
  - Instance buffers verwijderd (286 bytes DRAM bespaard)
  - Lokale stack buffers gebruikt (functie wordt minder frequent aangeroepen)
  - Cache verwijderd om geheugen te besparen
  - Berichten gecompacteerd voor kleinere buffers
  - **Enum i.p.v. string vergelijkingen** (sneller, minder geheugen)
  - **Code duplicatie geëlimineerd** met `sendAnchorAlert()` helper
  - **Geconsolideerde validatie** checks (minder branches)
  - **Inline helper** voor trend name (`getTrendName()`)
  - **Geconsolideerde clamp operaties** in `calculateEffectiveAnchorThresholds()`

- **AlertEngine optimalisatie (tweede ronde)**: `src/AlertEngine/AlertEngine.cpp` en `AlertEngine.h`
  - **Geconsolideerde validatie** in `check2HNotifications()` (minder branches, snellere exit)
  - **Code duplicatie geëlimineerd** met `send2HBreakoutNotification()` helper (breakout up/down)
  - **Inline helper** voor trend name (`getTrendName()`) - hergebruikt van AnchorSystem pattern
  - **Geconsolideerde berekeningen**: `breakMargin` en `breakThreshold` één keer berekend i.p.v. meerdere keren
  - **Hergebruik berekende waarden**: thresholds worden hergebruikt in debug logging

- **ApiClient optimalisatie**: `src/ApiClient/ApiClient.cpp` en `ApiClient.h`
  - **Code duplicatie geëlimineerd** met `logHttpError()` helper (error logging tussen `httpGETInternal` en `fetchBinancePrice`)
  - **Helper functie** voor fase detectie (`detectHttpErrorPhase()`) - elimineert duplicatie
  - **Geconsolideerde validatie** in `parseBinancePrice()` (minder branches)
  - **Geconsolideerde retry check** in `httpGETInternal()` (één check i.p.v. meerdere if-else)
  - **Geconsolideerde validatie** in `isValidPrice()` (alle checks in één statement)

- **Memory (HeapMon) optimalisatie**: `src/Memory/HeapMon.cpp` en `HeapMon.h`
  - **Code duplicatie geëlimineerd** met `findTagIndex()` helper (tag lookup tussen `logHeap()` en `resetRateLimit()`)
  - **Geconsolideerde logica** in `logHeap()` (minder variabelen, duidelijkere flow)
  - **Geoptimaliseerde tag lookup**: pointer vergelijking eerst (sneller), dan string vergelijking alleen als nodig

- **Net (HttpFetch) optimalisatie**: `src/Net/HttpFetch.cpp` en `HttpFetch.h`
  - **Geconsolideerde loop conditie** in `httpGetToBuffer()` (contentLength check gecombineerd met totalRead check)
  - **Geconsolideerde buffer check** (minder branches in loop)
  - **Error logging toegevoegd** voor betere debugging (conditional met DEBUG_BUTTON_ONLY)

- **PriceData optimalisatie**: `src/PriceData/PriceData.cpp` en `PriceData.h`
  - **Code duplicatie geëlimineerd** met lambda helper `updateAveragePrice()` (elimineert 3x herhaalde check)
  - **Geconsolideerde checks** in `calculateReturn1Minute()` (minder branches)
  - **Geconsolideerde index updates** in `addPriceToSecondArray()` (duidelijkere code flow)

- **SettingsStore optimalisatie**: `src/SettingsStore/SettingsStore.cpp` en `SettingsStore.h`
  - **Code duplicatie geëlimineerd** met `loadStringPreference()` helper (elimineert getString + toCharArray duplicatie)
  - **Helper functie** voor migration check (`needsTopicMigration()`) - elimineert complexe String operaties
  - **Geoptimaliseerde migration logica**: gebruikt char buffers i.p.v. String operaties waar mogelijk

- **TrendDetector optimalisatie**: `src/TrendDetector/TrendDetector.cpp` en `TrendDetector.h`
  - **Code duplicatie geëlimineerd** met helper functies (`getTrendName()`, `getTrendColorTag()`, `getVolatilityText()`)
  - **Geconsolideerde checks** in `checkTrendChange()` (minder branches, early return)
  - **Geconsolideerde checks** in `determineTrendState()` (early returns i.p.v. else-if)
  - **Lokale buffers** gebruikt i.p.v. instance members (stack geheugen)
  - **Conditional debug logging** met `DEBUG_BUTTON_ONLY` flag

- **UIController optimalisatie**: `src/UIController/UIController.cpp` en `UIController.h`
  - **Code duplicatie geëlimineerd** met `updateMinMaxDiffLabels()` helper (elimineert ~90 regels duplicatie voor 1m/30m/2h labels)
  - **Language check eliminatie**: Verwijderd onnodige `if (language == 1)` checks waar beide branches identiek waren
  - **Geconsolideerde berekeningen**: Diff berekening geconsolideerd in één regel
  - **Helper functie voor label updates**: Herbruikbare functie voor min/max/diff label updates

- **VolatilityTracker optimalisatie**: `src/VolatilityTracker/VolatilityTracker.cpp` en `VolatilityTracker.h`
  - **Code duplicatie geëlimineerd** met helper functies (`getClampedWindowSize()`, `setBaseThresholds()`)
  - **Geconsolideerde checks** in `calculateEffectiveThresholds()` (minder early returns, helper functie)
  - **Geconsolideerde clamp operaties** in `calculateEffectiveThresholds()` (if-else-if i.p.v. aparte if's)
  - **Early returns** in `determineVolatilityState()` (i.p.v. else-if chain)

- **WarmStart optimalisatie**: `src/WarmStart/WarmStart.cpp` en `WarmStart.h`
  - **Code duplicatie geëlimineerd** met template helper functie `getSettingValue()` (elimineert 5x herhaalde getter pattern)
  - **Code duplicatie geëlimineerd** met `logTimeframeStatus()` helper (elimineert 3x herhaalde logging code)
  - **Template helper functie** voor getter pattern (nullptr check + default waarde)

- **WebServer optimalisatie**: `src/WebServer/WebServer.cpp` en `WebServer.h`
  - **Code duplicatie geëlimineerd** met helper functies (`getTrendText()`, `getVolatilityText()`, `parseFloatArg()`, `parseIntArg()`, `parseStringArg()`)
  - **Geconsolideerde form argument parsing**: Elimineert ~50x herhaalde patterns voor form argument parsing
  - **Helper functies voor enum-to-string**: Hergebruik van bestaande helpers i.p.v. switch statements
  - **Geconsolideerde String operaties**: Helper functie voor String-to-char array conversie met validatie

- **DRAM overflow fixes**:
  - AlertEngine: 72 bytes overflow → buffer verkleining
  - AnchorSystem: 296 bytes overflow → instance buffers verwijderd

- **Static functie fix**: Lokale buffers in plaats van instance members
- **CPU optimalisaties**: Caching (waar nodig), early returns, inline helpers

---

*Laatste update: Na WebServer module optimalisatie*
*Versie: 2.12*

## Nieuwe Inzichten (Versie 2.1)

### Enum i.p.v. String Vergelijkingen
- **Performance**: Enum switches zijn veel sneller dan `strcmp()`
- **Geheugen**: Geen string constanten nodig
- **Type safety**: Compiler kan type checking doen
- **Gebruik wanneer**: Meerdere string vergelijkingen in dezelfde functie

### Code Duplicatie Eliminatie Strategie
- **Identificeer duplicatie**: Zoek naar vergelijkbare code blokken
- **Maak helper functie**: Consolideer gedeelde logica
- **Parameters voor verschillen**: Gebruik parameters om variaties te hanteren
- **Voordeel**: Eén plek voor wijzigingen, minder bugs

### Geconsolideerde Validatie
- **Combineer checks**: Meerdere gerelateerde checks in één statement
- **Minder branches**: Snellere code execution
- **Betere leesbaarheid**: Duidelijk wat alle voorwaarden zijn
- **Gebruik wanneer**: Meerdere early returns met gerelateerde checks

## Nieuwe Inzichten (Versie 2.2)

### Geconsolideerde Berekeningen
- **Hergebruik berekende waarden**: Bereken thresholds één keer en hergebruik ze
- **Voorkom herhaalde berekeningen**: Identificeer berekeningen die meerdere keren worden uitgevoerd
- **Performance**: Minder CPU cycles, snellere code
- **Voorbeeld**: `breakMargin` en `breakThreshold` in `check2HNotifications()` worden nu één keer berekend
- **Gebruik wanneer**: Dezelfde berekening wordt meerdere keren uitgevoerd in dezelfde functie

### Helper Functie Hergebruik
- **Hergebruik helpers tussen modules**: Als een helper nuttig is in meerdere modules, maak het static inline
- **Consistentie**: Gebruik dezelfde helper functies voor vergelijkbare operaties
- **Voorbeeld**: `getTrendName()` helper wordt nu gebruikt in zowel AnchorSystem als AlertEngine
- **Voordeel**: Minder code duplicatie, consistent gedrag

## Nieuwe Inzichten (Versie 2.3)

### Helper Functies voor Error Handling
- **Consolideer error handling logica**: Error logging en fase detectie zijn vaak gedupliceerd
- **Maak helper functies**: Voor error logging (`logHttpError()`) en fase detectie (`detectHttpErrorPhase()`)
- **Voordeel**: Eén plek voor wijzigingen, consistent error logging
- **Voorbeeld (ApiClient)**: Error logging tussen `httpGETInternal()` en `fetchBinancePrice()` was gedupliceerd, nu geconsolideerd in `logHttpError()` helper

### Geconsolideerde Retry Checks
- **Combineer retry voorwaarden**: Meerdere if-else checks voor retry-waardige fouten kunnen geconsolideerd worden
- **Minder branches**: Snellere code execution
- **Voorbeeld (ApiClient)**: Retry check in `httpGETInternal()` was meerdere if-else statements, nu één geconsolideerde check

## Nieuwe Inzichten (Versie 2.4)

### Helper Functies voor Lookup Operaties
- **Consolideer lookup logica**: Lookup operaties zijn vaak gedupliceerd tussen functies
- **Maak helper functie**: Voor lookup operaties (`findTagIndex()`, etc.)
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie
- **Voorbeeld (HeapMon)**: Tag lookup tussen `logHeap()` en `resetRateLimit()` was gedupliceerd, nu geconsolideerd in `findTagIndex()` helper

### Geoptimaliseerde String Vergelijkingen
- **Pointer vergelijking eerst**: Als pointers gelijk zijn, skip string vergelijking (sneller)
- **String vergelijking alleen als nodig**: Alleen `strcmp()` gebruiken als pointer vergelijking faalt
- **Performance**: Minder CPU cycles, snellere code
- **Voorbeeld (HeapMon)**: Tag lookup gebruikt nu eerst pointer vergelijking, dan pas `strcmp()` indien nodig

## Nieuwe Inzichten (Versie 2.5)

### Geconsolideerde Loop Condities
- **Combineer condities in loops**: Meerdere checks in loop condities kunnen gecombineerd worden
- **Minder branches**: Snellere code execution, minder overhead per iteratie
- **Voorbeeld (HttpFetch)**: Loop conditie combineert nu `contentLength < 0 || totalRead < (size_t)contentLength` i.p.v. aparte check binnen loop
- **Voordeel**: Minder branches per iteratie, snellere code

### Geconsolideerde Buffer Checks
- **Combineer buffer checks**: Meerdere gerelateerde buffer checks kunnen geconsolideerd worden
- **Minder branches**: Snellere code execution
- **Voorbeeld (HttpFetch)**: Buffer ruimte check is nu geconsolideerd in één check

## Nieuwe Inzichten (Versie 2.6)

### Lambda Helpers voor Code Duplicatie Eliminatie
- **Gebruik lambda's voor kleine helpers**: Lambda's kunnen gebruikt worden om code duplicatie te elimineren binnen functies
- **Voordeel**: Geen extra functie declaratie nodig, scope-limited, duidelijk
- **Voorbeeld (PriceData)**: `updateAveragePrice` lambda elimineert 3x herhaalde check `if (averagePrices != nullptr && averagePriceIndex < 3)`
- **Gebruik wanneer**: Kleine helper logica die alleen binnen één functie nodig is

### Geconsolideerde Index Updates
- **Combineer index updates met checks**: Index updates en gerelateerde checks kunnen geconsolideerd worden
- **Duidelijkere code flow**: Minder nested if-statements
- **Voorbeeld (PriceData)**: Index update en `arrayFilled` check zijn nu duidelijk gescheiden maar geconsolideerd

## Nieuwe Inzichten (Versie 2.7)

### Helper Functies voor String Operaties
- **Consolideer string loading logica**: String loading (getString + toCharArray) is vaak gedupliceerd
- **Maak helper functie**: Voor string loading (`loadStringPreference()`, etc.)
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie
- **Voorbeeld (SettingsStore)**: String loading tussen verschillende settings was gedupliceerd, nu geconsolideerd in `loadStringPreference()` helper

### Geoptimaliseerde String Vergelijkingen (C-style)
- **Gebruik C-style string functies**: `strcmp()`, `strncmp()`, `strlen()` zijn sneller dan String operaties
- **Vermijd String objecten waar mogelijk**: Gebruik char buffers direct
- **Performance**: Minder heap allocaties, snellere code
- **Voorbeeld (SettingsStore)**: Migration check gebruikt nu C-style string functies i.p.v. String operaties (`endsWith()`, `startsWith()`, etc.)

## Nieuwe Inzichten (Versie 2.8)

### Helper Functies voor Enum-to-String Conversies
- **Consolideer enum-to-string conversies**: Switch statements voor enum-to-string zijn vaak gedupliceerd
- **Maak inline helper functies**: Voor enum-to-string conversies (`getTrendName()`, `getTrendColorTag()`, etc.)
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag
- **Voorbeeld (TrendDetector)**: 3 switch statements geëlimineerd met helpers (`getTrendName()`, `getTrendColorTag()`, `getVolatilityText()`)

### Early Returns in Conditional Logic
- **Gebruik early returns**: In plaats van nested if-statements, gebruik early returns voor duidelijkere flow
- **Minder nesting**: Snellere code execution, betere leesbaarheid
- **Voorbeeld (TrendDetector)**: `checkTrendChange()` gebruikt nu early return i.p.v. nested if-statement

### Eliminatie van Onnodige Conditionals
- **Identificeer identieke branches**: Als beide branches van een if-statement identiek zijn, elimineer de conditional
- **Performance**: Minder branches, snellere code
- **Voorbeeld (UIController)**: `if (language == 1)` checks verwijderd waar beide branches identiek waren ("Warm-up 30m %um")

### Helper Functies voor Herhaalde UI Updates
- **Consolideer UI update logica**: Herhaalde patterns voor label updates kunnen geconsolideerd worden
- **Maak herbruikbare helpers**: Voor complexe update logica (min/max/diff labels, etc.)
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag
- **Voorbeeld (UIController)**: `updateMinMaxDiffLabels()` helper elimineert ~90 regels duplicatie voor 1m/30m/2h label updates

### Helper Functies voor Herhaalde Berekeningen
- **Consolideer berekeningen**: Herhaalde berekeningen (zoals window size clamping) kunnen geconsolideerd worden
- **Maak inline helper functies**: Voor kleine, veelgebruikte berekeningen
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag
- **Voorbeeld (VolatilityTracker)**: `getClampedWindowSize()` helper elimineert 3x herhaalde window size berekening

### Helper Functies voor Herhaalde Initialisaties
- **Consolideer initialisatie logica**: Herhaalde initialisatie patterns kunnen geconsolideerd worden
- **Maak inline helper functies**: Voor struct initialisaties met dezelfde waarden
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag
- **Voorbeeld (VolatilityTracker)**: `setBaseThresholds()` helper elimineert 3x herhaalde threshold initialisatie

### Geconsolideerde Clamp Operaties
- **Combineer clamp checks**: Meerdere gerelateerde clamp operaties kunnen geconsolideerd worden met if-else-if
- **Minder branches**: Snellere code execution, duidelijkere logica
- **Voorbeeld (VolatilityTracker)**: Clamp operaties voor volFactor zijn nu geconsolideerd in één if-else-if chain i.p.v. aparte if-statements

### Template Helper Functies voor Getter Patterns
- **Consolideer getter logica**: Herhaalde getter patterns (nullptr check + default waarde) kunnen geconsolideerd worden
- **Maak template helper functie**: Voor type-safe getter patterns met member pointers
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, type-safe
- **Voorbeeld (WarmStart)**: `getSettingValue()` template helper elimineert 5x herhaalde getter pattern

### Helper Functies voor Herhaalde Logging
- **Consolideer logging logica**: Herhaalde logging patterns kunnen geconsolideerd worden
- **Maak helper functie**: Voor complexe logging logica (timeframe status, etc.)
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag
- **Voorbeeld (WarmStart)**: `logTimeframeStatus()` helper elimineert 3x herhaalde logging code voor 5m/30m/2h timeframes

### Helper Functies voor Form Argument Parsing
- **Consolideer form parsing logica**: Herhaalde patterns voor form argument parsing kunnen geconsolideerd worden
- **Maak helper functies**: Voor float, int en string argument parsing met validatie
- **Voordeel**: Eén plek voor wijzigingen, minder code duplicatie, consistent gedrag, type-safe
- **Voorbeeld (WebServer)**: `parseFloatArg()`, `parseIntArg()`, `parseStringArg()` helpers elimineren ~50x herhaalde form parsing patterns

### Helper Functie Hergebruik tussen Modules
- **Hergebruik bestaande helpers**: Modules kunnen bestaande helper functies van andere modules gebruiken
- **Voordeel**: Minder code duplicatie, consistent gedrag tussen modules
- **Voorbeeld (WebServer)**: Hergebruikt `getTrendText()` en `getVolatilityText()` helpers i.p.v. eigen switch statements

