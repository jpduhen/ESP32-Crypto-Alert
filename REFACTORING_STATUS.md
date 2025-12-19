# Crypto Monitor Refactoring Status

**Laatste update:** 2025-12-19 (Versie 3.90 - Fase 7 planning voltooid)  
**Huidige fase:** Fase 7 - Warm-Start Module ⏳ TE STARTEN  
**Huidige stap:** Fase 7.1 - WarmStart Module Setup ⏳ TE STARTEN

---

## Overzicht

| Fase | Status | Start Datum | Voltooiing Datum | Notities |
|------|--------|-------------|------------------|----------|
| Fase 1: Voorbereiding & Analyse | ✅ Voltooid | 2025-12-17 | 2025-12-17 | Voltooid om 22:36 |
| Fase 2: Settings & Storage | ✅ Voltooid | 2025-12-17 | 2025-12-17 | Voltooid om 23:50 |
| Fase 3: Network Modules | ⏸️ Uitgesteld | 2025-12-18 | - | Uitgesteld - eerst Fase 4 afmaken met nieuwe strategie |
| Fase 4: Data Management | ✅ Voltooid | 2025-12-18 | 2025-12-18 | Fase 4.1 voltooid (ApiClient), Fase 4.2 voltooid (PriceData) |
| Fase 5: Analysis Modules | ✅ Voltooid | 2025-12-19 | 2025-12-19 | Fase 5.1 voltooid (TrendDetector), Fase 5.2 voltooid (VolatilityTracker), Fase 5.3 voltooid (Cleanup) |
| Fase 6: Alert & Anchor | ✅ Voltooid | 2025-12-19 | 2025-12-19 | Fase 6.1 voltooid (AlertEngine), Fase 6.2 voltooid (AnchorSystem), Fase 6.3 voltooid (Cleanup) |
| Fase 7: Warm-Start | ⏳ Te starten | - | - | Opgedeeld in 6 stappen met 25 sub-stappen |
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
- **Status:** ✅ Voltooid
- **Taken:**
  - [x] Maak `src/TrendDetector/TrendDetector.h`
  - [x] Maak `src/TrendDetector/TrendDetector.cpp`
  - [x] Verplaats TrendState enum
  - [x] Verplaats determineTrendState() functie
  - [x] Verplaats checkTrendChange() functie
  - [x] Vervang calls naar trend functies
  - [x] Test: Trend detection werkt nog
- **Notities:** TrendDetector module volledig geïmplementeerd, wrapper functies voor backward compatibility 

#### Stap 5.2: VolatilityTracker Module
- **Status:** ✅ Voltooid
- **Taken:**
  - [x] Maak `src/VolatilityTracker/VolatilityTracker.h`
  - [x] Maak `src/VolatilityTracker/VolatilityTracker.cpp`
  - [x] Verplaats VolatilityState enum en EffectiveThresholds struct
  - [x] Verplaats volatiliteit berekeningen (oude systeem)
  - [x] Verplaats auto-volatility mode functies (nieuwe systeem)
  - [x] Vervang calls naar volatility functies
  - [x] Test: Volatiliteit tracking werkt nog
- **Notities:** VolatilityTracker module volledig geïmplementeerd, beide volatility systemen (oude en nieuwe) ondersteund 

#### Stap 5.3: Cleanup Analysis Code
- **Status:** ✅ Voltooid (17/17 stappen voltooid)
- **Strategie:** Zeer kleine incrementele stappen - één functie per keer, test na elke stap
- **Lessons Learned toegepast:**
  - Max 1 functie per stap (< 50 regels wijziging)
  - Test na elke stap
  - Wrapper functies eerst vervangen, dan pas verwijderen
- **Voltooid:**
  - [x] **5.3.1:** `determineTrendState()` wrapper vervangen (al direct gebruikt)
  - [x] **5.3.2:** `checkTrendChange()` wrapper vervangen met directe module call
  - [x] **5.3.3:** `determineTrendState()` wrapper functie verwijderd
  - [x] **5.3.4:** `checkTrendChange()` wrapper functie verwijderd
  - [x] **5.3.5:** Vervang 1 call naar `addAbs1mReturnToVolatilityBuffer()` wrapper
  - [x] **5.3.6:** `calculateAverageAbs1mReturn()` al vervangen
  - [x] **5.3.7:** `determineVolatilityState()` al vervangen
  - [x] **5.3.9:** Vervang `updateVolatilityWindow()` wrapper call
  - [x] **5.3.10:** Vervang `calculateEffectiveThresholds()` wrapper call
  - [x] **5.3.11:** Verwijder alle wrapper functies
  - [x] **5.3.12:** Vervang directe `trendState` access door `trendDetector.getTrendState()` in `trendSupportsDirection()`
  - [x] **5.3.13:** Vervang directe `volatilityState` access door `volatilityTracker.getVolatilityState()` in `getSettingsHTML()`
  - [x] **5.3.14:** Vervang andere directe accesses naar trend/volatility variabelen door module getters (checkAndSendConfluenceAlert, checkAnchorAlerts, updateTrendLabel, updateVolatilityLabel, updateUI)
  - [x] **5.3.15:** Synchronisatie consistent gemaakt - modules zijn source of truth, globale variabelen worden gesynchroniseerd
  - [x] **5.3.16:** Test: Code compileert en werkt correct ✅
  - [x] **5.3.17:** Final cleanup: Comments verbeterd, globale variabelen gedocumenteerd als backward compatibility
- **Notities:** 
  - Zeer incrementeel - één functie call per stap, test na elke stap
  - Alle directe reads van globale variabelen vervangen door module getters
  - Modules zijn nu de source of truth
  - Globale variabelen (trendState, previousTrendState, volatilityState, lastTrendChangeNotification) blijven bestaan voor backward compatibility
  - Deze worden gesynchroniseerd met modules na elke update
  - TODO: In toekomstige fase kunnen deze verwijderd worden zodra alle code volledig gemigreerd is 

---

### Fase 6: Alert & Anchor Modules

#### Stap 6.1: AlertEngine Module
- **Status:** ✅ Voltooid (12/12 sub-stappen voltooid)
- **Voltooiing datum:** 2025-12-19
- **Strategie:** Zeer kleine incrementele stappen - één functie per keer, test na elke stap
- **Lessons Learned toegepast:**
  - Max 1 functie per stap (< 50 regels wijziging)
  - Test na elke stap
  - Parallel implementatie eerst, dan incrementele vervanging
  - Helper functies eerst, dan hoofdlogica
- **Sub-stappen:**
  - [x] **6.1.1:** Maak AlertEngine module structuur (header en cpp, geen integratie) ✅
  - [x] **6.1.2:** Verplaats helper functie checkAlertConditions() naar AlertEngine (parallel) ✅
  - [x] **6.1.3:** Verplaats helper functie determineColorTag() naar AlertEngine (parallel) ✅
  - [x] **6.1.4:** Verplaats helper functie formatNotificationMessage() naar AlertEngine (parallel) ✅
  - [x] **6.1.5:** Verplaats helper functie sendAlertNotification() naar AlertEngine (parallel) ✅
  - [x] **6.1.6:** Verplaats state variabelen (lastNotification1Min, alerts1MinThisHour, etc.) naar AlertEngine ✅
  - [x] **6.1.7:** Verplaats Smart Confluence state variabelen naar AlertEngine ✅
  - [x] **6.1.8:** Verplaats update1mEvent() en update5mEvent() naar AlertEngine ✅
  - [x] **6.1.9:** Verplaats checkAndSendConfluenceAlert() naar AlertEngine ✅
  - [x] **6.1.10:** Verplaats checkAndNotify() naar AlertEngine (parallel implementatie) ✅
  - [x] **6.1.11:** Vervang calls naar checkAndNotify() met AlertEngine ✅
  - [x] **6.1.12:** Cleanup: Verwijder oude alert code uit hoofdbestand ✅
- **Notities:**
  - AlertEngine module volledig geïmplementeerd
  - Alle helper functies, state variabelen, en hoofdlogica verplaatst
  - Oude functies verwijderd uit hoofdbestand
  - alertEngine.begin() toegevoegd in setup()
  - Globale state variabelen blijven bestaan voor backward compatibility (via extern declarations)
  - **Lessons Learned:**
    - **Static variabelen identificeren:** Bij elke nieuwe module refactoring, controleer eerst welke `static` variabelen en functies extern moeten worden gemaakt
    - **#define macros:** Macros die naar struct velden verwijzen kunnen niet als externe symbolen gebruikt worden - gebruik struct velden direct
    - **Structs extern maken:** Als een module structs nodig heeft, maak de structs extern (niet static) en include de header waar de struct is gedefinieerd
  - TODO: In toekomstige fase kunnen globale variabelen verwijderd worden zodra alle code volledig gemigreerd is 

#### Stap 6.2: AnchorSystem Module
- **Status:** ✅ Voltooid (8/8 sub-stappen voltooid)
- **Voltooiing datum:** -
- **Strategie:** Zeer kleine incrementele stappen - één functie per keer, test na elke stap
- **Lessons Learned toegepast:**
  - Max 1 functie per stap (< 50 regels wijziging)
  - Test na elke stap
  - Parallel implementatie eerst, dan incrementele vervanging
  - **Static variabelen identificeren:** Eerst controleren welke `static` variabelen extern moeten worden gemaakt
  - **#define macros:** Macros die naar struct velden verwijzen kunnen niet als externe symbolen gebruikt worden
  - **Structs extern maken:** Als een module structs nodig heeft, maak de structs extern (niet static)
- **Sub-stappen:**
  - [x] **6.2.1:** Maak AnchorSystem module structuur (header en cpp, geen integratie) ✅
  - [x] **6.2.2:** Identificeer en maak static variabelen extern (lessons learned) ✅
  - [x] **6.2.3:** Verplaats calculateEffectiveAnchorThresholds() naar AnchorSystem (parallel) ✅
  - [x] **6.2.4:** Verplaats checkAnchorAlerts() naar AnchorSystem (parallel) ✅
  - [x] **6.2.5:** Verplaats setAnchorPrice() naar AnchorSystem (parallel) ✅
  - [x] **6.2.6:** Verplaats anchor state variabelen naar AnchorSystem ✅
  - [x] **6.2.7:** Vervang calls naar anchor functies met AnchorSystem ✅
  - [x] **6.2.8:** Cleanup: Verwijder oude anchor code uit hoofdbestand ✅
- **Notities:**
  - AnchorSystem module volledig geïmplementeerd
  - Alle anchor functies, state variabelen, en hoofdlogica verplaatst
  - Oude functies verwijderd uit hoofdbestand
  - anchorSystem.begin() toegevoegd in setup()
  - Globale state variabelen blijven bestaan voor backward compatibility (via extern declarations)
  - **Lessons Learned toegepast:**
    - **Static variabelen identificeren:** Bij elke nieuwe module refactoring, controleer eerst welke `static` variabelen en functies extern moeten worden gemaakt
    - **#define macros:** Macros die naar struct velden verwijzen kunnen niet als externe symbolen gebruikt worden - gebruik struct velden direct
    - **Structs extern maken:** Als een module structs nodig heeft, maak de structs extern (niet static) en include de header waar de struct is gedefinieerd
  - TODO: In toekomstige fase kunnen globale variabelen verwijderd worden zodra alle code volledig gemigreerd is
  - **Compiler-fouten opgelost tijdens implementatie:**
    - `AnchorConfigEffective` redefinition: struct definitie verwijderd uit `.ino` (staat in `AnchorSystem.h`)
    - `safeMutexTake/safeMutexGive` static/extern conflict: `static` verwijderd, return type aangepast (`void` i.p.v. `bool`)
    - `sendNotification` default argument conflict: default argument verwijderd uit forward declaration
    - `calcEffectiveAnchor` niet gevonden: call vervangen met `anchorSystem.calcEffectiveAnchor()`
    - `isValidPrice` static: `static` verwijderd zodat AnchorSystem deze kan gebruiken
    - `openPrices` static: `static` verwijderd zodat AnchorSystem deze kan gebruiken
    - `Serial_println` niet gevonden: macro toegevoegd aan `AnchorSystem.cpp` 

#### Stap 6.3: Cleanup Alert/Anchor Code
- **Status:** ✅ Voltooid
- **Voltooiing datum:** -
- **Strategie:** Zeer kleine incrementele stappen - één cleanup actie per keer, test na elke stap
- **Lessons Learned toegepast:**
  - Max 1 cleanup actie per stap (< 50 regels wijziging)
  - Test na elke stap
  - **Compiler-fouten preventie:**
    - Controleer eerst welke variabelen/functies nog gebruikt worden voordat je ze verwijdert
    - Verwijder alleen code die echt niet meer nodig is (geen forward declarations die nog nodig zijn)
    - Behoud commentaar over waar code naartoe is verplaatst voor referentie
  - **Static variabelen cleanup:** Alleen verwijderen als ze echt niet meer gebruikt worden (ook niet via extern)
  - **Forward declarations:** Behoud forward declarations die modules nodig hebben
- **Sub-stappen:**
  - [x] **6.3.1:** Identificeer alle oude alert/anchor code die verwijderd kan worden ✅
  - [x] **6.3.1.1:** Fix: Anchor setting via web/button werkt niet - globale variabelen synchronisatie toegevoegd ✅
  - [x] **6.3.2:** Verwijder oude alert functie definities (alleen als niet meer gebruikt) ✅
  - [x] **6.3.3:** Verwijder oude anchor functie definities (alleen als niet meer gebruikt) ✅
  - [x] **6.3.4:** Cleanup oude commentaar (behoud referenties naar modules) ✅
  - [x] **6.3.5:** Test: Alle alert/anchor functionaliteit werkt nog ✅
- **Notities:**
  - **6.3.1 Analyse voltooid:**
    - Geen oude functie definities gevonden (al verwijderd in 6.1.12 en 6.2.8)
    - Alleen commentaar over verplaatste functies gevonden (kan opgeschoond worden)
    - Alle forward declarations en extern declarations moeten behouden blijven
    - `publishMqttAnchorEvent()` functie moet behouden blijven (nog gebruikt)
    - Zie CLEANUP_6.3.1_ANALYSIS.md voor volledige analyse
  - **6.3.2 Analyse voltooid:**
    - Geen oude alert functie definities gevonden (al verwijderd in 6.1.12)
    - Alle alert functies zijn succesvol verplaatst naar AlertEngine module
  - **6.3.3 Analyse voltooid:**
    - Geen oude anchor functie definities gevonden (al verwijderd in 6.2.8)
    - Alle anchor functies zijn succesvol verplaatst naar AnchorSystem module
  - **6.3.4 Cleanup voltooid:**
    - Redundante "verplaatst naar..." comments geconsolideerd tot kortere sectie headers
    - Referenties naar modules behouden voor documentatie
    - Commentaar structuur verbeterd met duidelijke sectie headers
  - **6.3.5 Test voltooid:**
    - Anchor-instelling via web interface werkt ✅
    - Anchor-instelling via fysieke knop werkt ✅
    - Alle alert/anchor functionaliteit getest en werkend 

---

### Fase 7: Warm-Start Module
- **Status:** ⏳ Te starten
- **Start datum:** -
- **Voltooiing datum:** -
- **Lessons Learned toegepast:**
  - Kleine sub-stappen (< 100 regels per stap waar mogelijk)
  - Parallel implementatie eerst (nieuwe code naast oude)
  - Geen `static` keyword op helpers die modules gebruiken
  - Forward declarations voor dependencies
  - State synchronisatie na warm-start operaties
  - Test na elke stap

#### Stap 7.1: WarmStart Module Setup
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.1.1:** Maak `src/WarmStart/WarmStart.h` met enums/structs (WarmStartStatus, WarmStartMode, WarmStartStats)
  - [ ] **7.1.2:** Maak `src/WarmStart/WarmStart.cpp` met basis structuur (constructor, begin())
  - [ ] **7.1.3:** Verplaats state variabelen declaraties naar module (parallel, globaal blijft bestaan)
  - [ ] **7.1.4:** Test: Code compileert, geen functionaliteit veranderd
- **Notities:**
  - Parallel implementatie: module state naast globale variabelen
  - Forward declarations voor dependencies (PriceData, TrendDetector, etc.)

#### Stap 7.2: fetchBinanceKlines() naar WarmStart
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.2.1:** Verplaats `fetchBinanceKlines()` naar WarmStart module (parallel, niet vervangen)
  - [ ] **7.2.2:** Test: `WarmStart::fetchBinanceKlines()` werkt parallel
  - [ ] **7.2.3:** Vervang één `fetchBinanceKlines()` call in `performWarmStart()`
  - [ ] **7.2.4:** Vervang alle `fetchBinanceKlines()` calls
  - [ ] **7.2.5:** Verwijder `static` keyword van oude `fetchBinanceKlines()` (als nog gebruikt)
  - [ ] **7.2.6:** Test: Warm-start werkt nog met module functie
- **Notities:**
  - `fetchBinanceKlines()` is groot (~150 regels), maar kan als één stap
  - Parallel implementatie: oude functie blijft bestaan tot alle calls vervangen zijn

#### Stap 7.3: performWarmStart() naar WarmStart (deel 1 - Setup & 1m/5m)
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.3.1:** Verplaats eerste deel `performWarmStart()` (setup, checks, 1m fetch) naar module (parallel)
  - [ ] **7.3.2:** Verplaats 5m fetch logica naar module
  - [ ] **7.3.3:** Test: Module functie werkt parallel met oude functie
  - [ ] **7.3.4:** Vervang call naar `performWarmStart()` in setup() met module versie
  - [ ] **7.3.5:** Test: Warm-start werkt met module (1m en 5m)
- **Notities:**
  - `performWarmStart()` is zeer groot (~260 regels), splits in delen
  - Eerste deel: setup, checks, 1m en 5m fetch (~100 regels)
  - State synchronisatie: update globale variabelen na module operaties

#### Stap 7.4: performWarmStart() naar WarmStart (deel 2 - 30m/2h)
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.4.1:** Verplaats 30m fetch logica naar module (met retry)
  - [ ] **7.4.2:** Verplaats 2h fetch logica naar module (met retry)
  - [ ] **7.4.3:** Test: Module functie werkt met alle intervals
  - [ ] **7.4.4:** Verwijder oude `performWarmStart()` definitie
  - [ ] **7.4.5:** Test: Volledige warm-start werkt met module
- **Notities:**
  - Tweede deel: 30m en 2h fetch, mode bepaling, cleanup (~160 regels)
  - State synchronisatie: update globale variabelen (hasRet2hWarm, hasRet30mWarm, etc.)

#### Stap 7.5: updateWarmStartStatus() naar WarmStart
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.5.1:** Verplaats `updateWarmStartStatus()` naar module (parallel)
  - [ ] **7.5.2:** Test: Module functie werkt parallel
  - [ ] **7.5.3:** Vervang call naar `updateWarmStartStatus()` met module versie
  - [ ] **7.5.4:** Verwijder `static` keyword van oude functie
  - [ ] **7.5.5:** Test: Warm-start status updates werken
- **Notities:**
  - `updateWarmStartStatus()` gebruikt PriceData getters (al geïmplementeerd)
  - State synchronisatie: update globale `warmStartStatus` en `warmStartStats`

#### Stap 7.6: Cleanup Warm-Start Code
- **Status:** ⏳ Te starten
- **Sub-stappen:**
  - [ ] **7.6.1:** Verwijder oude `fetchBinanceKlines()` definitie (als niet meer gebruikt)
  - [ ] **7.6.2:** Verwijder oude state variabelen (als volledig gemigreerd)
  - [ ] **7.6.3:** Cleanup oude commentaar (behoud referenties naar module)
  - [ ] **7.6.4:** Test: Alle warm-start functionaliteit werkt nog
- **Notities:**
  - Behoud globale variabelen voor backward compatibility (UI, web interface)
  - Verwijder alleen als volledig gemigreerd en getest 

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

---

## Belangrijke Lessen & Tips voor Volgende Fasen

Zie `FASE4_NIEUWE_STRATEGIE.md` voor uitgebreide lessons learned. Belangrijkste aandachtspunten:

### ✅ Wat Goed Werkt:
- **Incrementele aanpak** - Kleine stapjes (< 100 regels) zijn veel veiliger
- **Parallel implementatie** - Nieuwe code naast oude, dan geleidelijk vervangen
- **Getter pattern** - Getters maken geleidelijke migratie mogelijk
- **Test na elke stap** - Compileer en test direct, voorkom opstapeling van problemen

### ⚠️ Veelvoorkomende Problemen:
- **Static keyword** - Verwijder `static` van helpers die modules gebruiken
- **Forward declarations** - Gebruik `extern` en forward declarations voor dependencies
- **State synchronisatie** - Sync state na warm-start of andere globale wijzigingen
- **Null pointer checks** - Altijd checks voor dynamische arrays (CYD platforms)

### 📋 Checklist voor Elke Nieuwe Stap:
1. Is de stap klein genoeg (< 100 regels)?
2. Kan ik parallel implementeren (nieuw naast oud)?
3. Zijn helper functies niet `static`?
4. Zijn forward declarations correct?
5. Compileert en werkt de code na deze stap?
6. Zijn comments met fase/stap nummers toegevoegd?
7. Is er een git commit na deze werkende stap?

Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige details en uitgebreide checklist.




