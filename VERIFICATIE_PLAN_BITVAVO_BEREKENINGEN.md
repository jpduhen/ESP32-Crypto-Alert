# Verificatie Plan: Bitvavo API Berekeningen

## Doel
Verifiëren en verbeteren van de betrouwbaarheid, robuustheid en consistentie van:
- API data parsing (Binance API wordt gebruikt, niet Bitvavo)
- Min/max waarde berekeningen
- Gemiddelde berekeningen
- Return berekeningen
- UI weergave van waarden
- Notificatie waarden

---

## FASE 1: API Data Parsing Verificatie

### 1.1 Binance Price API Response Parsing
**Locatie:** `src/ApiClient/ApiClient.cpp` - `fetchBinancePrice()`

**Te verifiëren:**
- [ ] JSON response wordt correct geparsed
- [ ] Price waarde wordt correct geëxtraheerd uit `{"symbol":"BTCEUR","price":"12345.67"}`
- [ ] Float conversie is correct (geen precision loss)
- [ ] Error handling bij malformed JSON
- [ ] Validatie van price waarde (> 0, geen NaN/Inf)

**Acties:**
1. Log raw JSON response voor verificatie
2. Log geparsed price waarde
3. Test edge cases: lege response, invalid JSON, zeer grote/kleine waarden
4. Voeg validatie toe: `isValidPrice()` check direct na parsing

### 1.2 Binance Klines API Response Parsing
**Locatie:** `src/ApiClient/ApiClient.cpp` - `fetchBinanceKlines()`

**Te verifiëren:**
- [ ] Streaming JSON parsing werkt correct
- [ ] Close prices worden correct geëxtraheerd uit klines array
- [ ] Array bounds worden gerespecteerd
- [ ] Timestamp validatie (openTime)
- [ ] Data wordt correct gesorteerd (oudste eerst)

**Acties:**
1. Log aantal geparsed klines
2. Log eerste en laatste close price
3. Verifieer dat klines in chronologische volgorde zijn
4. Test met verschillende limit waarden (1, 2, 60, 300)

---

## FASE 2: Ring Buffer Logica Verificatie

### 2.1 SecondPrices Buffer (60 seconden)
**Locatie:** `src/PriceData/PriceData.cpp` - `addPriceToSecondArray()`

**Te verifiëren:**
- [ ] Wraparound logica werkt correct (index 59 → 0)
- [ ] `secondIndex` wordt correct geïncrementeerd
- [ ] `secondArrayFilled` flag wordt correct gezet
- [ ] Oude data wordt correct overschreven
- [ ] DataSource tracking werkt (SOURCE_LIVE vs SOURCE_BINANCE)

**Acties:**
1. Test wraparound scenario: vul 60 entries, voeg 61e toe
2. Verifieer dat index 0 wordt overschreven
3. Test met warm-start data: verifieer SOURCE_BINANCE markers
4. Log index en filled status bij elke update

### 2.2 FiveMinutePrices Buffer (300 seconden)
**Locatie:** `src/PriceData/PriceData.cpp` - `addPriceToSecondArray()`

**Te verifiëren:**
- [ ] Elke 5 minuten wordt gemiddelde correct berekend
- [ ] Gemiddelde wordt correct toegevoegd aan fiveMinutePrices
- [ ] Wraparound werkt correct (index 299 → 0)
- [ ] DataSource tracking werkt

**Acties:**
1. Test 5-minuut aggregatie: wacht 5 minuten, verifieer gemiddelde
2. Test wraparound na 300 entries
3. Verifieer dat gemiddelde correct is (sum/60)

### 2.3 MinuteAverages Buffer (120 minuten)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `updateMinuteAverage()`

**Te verifiëren:**
- [ ] Elke minuut wordt gemiddelde van 60 seconden correct berekend
- [ ] Gemiddelde wordt correct toegevoegd aan minuteAverages
- [ ] Wraparound werkt correct (index 119 → 0)
- [ ] `minuteIndex` en `minuteArrayFilled` worden correct bijgewerkt
- [ ] Bounds checking voorkomt array overflow

**Acties:**
1. Test minuut aggregatie: wacht 1 minuut, verifieer gemiddelde
2. Test wraparound na 120 minuten
3. Verifieer bounds checking (minuteIndex < MINUTES_FOR_30MIN_CALC)
4. Log minuteIndex en filled status bij elke update

---

## FASE 3: Min/Max Berekeningen Verificatie

### 3.1 findMinMaxInArray() Generic Function
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `findMinMaxInArray()`

**Te verifiëren:**
- [ ] Ring buffer mode werkt correct (backward iteration)
- [ ] Direct mode werkt correct (forward iteration)
- [ ] Invalid prices (0.0f, NaN, Inf) worden gefilterd
- [ ] Edge cases: lege array, alle invalid, één valid waarde
- [ ] `elementsToCheck` parameter wordt gerespecteerd

**Acties:**
1. Test met bekende data: [100, 200, 150, 300, 50] → min=50, max=300
2. Test met invalid data: [0.0f, 100.0f, 0.0f] → min=max=100
3. Test ring buffer wraparound: [..., 100, 200, 150] met index=2
4. Test met elementsToCheck limit: verifieer dat alleen laatste N worden gecheckt

### 3.2 findMinMaxInSecondPrices() (1m)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `findMinMaxInSecondPrices()`

**Te verifiëren:**
- [ ] Correcte min/max van laatste 60 seconden
- [ ] SOURCE_LIVE filtering werkt (alleen live data)
- [ ] Stopt bij eerste SOURCE_BINANCE entry
- [ ] Werkt correct met niet-gevulde array

**Acties:**
1. Vul array met bekende waarden, verifieer min/max
2. Test met warm-start data: verifieer dat SOURCE_BINANCE wordt overgeslagen
3. Test met gedeeltelijk gevulde array (bijv. 30 seconden)
4. Log alle waarden die worden gecheckt

### 3.3 findMinMaxInLast30Minutes() (30m)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `findMinMaxInLast30Minutes()`

**Te verifiëren:**
- [ ] Correcte min/max van laatste 30 minuten
- [ ] SOURCE_LIVE filtering werkt
- [ ] Stopt bij eerste SOURCE_BINANCE entry
- [ ] Vereist minimaal 2 SOURCE_LIVE entries

**Acties:**
1. Vul minuteAverages met bekende waarden, verifieer min/max
2. Test met warm-start data: verifieer filtering
3. Test met < 2 SOURCE_LIVE entries: verifieer dat 0.0f wordt geretourneerd
4. Log count van SOURCE_LIVE entries

### 3.4 findMinMaxInLast2Hours() (2h)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `findMinMaxInLast2Hours()`

**Te verifiëren:**
- [ ] Correcte min/max van laatste 120 minuten
- [ ] SOURCE_LIVE filtering werkt
- [ ] Stopt bij eerste SOURCE_BINANCE entry
- [ ] Vereist minimaal 2 SOURCE_LIVE entries

**Acties:**
1. Vul minuteAverages met bekende waarden, verifieer min/max
2. Test met warm-start data: verifieer filtering
3. Test met < 2 SOURCE_LIVE entries: verifieer dat 0.0f wordt geretourneerd
4. Log count van SOURCE_LIVE entries

---

## FASE 4: Gemiddelde Berekeningen Verificatie

### 4.1 calculateAverage() Generic Function
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateAverage()`

**Te verifiëren:**
- [ ] Correcte berekening: sum / count
- [ ] Invalid prices worden gefilterd
- [ ] Edge cases: lege array, alle invalid, één valid waarde
- [ ] Werkt correct met ring buffer (backward iteration)

**Acties:**
1. Test met bekende data: [100, 200, 300] → avg=200
2. Test met invalid data: [0.0f, 100.0f, 0.0f] → avg=100
3. Test ring buffer: verifieer backward iteration
4. Log sum en count voor verificatie

### 4.2 updateMinuteAverage() (1 minuut gemiddelde)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `updateMinuteAverage()`

**Te verifiëren:**
- [ ] Gemiddelde van 60 seconden wordt correct berekend
- [ ] Validatie voorkomt invalid waarden (NaN, Inf, <= 0)
- [ ] Gemiddelde wordt correct opgeslagen in minuteAverages
- [ ] DataSource wordt correct gezet (SOURCE_LIVE)

**Acties:**
1. Vul secondPrices met bekende waarden, verifieer gemiddelde
2. Test met invalid data: verifieer dat update wordt overgeslagen
3. Verifieer dat minuteAvg correct is: sum(60 seconden) / 60
4. Log minuteAvg waarde bij elke update

### 4.3 calculateReturn30Minutes() Average
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateLinearTrend30Minutes()`

**Te verifiëren:**
- [ ] Gemiddelde van laatste 30 minuten wordt correct berekend
- [ ] Linear trend berekening is correct
- [ ] averagePrices[2] wordt correct gezet

**Acties:**
1. Vul minuteAverages met bekende waarden, verifieer gemiddelde
2. Test linear trend: verifieer slope berekening
3. Log last30Avg en slope waarden

### 4.4 calculateReturn2Hours() Average
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturn2Hours()`

**Te verifiëren:**
- [ ] Gemiddelde van laatste 120 minuten wordt correct berekend (CYD platforms)
- [ ] averagePrices[3] wordt correct gezet
- [ ] Werkt correct met < 120 minuten beschikbaar

**Acties:**
1. Vul minuteAverages met bekende waarden, verifieer gemiddelde
2. Test met < 120 minuten: verifieer dat beschikbare minuten worden gebruikt
3. Log last120Sum en last120Count

---

## FASE 5: Return Berekeningen Verificatie

### 5.1 calculateReturnGeneric() Function
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturnGeneric()`

**Te verifiëren:**
- [ ] Return formule is correct: `(priceNow - priceXAgo) / priceXAgo * 100`
- [ ] Ring buffer index berekening is correct
- [ ] Price validatie voorkomt invalid returns
- [ ] Edge cases: priceXAgo = 0, priceNow = priceXAgo

**Acties:**
1. Test met bekende waarden: priceNow=110, priceXAgo=100 → return=10%
2. Test met negatieve return: priceNow=90, priceXAgo=100 → return=-10%
3. Test ring buffer: verifieer correcte index berekening
4. Log priceNow, priceXAgo, en return waarde

### 5.2 calculateReturn1Minute() (1m return)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturn1Minute()`

**Te verifiëren:**
- [ ] Vergelijkt huidige prijs met prijs 60 seconden geleden
- [ ] Werkt correct met ring buffer wraparound
- [ ] Retourneert 0.0f als niet genoeg data

**Acties:**
1. Test met bekende secondPrices array
2. Verifieer dat index 60 posities terug wordt gebruikt
3. Test wraparound scenario
4. Log ret_1m waarde

### 5.3 calculateReturn5Minutes() (5m return)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturn5Minutes()`

**Te verifiëren:**
- [ ] Vergelijkt huidige prijs met prijs 5 minuten geleden
- [ ] Gebruikt fiveMinutePrices array correct
- [ ] Werkt correct met ring buffer

**Acties:**
1. Test met bekende fiveMinutePrices array
2. Verifieer dat index 5 posities terug wordt gebruikt
3. Test wraparound scenario
4. Log ret_5m waarde

### 5.4 calculateReturn30Minutes() (30m return)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturn30Minutes()`

**Te verifiëren:**
- [ ] Vergelijkt laatste minuut gemiddelde met gemiddelde 30 minuten geleden
- [ ] Gebruikt minuteAverages array correct
- [ ] Werkt correct met ring buffer
- [ ] Vereist minimaal 30 minuten data

**Acties:**
1. Test met bekende minuteAverages array
2. Verifieer dat index 30 posities terug wordt gebruikt
3. Test met < 30 minuten: verifieer dat 0.0f wordt geretourneerd
4. Log ret_30m waarde

### 5.5 calculateReturn2Hours() (2h return)
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `calculateReturn2Hours()`

**Te verifiëren:**
- [ ] Vergelijkt laatste minuut gemiddelde met gemiddelde 120 minuten geleden
- [ ] Gebruikt minuteAverages array correct
- [ ] Werkt correct met < 120 minuten (gebruikt beschikbare data)
- [ ] Vereist minimaal 2 minuten data

**Acties:**
1. Test met bekende minuteAverages array
2. Verifieer dat index 120 posities terug wordt gebruikt (of beschikbare minuten)
3. Test met < 120 minuten: verifieer dat beschikbare data wordt gebruikt
4. Log ret_2h waarde

---

## FASE 6: Warm-Start Integratie Verificatie

### 6.1 Warm-Start Data Loading
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `performWarmStart()`

**Te verifiëren:**
- [ ] Historische data wordt correct opgehaald van Binance API
- [ ] Data wordt correct geladen in arrays (secondPrices, fiveMinutePrices, minuteAverages)
- [ ] DataSource wordt correct gezet (SOURCE_BINANCE)
- [ ] Arrays worden correct geïnitialiseerd (index, filled flags)

**Acties:**
1. Test warm-start: verifieer aantal opgehaalde klines
2. Verifieer dat data correct wordt geladen in arrays
3. Verifieer DataSource markers
4. Log warm-start statistieken

### 6.2 Live Data Mixing
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `fetchPrice()`

**Te verifiëren:**
- [ ] Live data wordt correct toegevoegd na warm-start
- [ ] SOURCE_LIVE markers worden correct gezet
- [ ] Oude SOURCE_BINANCE data wordt correct overschreven
- [ ] Min/max berekeningen filteren SOURCE_BINANCE correct

**Acties:**
1. Test na warm-start: voeg live data toe, verifieer SOURCE_LIVE markers
2. Verifieer dat SOURCE_BINANCE data wordt overschreven
3. Test min/max: verifieer dat alleen SOURCE_LIVE wordt gebruikt
4. Log DataSource transitions

---

## FASE 7: UI Weergave Verificatie

### 7.1 Min/Max Labels
**Locatie:** `src/UIController/UIController.cpp` - `updateMinMaxLabels()`

**Te verifiëren:**
- [ ] Min/max waarden worden correct weergegeven in UI
- [ ] Formatting is correct (2 decimalen)
- [ ] "0.00" wordt getoond als geen data beschikbaar
- [ ] Waarden worden alleen geüpdatet bij wijziging

**Acties:**
1. Test UI update: verifieer dat min/max labels correct worden geüpdatet
2. Test met geen data: verifieer "0.00" weergave
3. Test formatting: verifieer 2 decimalen
4. Log label updates

### 7.2 Average Labels
**Locatie:** `src/UIController/UIController.cpp` - `updateAveragePriceCard()`

**Te verifiëren:**
- [ ] Gemiddelde waarden worden correct weergegeven
- [ ] Formatting is correct
- [ ] Waarden worden alleen geüpdatet bij wijziging

**Acties:**
1. Test UI update: verifieer dat average labels correct worden geüpdatet
2. Test formatting: verifieer 2 decimalen
3. Log label updates

---

## FASE 8: Notificatie Waarden Verificatie

### 8.1 Alert Notification Values
**Locatie:** `src/AlertEngine/AlertEngine.cpp` - `formatNotificationMessage()`

**Te verifiëren:**
- [ ] Min/max waarden in notificaties zijn correct
- [ ] Return waarden zijn correct
- [ ] Formatting is consistent met UI

**Acties:**
1. Test notificatie: verifieer min/max waarden
2. Verifieer return waarden
3. Test formatting: verifieer consistentie met UI
4. Log notificatie waarden

---

## Implementatie Strategie

### Stap 1: Debug Logging Toevoegen
- Voeg uitgebreide logging toe aan alle berekeningen
- Log input waarden, tussenresultaten, en output waarden
- Maak logging conditioneel (DEBUG_CALCULATIONS flag)

### Stap 2: Unit Test Functies
- Maak test functies met bekende input/output
- Test edge cases systematisch
- Verifieer resultaten met handmatige berekeningen

### Stap 3: Validatie Verbeteren
- Voeg extra validatie toe aan alle berekeningen
- Voeg bounds checking toe waar nodig
- Voeg error recovery toe

### Stap 4: Code Refactoring
- Consolideer duplicatie code
- Verbeter code leesbaarheid
- Document complexe logica

### Stap 5: Testing & Verificatie
- Test op hardware (CYD, TTGO)
- Verifieer waarden met externe bronnen (Binance website)
- Monitor voor inconsistenties

---

## Prioriteit

**Hoge Prioriteit:**
1. FASE 1: API Data Parsing (basis voor alles)
2. FASE 3: Min/Max Berekeningen (veel gebruikt in UI)
3. FASE 4: Gemiddelde Berekeningen (basis voor returns)

**Middel Prioriteit:**
4. FASE 2: Ring Buffer Logica (kritiek voor data integriteit)
5. FASE 5: Return Berekeningen (gebruikt in notificaties)

**Lage Prioriteit:**
6. FASE 6: Warm-Start Integratie (opstart alleen)
7. FASE 7: UI Weergave (cosmetisch)
8. FASE 8: Notificatie Waarden (afhankelijk van andere fases)

---

## Success Criteria

- [ ] Alle berekeningen produceren correcte waarden
- [ ] Edge cases worden correct afgehandeld
- [ ] Geen crashes of invalid waarden in UI
- [ ] Notificaties bevatten correcte waarden
- [ ] Code is goed gedocumenteerd en getest
- [ ] Performance impact is minimaal
