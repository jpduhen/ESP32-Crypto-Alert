# Fase 4: Nieuwe Strategie - Data Management Modules

**Datum:** 2025-12-18  
**Status:** Plan  
**Reden voor nieuwe strategie:** Eerdere poging was te complex en veroorzaakte crashes. Nieuwe aanpak in veel kleinere, geïsoleerde stapjes.

---

## Problemen met eerdere aanpak

1. **Te grote stapjes:** ApiClient en PriceData tegelijk geïntegreerd
2. **Te veel wijzigingen:** Directe array access overal vervangen in één keer
3. **Complexe dependencies:** PriceData had dynamische allocatie voor CYD platforms
4. **Moeilijk te testen:** Veel wijzigingen tegelijk maakten debugging lastig
5. **Crashes:** LoadProhibited errors door null pointers en memory issues

---

## Nieuwe strategie: Incrementele refactoring

### Principe
- **Één kleine wijziging per stap**
- **Test na elke stap**
- **Geen functionaliteit verliezen**
- **Geïsoleerde wijzigingen** (één functie of één array tegelijk)

---

## Fase 4.1: ApiClient Module (NIEUWE AANPAK)

### Stap 4.1.1: Maak ApiClient module structuur (geen integratie)
- [ ] Maak `src/ApiClient/ApiClient.h` met alleen class definitie
- [ ] Maak `src/ApiClient/ApiClient.cpp` met alleen constructor
- [ ] **Test:** Code compileert nog steeds
- **Doel:** Module structuur zonder functionaliteit

### Stap 4.1.2: Verplaats httpGET() naar ApiClient (parallel, niet vervangen)
- [ ] Kopieer `httpGET()` naar `ApiClient::httpGET()` als PRIVATE method
- [ ] Oude `httpGET()` blijft bestaan en werkt nog
- [ ] **Test:** Code compileert, oude functie werkt nog
- **Doel:** Code verplaatst maar niet gebruikt

### Stap 4.1.3: Test ApiClient::httpGET() parallel
- [ ] Voeg tijdelijke test code toe die beide versies test
- [ ] Verifieer dat ApiClient versie hetzelfde resultaat geeft
- [ ] **Test:** Beide versies werken identiek
- **Doel:** Verificatie dat verplaatsing correct is

### Stap 4.1.4: Vervang één httpGET() call
- [ ] Vervang `httpGET()` call in `fetchPrice()` met `apiClient.httpGET()`
- [ ] Maak ApiClient instance aan
- [ ] **Test:** fetchPrice() werkt nog steeds
- **Doel:** Eerste integratie, geïsoleerd

### Stap 4.1.5: Vervang alle httpGET() calls
- [ ] Vervang alle andere `httpGET()` calls één voor één
- [ ] Test na elke vervanging
- [ ] **Test:** Alle API calls werken nog
- **Doel:** Volledige migratie naar ApiClient

### Stap 4.1.6: Verplaats parsePrice() naar ApiClient
- [ ] Verplaats `parsePrice()` naar `ApiClient::parseBinancePrice()`
- [ ] Vervang calls één voor één
- [ ] **Test:** Parsing werkt nog
- **Doel:** Parse functionaliteit gemodulariseerd

### Stap 4.1.7: Verplaats fetchBinancePrice() logica
- [ ] Maak `ApiClient::fetchBinancePrice()` die httpGET + parse combineert
- [ ] Vervang in `fetchPrice()` één voor één
- [ ] **Test:** fetchPrice() werkt nog
- **Doel:** Hoog-niveau API functionaliteit

### Stap 4.1.8: Cleanup oude code
- [x] Verwijder oude `httpGET()` functie
- [x] Verwijder oude `parsePrice()` functie
- [x] **Test:** Alles werkt nog
- **Doel:** Oude code verwijderd
- **Status:** ✅ Voltooid

---

## Fase 4.2: PriceData Module (NIEUWE AANPAK)

### Stap 4.2.1: Maak PriceData module structuur
- [ ] Maak `src/PriceData/PriceData.h` met alleen class definitie en DataSource enum
- [ ] Maak `src/PriceData/PriceData.cpp` met alleen constructor
- [ ] **Test:** Code compileert nog steeds
- **Doel:** Module structuur zonder functionaliteit

### Stap 4.2.2: Verplaats array declaraties (parallel, niet vervangen)
- [ ] Voeg arrays toe aan PriceData als PRIVATE members
- [ ] Oude arrays blijven bestaan en werken nog
- [ ] **Test:** Code compileert, oude arrays werken nog
- **Doel:** Arrays verplaatst maar niet gebruikt

### Stap 4.2.3: Verplaats addPriceToSecondArray() (parallel)
- [ ] Kopieer `addPriceToSecondArray()` naar `PriceData::addPriceToSecondArray()`
- [ ] Oude functie blijft bestaan
- [ ] **Test:** Code compileert
- **Doel:** Functie verplaatst maar niet gebruikt

### Stap 4.2.4: Vervang één addPriceToSecondArray() call
- [ ] Vervang call in `fetchPrice()` met `priceData.addPriceToSecondArray()`
- [ ] Maak PriceData instance aan
- [ ] **Test:** fetchPrice() werkt nog, prijs wordt toegevoegd
- **Doel:** Eerste integratie, geïsoleerd

### Stap 4.2.5: Verplaats state variabelen (secondIndex, secondArrayFilled)
- [ ] Voeg state variabelen toe aan PriceData
- [ ] Update `addPriceToSecondArray()` om PriceData state te gebruiken
- [ ] **Test:** State wordt correct bijgehouden
- **Doel:** State gemodulariseerd

### Stap 4.2.6: Vervang directe array access in één functie
- [ ] Kies één functie die `secondPrices[]` gebruikt (bijv. `calculateReturn1Minute()`)
- [ ] Voeg getter toe aan PriceData: `getSecondPrices()`
- [ ] Vervang directe access met getter in die ene functie
- [ ] **Test:** Die functie werkt nog
- **Doel:** Eén functie gemigreerd

### Stap 4.2.7: Herhaal voor andere functies (incrementally)
- [x] Herhaal stap 4.2.6 voor elke functie die arrays gebruikt
- [x] Test na elke functie
- [x] **Doel:** Alle functies gemigreerd ✅

### Stap 4.2.8: Verplaats calculateReturn functies
- [x] Verplaats `calculateReturn1Minute()` naar PriceData
- [x] Vervang calls één voor één
- [x] **Test:** Returns worden correct berekend ✅
- **Doel:** Return berekeningen gemodulariseerd ✅

### Stap 4.2.9: Verplaats fiveMinutePrices en minuteAverages
- [x] Herhaal proces voor `fiveMinutePrices[]` en `minuteAverages[]`
- [x] Eén array per keer
- [x] **Test:** Elke array werkt nog ✅
- **Doel:** Alle arrays gemodulariseerd ✅

### Stap 4.2.10: Dynamische allocatie voor CYD (optioneel, later)
- [ ] Alleen als nodig voor geheugen optimalisatie
- [ ] Test grondig op CYD platform
- **Doel:** Geheugen optimalisatie
- **Status:** Optioneel, later te doen

### Stap 4.2.11: Cleanup oude code
- [x] Verwijder oude array declaraties
- [x] Verwijder oude functies
- [x] **Test:** Alles werkt nog ✅
- **Doel:** Oude code verwijderd ✅

---

## Belangrijke principes

1. **Één wijziging per stap:** Nooit meerdere dingen tegelijk
2. **Test na elke stap:** Compilatie + runtime test
3. **Parallel implementatie:** Nieuwe code werkt naast oude code
4. **Incrementele vervanging:** Eén call per keer vervangen
5. **Geen functionaliteit verliezen:** Oude code blijft werken tot nieuwe code bewezen werkt

---

## Test checklist per stap

Voor elke stap:
- [ ] Code compileert zonder errors
- [ ] Code compileert zonder warnings (of bestaande warnings)
- [ ] Device start op zonder crashes
- [ ] WiFi verbindt nog
- [ ] API calls werken nog
- [ ] Prijs data wordt correct opgehaald
- [ ] Returns worden correct berekend
- [ ] UI toont correcte data
- [ ] Geen memory leaks of crashes

---

## Tijdsinschatting

- **Fase 4.1 (ApiClient):** 8 stappen × ~15 minuten = ~2 uur
- **Fase 4.2 (PriceData):** 11 stappen × ~20 minuten = ~3.5 uur
- **Totaal:** ~5.5 uur (verspreid over meerdere sessies)

---

## Volgende stappen

1. Start met Fase 4.1.1: ApiClient module structuur
2. Werk stap voor stap door de lijst
3. Test grondig na elke stap
4. Documenteer eventuele problemen
5. Pas strategie aan indien nodig

---

**Laatste update:** 2025-12-18 - Fase 4.2 voltooid (alle stappen behalve 4.2.10 - optioneel)

