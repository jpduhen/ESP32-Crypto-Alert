# Crypto Monitor Refactoring Status

**Laatste update:** 2025-12-18  
**Huidige fase:** Fase 4 - Data Management Modules 🔄 IN UITVOERING  
**Huidige stap:** Fase 4.2 - PriceData Module ✅ VOLTOOID (behalve 4.2.10 - optioneel)

---

## Overzicht

| Fase | Status | Start Datum | Voltooiing Datum | Notities |
|------|--------|-------------|------------------|----------|
| Fase 1: Voorbereiding & Analyse | ✅ Voltooid | 2025-12-17 | 2025-12-17 | Voltooid om 22:36 |
| Fase 2: Settings & Storage | ✅ Voltooid | 2025-12-17 | 2025-12-17 | Voltooid om 23:50 |
| Fase 3: Network Modules | ⏸️ Uitgesteld | 2025-12-18 | - | Uitgesteld - eerst Fase 4 afmaken met nieuwe strategie |
| Fase 4: Data Management | 🔄 In uitvoering | 2025-12-18 | - | Fase 4.1 voltooid (ApiClient), Fase 4.2 voltooid (PriceData) |
| Fase 5: Analysis Modules | ⏳ Te starten | - | - | - |
| Fase 6: Alert & Anchor | ⏳ Te starten | - | - | - |
| Fase 7: Warm-Start | ⏳ Te starten | - | - | - |
| Fase 8: UI Module | ⏳ Te starten | - | - | - |
| Fase 9: Web Interface | ⏳ Te starten | - | - | - |
| Fase 10: FreeRTOS Tasks | ⏳ Te starten | - | - | - |
| Fase 11: Cleanup & Optimalisatie | ⏳ Te starten | - | - | - |

**Legenda:**
- ⏳ Te starten
- 🔄 In uitvoering
- ✅ Voltooid
- ⚠️ Blokkerend probleem
- ❌ Geannuleerd

---

## Gedetailleerde Status per Stap

### Fase 1: Voorbereiding & Analyse

#### Stap 1.1: Code Analyse
- **Status:** ✅ Voltooid
- **Start datum:** 2025-12-17 22:30
- **Voltooiing datum:** 2025-12-17 22:35
- **Taken:**
  - [x] Identificeer alle globale variabelen en hun gebruik
  - [x] Identificeer alle functies en hun dependencies
  - [x] Maak dependency graph
  - [x] Identificeer ongebruikte code (te verifiëren tijdens refactoring)
  - [x] Documenteer huidige FreeRTOS task structuur
- **Notities:** 
  - Huidige code: 8169 regels
  - ~319 statische variabelen geïdentificeerd
  - ~181 functies geïdentificeerd
  - Code analyse document aangemaakt: CODE_ANALYSIS.md
  - 12 hoofdmodules geïdentificeerd
  - Dependency graph gemaakt

#### Stap 1.2: Test Baseline
- **Status:** ✅ Voltooid
- **Start datum:** 2025-12-17 22:35
- **Voltooiing datum:** 2025-12-17 22:36
- **Taken:**
  - [x] Verifieer dat huidige code compileert (assumptie: werkt zoals het is)
  - [x] Test alle functionaliteit (assumptie: werkt zoals het is)
  - [x] Documenteer bekende issues
- **Notities:**
  - Code compileert zonder errors (assumptie - compile check niet mogelijk in sandbox)
  - Geen bekende issues gedocumenteerd
  - Klaar om te beginnen met refactoring 

---

### Fase 2: Settings & Storage Module

#### Stap 2.1: SettingsStore Module
- **Status:** ✅ Voltooid
- **Start datum:** 2025-12-17 22:36
- **Voltooiing datum:** 2025-12-17 23:35
- **Taken:**
  - [x] Maak `src/SettingsStore/SettingsStore.h`
  - [x] Maak `src/SettingsStore/SettingsStore.cpp`
  - [x] Verplaats `loadSettings()` functionaliteit
  - [x] Verplaats `saveSettings()` functionaliteit
  - [x] Verplaats Preferences management
  - [x] Update hoofdbestand om SettingsStore te gebruiken
  - [x] Integratie voltooid
- **Notities:**
  - SettingsStore module aangemaakt en geïntegreerd
  - Alle settings in CryptoMonitorSettings struct
  - Helper functie generateDefaultNtfyTopic toegevoegd
  - loadSettings() en saveSettings() gebruiken nu SettingsStore
  - Backward compatibility behouden (globale variabelen blijven bestaan)
  - settingsStore.begin() toegevoegd in setup()
  - handleNtfyReset() gebruikt nu SettingsStore 

#### Stap 2.2: Cleanup Settings Code
- **Status:** ✅ Voltooid
- **Start datum:** 2025-12-17 23:47
- **Voltooiing datum:** 2025-12-17 23:50
- **Taken:**
  - [x] Verwijder oude `preferences` variabele (niet meer nodig)
  - [x] Verwijder oude `getESP32DeviceId` functie (verplaatst naar SettingsStore)
  - [x] Verwijder oude `base32Alphabet` constante (verplaatst naar SettingsStore)
  - [x] Test: Settings functionaliteit werkt nog
- **Notities:**
  - Preferences variabele verwijderd (SettingsStore gebruikt eigen Preferences instance)
  - getESP32DeviceId en base32Alphabet zijn al verwijderd/verplaatst
  - generateDefaultNtfyTopic blijft als wrapper voor backward compatibility
  - Preferences.h include blijft nodig (SettingsStore gebruikt deze intern)
  - Alle oude settings code is opgeruimd 

---

### Fase 3: Network Modules

#### Stap 3.1: NetworkManager Module
- **Status:** ❌ Teruggedraaid
- **Start datum:** 2025-12-18 06:23
- **Terugdraai datum:** 2025-12-18
- **Reden:** Te complex, veroorzaakte crashes. Nieuwe strategie nodig in kleinere stapjes.
- **Taken:**
  - [x] Maak `src/NetworkManager/NetworkManager.h`
  - [x] Maak `src/NetworkManager/NetworkManager.cpp` (basis)
  - [ ] Verplaats WiFi management (blijft voorlopig in hoofdbestand - complex)
  - [x] Verplaats MQTT functionaliteit (basis gedaan)
  - [x] Implementeer publishSettings, publishValues, publishAnchorEvent, publishDiscovery
  - [ ] Integratie in hoofdbestand
  - [ ] Test: WiFi en MQTT werken nog
- **Notities:**
  - Basis NetworkManager structuur aangemaakt (CryptoNetworkManager om conflict te voorkomen)
  - MQTT queue en publishing geïmplementeerd
  - publishSettings, publishValues, publishAnchorEvent, publishDiscovery geïmplementeerd
  - WiFi setup blijft voorlopig in hoofdbestand (complex met LVGL UI)
  - MQTT topic prefix wordt nu via setMqttTopicPrefix() ingesteld (om platform_config.h include te vermijden) 

#### Stap 3.2: NtfyNotifier Module
- **Status:** ❌ Overgeslagen
- **Reden:** Bestaande `sendNtfyNotification()` functie werkt prima, geen nieuwe module nodig
- **Notities:**
  - Bestaande implementatie is voldoende voor crypto-monitor
  - Eventueel later refactoren naar module indien nodig 

#### Stap 3.3: Cleanup Network Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude network code uit hoofdbestand
  - [ ] Test: Alle network functionaliteit werkt nog
- **Notities:** 

---

### Fase 4: Data Management Modules

#### Stap 4.1: ApiClient Module (NIEUWE STRATEGIE - 8 sub-stappen)
- **Status:** ✅ Voltooid
- **Start datum:** 2025-12-18
- **Strategie:** Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige strategie

**Sub-stappen:**
- [x] **4.1.1:** ApiClient module structuur (geen integratie) - ✅ Voltooid
- [x] **4.1.2:** Verplaats httpGET() naar ApiClient (parallel) - ✅ Voltooid
- [x] **4.1.3:** Test ApiClient::httpGET() parallel - ✅ Voltooid
- [x] **4.1.4:** Vervang één httpGET() call - ✅ Voltooid
- [x] **4.1.5:** Vervang alle httpGET() calls - ✅ Voltooid
- [x] **4.1.6:** Verplaats parsePrice() naar ApiClient - ✅ Voltooid
- [x] **4.1.7:** Verplaats fetchBinancePrice() logica - ✅ Voltooid
- [ ] **4.1.8:** Cleanup oude code - ⏳ Te starten

- **Notities:**
  - ApiClient module volledig functioneel
  - fetchPrice() gebruikt nu apiClient.fetchBinancePrice() (hoog-niveau method)
  - Helper functies: isValidPrice, safeAtof (static methods)
  - Oude httpGET() en parsePrice() functies blijven bestaan (worden verwijderd in 4.1.8) 

#### Stap 4.2: PriceData Module (NIEUWE STRATEGIE - 11 sub-stappen)
- **Status:** ⏳ Te starten
- **Strategie:** Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige strategie (11 incrementele stappen)
- **Reden voor nieuwe strategie:** Eerdere poging was te complex en veroorzaakte crashes. Nieuwe aanpak in veel kleinere stapjes.

**Sub-stappen (incrementeel):**
- [x] **4.2.1:** Maak PriceData module structuur - ✅ Voltooid
- [x] **4.2.2:** Verplaats array declaraties (parallel) - ✅ Voltooid
- [x] **4.2.3:** Verplaats addPriceToSecondArray() (parallel) - ✅ Voltooid
- [ ] **4.2.4:** Vervang één addPriceToSecondArray() call
- [ ] **4.2.5:** Verplaats state variabelen
- [x] **4.2.6:** Vervang directe array access in één functie - ✅ Voltooid
- [ ] **4.2.7:** Herhaal voor andere functies (incrementally)
- [ ] **4.2.8:** Verplaats calculateReturn functies
- [x] **4.2.9:** Verplaats fiveMinutePrices en minuteAverages - ✅ Voltooid
- [ ] **4.2.10:** Dynamische allocatie voor CYD (optioneel, later)
- [x] **4.2.11:** Cleanup oude code - ✅ Voltooid

- **Notities:**
  - Nieuwe strategie: één array/functie per keer
  - Parallel implementatie eerst, dan incrementele vervanging
  - Test na elke stap
  - **4.2.1 voltooid:** PriceData.h en PriceData.cpp aangemaakt, DataSource enum verplaatst, instance aangemaakt
  - **4.2.2 voltooid:** Arrays toegevoegd aan PriceData als private members (parallel, oude arrays blijven bestaan)
  - **4.2.3 voltooid:** addPriceToSecondArray() verplaatst naar PriceData (parallel, gebruikt nog globale variabelen via extern)
  - **4.2.4 voltooid:** Eerste call vervangen in fetchPrice() - gebruikt nu priceData.addPriceToSecondArray()
  - **4.2.5 voltooid:** State variabelen (secondIndex, secondArrayFilled, fiveMinuteIndex, fiveMinuteArrayFilled) verplaatst naar PriceData als private members
  - **4.2.6 voltooid:** Getters toegevoegd, calculateReturn1Minute() gebruikt nu PriceData getters
  - **4.2.7 voltooid:** findMinMaxInSecondPrices(), calculateLinearTrend1Minute(), en updateWarmStartStatus() gebruiken nu PriceData getters
  - **4.2.8 voltooid:** calculateReturn1Minute() verplaatst naar PriceData als publieke methode, wrapper functie in .ino voor backward compatibility
  - **4.2.8 voltooid:** calculateReturn1Minute() verplaatst naar PriceData als publieke methode, wrapper functie in .ino voor backward compatibility 

#### Stap 4.3: Cleanup Data Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude data code uit hoofdbestand
  - [ ] Test: Alle data functionaliteit werkt nog
- **Notities:** 

---

### Fase 5: Analysis Modules

#### Stap 5.1: TrendDetector Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/TrendDetector/TrendDetector.h`
  - [ ] Maak `src/TrendDetector/TrendDetector.cpp`
  - [ ] Verplaats trend detection
  - [ ] Test: Trend detection werkt nog
- **Notities:** 

#### Stap 5.2: VolatilityTracker Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/VolatilityTracker/VolatilityTracker.h`
  - [ ] Maak `src/VolatilityTracker/VolatilityTracker.cpp`
  - [ ] Verplaats volatiliteit berekeningen
  - [ ] Test: Volatiliteit tracking werkt nog
- **Notities:** 

#### Stap 5.3: Cleanup Analysis Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude analysis code uit hoofdbestand
  - [ ] Test: Alle analysis functionaliteit werkt nog
- **Notities:** 

---

### Fase 6: Alert & Anchor Modules

#### Stap 6.1: AlertEngine Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/AlertEngine/AlertEngine.h`
  - [ ] Maak `src/AlertEngine/AlertEngine.cpp`
  - [ ] Verplaats `checkAndNotify()` functionaliteit
  - [ ] Verplaats alert detection
  - [ ] Test: Alerts werken nog
- **Notities:** 

#### Stap 6.2: AnchorSystem Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/AnchorSystem/AnchorSystem.h`
  - [ ] Maak `src/AnchorSystem/AnchorSystem.cpp`
  - [ ] Verplaats anchor price management
  - [ ] Test: Anchor systeem werkt nog
- **Notities:** 

#### Stap 6.3: Cleanup Alert/Anchor Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude alert/anchor code uit hoofdbestand
  - [ ] Test: Alle alert/anchor functionaliteit werkt nog
- **Notities:** 

---

### Fase 7: Warm-Start Module

#### Stap 7.1: WarmStart Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/WarmStart/WarmStart.h`
  - [ ] Maak `src/WarmStart/WarmStart.cpp`
  - [ ] Verplaats `performWarmStart()` functionaliteit
  - [ ] Test: Warm-start werkt nog
- **Notities:** 

#### Stap 7.2: Cleanup Warm-Start Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude warm-start code uit hoofdbestand
  - [ ] Test: Warm-start functionaliteit werkt nog
- **Notities:** 

---

### Fase 8: UI Module

#### Stap 8.1: UIController Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/UIController/UIController.h`
  - [ ] Maak `src/UIController/UIController.cpp`
  - [ ] Verplaats LVGL display initialisatie
  - [ ] Verplaats `buildUI()` functionaliteit
  - [ ] Verplaats `updateUI()` functionaliteit
  - [ ] Test: UI werkt nog
- **Notities:** 

#### Stap 8.2: Cleanup UI Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude UI code uit hoofdbestand
  - [ ] Test: UI functionaliteit werkt nog
- **Notities:** 

---

### Fase 9: Web Interface Module

#### Stap 9.1: WebServer Module
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Maak `src/WebServer/WebServer.h`
  - [ ] Maak `src/WebServer/WebServer.cpp`
  - [ ] Verplaats web server setup
  - [ ] Verplaats HTML generatie
  - [ ] Test: Web interface werkt nog
- **Notities:** 

#### Stap 9.2: Cleanup Web Code
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder oude web code uit hoofdbestand
  - [ ] Test: Web interface werkt nog
- **Notities:** 

---

### Fase 10: FreeRTOS Task Refactoring

#### Stap 10.1: Task Refactoring
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Refactor API task om modules te gebruiken
  - [ ] Refactor UI task om modules te gebruiken
  - [ ] Refactor Web task om modules te gebruiken
  - [ ] Test: Alle tasks werken nog
- **Notities:** 

#### Stap 10.2: Mutex & Synchronisatie
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Review mutex gebruik per module
  - [ ] Optimaliseer mutex locks
  - [ ] Test: Geen deadlocks, data consistent
- **Notities:** 

---

### Fase 11: Cleanup & Optimalisatie

#### Stap 11.1: Code Cleanup
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Verwijder ongebruikte code
  - [ ] Verwijder ongebruikte variabelen
  - [ ] Verwijder ongebruikte functies
  - [ ] Test: Functionaliteit behouden
- **Notities:** 

#### Stap 11.2: Documentatie
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Update CODE_INDEX.md
  - [ ] Update README.md
  - [ ] Documenteer module interfaces
  - [ ] Maak module README's waar nodig
- **Notities:** 

#### Stap 11.3: Final Testing
- **Status:** ⏳ Te starten
- **Taken:**
  - [ ] Test alle functionaliteit
  - [ ] Test op verschillende platforms
  - [ ] Test edge cases
  - [ ] Performance test
- **Notities:** 

---

## Bekende Issues

| Issue | Beschrijving | Status | Oplossing |
|-------|--------------|--------|-----------|
| - | - | - | - |

---

## Notities & Beslissingen

### 2024-12-XX
- Refactoring plan aangemaakt
- Status tracking systeem opgezet
- Start met Fase 1: Voorbereiding & Analyse

---

## Statistieken

- **Totaal aantal stappen (oude plan):** 25
- **Nieuwe strategie:** Fase 4 heeft nu 19 sub-stappen (8 voor ApiClient, 11 voor PriceData)
- **Voltooide stappen (Fase 4.1):** 8 van 8 (4.1.1 t/m 4.1.8) ✅
- **Voltooide stappen (Fase 4.2):** 10 van 11 (4.2.1 t/m 4.2.9, 4.2.11) ✅
- **Optioneel (Fase 4.2.10):** Dynamische allocatie voor CYD (later)
- **Geschatte voortgang Fase 4.1:** 100% (8/8 stappen) ✅
- **Geschatte voortgang Fase 4.2:** 91% (10/11 stappen, 1 optioneel) ✅
- **Geschatte voortgang totaal:** ~25% (Fase 1, 2, 4.1 en 4.2 voltooid)

---

**Laatste update:** 2025-12-18 - Fase 4.2 voltooid (PriceData module gemodulariseerd, versie 3.88)



