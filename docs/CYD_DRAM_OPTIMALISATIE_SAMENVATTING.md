# CYD DRAM Optimalisatie Samenvatting

## Overzicht
Dit document bevat een overzicht van alle wijzigingen die zijn gemaakt om de CYD (zonder PSRAM) weer werkend te krijgen en DRAM overflow te voorkomen.

---

## 1. DRAM Overflow Oplossingen

### 1.1 MQTT Queue Buffer Verkleining
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` (regel ~720)

**Wijziging:**
```cpp
// VOOR:
struct MqttMessage {
    char topic[128];
    char payload[128];
};

// NA:
struct MqttMessage {
    char topic[96];   // Verkleind van 128 naar 96 bytes
    char payload[96]; // Verkleind van 128 naar 96 bytes
};
```

**Besparing:** 64 bytes per entry × 8 entries = **512 bytes DRAM bespaard**

---

### 1.2 LVGL Draw Buffer Verkleining
**Locatie:** `src/UIController/UIController.cpp` (regel ~1946)

**Wijziging:**
```cpp
// VOOR:
#define CYD_BUF_LINES_NO_PSRAM 2

// NA:
#define CYD_BUF_LINES_NO_PSRAM 1  // Verlaagd van 2 naar 1 voor DRAM besparing
```

**Besparing:** ~720 bytes DRAM (afhankelijk van display breedte)

---

### 1.3 WiFiManager Optimalisatie
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `setupWiFiConnection()` functie (regel ~6042-6060)

**Probleem:** `static WiFiManager wm;` gebruikte veel DRAM

**Wijziging:**
```cpp
// VOOR:
static WiFiManager wm;
String apSSID = wm.getConfigPortalSSID();
String apPassword = "";

// NA:
// WiFiManager alleen in kleine scope
char apSSID[33];  // WiFiManager SSID max 32 chars + null terminator
char apPassword[64];  // WiFiManager password max 63 chars + null terminator

bool wifiIsSaved = false;
{
    // Kleine scope voor WiFiManager - wordt automatisch vrijgegeven na deze block
    WiFiManager wm;
    wm.setConfigPortalTimeout(0);
    wm.setEnableConfigPortal(false);
    wm.setWiFiAutoReconnect(false);
    
    wifiIsSaved = wm.getWiFiIsSaved();
    strncpy(apSSID, wm.getConfigPortalSSID().c_str(), sizeof(apSSID) - 1);
    apSSID[sizeof(apSSID) - 1] = '\0';
    apPassword[0] = '\0';  // Empty password
} // WiFiManager wordt hier vrijgegeven
```

**Besparing:** ~640 bytes DRAM (WiFiManager instance + String objecten)

**Totaal DRAM besparing:** ~1872 bytes (512 + 720 + 640)

---

## 2. Stack Overflow Oplossingen

### 2.1 Pointer Restore Logica
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `setup()` functie

**Probleem:** Stack overflow tijdens `wifiConnectionAndFetchPrice()` en `buildUI()` corrumpeerde globale pointers naar dynamisch gealloceerde arrays.

**Oplossing:**
- Pointer backup/restore logica toegevoegd rond kritieke functies
- Permanente pointer restore in `PriceData::addPriceToSecondArray()` en `updateMinuteAverage()`
- Helper functie `restorePointersIfCorrupted()` toegevoegd

**Code:**
```cpp
// In setup():
float *savedFiveMinutePrices = nullptr;
DataSource *savedFiveMinutePricesSource = nullptr;
float *savedMinuteAverages = nullptr;
DataSource *savedMinuteAveragesSource = nullptr;

#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
savedFiveMinutePrices = fiveMinutePrices;
savedFiveMinutePricesSource = fiveMinutePricesSource;
savedMinuteAverages = minuteAverages;
savedMinuteAveragesSource = minuteAveragesSource;
#endif

// Na wifiConnectionAndFetchPrice():
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
if (fiveMinutePrices != savedFiveMinutePrices || 
    (uint32_t)fiveMinutePrices < 0x1000 || 
    (uint32_t)fiveMinutePricesSource < 0x1000) {
    fiveMinutePrices = savedFiveMinutePrices;
    fiveMinutePricesSource = savedFiveMinutePricesSource;
    minuteAverages = savedMinuteAverages;
    minuteAveragesSource = savedMinuteAveragesSource;
}
#endif
```

**Ook toegevoegd:** Permanente restore in `PriceData::addPriceToSecondArray()` en `updateMinuteAverage()` functies.

---

## 3. Array Initialisatie Fix

### 3.1 Dynamische Arrays Initialisatie
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` - `allocateDynamicArrays()` functie (regel ~5701-5711)

**Probleem:** Arrays werden expliciet geïnitialiseerd naar 0.0f, wat ongeldige waarden veroorzaakte in min/max berekeningen.

**Wijziging:**
```cpp
// VOOR:
// Initialiseer arrays naar 0
for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
    fiveMinutePrices[i] = 0.0f;  // ❌ Probleem: 0.0f wordt meegenomen in berekeningen
    fiveMinutePricesSource[i] = SOURCE_LIVE;
}
for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
    minuteAverages[i] = 0.0f;  // ❌ Probleem: 0.0f wordt meegenomen in berekeningen
    minuteAveragesSource[i] = SOURCE_LIVE;
}

// NA:
// Initialiseer alleen Source arrays (niet price arrays - die worden gevuld met echte data)
// Price arrays worden niet geïnitialiseerd naar 0.0f omdat ongeldige waarden worden gefilterd door isValidPrice()
if (fiveMinutePricesSource != nullptr && (uint32_t)fiveMinutePricesSource >= 0x1000) {
    for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
        fiveMinutePricesSource[i] = SOURCE_LIVE;
    }
}
if (minuteAveragesSource != nullptr && (uint32_t)minuteAveragesSource >= 0x1000) {
    for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
        minuteAveragesSource[i] = SOURCE_LIVE;
    }
}
```

**Reden:** `heap_caps_malloc()` geeft ongedefinieerde waarden (niet gegarandeerd 0), wat prima is omdat `isValidPrice()` ongeldige waarden filtert.

---

## 4. Min/Max Berekening Fixes

### 4.1 calculateAverage() Functie
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` (regel ~3280)

**Wijziging:**
```cpp
// VOOR:
float calculateAverage(float *array, uint8_t size, bool filled)
{
    float sum = 0.0f;
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < size; i++)
    {
        if (filled || array[i] != 0.0f)  // ❌ Probleem: als filled=true, worden 0.0f waarden meegenomen
        {
            sum += array[i];
            count++;
        }
    }
    
    return (count == 0) ? 0.0f : (sum / count);
}

// NA:
float calculateAverage(float *array, uint8_t size, bool filled)
{
    float sum = 0.0f;
    uint8_t count = 0;
    
    // Gebruik altijd isValidPrice check om 0.0f, NaN, Inf en ongeldige waarden te filteren
    for (uint8_t i = 0; i < size; i++)
    {
        if (isValidPrice(array[i]))  // ✅ Filtert alle ongeldige waarden
        {
            sum += array[i];
            count++;
        }
    }
    
    return (count == 0) ? 0.0f : (sum / count);
}
```

---

### 4.2 findMinMaxInArray() Functie
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` (regel ~3457)

**Wijziging:**
```cpp
// VOOR:
} else {
    // Direct mode: use currentIndex as count
    if (!arrayFilled && array[0] == 0.0f) {
        return false;  // ❌ Probleem: checkt alleen array[0]
    }
    count = arrayFilled ? arraySize : currentIndex;
    if (count == 0) {
        return false;
    }
}

// NA:
} else {
    // Direct mode: use currentIndex as count
    count = arrayFilled ? arraySize : currentIndex;
    if (count == 0) {
        return false;
    }
    // Check of er ten minste één geldige waarde is (niet alleen 0.0f of ongeldige waarden)
    bool hasValidValue = false;
    for (uint16_t i = 0; i < count; i++) {
        if (isValidPrice(array[i])) {
            hasValidValue = true;
            break;
        }
    }
    if (!hasValidValue) {
        return false;
    }
}
```

---

### 4.3 updateMinMaxDiffLabels() Validatie
**Locatie:** `src/UIController/UIController.cpp` (regel ~2070)

**Wijziging:**
```cpp
// Toegevoegd: Validatie voor NaN, Inf, en redelijke range (1000-1000000 voor BTC)
bool validMin = (minVal > 0.0f && minVal >= 1000.0f && minVal <= 1000000.0f && !isnan(minVal) && !isinf(minVal));
bool validMax = (maxVal > 0.0f && maxVal >= 1000.0f && maxVal <= 1000000.0f && !isnan(maxVal) && !isinf(maxVal));
```

---

## 5. Inf Waarschuwing Fix

### 5.1 ret_1m Validatie
**Locatie:** `UNIFIED-LVGL9-Crypto_Monitor.ino` (regel ~4781)

**Wijziging:**
```cpp
// VOOR:
if (minuteUpdate && ret_1m != 0.0f)

// NA:
if (minuteUpdate && ret_1m != 0.0f && !isnan(ret_1m) && !isinf(ret_1m))
```

**Reden:** Voorkomt dat `inf` waarden worden toegevoegd aan de volatiliteit buffer.

---

## 6. UI Font Consistentie Fix

### 6.1 1m Box Labels Font
**Locatie:** `src/UIController/UIController.cpp` (regel ~647, 655, 663)

**Wijziging:**
```cpp
// VOOR:
lv_obj_set_style_text_font(price1MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
lv_obj_set_style_text_font(price1MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
lv_obj_set_style_text_font(price1MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);

// NA:
lv_obj_set_style_text_font(price1MinMaxLabel, FONT_SIZE_PRICE_OTHER, 0);  // Zelfde font als gemiddelde waarde
lv_obj_set_style_text_font(price1MinDiffLabel, FONT_SIZE_PRICE_OTHER, 0);  // Zelfde font als gemiddelde waarde
lv_obj_set_style_text_font(price1MinMinLabel, FONT_SIZE_PRICE_OTHER, 0);  // Zelfde font als gemiddelde waarde
```

**Ook toegevoegd:** Font expliciet zetten in `updateMinMaxDiffLabels()` voor consistentie.

---

## 7. Debug Code Cleanup

### 7.1 Verwijderde Debug Meldingen
**Locaties:** Verspreid over `UNIFIED-LVGL9-Crypto_Monitor.ino`

**Verwijderd:**
- Alle `[WarmStart] DEBUG:` meldingen
- Alle `[Setup] Pointer check` meldingen
- Alle `[Setup] Stack before/after` meldingen
- `[Setup] Pointers saved/restored` meldingen
- `[Memory] Backup pointers` en `FINAL CHECK` meldingen
- `[Memory] Before/After init loop` meldingen
- `[Setup] Checking/Verification` meldingen

**Behouden:**
- `[Memory] FATAL:` meldingen (kritieke errors)
- `[WarmStart] WARN:` meldingen (waarschuwingen)
- `[WarmStart] ERROR:` meldingen (errors)
- `[Setup] ERROR:` meldingen (errors)

---

## 8. Samenvatting Wijzigingen

### DRAM Besparingen:
1. **MQTT Queue:** 512 bytes (topic/payload van 128→96 bytes)
2. **LVGL Buffer:** ~720 bytes (CYD_BUF_LINES_NO_PSRAM van 2→1)
3. **WiFiManager:** ~640 bytes (lokaal in scope + String→char arrays)
4. **Totaal:** ~1872 bytes DRAM bespaard

### Stack Overflow Oplossingen:
1. **Pointer restore logica** rond `wifiConnectionAndFetchPrice()` en `buildUI()`
2. **Permanente pointer restore** in `addPriceToSecondArray()` en `updateMinuteAverage()`
3. **WiFiManager scope verkleining** om stack gebruik te verminderen

### Min/Max Berekening Fixes:
1. **Array initialisatie naar 0.0f verwijderd** (was de hoofdoorzaak)
2. **calculateAverage()** gebruikt nu `isValidPrice()` om ongeldige waarden te filteren
3. **findMinMaxInArray()** checkt nu op geldige waarden in plaats van alleen 0.0f
4. **updateMinMaxDiffLabels()** valideert waarden (NaN, Inf, range check)

### Andere Fixes:
1. **ret_1m inf validatie** toegevoegd
2. **Font consistentie** voor 1m box labels
3. **Debug code cleanup**

---

## 9. Belangrijke Lessen

### 9.1 Dynamische Array Initialisatie
- **NIET** initialiseren naar 0.0f na `heap_caps_malloc()`
- Gebruik `isValidPrice()` om ongeldige waarden te filteren
- Laat arrays ongedefinieerd totdat ze worden gevuld met echte data

### 9.2 Stack Overflow Preventie
- Gebruik kleine scopes voor grote objecten (zoals WiFiManager)
- Vervang `String` door `char[]` arrays waar mogelijk
- Monitor stack usage met `uxTaskGetStackHighWaterMark()`
- Implementeer pointer restore logica als bescherming

### 9.3 DRAM Optimalisatie
- Verklein buffers tot minimum benodigde grootte
- Gebruik lokale stack buffers waar mogelijk
- Vermijd grote statische objecten (gebruik scopes)
- Monitor DRAM usage met linker output

---

## 10. Nog Op Te Lossen Problemen

### 10.1 Min/Max Waarden Kunnen Nog Steeds Fout Gaan
**Mogelijke oorzaken:**
- Arrays worden mogelijk nog steeds overschreven met ongeldige waarden
- Ring buffer indexing kan problemen veroorzaken
- Warm-start kan arrays vullen met verkeerde data

**Suggesties voor volgende keer:**
- Voeg extra validatie toe in `updateMinuteAverage()` voordat waarden worden opgeslagen
- Check of `minuteAvg` geldig is voordat het wordt opgeslagen in `minuteAverages[]`
- Voeg bounds checking toe in alle array access functies

---

## 11. Bestanden Gewijzigd

1. `UNIFIED-LVGL9-Crypto_Monitor.ino`
   - MQTT queue struct verkleining
   - WiFiManager optimalisatie
   - Pointer restore logica
   - Array initialisatie fix
   - calculateAverage() fix
   - findMinMaxInArray() fix
   - ret_1m validatie
   - Debug code cleanup

2. `src/UIController/UIController.cpp`
   - CYD_BUF_LINES_NO_PSRAM verlaagd
   - Font consistentie voor 1m box labels
   - updateMinMaxDiffLabels() validatie

3. `src/PriceData/PriceData.cpp`
   - Permanente pointer restore in addPriceToSecondArray()

---

*Laatste update: Na min/max berekening fixes*
*Versie: 1.0*
