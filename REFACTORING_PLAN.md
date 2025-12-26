# Crypto Monitor Refactoring Plan

**Versie:** 2.0 (bijgewerkt met nieuwe strategie)  
**Datum:** 2025-12-18  
**Doel:** Code refactoren naar modulaire structuur voor betere stabiliteit en overzichtelijkheid  
**Principe:** Alle functionaliteit behouden, ongebruikte code verwijderen, stap-voor-stap met werkende code na elke stap

**Belangrijk:** Fase 4 gebruikt een nieuwe incrementele strategie (zie `FASE4_NIEUWE_STRATEGIE.md`) met veel kleinere stapjes om crashes te voorkomen.

---

## Overzicht

Het huidige bestand `UNIFIED-LVGL9-Crypto_Monitor.ino` is een monolithisch bestand van **8169 regels** met alle functionaliteit in één bestand. Het doel is om dit te refactoren naar een modulaire structuur vergelijkbaar met het temperatuurproject.

### Huidige Structuur
- Monolithisch bestand: 8169 regels
- ~353 functies/variabelen
- FreeRTOS multi-task architectuur
- Veel globale variabelen
- Geen duidelijke module scheiding

### Gewenste Structuur
```
UNIFIED-LVGL9-Crypto_Monitor/
├── UNIFIED-LVGL9-Crypto_Monitor.ino (hoofdbestand, setup/loop)
├── src/
│   ├── ApiClient/          # Binance API communicatie
│   ├── PriceData/           # Prijs data management & returns
│   ├── TrendDetector/       # Trend detection & state
│   ├── VolatilityTracker/   # Volatiliteit berekeningen
│   ├── AlertEngine/         # Alert detection & notificaties
│   ├── AnchorSystem/        # Anchor price tracking
│   ├── WarmStart/           # Warm-start functionaliteit
│   ├── UIController/        # LVGL UI management
│   ├── WebServer/           # Web interface
│   ├── SettingsStore/       # Persistent storage (Preferences)
│   ├── NetworkManager/      # WiFi & MQTT management
│   └── NtfyNotifier/        # NTFY notificaties (herbruikbaar)
└── [config files blijven zoals ze zijn]
```

---

## Refactoring Stappen

### Fase 1: Voorbereiding & Analyse
**Status:** ⏳ Te starten

#### Stap 1.1: Code Analyse
- [ ] Identificeer alle globale variabelen en hun gebruik
- [ ] Identificeer alle functies en hun dependencies
- [ ] Maak dependency graph
- [ ] Identificeer ongebruikte code
- [ ] Documenteer huidige FreeRTOS task structuur

**Verwachte output:** Analyse document met module boundaries

#### Stap 1.2: Test Baseline
- [ ] Verifieer dat huidige code compileert
- [ ] Test alle functionaliteit (indien mogelijk)
- [ ] Documenteer bekende issues

**Verwachte output:** Werkende baseline

---

### Fase 2: Settings & Storage Module
**Status:** ⏳ Te starten

#### Stap 2.1: SettingsStore Module
- [ ] Maak `src/SettingsStore/SettingsStore.h` en `.cpp`
- [ ] Verplaats `loadSettings()`, `saveSettings()` functionaliteit
- [ ] Verplaats Preferences management
- [ ] Test: Settings laden/opslaan werkt nog
- [ ] Update hoofdbestand om SettingsStore te gebruiken

**Verwachte output:** SettingsStore module, code compileert en werkt

#### Stap 2.2: Cleanup Settings Code
- [ ] Verwijder oude settings code uit hoofdbestand
- [ ] Test: Settings functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 3: Network Modules
**Status:** ⏳ Te starten

#### Stap 3.1: NetworkManager Module
- [ ] Maak `src/NetworkManager/NetworkManager.h` en `.cpp`
- [ ] Verplaats WiFi management (WiFiManager, reconnect logica)
- [ ] Verplaats MQTT functionaliteit (connectMQTT, mqttCallback, queue)
- [ ] Test: WiFi en MQTT werken nog

**Verwachte output:** NetworkManager module

#### Stap 3.2: NtfyNotifier Module (Herbruikbaar)
- [ ] Kopieer `NtfyNotifier` module uit temperatuurproject
- [ ] Pas aan voor crypto-monitor gebruik
- [ ] Vervang `sendNtfyNotification()` calls
- [ ] Test: NTFY notificaties werken nog

**Verwachte output:** Herbruikbare NtfyNotifier module

#### Stap 3.3: Cleanup Network Code
- [ ] Verwijder oude network code uit hoofdbestand
- [ ] Test: Alle network functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 4: Data Management Modules
**Status:** 🔄 In uitvoering (NIEUWE STRATEGIE)
**Belangrijk:** Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige gedetailleerde strategie

**Reden voor nieuwe strategie:** Eerdere poging was te complex en veroorzaakte crashes. Nieuwe aanpak gebruikt veel kleinere, geïsoleerde stapjes met test na elke stap.

**Principe:** Één kleine wijziging per stap, test na elke stap, parallel implementatie eerst, geen functionaliteit verliezen.

#### Stap 4.1: ApiClient Module (NIEUWE AANPAK - 8 sub-stappen)
**Status:** 🔄 In uitvoering (stap 4.1.7 voltooid, 4.1.8 te doen)

**Sub-stappen (incrementeel):**
- [x] 4.1.1: Maak ApiClient module structuur (geen integratie)
- [x] 4.1.2: Verplaats httpGET() naar ApiClient (parallel, niet vervangen)
- [x] 4.1.3: Test ApiClient::httpGET() parallel
- [x] 4.1.4: Vervang één httpGET() call
- [x] 4.1.5: Vervang alle httpGET() calls
- [x] 4.1.6: Verplaats parsePrice() naar ApiClient
- [x] 4.1.7: Verplaats fetchBinancePrice() logica (hoog-niveau method)
- [ ] 4.1.8: Cleanup oude code (verwijder oude httpGET() en parsePrice())

**Verwachte output:** ApiClient module, alle API calls gebruiken ApiClient

#### Stap 4.2: PriceData Module (NIEUWE AANPAK - 11 sub-stappen)
**Status:** ⏳ Te starten

**Sub-stappen (incrementeel):** Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige lijst
- [ ] 4.2.1: Maak PriceData module structuur
- [ ] 4.2.2: Verplaats array declaraties (parallel)
- [ ] 4.2.3: Verplaats addPriceToSecondArray() (parallel)
- [ ] 4.2.4: Vervang één addPriceToSecondArray() call
- [ ] 4.2.5: Verplaats state variabelen
- [ ] 4.2.6: Vervang directe array access in één functie
- [ ] 4.2.7: Herhaal voor andere functies (incrementally)
- [ ] 4.2.8: Verplaats calculateReturn functies
- [ ] 4.2.9: Verplaats fiveMinutePrices en minuteAverages
- [ ] 4.2.10: Dynamische allocatie voor CYD (optioneel, later)
- [ ] 4.2.11: Cleanup oude code

**Verwachte output:** PriceData module, alle data management via PriceData

#### Stap 4.3: Cleanup Data Code
- [ ] Verwijder oude data code uit hoofdbestand
- [ ] Test: Alle data functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 5: Analysis Modules
**Status:** ⏳ Te starten

#### Stap 5.1: TrendDetector Module
- [ ] Maak `src/TrendDetector/TrendDetector.h` en `.cpp`
- [ ] Verplaats trend detection (`determineTrendState()`, `checkTrendChange()`)
- [ ] Verplaats trend state management
- [ ] Test: Trend detection werkt nog

**Verwachte output:** TrendDetector module

#### Stap 5.2: VolatilityTracker Module
- [ ] Maak `src/VolatilityTracker/VolatilityTracker.h` en `.cpp`
- [ ] Verplaats volatiliteit berekeningen (`calculateStdDev1mReturns()`)
- [ ] Verplaats auto-volatility mode
- [ ] Verplaats effective thresholds berekening
- [ ] Test: Volatiliteit tracking werkt nog

**Verwachte output:** VolatilityTracker module

#### Stap 5.3: Cleanup Analysis Code
- [ ] Verwijder oude analysis code uit hoofdbestand
- [ ] Test: Alle analysis functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 6: Alert & Anchor Modules
**Status:** ⏳ Te starten

#### Stap 6.1: AlertEngine Module
- [ ] Maak `src/AlertEngine/AlertEngine.h` en `.cpp`
- [ ] Verplaats `checkAndNotify()` functionaliteit
- [ ] Verplaats alert detection (spike, move, confluence)
- [ ] Verplaats cooldown management
- [ ] Test: Alerts werken nog

**Verwachte output:** AlertEngine module

#### Stap 6.2: AnchorSystem Module
- [ ] Maak `src/AnchorSystem/AnchorSystem.h` en `.cpp`
- [ ] Verplaats anchor price management
- [ ] Verplaats trend-adaptive anchor logic
- [ ] Verplaats anchor alerts
- [ ] Test: Anchor systeem werkt nog

**Verwachte output:** AnchorSystem module

#### Stap 6.3: Cleanup Alert/Anchor Code
- [ ] Verwijder oude alert/anchor code uit hoofdbestand
- [ ] Test: Alle alert/anchor functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 7: Warm-Start Module
**Status:** ⏸️ Nu overslaan, later uitvoeren

**Notitie:** Warm-start functionaliteit blijft voorlopig in de hoofdcode. Er is een WarmStartWrapper module toegevoegd voor status/logging/settings, maar de volledige migratie wordt uitgesteld.

#### Stap 7.1: WarmStart Module
- [ ] Maak `src/WarmStart/WarmStart.h` en `.cpp`
- [ ] Verplaats `performWarmStart()` functionaliteit
- [ ] Verplaats `fetchBinanceKlines()` functionaliteit
- [ ] Verplaats warm-start state management
- [ ] Test: Warm-start werkt nog

**Verwachte output:** WarmStart module

#### Stap 7.2: Cleanup Warm-Start Code
- [ ] Verwijder oude warm-start code uit hoofdbestand
- [ ] Test: Warm-start functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 8: UI Module
**Status:** ⏳ Te starten
**Lessons Learned toegepast:**
- Kleine sub-stappen (< 100 regels per stap waar mogelijk)
- Parallel implementatie eerst (nieuwe code naast oude)
- Geen `static` keyword op helpers die modules gebruiken
- Forward declarations voor dependencies
- Test na elke stap

#### Stap 8.1: UIController Module Setup
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.1.1:** Maak `src/UIController/UIController.h` met basis structuur (class, forward declarations)
- [ ] **8.1.2:** Maak `src/UIController/UIController.cpp` met constructor en begin() method
- [ ] **8.1.3:** Verplaats LVGL callback functies naar module (my_print, millis_cb, my_disp_flush)
- [ ] **8.1.4:** Test: Code compileert, callbacks werken nog
- **Notities:**
  - Callbacks moeten extern beschikbaar blijven voor LVGL
  - Forward declarations voor dependencies (PriceData, TrendDetector, VolatilityTracker, etc.)

#### Stap 8.2: UI Object Pointers naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.2.1:** Verplaats lv_obj_t pointer declaraties naar module (parallel, globaal blijft bestaan)
- [ ] **8.2.2:** Voeg getters toe voor externe access (indien nodig)
- [ ] **8.2.3:** Test: Code compileert, geen functionaliteit veranderd
- **Notities:**
  - Parallel implementatie: module pointers naast globale pointers
  - Getters voor backward compatibility

#### Stap 8.3: create*() functies naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.3.1:** Verplaats `createChart()` naar module (parallel, niet vervangen)
- [ ] **8.3.2:** Verplaats `createHeaderLabels()` naar module (parallel)
- [ ] **8.3.3:** Verplaats `createPriceBoxes()` naar module (parallel)
- [ ] **8.3.4:** Verplaats `createFooter()` naar module (parallel)
- [ ] **8.3.5:** Test: Module functies werken parallel
- **Notities:**
  - Elke create functie is ~50-100 regels
  - Parallel implementatie: oude functies blijven bestaan

#### Stap 8.4: buildUI() naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.4.1:** Verplaats `buildUI()` naar module (gebruikt module create functies)
- [ ] **8.4.2:** Test: Module buildUI() werkt parallel
- [ ] **8.4.3:** Vervang call naar `buildUI()` in setup() met module versie
- [ ] **8.4.4:** Test: UI wordt correct gebouwd met module
- **Notities:**
  - buildUI() is klein (~10 regels), maar gebruikt alle create functies
  - Vervang pas na alle create functies gemigreerd zijn

#### Stap 8.5: update*Label() functies naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.5.1:** Verplaats `updateDateTimeLabels()` naar module (parallel)
- [ ] **8.5.2:** Verplaats `updateTrendLabel()` naar module (parallel)
- [ ] **8.5.3:** Verplaats `updateVolatilityLabel()` naar module (parallel)
- [ ] **8.5.4:** Test: Module update functies werken parallel
- **Notities:**
  - Elke update functie is ~30-100 regels
  - Gebruikt TrendDetector en VolatilityTracker getters

#### Stap 8.6: update*Card() functies naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.6.1:** Verplaats `updateBTCEURCard()` naar module (parallel)
- [ ] **8.6.2:** Verplaats `updateAveragePriceCard()` naar module (parallel)
- [ ] **8.6.3:** Verplaats `updatePriceCardColor()` naar module (parallel)
- [ ] **8.6.4:** Test: Module update functies werken parallel
- **Notities:**
  - Elke update functie is ~50-150 regels
  - Gebruikt PriceData getters

#### Stap 8.7: update*Section() functies naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.7.1:** Verplaats `updateChartSection()` naar module (parallel)
- [ ] **8.7.2:** Verplaats `updateHeaderSection()` naar module (parallel)
- [ ] **8.7.3:** Verplaats `updatePriceCardsSection()` naar module (parallel)
- [ ] **8.7.4:** Test: Module update functies werken parallel
- **Notities:**
  - Elke update functie is ~20-50 regels
  - Gebruikt andere update functies

#### Stap 8.8: updateUI() naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.8.1:** Verplaats `updateUI()` naar module (gebruikt module update functies)
- [ ] **8.8.2:** Test: Module updateUI() werkt parallel
- [ ] **8.8.3:** Vervang call naar `updateUI()` in uiTask met module versie
- [ ] **8.8.4:** Test: UI updates werken met module
- **Notities:**
  - updateUI() is ~30 regels, maar gebruikt alle update functies
  - Vervang pas na alle update functies gemigreerd zijn

#### Stap 8.9: checkButton() naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.9.1:** Verplaats `checkButton()` naar module (parallel)
- [ ] **8.9.2:** Test: Module checkButton() werkt parallel
- [ ] **8.9.3:** Vervang call naar `checkButton()` met module versie
- [ ] **8.9.4:** Test: Button functionaliteit werkt met module
- **Notities:**
  - checkButton() is ~50-100 regels
  - Gebruikt AnchorSystem voor anchor setting

#### Stap 8.10: LVGL Initialisatie naar Module
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.10.1:** Verplaats LVGL display initialisatie code naar module (parallel)
- [ ] **8.10.2:** Test: Module initialisatie werkt parallel
- [ ] **8.10.3:** Vervang LVGL init code in setup() met module versie
- [ ] **8.10.4:** Test: Display initialisatie werkt met module
- **Notities:**
  - LVGL init is ~50-100 regels
  - Platform-specifieke code (TTGO, CYD, ESP32-S3)

#### Stap 8.11: Cleanup UI Code
**Status:** ⏳ Te starten
**Sub-stappen:**
- [ ] **8.11.1:** Verwijder oude create functies (als niet meer gebruikt)
- [ ] **8.11.2:** Verwijder oude update functies (als niet meer gebruikt)
- [ ] **8.11.3:** Verwijder oude UI object pointers (als volledig gemigreerd)
- [ ] **8.11.4:** Cleanup oude commentaar (behoud referenties naar module)
- [ ] **8.11.5:** Test: Alle UI functionaliteit werkt nog
- **Notities:**
  - Behoud globale pointers voor backward compatibility (indien nodig)
  - Verwijder alleen als volledig gemigreerd en getest

---

### Fase 9: Web Interface Module
**Status:** ⏳ Te starten

#### Stap 9.1: WebServer Module
- [ ] Maak `src/WebServer/WebServer.h` en `.cpp`
- [ ] Verplaats web server setup
- [ ] Verplaats HTML generatie (`getSettingsHTML()`)
- [ ] Verplaats web handlers
- [ ] Test: Web interface werkt nog

**Verwachte output:** WebServer module

#### Stap 9.2: Cleanup Web Code
- [ ] Verwijder oude web code uit hoofdbestand
- [ ] Test: Web interface werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

---

### Fase 10: FreeRTOS Task Refactoring
**Status:** ⏳ Te starten

#### Stap 10.1: Task Refactoring
- [ ] Refactor API task om modules te gebruiken
- [ ] Refactor UI task om modules te gebruiken
- [ ] Refactor Web task om modules te gebruiken
- [ ] Test: Alle tasks werken nog

**Verwachte output:** Tasks gebruiken modules

#### Stap 10.2: Mutex & Synchronisatie
- [ ] Review mutex gebruik per module
- [ ] Optimaliseer mutex locks
- [ ] Test: Geen deadlocks, data consistent

**Verwachte output:** Geoptimaliseerde synchronisatie

---

### Fase 11: Cleanup & Optimalisatie
**Status:** ⏳ Te starten

#### Stap 11.1: Code Cleanup
- [ ] Verwijder ongebruikte code
- [ ] Verwijder ongebruikte variabelen
- [ ] Verwijder ongebruikte functies
- [ ] Test: Functionaliteit behouden

**Verwachte output:** Schonere codebase

#### Stap 11.2: Documentatie
- [ ] Update CODE_INDEX.md
- [ ] Update README.md
- [ ] Documenteer module interfaces
- [ ] Maak module README's waar nodig

**Verwachte output:** Bijgewerkte documentatie

#### Stap 11.3: Final Testing
- [ ] Test alle functionaliteit
- [ ] Test op verschillende platforms (TTGO, CYD, ESP32-S3)
- [ ] Test edge cases
- [ ] Performance test

**Verwachte output:** Volledig geteste, werkende code

---

## Belangrijke Principes

### 1. Incrementele Refactoring
- **Eén module per keer** - Begin met de minst afhankelijke modules
- **Werkende code na elke stap** - Code moet compileren en werken na elke stap
- **Test na elke stap** - Verifieer dat functionaliteit behouden is

### 2. Backward Compatibility
- **Geen breaking changes** - Functionaliteit moet identiek blijven
- **Geleidelijke migratie** - Oude code kan tijdelijk blijven tot nieuwe module werkt

### 3. Module Design
- **Duidelijke interfaces** - Public API moet duidelijk zijn
- **Minimale dependencies** - Modules moeten zo onafhankelijk mogelijk zijn
- **Thread-safe** - Modules moeten veilig zijn voor FreeRTOS multi-task gebruik

### 4. Code Quality
- [ ] **Geen code duplicatie** - Hergebruik waar mogelijk
- [ ] **Duidelijke namen** - Functies en variabelen moeten duidelijk zijn
- [ ] **Comments waar nodig** - Complexe logica moet uitgelegd worden
- [ ] **Consistente stijl** - Volg bestaande code stijl

---

## Risico's & Mitigatie

### Risico 1: Breaking Changes
**Mitigatie:** 
- Test na elke stap
- Behoud oude code tot nieuwe werkt
- Incrementele migratie

### Risico 2: FreeRTOS Synchronisatie Issues
**Mitigatie:**
- Behoud mutex structuur
- Test multi-task scenarios
- Review mutex locks per module

### Risico 3: Memory Issues
**Mitigatie:**
- Monitor heap usage
- Behoud bestaande memory strategieën
- Test op verschillende platforms

### Risico 4: Performance Degradatie
**Mitigatie:**
- Benchmark voor/na refactoring
- Optimaliseer waar nodig
- Behoud bestaande optimalisaties

---

## Success Criteria

✅ **Code compileert zonder errors**  
✅ **Alle functionaliteit werkt identiek**  
✅ **Code is modulair en overzichtelijk**  
✅ **Geen ongebruikte code**  
✅ **Documentatie is bijgewerkt**  
✅ **Tests slagen op alle platforms**  

---

## Notities

- Start met minst afhankelijke modules (SettingsStore, NetworkManager)
- Werk naar meer complexe modules (AlertEngine, UIController)
- Behoud FreeRTOS task structuur zoveel mogelijk
- Test op echte hardware waar mogelijk

---

**Laatste update:** 2025-12-18 - Lessons learned toegevoegd na voltooiing Fase 4.2

---

## Lessons Learned (na Fase 4.1 & 4.2)

Zie `FASE4_NIEUWE_STRATEGIE.md` voor uitgebreide lessons learned. Belangrijkste punten:

1. **Incrementele aanpak werkt** - Kleine stapjes (< 100 regels) zijn beter dan grote refactoring
2. **Parallel implementatie** - Nieuwe code naast oude, dan geleidelijk vervangen
3. **Static keyword problemen** - Verwijder `static` van helpers die modules gebruiken
4. **Test na elke stap** - Compileer en test direct, niet later
5. **Getter pattern** - Gebruik getters voor geleidelijke migratie
6. **Forward declarations** - Voorkom circular dependencies
7. **Documentatie tijdens refactoring** - Comments met fase/stap nummers
8. **State synchronisatie** - Sync state na operaties die globale state wijzigen
9. **Null pointer checks** - Altijd checks voor dynamische arrays
10. **Bounds checking** - Check array bounds, vooral na warm-start

Zie `FASE4_NIEUWE_STRATEGIE.md` voor volledige details en checklist.




