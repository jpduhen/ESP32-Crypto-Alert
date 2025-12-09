# Fase 1.5: Vereenvoudiging Analyse
**Datum**: 2025-12-09 20:10  
**Status**: ✅ Voltooid  
**Analist**: Auto (AI Assistant)

---

## 1. Code Complexiteit Analyse

### 1.1 Geneste If-Else Chains

**Gevonden Complexe Chains**:

1. **mqttCallback()** - regel ~1236
   - **Nesting level**: 10+ niveaus
   - **Aantal branches**: ~15 settings
   - **Complexiteit**: 🔴 Zeer Hoog
   - **Impact**: Zeer moeilijk te lezen en onderhouden
   - **Oplossing**: Lookup table met function pointers

2. **checkAndNotify()** - regel ~760
   - **Nesting level**: 4-5 niveaus
   - **Aantal branches**: 3 alert types
   - **Complexiteit**: 🟠 Hoog
   - **Impact**: Veel duplicatie tussen alert types
   - **Oplossing**: Generieke alert functie

3. **loadSettings()** - regel ~1102
   - **Nesting level**: 3-4 niveaus
   - **Aantal branches**: Migration logic
   - **Complexiteit**: 🟡 Medium
   - **Impact**: Complexe migration logica
   - **Oplossing**: Split migration in aparte functie

---

### 1.2 Lange Functies

**Functies > 100 regels**:

| Functie | Regels | Complexiteit | Vereenvoudiging |
|---------|--------|--------------|-----------------|
| `setup()` | ~443 | 🔴 Zeer Hoog | Split in meerdere functies |
| `updateUI()` | ~854 | 🟠 Hoog | Split in secties |
| `mqttCallback()` | ~263 | 🔴 Zeer Hoog | Lookup table |
| `checkAndNotify()` | ~250 | 🟠 Hoog | Generieke alert functie |
| `publishMqttDiscovery()` | ~138 | 🟠 Hoog | Helper functies |
| `calculateReturn30Minutes()` | ~115 | 🟡 Medium | Al geoptimaliseerd |

**Vereenvoudiging Suggesties**:
1. **setup()**: Split in:
   - `setupHardware()`
   - `setupWiFi()`
   - `setupMQTT()`
   - `setupLVGL()`
   - `setupTasks()`

2. **updateUI()**: Split in:
   - `updateChart()`
   - `updateLabels()`
   - `updateStatus()`

3. **mqttCallback()**: Vervang met lookup table

---

## 2. Code Duplicatie Vereenvoudiging

### 2.1 Return Calculation Functies

**Huidige Situatie**:
- 3 functies met ~80% duplicatie
- `calculateReturn1Minute()`, `calculateReturn5Minutes()`, `calculateReturn30Minutes()`

**Vereenvoudiging**:
```cpp
// Generieke functie
static float calculateReturn(
    float* priceArray,
    uint16_t arraySize,
    uint16_t index,
    bool arrayFilled,
    uint16_t positionsAgo,
    const char* logPrefix
)
```

**Voordelen**:
- ~150 regels → ~50 regels
- Minder duplicatie
- Makkelijker onderhoud

---

### 2.2 MQTT Callback Settings

**Huidige Situatie**:
- 15+ settings met identiek pattern
- Diep geneste if-else chain

**Vereenvoudiging**:
```cpp
// Lookup table struct
struct MqttSetting {
    const char* topicSuffix;
    void (*handler)(const char* value);
    bool (*validator)(const char* value);
};

// Lookup table
static const MqttSetting mqttSettings[] = {
    {"/config/spike1m/set", handleSpike1m, validateFloat},
    {"/config/spike5m/set", handleSpike5m, validateFloat},
    // ...
};

// Vereenvoudigde callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Parse topic en payload
    // Loop door lookup table
    // Roep handler aan
}
```

**Voordelen**:
- ~220 regels → ~80 regels
- Veel betere leesbaarheid
- Makkelijker nieuwe settings toevoegen

---

### 2.3 Notification Alert Logic

**Huidige Situatie**:
- 3 alert types met ~60% duplicatie
- Herhaalde cooldown, limit, formatting logic

**Vereenvoudiging**:
```cpp
// Alert configuratie struct
struct AlertConfig {
    float threshold;
    float filterThreshold;
    uint32_t cooldownMs;
    uint8_t maxPerHour;
    const char* titleFormat;
    const char* messageFormat;
};

// Generieke alert functie
static void sendAlert(
    AlertType type,
    float ret_primary,
    float ret_filter,
    const AlertConfig& config
)
```

**Voordelen**:
- ~230 regels → ~120 regels
- Minder duplicatie
- Makkelijker nieuwe alert types toevoegen

---

## 3. Magic Numbers Eliminatie

### 3.1 Gevonden Magic Numbers

**Huidige Status**:
- ✅ Meeste zijn al constanten
- ⚠️ **Restanten**:
  - `3600000UL` - 1 uur in ms (regel ~772)
  - `600000UL` - 10 minuten in ms (regel ~795)
  - `2500` - 2.5 seconden voor "nieuwe data" check (regel ~3095)
  - `1500` - 1.5 seconden voor langzame response (regel ~363)
  - `1200` - 1.2 seconden voor langzame API call (regel ~2956)

**Verbeterpunten**:
```cpp
#define ONE_HOUR_MS (3600000UL)
#define TEN_MINUTES_MS (600000UL)
#define NEW_DATA_TIMEOUT_MS (2500UL)
#define SLOW_RESPONSE_THRESHOLD_MS (1500UL)
#define SLOW_API_CALL_THRESHOLD_MS (1200UL)
```

---

## 4. Helper Functies Toevoegen

### 4.1 String Formatting Helpers

**Huidige Situatie**:
- Herhaald gebruik van `snprintf()` voor timestamp, price formatting

**Vereenvoudiging**:
```cpp
// Helper functies
static void formatTimestamp(char* buffer, size_t size);
static void formatPrice(char* buffer, size_t size, float price);
static void formatPercentage(char* buffer, size_t size, float percentage);
```

**Voordelen**:
- Minder code duplicatie
- Consistente formatting
- Makkelijker aanpassen

---

### 4.2 MQTT Publish Helpers

**Huidige Situatie**:
- Herhaald pattern: `dtostrf()` → `snprintf()` → `mqttClient.publish()`

**Vereenvoudiging**:
```cpp
// Helper functies
static void publishMqttFloat(const char* topic, float value, bool retained = true);
static void publishMqttString(const char* topic, const char* value, bool retained = true);
static void publishMqttInt(const char* topic, int value, bool retained = true);
```

**Voordelen**:
- ~60 regels → ~20 regels
- Minder duplicatie
- Consistente publishing

---

### 4.3 Array Bounds Check Helpers

**Huidige Situatie**:
- Herhaald pattern: `if (index == 0) return 0.0f;`

**Vereenvoudiging**:
```cpp
// Helper functies
static bool hasEnoughData(uint16_t index, uint16_t required, bool arrayFilled);
static bool isValidIndex(uint16_t index, uint16_t arraySize);
```

**Voordelen**:
- Minder code duplicatie
- Betere leesbaarheid

---

## 5. Configuratie Vereenvoudiging

### 5.1 Settings Structurering

**Huidige Situatie**:
- Veel losse globale variabelen voor settings
- Gerelateerde settings zijn niet gegroepeerd

**Vereenvoudiging**:
```cpp
// Settings structs
struct AlertSettings {
    float spike1mThreshold;
    float spike5mThreshold;
    float move30mThreshold;
    float move5mThreshold;
    float move5mAlertThreshold;
};

struct CooldownSettings {
    uint32_t cooldown1MinMs;
    uint32_t cooldown30MinMs;
    uint32_t cooldown5MinMs;
};

struct AnchorSettings {
    float takeProfit;
    float maxLoss;
    bool active;
    float price;
    // ...
};
```

**Voordelen**:
- Betere organisatie
- Makkelijker settings beheren
- Minder globale variabelen

---

### 5.2 Buffer State Structurering

**Huidige Situatie**:
- Losse variabelen voor buffer state (index, filled flag)

**Vereenvoudiging**:
```cpp
// Buffer state struct
struct BufferState {
    uint16_t index;
    bool filled;
    uint16_t size;
};

// Gebruik
BufferState secondBuffer = {0, false, SECONDS_PER_MINUTE};
BufferState fiveMinuteBuffer = {0, false, SECONDS_PER_5MINUTES};
```

**Voordelen**:
- Betere organisatie
- Minder globale variabelen
- Makkelijker doorgeven aan functies

---

## 6. Code Organisatie Vereenvoudiging

### 6.1 Functie Groepering

**Huidige Situatie**:
- Functies zijn goed georganiseerd in secties
- ⚠️ Sommige functies zijn te groot

**Verbeterpunten**:
1. **Split grote functies**: setup(), updateUI(), mqttCallback()
2. **Groep gerelateerde functies**: Return calculations, Alert functions
3. **Consistente naming**: Gebruik prefixen (calc*, update*, check*)

---

### 6.2 Variabele Organisatie

**Huidige Situatie**:
- Variabelen zijn goed georganiseerd
- ⚠️ Veel globale variabelen

**Verbeterpunten**:
1. **Gebruik structs**: Voor gerelateerde variabelen
2. **Static waar mogelijk**: Beperk globale scope
3. **Const waar mogelijk**: Markeer read-only variabelen

---

## 7. Samenvatting

### Vereenvoudiging Kandidaten:

| Gebied | Huidige Complexiteit | Vereenvoudiging | Impact |
|--------|---------------------|-----------------|--------|
| mqttCallback() | 🔴 Zeer Hoog | Lookup table | 🔴 Hoog |
| Return calculations | 🟠 Hoog | Generieke functie | 🟠 Medium |
| Alert logic | 🟠 Hoog | Generieke functie | 🟠 Medium |
| setup() | 🔴 Zeer Hoog | Split functies | 🟡 Medium |
| updateUI() | 🟠 Hoog | Split functies | 🟡 Medium |
| Magic numbers | 🟡 Medium | Constanten | 🟢 Laag |
| Helper functies | 🟡 Medium | Toevoegen | 🟢 Laag |

### Geschatte Code Reductie:
- **mqttCallback()**: ~140 regels reductie
- **Return calculations**: ~100 regels reductie
- **Alert logic**: ~110 regels reductie
- **Helper functies**: ~40 regels reductie
- **Totaal**: ~390 regels reductie (8% van codebase)

### Voordelen:
- ✅ Minder code duplicatie
- ✅ Betere leesbaarheid
- ✅ Makkelijker onderhoud
- ✅ Makkelijker uitbreiden
- ✅ Minder bugs

---

**Laatste update**: 2025-12-09 20:10  
**Status**: ✅ Voltooid

