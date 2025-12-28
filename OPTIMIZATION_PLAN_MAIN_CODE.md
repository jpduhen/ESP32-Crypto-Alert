# Optimalisatie Stappenplan: Hoofdcode (UNIFIED-LVGL9-Crypto_Monitor.ino)

**Versie:** 1.0  
**Datum:** 2025-01-XX  
**Doel:** Optimaliseren van de hoofdcode volgens MODULE_OPTIMIZATION_GUIDELINES.md

---

## Overzicht

De hoofdcode bevat ~5594 regels en heeft verschillende optimalisatie mogelijkheden:
- **215 functie/variabele definities**
- **507 if-statements** (mogelijk te consolideren)
- **24 String operaties** (mogelijk te vervangen door C-style)
- **49 static functies** (mogelijk te consolideren)
- **Code duplicatie** in helper functies, return calculations, min/max finding

---

## Fase 1: Analyse & Inventarisatie

### 1.1 Helper Functies Inventariseren
**Doel:** Identificeer alle helper functies en hun gebruik

**Te analyseren functies:**
- `calculateReturnGeneric()` - Generic return calculation
- `findMinMaxInSecondPrices()` - Min/max finding
- `findMinMaxInLast30Minutes()` - Min/max finding
- `findMinMaxInLast2Hours()` - Min/max finding (CYD only)
- `formatIPAddress()` - IP formatting
- `generateDefaultNtfyTopic()` - Topic generation
- `safeAtof()` - Safe float parsing
- `safeSecondsToMs()` - Time conversion
- `safeStrncpy()` - Safe string copy
- `isValidPrice()` - Price validation
- `areValidPrices()` - Multiple price validation
- `getLastWrittenIndex()` - Ring buffer helper
- `getRingBufferIndexAgo()` - Ring buffer helper
- `calcLivePctMinuteAverages()` - Live percentage calculation
- `calculateAverage()` - Average calculation
- `updateMinuteAverage()` - Minute average update
- `computeTwoHMetrics()` - 2-hour metrics

**Actie:**
- [ ] Maak lijst van alle helper functies
- [ ] Identificeer code duplicatie tussen helpers
- [ ] Identificeer herhaalde patterns

### 1.2 String Operaties Inventariseren
**Doel:** Identificeer alle String operaties die geoptimaliseerd kunnen worden

**Te analyseren:**
- `String` object creaties
- `.toCharArray()` calls
- `.trim()` calls
- `.length()` checks
- `.toInt()` / `.toFloat()` calls
- String concatenatie

**Actie:**
- [ ] Maak lijst van alle String operaties
- [ ] Identificeer welke vervangen kunnen worden door C-style
- [ ] Bepaal impact op geheugen en performance

### 1.3 Code Duplicatie Identificeren
**Doel:** Identificeer herhaalde code patterns

**Te analyseren:**
- Return calculation patterns
- Min/max finding patterns
- Validation patterns
- Error handling patterns
- Mutex lock/unlock patterns
- Early return patterns

**Actie:**
- [ ] Scan code voor herhaalde patterns
- [ ] Document duplicatie locaties
- [ ] Bepaal consolidatie mogelijkheden

---

## Fase 2: Helper Functie Optimalisaties

### 2.1 Geconsolideerde Min/Max Finding
**Doel:** Consolideer `findMinMaxInSecondPrices()`, `findMinMaxInLast30Minutes()`, `findMinMaxInLast2Hours()`

**Huidige situatie:**
- 3 aparte functies met vergelijkbare logica
- Herhaalde validatie en loop patterns

**Optimalisatie:**
- [ ] Maak generic `findMinMaxInArray()` helper
- [ ] Consolideer validatie logica
- [ ] Consolideer loop patterns
- [ ] Behoud backward compatibility met wrapper functies

**Verwachte winst:**
- ~50 regels code duplicatie geëlimineerd
- Betere onderhoudbaarheid

### 2.2 Geconsolideerde Return Calculations
**Doel:** Optimaliseer return calculation helpers

**Huidige situatie:**
- `calculateReturnGeneric()` bestaat al
- Wrapper functies voor backward compatibility
- Mogelijk nog optimalisaties mogelijk

**Optimalisatie:**
- [ ] Review `calculateReturnGeneric()` voor optimalisaties
- [ ] Consolideer validatie checks
- [ ] Consolideer early returns
- [ ] Optimaliseer ring buffer index berekeningen

**Verwachte winst:**
- Betere CPU performance
- Minder code duplicatie

### 2.3 Geconsolideerde Validatie Helpers
**Doel:** Consolideer validatie logica

**Huidige situatie:**
- `isValidPrice()` - Single price validation
- `areValidPrices()` - Multiple price validation
- Herhaalde validatie patterns in verschillende functies

**Optimalisatie:**
- [ ] Review validatie helpers
- [ ] Consolideer NaN/Inf checks
- [ ] Consolideer range checks
- [ ] Maak inline helpers waar mogelijk

**Verwachte winst:**
- Betere code organisatie
- Minder code duplicatie

### 2.4 Geconsolideerde Ring Buffer Helpers
**Doel:** Optimaliseer ring buffer index berekeningen

**Huidige situatie:**
- `getLastWrittenIndex()` - Last written index
- `getRingBufferIndexAgo()` - Index N positions ago
- Mogelijk herhaalde berekeningen

**Optimalisatie:**
- [ ] Review ring buffer helpers
- [ ] Consolideer index berekeningen
- [ ] Optimaliseer voor performance
- [ ] Maak inline helpers waar mogelijk

**Verwachte winst:**
- Betere CPU performance
- Minder code duplicatie

---

## Fase 3: String Optimalisaties

### 3.1 String naar C-style Conversies
**Doel:** Vervang String operaties door C-style waar mogelijk

**Te optimaliseren locaties:**
- WiFi connection code (`wifiConnectionAndFetchPrice()`)
- MQTT message handling
- Settings loading/saving
- Error messages

**Optimalisatie:**
- [ ] Identificeer String operaties die vervangen kunnen worden
- [ ] Vervang door C-style char arrays
- [ ] Gebruik `snprintf()` i.p.v. String concatenatie
- [ ] Test functionaliteit na conversie

**Verwachte winst:**
- Minder heap fragmentatie
- Betere geheugen efficiency
- Snellere performance

### 3.2 Buffer Sizing Optimalisatie
**Doel:** Minimaliseer buffer groottes waar mogelijk

**Te optimaliseren buffers:**
- `httpResponseBuffer[384]` - HTTP responses
- `notificationMsgBuffer[384]` - Notification messages
- `notificationTitleBuffer[96]` - Notification titles
- `binanceStreamBuffer[768]` - JSON parsing
- Lokale buffers in functies

**Optimalisatie:**
- [ ] Review buffer groottes
- [ ] Verklein waar mogelijk (zonder functionaliteit te verliezen)
- [ ] Document buffer sizing rationale

**Verwachte winst:**
- Minder DRAM gebruik
- Betere geheugen efficiency

---

## Fase 4: Code Duplicatie Eliminatie

### 4.1 Geconsolideerde Error Handling
**Doel:** Consolideer error handling patterns

**Huidige situatie:**
- Herhaalde error logging patterns
- Herhaalde mutex timeout handling
- Herhaalde validation error handling

**Optimalisatie:**
- [ ] Maak error handling helper functies
- [ ] Consolideer error logging
- [ ] Consolideer mutex timeout handling
- [ ] Consolideer validation error handling

**Verwachte winst:**
- Minder code duplicatie
- Betere code organisatie
- Consistent error handling

### 4.2 Geconsolideerde Mutex Patterns
**Doel:** Consolideer mutex lock/unlock patterns

**Huidige situatie:**
- `safeMutexTake()` / `safeMutexGive()` helpers bestaan al
- Mogelijk nog herhaalde patterns

**Optimalisatie:**
- [ ] Review mutex usage patterns
- [ ] Consolideer waar mogelijk
- [ ] Optimaliseer timeout handling

**Verwachte winst:**
- Betere code organisatie
- Minder code duplicatie

### 4.3 Geconsolideerde Early Returns
**Doel:** Consolideer early return patterns

**Huidige situatie:**
- Veel functies hebben early returns
- Mogelijk te consolideren checks

**Optimalisatie:**
- [ ] Review early return patterns
- [ ] Consolideer waar mogelijk
- [ ] Optimaliseer check volgorde

**Verwachte winst:**
- Betere CPU performance
- Minder code duplicatie

---

## Fase 5: CPU Optimalisaties

### 5.1 Geconsolideerde Berekeningen
**Doel:** Consolideer herhaalde berekeningen

**Te optimaliseren:**
- Ring buffer index berekeningen
- Percentage berekeningen
- Average berekeningen
- Return berekeningen

**Optimalisatie:**
- [ ] Identificeer herhaalde berekeningen
- [ ] Cache waar mogelijk
- [ ] Consolideer berekeningen

**Verwachte winst:**
- Betere CPU performance
- Minder code duplicatie

### 5.2 Geconsolideerde Loop Condities
**Doel:** Consolideer loop condities

**Te optimaliseren:**
- For loops met vergelijkbare condities
- While loops met vergelijkbare condities

**Optimalisatie:**
- [ ] Review loop condities
- [ ] Consolideer waar mogelijk
- [ ] Optimaliseer loop performance

**Verwachte winst:**
- Betere CPU performance
- Minder code duplicatie

---

## Fase 6: Stabiliteit Verbeteringen

### 6.1 Geconsolideerde Validatie
**Doel:** Verbeter validatie consistentie

**Te optimaliseren:**
- Price validatie
- Array index validatie
- Buffer size validatie
- Parameter validatie

**Optimalisatie:**
- [ ] Review validatie patterns
- [ ] Consolideer validatie logica
- [ ] Verbeter error messages
- [ ] Voeg NaN/Inf checks toe waar nodig

**Verwachte winst:**
- Betere stabiliteit
- Minder crashes
- Betere error handling

### 6.2 Geconsolideerde Error Logging
**Doel:** Verbeter error logging consistentie

**Te optimaliseren:**
- Error logging patterns
- Debug logging patterns
- Conditional logging

**Optimalisatie:**
- [ ] Review logging patterns
- [ ] Consolideer logging logica
- [ ] Verbeter error messages
- [ ] Gebruik conditional compilation waar nodig

**Verwachte winst:**
- Betere debugging
- Consistent logging
- Minder code duplicatie

---

## Fase 7: Testing & Validatie

### 7.1 Compile Test
**Doel:** Zorg dat code compileert zonder errors

**Actie:**
- [ ] Compile voor alle platforms (TTGO, CYD24, CYD28, ESP32-S3)
- [ ] Fix compile errors
- [ ] Fix warnings waar mogelijk

### 7.2 Functionaliteit Test
**Doel:** Zorg dat functionaliteit behouden blijft

**Actie:**
- [ ] Test alle core functionaliteit
- [ ] Test edge cases
- [ ] Test error handling
- [ ] Test performance

### 7.3 Geheugen Test
**Doel:** Zorg dat geheugen gebruik acceptabel is

**Actie:**
- [ ] Check DRAM usage
- [ ] Check heap usage
- [ ] Check stack usage
- [ ] Verify geen overflow

---

## Fase 8: Documentatie Update

### 8.1 Code Comments
**Doel:** Update code comments

**Actie:**
- [ ] Update functie comments
- [ ] Update inline comments
- [ ] Document optimalisaties

### 8.2 MODULE_OPTIMIZATION_GUIDELINES.md Update
**Doel:** Update guidelines met nieuwe lessen

**Actie:**
- [ ] Document nieuwe optimalisatie patterns
- [ ] Update voorbeelden
- [ ] Update checklist

---

## Prioriteit & Volgorde

### Hoge Prioriteit (Directe Impact)
1. **Fase 2.1**: Geconsolideerde Min/Max Finding (~50 regels duplicatie)
2. **Fase 3.1**: String naar C-style Conversies (geheugen & performance)
3. **Fase 4.1**: Geconsolideerde Error Handling (code organisatie)

### Medium Prioriteit (Significante Impact)
4. **Fase 2.2**: Geconsolideerde Return Calculations (CPU performance)
5. **Fase 4.2**: Geconsolideerde Mutex Patterns (code organisatie)
6. **Fase 5.1**: Geconsolideerde Berekeningen (CPU performance)

### Lage Prioriteit (Incrementele Verbetering)
7. **Fase 2.3**: Geconsolideerde Validatie Helpers
8. **Fase 2.4**: Geconsolideerde Ring Buffer Helpers
9. **Fase 3.2**: Buffer Sizing Optimalisatie
10. **Fase 4.3**: Geconsolideerde Early Returns
11. **Fase 5.2**: Geconsolideerde Loop Condities
12. **Fase 6.1**: Geconsolideerde Validatie
13. **Fase 6.2**: Geconsolideerde Error Logging

---

## Risico's & Aandachtspunten

### Risico's
1. **Backward Compatibility**: Zorg dat bestaande code blijft werken
2. **Functionaliteit**: Test alle functionaliteit na wijzigingen
3. **Performance**: Meet performance impact van wijzigingen
4. **Geheugen**: Monitor geheugen usage na wijzigingen

### Aandachtspunten
1. **Static Functies**: Let op static vs non-static functies
2. **Global Variables**: Let op globale variabelen die gebruikt worden
3. **Module Dependencies**: Let op dependencies tussen modules
4. **Platform Differences**: Let op platform-specifieke code

---

## Success Criteria

### Geheugen
- [ ] Geen DRAM overflow
- [ ] Geheugen usage binnen acceptabele limieten
- [ ] Minder heap fragmentatie

### Performance
- [ ] Geen performance degradatie
- [ ] Betere CPU efficiency waar mogelijk
- [ ] Snellere code execution waar mogelijk

### Code Kwaliteit
- [ ] Minder code duplicatie
- [ ] Betere code organisatie
- [ ] Betere onderhoudbaarheid
- [ ] Consistent patterns

### Stabiliteit
- [ ] Geen nieuwe bugs
- [ ] Betere error handling
- [ ] Betere validatie
- [ ] Betere logging

---

## Volgende Stappen

1. **Start met Fase 1**: Analyse & Inventarisatie
2. **Prioriteer optimalisaties**: Begin met hoge prioriteit items
3. **Test iteratief**: Test na elke optimalisatie
4. **Document wijzigingen**: Update documentatie tijdens optimalisatie
5. **Review & Refine**: Review optimalisaties en refine waar nodig

---

*Dit stappenplan is een levend document en zal worden bijgewerkt tijdens de optimalisatie proces.*

