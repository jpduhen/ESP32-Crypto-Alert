#ifndef VOLATILITYTRACKER_H
#define VOLATILITYTRACKER_H

#include <Arduino.h>

// VolatilityState enum - gebruikt voor volatiliteit detection
enum VolatilityState {
    VOLATILITY_LOW,      // Rustig: < 0.05%
    VOLATILITY_MEDIUM,  // Gemiddeld: 0.05% - 0.15%
    VOLATILITY_HIGH     // Volatiel: >= 0.15%
};

// EffectiveThresholds struct - gebruikt voor auto-volatility mode
struct EffectiveThresholds {
    float spike1m;
    float move5m;
    float move30m;
    float volFactor;
    float stdDev;
};

// Constants (moeten overeenkomen met .ino bestand)
#define VOLATILITY_LOOKBACK_MINUTES 60  // Bewaar laatste 60 minuten aan absolute 1m returns
#define MAX_VOLATILITY_WINDOW_SIZE 120  // Maximum window size (voor array grootte)

// VolatilityTracker class - beheert volatiliteit berekeningen en auto-volatility mode
class VolatilityTracker {
public:
    VolatilityTracker();
    void begin();
    
    // State management
    VolatilityState getVolatilityState() const { return volatilityState; }
    void setVolatilityState(VolatilityState state) { volatilityState = state; }
    
    // Volatiliteit berekeningen (oude systeem - 60 minuten lookback)
    // Voeg absolute 1m return toe aan volatiliteit buffer
    void addAbs1mReturnToVolatilityBuffer(float abs_ret_1m);
    
    // Bereken gemiddelde van absolute 1m returns over laatste 60 minuten
    float calculateAverageAbs1mReturn();
    
    // Bepaal volatiliteit state op basis van gemiddelde absolute 1m return
    VolatilityState determineVolatilityState(float avg_abs_1m, float volatilityLowThreshold, float volatilityHighThreshold);
    
    // Auto-Volatility Mode functies (nieuwe systeem - sliding window)
    // Bereken standaarddeviatie van 1m returns in sliding window
    float calculateStdDev1mReturns();
    
    // Update sliding window met nieuwe 1m return
    void updateVolatilityWindow(float ret_1m);
    
    // Bereken volatility factor en effective thresholds
    EffectiveThresholds calculateEffectiveThresholds(float baseSpike1m, float baseMove5m, float baseMove30m);
    
    // Sync state: Update VolatilityTracker state met globale variabelen (voor parallel implementatie)
    void syncStateFromGlobals();
    
private:
    VolatilityState volatilityState;
    
    // Oude systeem (60 minuten lookback)
    uint8_t volatilityIndex;
    bool volatilityArrayFilled;
    
    // Auto-Volatility Mode state (sliding window)
    uint8_t volatility1mIndex;
    bool volatility1mArrayFilled;
    
    // Note: Globale arrays en settings worden via extern declarations in .cpp file gebruikt
    // (parallel implementatie - arrays blijven globaal in .ino)
};

#endif // VOLATILITYTRACKER_H


