# Fase 2: Prioritization & Planning
**Datum**: 2025-12-09 20:50  
**Status**: 🟡 In Progress  
**Analist**: Auto (AI Assistant)

---

## 1. Verbeteringen Inventarisatie

### 1.1 Code Duplicatie Reductie

| Verbetering | Impact | Risico | Complexiteit | Code Reductie | Prioriteit |
|-------------|--------|--------|-------------|---------------|------------|
| MQTT Callback Lookup Table | 🔴 Hoog | 🟡 Medium | 🟡 Medium | ~140 regels | 🔴 P1 |
| Return Calculation Generiek | 🟠 Medium | 🟡 Medium | 🟡 Medium | ~100 regels | 🟠 P2 |
| Alert Logic Generiek | 🟠 Medium | 🟡 Medium | 🟡 Medium | ~110 regels | 🟠 P2 |
| Helper Functies (MQTT, String) | 🟡 Laag | 🟢 Laag | 🟢 Laag | ~40 regels | 🟡 P3 |

**Totaal Code Reductie**: ~390 regels (8%)

---

### 1.2 Performance Verbeteringen

| Verbetering | Impact | Risico | Complexiteit | Performance Win | Prioriteit |
|-------------|--------|--------|-------------|-----------------|------------|
| String → char[] in httpGET | 🟢 Hoog | 🟡 Medium | 🟡 Medium | 10-20% memory | 🔴 P1 |
| Conditional Chart Invalidate | 🟡 Medium | 🟢 Laag | 🟢 Laag | 5-10% rendering | 🟠 P2 |
| HTTP Retry Logic | 🟡 Medium | 🟡 Medium | 🟡 Medium | 15-25% reliability | 🟠 P2 |
| Split updateUI() | 🟡 Medium | 🟡 Medium | 🟠 Hoog | 5-10% performance | 🟡 P3 |

**Geschatte Performance Win**: 10-25%

---

### 1.3 Betrouwbaarheid Verbeteringen

| Verbetering | Impact | Risico | Complexiteit | Reliability Win | Prioriteit |
|-------------|--------|--------|-------------|-----------------|------------|
| HTTP Retry Logic | 🟢 Hoog | 🟡 Medium | 🟡 Medium | 15-25% | 🔴 P1 |
| Mutex Give Error Handling | 🟠 Medium | 🟢 Laag | 🟢 Laag | Voorkom leaks | 🟠 P2 |
| Integer Overflow Check | 🟠 Medium | 🟢 Laag | 🟢 Laag | Voorkom crashes | 🟠 P2 |
| Deadlock Detection | 🟡 Medium | 🟡 Medium | 🟡 Medium | Betere recovery | 🟡 P3 |
| MQTT Message Queue | 🟡 Medium | 🟡 Medium | 🟡 Medium | Voorkom loss | 🟡 P3 |

**Geschatte Reliability Win**: 15-30%

---

### 1.4 Robuustheid Verbeteringen

| Verbetering | Impact | Risico | Complexiteit | Robustness Win | Prioriteit |
|-------------|--------|--------|-------------|----------------|------------|
| atof() Validatie (NaN/Inf) | 🟢 Hoog | 🟢 Laag | 🟢 Laag | Voorkom errors | 🔴 P1 |
| Range Checks (alle inputs) | 🟢 Hoog | 🟢 Laag | 🟢 Laag | Voorkom invalid config | 🔴 P1 |
| Integer Overflow Check | 🟠 Medium | 🟢 Laag | 🟢 Laag | Voorkom crashes | 🟠 P2 |
| Exponential Backoff (WiFi/MQTT) | 🟡 Medium | 🟢 Laag | 🟢 Laag | Betere reconnect | 🟡 P3 |

**Geschatte Robustness Win**: 20-30%

---

### 1.5 Vereenvoudiging Verbeteringen

| Verbetering | Impact | Risico | Complexiteit | Maintainability Win | Prioriteit |
|-------------|--------|--------|-------------|-------------------|------------|
| setup() Split | 🟡 Medium | 🟢 Laag | 🟡 Medium | Betere leesbaarheid | 🟠 P2 |
| Magic Numbers → Constanten | 🟡 Laag | 🟢 Laag | 🟢 Laag | Betere leesbaarheid | 🟡 P3 |
| Settings Structs | 🟡 Medium | 🟢 Laag | 🟡 Medium | Betere organisatie | 🟡 P3 |

**Geschatte Maintainability Win**: 15-20%

---

## 2. Prioritering Matrix

### 2.1 Prioriteit 1 (P1) - Hoog Impact, Laag/Medium Risico

**Quick Wins - Directe Implementatie Aanbevolen**:

1. **atof() Validatie (NaN/Inf Check)**
   - Impact: 🟢 Hoog - Voorkomt invalid data
   - Risico: 🟢 Laag - Simpele check toevoegen
   - Complexiteit: 🟢 Laag - ~10 regels code
   - Geschatte tijd: 15 minuten
   - Test: Validatie testen met NaN/Inf waarden

2. **Range Checks voor Alle Numerieke Inputs**
   - Impact: 🟢 Hoog - Voorkomt invalid configuratie
   - Risico: 🟢 Laag - Validatie toevoegen
   - Complexiteit: 🟢 Laag - ~30 regels code
   - Geschatte tijd: 30 minuten
   - Test: Testen met out-of-range waarden

3. **String → char[] in httpGET()**
   - Impact: 🟢 Hoog - 10-20% memory win
   - Risico: 🟡 Medium - Core functionaliteit
   - Complexiteit: 🟡 Medium - ~50 regels code
   - Geschatte tijd: 1 uur
   - Test: Memory usage testen, functionaliteit verifiëren

4. **MQTT Callback Lookup Table**
   - Impact: 🔴 Hoog - ~140 regels reductie, veel betere leesbaarheid
   - Risico: 🟡 Medium - Core functionaliteit
   - Complexiteit: 🟡 Medium - ~100 regels code
   - Geschatte tijd: 2 uur
   - Test: Alle MQTT settings testen

5. **HTTP Retry Logic**
   - Impact: 🟢 Hoog - 15-25% reliability win
   - Risico: 🟡 Medium - Network logic
   - Complexiteit: 🟡 Medium - ~40 regels code
   - Geschatte tijd: 1.5 uur
   - Test: Testen met network failures

---

### 2.2 Prioriteit 2 (P2) - Medium Impact, Medium Risico

**Belangrijke Verbeteringen - Implementatie na P1**:

6. **Return Calculation Generieke Functie**
   - Impact: 🟠 Medium - ~100 regels reductie
   - Risico: 🟡 Medium - Core calculation logic
   - Complexiteit: 🟡 Medium - ~80 regels code
   - Geschatte tijd: 2 uur
   - Test: Alle return calculations testen

7. **Alert Logic Generieke Functie**
   - Impact: 🟠 Medium - ~110 regels reductie
   - Risico: 🟡 Medium - Notification logic
   - Complexiteit: 🟡 Medium - ~90 regels code
   - Geschatte tijd: 2 uur
   - Test: Alle alert types testen

8. **Mutex Give Error Handling**
   - Impact: 🟠 Medium - Voorkomt mutex leaks
   - Risico: 🟢 Laag - Error handling toevoegen
   - Complexiteit: 🟢 Laag - ~20 regels code
   - Geschatte tijd: 30 minuten
   - Test: Mutex error scenarios testen

9. **Integer Overflow Check**
   - Impact: 🟠 Medium - Voorkomt crashes
   - Risico: 🟢 Laag - Validatie toevoegen
   - Complexiteit: 🟢 Laag - ~15 regels code
   - Geschatte tijd: 20 minuten
   - Test: Extreme prijzen testen

10. **Conditional Chart Invalidate**
    - Impact: 🟡 Medium - 5-10% rendering win
    - Risico: 🟢 Laag - UI optimalisatie
    - Complexiteit: 🟢 Laag - ~10 regels code
    - Geschatte tijd: 15 minuten
    - Test: Chart updates testen

11. **setup() Split**
    - Impact: 🟡 Medium - Betere leesbaarheid
    - Risico: 🟢 Laag - Organisatie alleen
    - Complexiteit: 🟡 Medium - ~50 regels code
    - Geschatte tijd: 1 uur
    - Test: Startup testen

---

### 2.3 Prioriteit 3 (P3) - Laag Impact, Laag Risico

**Nice to Have - Optioneel**:

12. **Helper Functies (MQTT, String)**
    - Impact: 🟡 Laag - ~40 regels reductie
    - Risico: 🟢 Laag - Helper functies
    - Complexiteit: 🟢 Laag - ~30 regels code
    - Geschatte tijd: 45 minuten

13. **Deadlock Detection**
    - Impact: 🟡 Medium - Betere recovery
    - Risico: 🟡 Medium - Complex logic
    - Complexiteit: 🟡 Medium - ~60 regels code
    - Geschatte tijd: 2 uur

14. **MQTT Message Queue**
    - Impact: 🟡 Medium - Voorkomt message loss
    - Risico: 🟡 Medium - Queue management
    - Complexiteit: 🟡 Medium - ~80 regels code
    - Geschatte tijd: 2.5 uur

15. **Exponential Backoff (WiFi/MQTT)**
    - Impact: 🟡 Laag - Betere reconnect
    - Risico: 🟢 Laag - Backoff logic
    - Complexiteit: 🟢 Laag - ~30 regels code
    - Geschatte tijd: 45 minuten

16. **Magic Numbers → Constanten**
    - Impact: 🟡 Laag - Betere leesbaarheid
    - Risico: 🟢 Laag - Refactoring
    - Complexiteit: 🟢 Laag - ~10 regels code
    - Geschatte tijd: 15 minuten

17. **Settings Structs**
    - Impact: 🟡 Medium - Betere organisatie
    - Risico: 🟢 Laag - Structurering
    - Complexiteit: 🟡 Medium - ~100 regels code
    - Geschatte tijd: 2 uur

18. **Split updateUI()**
    - Impact: 🟡 Medium - Betere leesbaarheid
    - Risico: 🟡 Medium - UI logic
    - Complexiteit: 🟠 Hoog - ~150 regels code
    - Geschatte tijd: 3 uur

---

## 3. Implementatie Plan

### 3.1 Sprint 1: Quick Wins (P1 - Hoogste Prioriteit)

**Doel**: Directe impact met laag risico  
**Geschatte tijd**: ~6 uur  
**Verwachte winst**: 
- Code reductie: ~140 regels
- Performance: 10-20% memory
- Reliability: 15-25%

**Taken**:
1. ✅ atof() Validatie (15 min)
2. ✅ Range Checks (30 min)
3. ✅ String → char[] in httpGET() (1 uur)
4. ✅ MQTT Callback Lookup Table (2 uur)
5. ✅ HTTP Retry Logic (1.5 uur)
6. ✅ Testing & Verification (1 uur)

**Acceptatie Criteria**:
- Alle P1 verbeteringen geïmplementeerd
- Geen regressies in functionaliteit
- Alle tests slagen
- Code compileert zonder errors
- Werkt op alle platforms (TTGO, CYD)

---

### 3.2 Sprint 2: Belangrijke Verbeteringen (P2)

**Doel**: Code reductie en betrouwbaarheid  
**Geschatte tijd**: ~8 uur  
**Verwachte winst**:
- Code reductie: ~210 regels
- Betere maintainability
- Betere error handling

**Taken**:
1. ✅ Return Calculation Generiek (2 uur)
2. ✅ Alert Logic Generiek (2 uur)
3. ✅ Mutex Give Error Handling (30 min)
4. ✅ Integer Overflow Check (20 min)
5. ✅ Conditional Chart Invalidate (15 min)
6. ✅ setup() Split (1 uur)
7. ✅ Testing & Verification (2 uur)

**Acceptatie Criteria**:
- Alle P2 verbeteringen geïmplementeerd
- Geen regressies
- Alle tests slagen
- Code kwaliteit verbeterd

---

### 3.3 Sprint 3: Nice to Have (P3 - Optioneel)

**Doel**: Extra optimalisaties en polish  
**Geschatte tijd**: ~12 uur  
**Verwachte winst**:
- Code reductie: ~40 regels
- Betere organisatie
- Extra features

**Taken**:
1. ⚪ Helper Functies (45 min)
2. ⚪ Deadlock Detection (2 uur)
3. ⚪ MQTT Message Queue (2.5 uur)
4. ⚪ Exponential Backoff (45 min)
5. ⚪ Magic Numbers → Constanten (15 min)
6. ⚪ Settings Structs (2 uur)
7. ⚪ Split updateUI() (3 uur)
8. ⚪ Testing & Verification (1.5 uur)

**Acceptatie Criteria**:
- Optionele verbeteringen geïmplementeerd
- Geen regressies
- Alle tests slagen

---

## 4. Risico Analyse

### 4.1 Hoog Risico Items

| Item | Risico | Mitigatie |
|------|--------|-----------|
| MQTT Callback Refactoring | 🟡 Medium | Uitgebreide testing, backward compatibility |
| String → char[] Refactoring | 🟡 Medium | Stapsgewijs, uitgebreide testing |
| HTTP Retry Logic | 🟡 Medium | Testen met verschillende network scenarios |

### 4.2 Test Strategie

**Per Sprint**:
1. **Unit Tests**: Test individuele functies
2. **Integration Tests**: Test interacties tussen componenten
3. **Platform Tests**: Test op alle platforms (TTGO, CYD)
4. **Regression Tests**: Verifieer bestaande functionaliteit
5. **Stress Tests**: Test onder extreme condities

**Test Checklist per Verbetering**:
- [ ] Compileert zonder errors
- [ ] Werkt op TTGO platform
- [ ] Werkt op CYD platform
- [ ] Geen regressies in functionaliteit
- [ ] Performance niet verslechterd
- [ ] Memory usage niet verhoogd
- [ ] Error handling werkt correct

---

## 5. Success Metrics

### 5.1 Code Metrics

**Voor**:
- Code regels: 4,882
- Code duplicatie: ~820 regels (17%)
- Complexe functies: 8 functies >100 regels

**Na Sprint 1**:
- Code regels: ~4,742 (-140 regels)
- Code duplicatie: ~680 regels (14%)
- Complexe functies: 7 functies >100 regels

**Na Sprint 2**:
- Code regels: ~4,532 (-210 regels)
- Code duplicatie: ~470 regels (10%)
- Complexe functies: 5 functies >100 regels

**Na Sprint 3** (optioneel):
- Code regels: ~4,492 (-40 regels)
- Code duplicatie: ~430 regels (9%)
- Complexe functies: 4 functies >100 regels

### 5.2 Performance Metrics

**Voor**:
- Memory fragmentation: Medium
- API success rate: ~95%
- UI responsiveness: Good

**Na Sprint 1**:
- Memory fragmentation: Low (-10-20%)
- API success rate: ~98% (+3%)
- UI responsiveness: Good

**Na Sprint 2**:
- Memory fragmentation: Low
- API success rate: ~98%
- UI responsiveness: Better (+5-10%)

### 5.3 Reliability Metrics

**Voor**:
- Error recovery: Basic
- Deadlock detection: None
- Input validation: Partial

**Na Sprint 1**:
- Error recovery: Good (+retry logic)
- Deadlock detection: None
- Input validation: Complete

**Na Sprint 2**:
- Error recovery: Good
- Deadlock detection: None (optioneel in P3)
- Input validation: Complete

---

## 6. Implementatie Volgorde

### Fase 2.1: Sprint 1 Planning ✅
**Status**: Voltooid  
**Datum**: 2025-12-09 20:50

### Fase 2.2: Sprint 1 Implementatie
**Status**: ⚪ Niet gestart  
**Start**: Na goedkeuring  
**Geschatte duur**: ~6 uur

### Fase 2.3: Sprint 1 Testing
**Status**: ⚪ Niet gestart  
**Start**: Na implementatie  
**Geschatte duur**: ~1 uur

### Fase 2.4: Sprint 2 Planning
**Status**: ⚪ Niet gestart  
**Start**: Na Sprint 1 completion

### Fase 2.5: Sprint 2 Implementatie
**Status**: ⚪ Niet gestart  
**Start**: Na Sprint 2 planning

### Fase 2.6: Sprint 2 Testing
**Status**: ⚪ Niet gestart  
**Start**: Na Sprint 2 implementatie

---

## 7. Aanbevelingen

### 7.1 Directe Acties

1. **Start met Sprint 1**: Quick wins hebben directe impact met laag risico
2. **Test per item**: Test elke verbetering individueel voordat je doorgaat
3. **Version control**: Commit na elke succesvolle verbetering
4. **Documentatie**: Update documentatie bij wijzigingen

### 7.2 Best Practices

1. **Stapsgewijs**: Implementeer één verbetering per keer
2. **Test grondig**: Test op alle platforms na elke wijziging
3. **Backup**: Maak backup van werkende versie voor elke sprint
4. **Code review**: Review code voordat je commit

### 7.3 Risico Management

1. **Rollback plan**: Houd rollback plan klaar voor elke sprint
2. **Feature flags**: Overweeg feature flags voor grote wijzigingen
3. **Monitoring**: Monitor performance en errors tijdens implementatie
4. **Incrementeel**: Implementeer incrementeel, niet alles tegelijk

---

**Laatste update**: 2025-12-09 20:50  
**Status**: 🟡 In Progress - Planning voltooid, wacht op goedkeuring voor implementatie

