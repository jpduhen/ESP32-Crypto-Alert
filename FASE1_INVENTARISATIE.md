# Fase 1: Analyse & Inventarisatie - Hoofdcode

**Datum:** 2025-01-XX  
**Status:** In Progress  
**Doel:** Volledige inventarisatie van helper functies, string operaties en code duplicatie

---

## 1.1 Helper Functies Inventarisatie

### Calculation Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `calculateReturnGeneric()` | ~3081 | static float | Generic return calculation | ✅ Consolideer validatie, early returns |
| `calculateReturn1Minute()` | ~3165 | float | 1m return wrapper | ✅ Al geoptimaliseerd (gebruikt PriceData) |
| `calculateReturn5Minutes()` | ~3175 | float | 5m return wrapper | ✅ Al geoptimaliseerd (gebruikt calculateReturnGeneric) |
| `calculateReturn30Minutes()` | ~3192 | float | 30m return wrapper | ✅ Al geoptimaliseerd (gebruikt PriceData) |
| `calculateReturn2Hours()` | ~3595 | static float | 2h return calculation | ⚠️ Review voor optimalisaties |
| `calculateLinearTrend1Minute()` | ~3270 | static float | Linear trend 1m | ⚠️ Review voor consolidatie |
| `calculate1MinutePct()` | ~3346 | static float | 1m percentage | ⚠️ Review voor consolidatie |
| `calculateLinearTrend30Minutes()` | ~3381 | static float | Linear trend 30m | ⚠️ Review voor consolidatie |
| `calculate30MinutePct()` | ~3477 | static float | 30m percentage | ⚠️ Review voor consolidatie |
| `calculateAverage()` | ~2966 | float | Average calculation | ✅ Al geoptimaliseerd (veel gebruikt) |
| `updateMinuteAverage()` | ~3949 | static void | Minute average update | ⚠️ Review voor optimalisaties |

### Min/Max Finding Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `findMinMaxInSecondPrices()` | ~3043 | void | Min/max in secondPrices | ✅ **HOGE PRIORITEIT** - Consolideer met andere |
| `findMinMaxInLast30Minutes()` | ~3778 | void | Min/max in last 30m | ✅ **HOGE PRIORITEIT** - Consolideer met andere |
| `findMinMaxInLast2Hours()` | ~3823 | void | Min/max in last 2h (CYD only) | ✅ **HOGE PRIORITEIT** - Consolideer met andere |

**Code Duplicatie Analyse:**
- Alle 3 functies hebben vergelijkbare logica:
  - Validatie checks
  - Loop patterns
  - Min/max finding
- **Verwachte winst:** ~50 regels code duplicatie geëlimineerd

### Validation Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `isValidPrice()` | ~1722 | bool | Single price validation | ✅ Al geoptimaliseerd |
| `areValidPrices()` | ~1729 | bool | Multiple price validation | ✅ Al geoptimaliseerd |
| `safeAtof()` | ~1738 | bool | Safe float parsing | ✅ Al geoptimaliseerd |
| `safeSecondsToMs()` | ~2257 | bool | Time conversion | ✅ Al geoptimaliseerd |
| `safeStrncpy()` | ~1758 | void | Safe string copy | ✅ Al geoptimaliseerd |

### Ring Buffer Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `getLastWrittenIndex()` | ~3001 | uint32_t | Last written index | ⚠️ Review voor inline |
| `getRingBufferIndexAgo()` | ~2990 | int32_t | Index N positions ago | ✅ Al geoptimaliseerd (veel gebruikt) |
| `calcLivePctMinuteAverages()` | ~3010 | uint8_t | Live percentage calculation | ⚠️ Review voor optimalisaties |

### Utility Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `formatIPAddress()` | ~1873 | void | IP formatting | ✅ Al geoptimaliseerd |
| `generateDefaultNtfyTopic()` | ~2052 | void | Topic generation | ✅ Al geoptimaliseerd (wrapper) |
| `getFormattedTimestamp()` | ~1705 | void | Timestamp formatting | ✅ Al geoptimaliseerd |
| `computeTwoHMetrics()` | ~3868 | TwoHMetrics | 2-hour metrics | ⚠️ Review voor optimalisaties |

### MQTT Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `handleMqttFloatSetting()` | ~2240 | static bool | MQTT float setting | ⚠️ Review voor consolidatie |
| `handleMqttIntSetting()` | ~2284 | static bool | MQTT int setting | ⚠️ Review voor consolidatie |
| `handleMqttStringSetting()` | ~2305 | static bool | MQTT string setting | ⚠️ Review voor consolidatie |
| `enqueueMqttMessage()` | ~2590 | static bool | MQTT message queue | ✅ Al geoptimaliseerd |
| `processMqttQueue()` | ~2610 | static void | Process MQTT queue | ✅ Al geoptimaliseerd |
| `publishMqttFloat()` | ~2640 | static void | Publish float to MQTT | ⚠️ Review voor consolidatie |
| `publishMqttUint()` | ~2655 | static void | Publish uint to MQTT | ⚠️ Review voor consolidatie |
| `publishMqttString()` | ~2670 | static void | Publish string to MQTT | ⚠️ Review voor consolidatie |

### Other Helpers
| Functie | Locatie | Type | Gebruik | Optimalisatie Mogelijk |
|---------|---------|------|---------|------------------------|
| `getTrendWaitText()` | ~2037 | static void | Trend wait text | ⚠️ Review voor consolidatie |
| `getMqttDeviceId()` | ~2058 | static void | MQTT device ID | ✅ Al geoptimaliseerd |
| `logHeapTelemetry()` | ~1883 | static void | Heap telemetry logging | ✅ Al geoptimaliseerd |
| `checkHeapTelemetry()` | ~1922 | static void | Heap telemetry check | ✅ Al geoptimaliseerd |
| `updateFooter()` | ~4213 | void | Footer update | ⚠️ Review voor optimalisaties |
| `disableScroll()` | ~4297 | void | Disable scroll | ✅ Al geoptimaliseerd |

---

## 1.2 String Operaties Inventarisatie

### String Object Creaties
| Locatie | Code | Vervangbaar | Impact |
|---------|------|-------------|--------|
| ~4450 | `String LVGL_Arduino = String('V') + ...` | ✅ Ja | Laag (alleen bij setup) |
| ~5009 | `String apSSID = wm.getConfigPortalSSID();` | ⚠️ Mogelijk | Medium (WiFi setup) |
| ~5010 | `String apPassword = "";` | ✅ Ja | Laag |
| ~572-600 | `String topic = server->arg("ntfytopic");` | ⚠️ Mogelijk | Hoog (frequent) |
| ~602-610 | `String symbol = server->arg("binancesymbol");` | ⚠️ Mogelijk | Hoog (frequent) |
| ~733-759 | `String host/user/pass = server->arg(...)` | ⚠️ Mogelijk | Hoog (frequent) |
| ~917-928 | `String message = "File Not Found\n\n";` | ✅ Ja | Laag (alleen 404) |
| ~948 | `String anchorValueStr = server->arg("value");` | ⚠️ Mogelijk | Medium |

### String Method Calls
| Locatie | Code | Vervangbaar | Impact |
|---------|------|-------------|--------|
| ~591 | `topic.trim()` | ✅ Ja | Hoog |
| ~603 | `symbol.trim()` | ✅ Ja | Hoog |
| ~604 | `symbol.toUpperCase()` | ⚠️ Mogelijk | Hoog |
| ~605 | `symbol.length()` | ✅ Ja | Hoog |
| ~606 | `symbol.toCharArray()` | ✅ Ja | Hoog |
| ~734-758 | `host/user/pass.trim()` | ✅ Ja | Hoog |
| ~736-757 | `host/user/pass.length()` | ✅ Ja | Hoog |
| ~737-758 | `host/user/pass.toCharArray()` | ✅ Ja | Hoog |
| ~582 | `server->arg("language").toInt()` | ⚠️ Mogelijk | Medium |
| ~656-728 | `server->arg(...).toInt()` | ⚠️ Mogelijk | Medium |
| ~656-728 | `server->arg(...).c_str()` | ✅ Ja | Medium |

### String Concatenatie
| Locatie | Code | Vervangbaar | Impact |
|---------|------|-------------|--------|
| ~4450 | `String('V') + lv_version_major() + ...` | ✅ Ja | Laag (alleen bij setup) |
| ~917-928 | `message += "URI: ";` | ✅ Ja | Laag (alleen 404) |

**Totaal String Operaties:** ~24 (zoals geïdentificeerd in plan)

**Prioriteit:**
- **Hoog:** WebServer form argument parsing (~15 operaties)
- **Medium:** WiFi setup (~5 operaties)
- **Laag:** Setup/error messages (~4 operaties)

---

## 1.3 Code Duplicatie Identificatie

### Min/Max Finding Patterns
**Locatie:** `findMinMaxInSecondPrices()`, `findMinMaxInLast30Minutes()`, `findMinMaxInLast2Hours()`

**Duplicatie:**
```cpp
// Pattern 1: Validatie
if (!arrayFilled && prices[0] == 0.0f) return;
uint8_t count = arrayFilled ? ARRAY_SIZE : index;
if (count == 0) return;

// Pattern 2: Initialisatie
minVal = prices[0];
maxVal = prices[0];

// Pattern 3: Loop
for (uint8_t i = 1; i < count; i++) {
    if (isValidPrice(prices[i])) {
        if (prices[i] < minVal) minVal = prices[i];
        if (prices[i] > maxVal) maxVal = prices[i];
    }
}
```

**Consolidatie Mogelijkheid:** ✅ **HOGE PRIORITEIT**
- Maak generic `findMinMaxInArray()` helper
- Elimineer ~50 regels duplicatie

### Return Calculation Patterns
**Locatie:** `calculateReturnGeneric()`, `calculateReturn2Hours()`, etc.

**Duplicatie:**
```cpp
// Pattern 1: Data availability check
if (!arrayFilled && currentIndex < positionsAgo) {
    return 0.0f;
}

// Pattern 2: Price validation
if (!areValidPrices(priceNow, priceXAgo)) {
    return 0.0f;
}

// Pattern 3: Return calculation
return ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
```

**Consolidatie Mogelijkheid:** ⚠️ **MEDIUM PRIORITEIT**
- `calculateReturnGeneric()` bestaat al
- Review voor verdere optimalisaties

### MQTT Publishing Patterns
**Locatie:** `publishMqttFloat()`, `publishMqttUint()`, `publishMqttString()`

**Duplicatie:**
```cpp
// Pattern: MQTT connection check + publish
if (mqttConnected && mqttClient.connected()) {
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s", MQTT_TOPIC_PREFIX, topicSuffix);
    // ... publish logic ...
}
```

**Consolidatie Mogelijkheid:** ⚠️ **MEDIUM PRIORITEIT**
- Maak generic `publishMqttValue()` helper
- Elimineer ~30 regels duplicatie

### MQTT Setting Handling Patterns
**Locatie:** `handleMqttFloatSetting()`, `handleMqttIntSetting()`, `handleMqttStringSetting()`

**Duplicatie:**
```cpp
// Pattern: Validation + assignment + state publish
if (conversion_successful && value_in_range) {
    *target = value;
    publishMqttString(stateTopic, prefix);
    return true;
}
```

**Consolidatie Mogelijkheid:** ⚠️ **MEDIUM PRIORITEIT**
- Maak template helper functie
- Elimineer ~40 regels duplicatie

### Validation Patterns
**Locatie:** Meerdere functies

**Duplicatie:**
```cpp
// Pattern: NaN/Inf checks
if (isnan(value) || isinf(value)) {
    return false;
}

// Pattern: Range checks
if (value < minVal || value > maxVal) {
    return false;
}
```

**Consolidatie Mogelijkheid:** ✅ **LAGE PRIORITEIT**
- Al geconsolideerd in `isValidPrice()` en helpers
- Review voor verdere consolidatie

### Error Handling Patterns
**Locatie:** Meerdere functies

**Duplicatie:**
```cpp
// Pattern: Mutex timeout handling
if (!safeMutexTake(dataMutex, timeout, "function_name")) {
    Serial_printf("[Function] WARN -> mutex timeout\n");
    return;
}
```

**Consolidatie Mogelijkheid:** ⚠️ **LAGE PRIORITEIT**
- Al geconsolideerd in `safeMutexTake()` helper
- Review voor verdere consolidatie

---

## Samenvatting

### Helper Functies
- **Totaal geïdentificeerd:** ~35 functies
- **Al geoptimaliseerd:** ~15 functies
- **Te optimaliseren:** ~20 functies
- **Hoogste prioriteit:** Min/Max finding helpers (3 functies, ~50 regels duplicatie)

### String Operaties
- **Totaal geïdentificeerd:** ~24 operaties
- **Vervangbaar:** ~20 operaties
- **Hoogste prioriteit:** WebServer form argument parsing (~15 operaties)

### Code Duplicatie
- **Min/Max finding:** ~50 regels duplicatie (✅ **HOGE PRIORITEIT**)
- **MQTT publishing:** ~30 regels duplicatie (⚠️ **MEDIUM PRIORITEIT**)
- **MQTT setting handling:** ~40 regels duplicatie (⚠️ **MEDIUM PRIORITEIT**)
- **Return calculations:** Al geconsolideerd (⚠️ **LAGE PRIORITEIT**)

### Volgende Stappen
1. ✅ **Fase 1 voltooid** - Inventarisatie compleet
2. ⏭️ **Fase 2.1 starten** - Geconsolideerde Min/Max Finding (HOGE PRIORITEIT)
3. ⏭️ **Fase 3.1 starten** - String naar C-style Conversies (HOGE PRIORITEIT)

---

*Dit document wordt bijgewerkt tijdens de optimalisatie proces.*

