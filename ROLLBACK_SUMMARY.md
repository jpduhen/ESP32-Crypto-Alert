# Rollback Samenvatting - Crypto Monitor Refactoring

**Datum:** 2025-12-18  
**Reden:** Terugdraaien naar laatste werkende versie op GitHub na problemen met Fase 3 & 4

---

## ✅ Wat is al voltooid (behouden na rollback)

### Fase 1: Voorbereiding & Analyse
- ✅ **Stap 1.1: Code Analyse** - Voltooid 2025-12-17 22:35
  - Alle globale variabelen geïdentificeerd
  - Alle functies en dependencies gedocumenteerd
  - Dependency graph gemaakt
  - CODE_ANALYSIS.md aangemaakt
  
- ✅ **Stap 1.2: Test Baseline** - Voltooid 2025-12-17 22:36
  - Baseline gedocumenteerd

### Fase 2: Settings & Storage Module
- ✅ **Stap 2.1: SettingsStore Module** - Voltooid 2025-12-17 23:35
  - `src/SettingsStore/SettingsStore.h` en `.cpp` aangemaakt
  - Alle settings functionaliteit verplaatst
  - Integratie in hoofdbestand voltooid
  - Helper functie `generateDefaultNtfyTopic` toegevoegd
  
- ✅ **Stap 2.2: Cleanup Settings Code** - Voltooid 2025-12-17 23:50
  - Oude Preferences variabele verwijderd
  - Oude `getESP32DeviceId` functie verwijderd
  - Oude `base32Alphabet` constante verwijderd
  - Alle oude settings code opgeruimd

**Status na Fase 2:** Volledig werkende code met SettingsStore module geïntegreerd

---

## ❌ Wat is teruggedraaid (niet behouden)

### Fase 3: Network Modules
- ❌ **Stap 3.1: NetworkManager Module** - Gedeeltelijk gedaan, teruggedraaid
  - Module bestanden bestaan nog in `src/NetworkManager/` maar zijn niet geïntegreerd
  - Code gebruikt nog oude MQTT/WiFi implementatie

### Fase 4: Data Management Modules
- ❌ **Stap 4.1: ApiClient Module** - Gedeeltelijk gedaan, teruggedraaid
  - Module bestanden bestaan nog in `src/ApiClient/` maar zijn niet geïntegreerd
  - Code gebruikt nog oude `httpGET()` en `parsePrice()` functies
  
- ❌ **Stap 4.2: PriceData Module** - Gedeeltelijk gedaan, teruggedraaid
  - Module bestanden bestaan nog in `src/PriceData/` maar zijn niet geïntegreerd
  - Code gebruikt nog oude arrays en functies direct

---

## 🎯 Waar beginnen na rollback naar GitHub

### Huidige staat (na rollback)
- ✅ Fase 1: Volledig voltooid
- ✅ Fase 2: Volledig voltooid (SettingsStore module werkend)
- ❌ Fase 3: Niet gestart (of gedeeltelijk, maar niet geïntegreerd)
- ❌ Fase 4: Niet gestart (of gedeeltelijk, maar niet geïntegreerd)

### Volgende stap: Fase 3 - Network Modules

**Nieuwe strategie:** Begin met Fase 3, maar pas daarna Fase 4 aan in **veel kleinere stapjes** om crashes te voorkomen.

#### Stap 3.1: NetworkManager Module (opnieuw)
1. **Basis structuur** (kleine stap)
   - Maak `src/NetworkManager/NetworkManager.h` (basis definitie)
   - Maak `src/NetworkManager/NetworkManager.cpp` (lege implementatie)
   - Test: Code compileert nog

2. **MQTT basis** (kleine stap)
   - Verplaats alleen MQTT connectie logica
   - Test: MQTT werkt nog

3. **MQTT publishing** (kleine stap)
   - Verplaats MQTT publish functies
   - Test: MQTT publishing werkt nog

4. **WiFi management** (later, complex)
   - Blijft voorlopig in hoofdbestand

#### Stap 3.2: NtfyNotifier Module
- Overgeslagen (bestaande implementatie is voldoende)

#### Stap 3.3: Cleanup Network Code
- Pas doen na volledige integratie en testen

---

## 📋 Nieuwe strategie voor Fase 4 (in kleinere stapjes)

### Probleem met vorige aanpak
- Te grote stapjes veroorzaakten crashes
- Directe array access vervangen was te complex in één keer
- Memory allocatie problemen op CYD platforms

### Nieuwe aanpak: Incrementele integratie

#### Stap 4.1: ApiClient Module (opnieuw, kleinere stappen)

**4.1.1: Basis structuur**
- Maak alleen header en basis class
- Test: Code compileert

**4.1.2: httpGET functie**
- Verplaats alleen `httpGET()` naar ApiClient
- Vervang 1 call in `fetchPrice()`
- Test: API calls werken nog

**4.1.3: parsePrice functie**
- Verplaats `parsePrice()` naar ApiClient
- Vervang 1 call in `fetchPrice()`
- Test: Prijs parsing werkt nog

**4.1.4: fetchBinancePrice wrapper**
- Maak wrapper functie die httpGET + parsePrice combineert
- Vervang in `fetchPrice()`
- Test: Volledige API flow werkt nog

#### Stap 4.2: PriceData Module (opnieuw, veel kleinere stappen)

**4.2.1: Basis structuur**
- Maak alleen header met class definitie
- Test: Code compileert

**4.2.2: Array declaraties**
- Verplaats alleen array declaraties naar PriceData
- Behoud directe access in hoofdbestand (via getters)
- Test: Arrays werken nog

**4.2.3: State variabelen**
- Verplaats state variabelen (secondIndex, minuteIndex, etc.)
- Test: State tracking werkt nog

**4.2.4: addPriceToSecondArray functie**
- Verplaats functie naar PriceData
- Vervang 1 call in `fetchPrice()`
- Test: Prijs toevoegen werkt nog

**4.2.5: updateMinuteAverage functie**
- Verplaats functie naar PriceData
- Vervang 1 call in `fetchPrice()`
- Test: Minuut gemiddelde werkt nog

**4.2.6: calculateReturn1Minute functie**
- Verplaats functie naar PriceData
- Vervang calls één voor één
- Test na elke vervanging

**4.2.7: calculateReturn5Minutes functie**
- Verplaats functie naar PriceData
- Vervang calls één voor één
- Test na elke vervanging

**4.2.8: calculateReturn30Minutes functie**
- Verplaats functie naar PriceData
- Vervang calls één voor één
- Test na elke vervanging

**4.2.9: calculateReturn2Hours functie**
- Verplaats functie naar PriceData
- Vervang calls één voor één
- Test na elke vervanging

**4.2.10: Helper functies**
- Verplaats één voor één: findMinMaxInSecondPrices, findMinMaxInLast30Minutes, calcLivePctMinuteAverages
- Test na elke functie

**4.2.11: Directe array access vervangen**
- Vervang directe array access (secondPrices[], etc.) één locatie per keer
- Test na elke vervanging

**4.2.12: loadWarmStartData integratie**
- Integreer PriceData in loadWarmStartData
- Test: Warm-start werkt nog

**4.2.13: Dynamische allocatie (CYD platforms)**
- Implementeer dynamische allocatie alleen als alles werkt
- Test grondig op CYD platforms

---

## ✅ Checklist voor start na rollback

- [ ] Code staat op GitHub (laatste commit na Fase 2)
- [ ] Code compileert zonder errors
- [ ] Code werkt op test platform
- [ ] REFACTORING_STATUS.md bijgewerkt naar "Fase 2 voltooid"
- [ ] Nieuwe strategie documentatie gelezen
- [ ] Klaar om te beginnen met Fase 3.1 (kleine stapjes)

---

## 📝 Belangrijke lessen

1. **Kleine stapjes:** Elke stap moet testbaar zijn
2. **Test na elke stap:** Compileer en test functionaliteit
3. **Geen grote refactors:** Maximaal 1 functie of 1 call per stap
4. **Memory checks:** Extra aandacht voor CYD platforms zonder PSRAM
5. **Null pointer checks:** Altijd checken bij dynamische allocatie

---

**Laatste update:** 2025-12-18 (na rollback)



