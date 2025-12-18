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
**Status:** ⏳ Te starten

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

#### Stap 8.1: UIController Module
- [ ] Maak `src/UIController/UIController.h` en `.cpp`
- [ ] Verplaats LVGL display initialisatie
- [ ] Verplaats `buildUI()` functionaliteit
- [ ] Verplaats `updateUI()` functionaliteit
- [ ] Verplaats UI update functies (`updateTrendLabel()`, etc.)
- [ ] Test: UI werkt nog

**Verwachte output:** UIController module

#### Stap 8.2: Cleanup UI Code
- [ ] Verwijder oude UI code uit hoofdbestand
- [ ] Test: UI functionaliteit werkt nog

**Verwachte output:** Schonere code, functionaliteit behouden

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

**Laatste update:** Plan aangemaakt


