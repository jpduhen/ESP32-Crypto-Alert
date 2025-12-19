#include "VolatilityTracker.h"
#include <math.h>

// Forward declarations voor globale arrays en settings (parallel implementatie)
extern float abs1mReturns[];
extern float volatility1mReturns[];
extern bool autoVolatilityEnabled;
extern uint8_t autoVolatilityWindowMinutes;
extern float autoVolatilityBaseline1mStdPct;
extern float autoVolatilityMinMultiplier;
extern float autoVolatilityMaxMultiplier;
extern float volatilityLowThreshold;
extern float volatilityHighThreshold;
extern float currentVolFactor;  // Voor logging (moet niet static zijn in .ino)

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

// Constructor - initialiseer state variabelen
VolatilityTracker::VolatilityTracker() {
    volatilityState = VOLATILITY_MEDIUM;
    volatilityIndex = 0;
    volatilityArrayFilled = false;
    volatility1mIndex = 0;
    volatility1mArrayFilled = false;
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void VolatilityTracker::begin() {
    syncStateFromGlobals();
}

// Sync state: Update VolatilityTracker state met globale variabelen
void VolatilityTracker::syncStateFromGlobals() {
    // Forward declarations voor globale variabelen
    extern VolatilityState volatilityState;
    extern uint8_t volatilityIndex;
    extern bool volatilityArrayFilled;
    extern uint8_t volatility1mIndex;
    extern bool volatility1mArrayFilled;
    
    // Kopieer waarden van globale variabelen naar VolatilityTracker state
    this->volatilityState = volatilityState;
    this->volatilityIndex = volatilityIndex;
    this->volatilityArrayFilled = volatilityArrayFilled;
    this->volatility1mIndex = volatility1mIndex;
    this->volatility1mArrayFilled = volatility1mArrayFilled;
}

// Voeg absolute 1m return toe aan volatiliteit buffer (oude systeem)
void VolatilityTracker::addAbs1mReturnToVolatilityBuffer(float abs_ret_1m) {
    // Zorg dat het absoluut is
    if (abs_ret_1m < 0.0f) abs_ret_1m = -abs_ret_1m;
    
    // Valideer input
    if (isnan(abs_ret_1m) || isinf(abs_ret_1m))
    {
        Serial_printf("[Array] WARN: Ongeldige abs_ret_1m: %.2f\n", abs_ret_1m);
        return;
    }
    
    // Bounds check voor abs1mReturns array
    if (this->volatilityIndex >= VOLATILITY_LOOKBACK_MINUTES)
    {
        Serial_printf("[Array] ERROR: volatilityIndex buiten bereik: %u >= %u\n", this->volatilityIndex, VOLATILITY_LOOKBACK_MINUTES);
        this->volatilityIndex = 0; // Reset naar veilige waarde
    }
    
    abs1mReturns[this->volatilityIndex] = abs_ret_1m;
    this->volatilityIndex = (this->volatilityIndex + 1) % VOLATILITY_LOOKBACK_MINUTES;
    
    if (this->volatilityIndex == 0)
    {
        this->volatilityArrayFilled = true;
    }
    
    // Update globale variabelen voor backward compatibility
    extern uint8_t volatilityIndex;
    extern bool volatilityArrayFilled;
    volatilityIndex = this->volatilityIndex;
    volatilityArrayFilled = this->volatilityArrayFilled;
}

// Bereken gemiddelde van absolute 1m returns over laatste 60 minuten
float VolatilityTracker::calculateAverageAbs1mReturn() {
    uint8_t count = this->volatilityArrayFilled ? VOLATILITY_LOOKBACK_MINUTES : this->volatilityIndex;
    
    if (count == 0)
    {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; i++)
    {
        sum += abs1mReturns[i];
    }
    
    return sum / count;
}

// Bepaal volatiliteit state op basis van gemiddelde absolute 1m return
VolatilityState VolatilityTracker::determineVolatilityState(float avg_abs_1m, float volatilityLowThreshold, float volatilityHighThreshold) {
    // Volatiliteit bepaling (geoptimaliseerd: LOW < 0.05%, HIGH >= 0.15%)
    if (avg_abs_1m < volatilityLowThreshold)
    {
        return VOLATILITY_LOW;  // Rustig: < 0.05%
    }
    else if (avg_abs_1m < volatilityHighThreshold)
    {
        return VOLATILITY_MEDIUM;  // Gemiddeld: 0.05% - 0.15%
    }
    else
    {
        return VOLATILITY_HIGH;  // Volatiel: >= 0.15%
    }
}

// Bereken standaarddeviatie van 1m returns in sliding window (auto-volatility mode)
float VolatilityTracker::calculateStdDev1mReturns() {
    if (!this->volatility1mArrayFilled && this->volatility1mIndex == 0) {
        return 0.0f;  // Geen data beschikbaar
    }
    
    // Gebruik de geconfigureerde window size, maar clamp naar array grootte
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    
    // Validatie: window size moet minimaal 1 zijn
    if (windowSize == 0) {
        return 0.0f;
    }
    
    uint8_t count = this->volatility1mArrayFilled ? windowSize : this->volatility1mIndex;
    
    if (count < 2) {
        return 0.0f;  // Minimaal 2 samples nodig voor std dev
    }
    
    // Bereken gemiddelde
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        sum += volatility1mReturns[i];
    }
    float mean = sum / count;
    
    // Bereken variantie
    float variance = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float diff = volatility1mReturns[i] - mean;
        variance += diff * diff;
    }
    variance /= (count - 1);  // Sample variance (n-1)
    
    // Standaarddeviatie
    return sqrtf(variance);
}

// Update sliding window met nieuwe 1m return (auto-volatility mode)
void VolatilityTracker::updateVolatilityWindow(float ret_1m) {
    if (!autoVolatilityEnabled) return;
    
    // Gebruik de geconfigureerde window size, maar clamp naar array grootte
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    
    // Voeg nieuwe return toe aan circulaire buffer
    volatility1mReturns[this->volatility1mIndex] = ret_1m;
    this->volatility1mIndex++;
    
    if (this->volatility1mIndex >= windowSize) {
        this->volatility1mIndex = 0;
        this->volatility1mArrayFilled = true;
    }
    
    // Update globale variabelen voor backward compatibility
    extern uint8_t volatility1mIndex;
    extern bool volatility1mArrayFilled;
    volatility1mIndex = this->volatility1mIndex;
    volatility1mArrayFilled = this->volatility1mArrayFilled;
}

// Bereken volatility factor en effective thresholds (auto-volatility mode)
EffectiveThresholds VolatilityTracker::calculateEffectiveThresholds(float baseSpike1m, float baseMove5m, float baseMove30m) {
    EffectiveThresholds eff;
    eff.volFactor = 1.0f;
    eff.stdDev = 0.0f;
    
    if (!autoVolatilityEnabled) {
        // Als disabled, gebruik basiswaarden
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    // Bereken standaarddeviatie
    eff.stdDev = this->calculateStdDev1mReturns();
    
    // Als er onvoldoende data is, gebruik volFactor = 1.0
    // Minimaal 10 samples nodig voor betrouwbare berekening
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    uint8_t minSamples = (windowSize < 10) ? windowSize : 10;
    
    if (eff.stdDev <= 0.0f || (!this->volatility1mArrayFilled && this->volatility1mIndex < minSamples)) {
        eff.volFactor = 1.0f;
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    // Bereken volatility factor
    // Validatie: voorkom deling door nul
    if (autoVolatilityBaseline1mStdPct <= 0.0f) {
        eff.volFactor = 1.0f;
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    float rawVolFactor = eff.stdDev / autoVolatilityBaseline1mStdPct;
    
    // Clamp tussen min en max (validatie)
    eff.volFactor = rawVolFactor;
    if (eff.volFactor < autoVolatilityMinMultiplier) {
        eff.volFactor = autoVolatilityMinMultiplier;
    }
    if (eff.volFactor > autoVolatilityMaxMultiplier) {
        eff.volFactor = autoVolatilityMaxMultiplier;
    }
    
    // Validatie: voorkom negatieve of nul thresholds
    if (eff.volFactor <= 0.0f) {
        eff.volFactor = 1.0f;
    }
    
    // Update globale volFactor voor logging
    currentVolFactor = eff.volFactor;
    
    // Pas volFactor toe op thresholds
    eff.spike1m = baseSpike1m * eff.volFactor;
    eff.move5m = baseMove5m * sqrtf(eff.volFactor);  // sqrt voor langere timeframes
    eff.move30m = baseMove30m * sqrtf(eff.volFactor);
    
    // Validatie: voorkom negatieve thresholds (safety check)
    if (eff.spike1m < 0.0f) eff.spike1m = baseSpike1m;
    if (eff.move5m < 0.0f) eff.move5m = baseMove5m;
    if (eff.move30m < 0.0f) eff.move30m = baseMove30m;
    
    return eff;
}


