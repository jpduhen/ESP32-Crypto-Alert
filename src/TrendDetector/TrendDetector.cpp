#include "TrendDetector.h"

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char binanceSymbol[];

// VolatilityState enum (hoort bij VolatilityTracker module - Fase 5.2)
// Forward declaration - VolatilityTracker.h wordt geïncludeerd in .ino
#include "../VolatilityTracker/VolatilityTracker.h"
extern VolatilityState volatilityState;  // Hoort bij VolatilityTracker module

// Constants
#define TREND_CHANGE_COOLDOWN_MS 600000UL  // 10 minuten cooldown voor trend change notificaties

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

// Helper: Get volatility text (geoptimaliseerd: elimineert switch duplicatie)
const char* TrendDetector::getVolatilityText(VolatilityState volState) {
    switch (volState) {
        case VOLATILITY_LOW: return "Rustig";
        case VOLATILITY_MEDIUM: return "Gemiddeld";
        case VOLATILITY_HIGH: return "Volatiel";
        default: return "Onbekend";
    }
}

// Constructor - initialiseer state variabelen
TrendDetector::TrendDetector() {
    trendState = TREND_SIDEWAYS;
    previousTrendState = TREND_SIDEWAYS;
    lastTrendChangeNotification = 0;
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void TrendDetector::begin() {
    // Fase 5.1: Synchroniseer TrendDetector state met globale variabelen
    syncStateFromGlobals();
}

// Sync state: Update TrendDetector state met globale variabelen
void TrendDetector::syncStateFromGlobals() {
    // Forward declarations voor globale variabelen
    extern TrendState trendState;
    extern TrendState previousTrendState;
    extern unsigned long lastTrendChangeNotification;
    
    // Kopieer waarden van globale variabelen naar TrendDetector state
    this->trendState = trendState;
    this->previousTrendState = previousTrendState;
    this->lastTrendChangeNotification = lastTrendChangeNotification;
}

// Bepaal trend state op basis van 2h en 30m returns
// Geoptimaliseerd: geconsolideerde checks, early returns
TrendState TrendDetector::determineTrendState(float ret_2h_value, float ret_30m_value, float trendThreshold) {
    // Geconsolideerde checks: check alles in één keer
    if (ret_2h_value >= trendThreshold && ret_30m_value >= 0.0f) {
        return TREND_UP;
    }
    if (ret_2h_value <= -trendThreshold && ret_30m_value <= 0.0f) {
        return TREND_DOWN;
    }
    return TREND_SIDEWAYS;
}

// Trend change detection en notificatie
void TrendDetector::checkTrendChange(float ret_30m_value, float ret_2h, bool minuteArrayFilled, uint8_t minuteIndex) {
    unsigned long now = millis();
    
    // Fase 5.1: Synchroniseer TrendDetector state met globale waarden (parallel implementatie)
    // Dit zorgt ervoor dat we de laatste trendState hebben voordat we checken op changes
    extern TrendState trendState;
    extern TrendState previousTrendState;
    this->trendState = trendState;
    this->previousTrendState = previousTrendState;
    
    // Geconsolideerde check: check of trend state is veranderd
    if (this->trendState == this->previousTrendState) {
        return; // Geen change, skip rest
    }
    
    // Geconsolideerde checks: cooldown en data validiteit in één keer
    bool cooldownPassed = (lastTrendChangeNotification == 0 || 
                          (now - lastTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
    bool hasValidData = (ret_2h != 0.0f && (minuteArrayFilled || minuteIndex >= 120));
    
    if (cooldownPassed && hasValidData) {
        // Geoptimaliseerd: gebruik helper functies i.p.v. switch statements
        const char* fromTrend = getTrendName(this->previousTrendState);
        const char* toTrend = getTrendName(this->trendState);
        const char* colorTag = getTrendColorTag(this->trendState);
        const char* volText = getVolatilityText(volatilityState);
            
        // Gebruik lokale buffers (stack geheugen i.p.v. DRAM)
        char title[64];
        char msg[256];
        snprintf(title, sizeof(title), "%s Trend Change", binanceSymbol);
        snprintf(msg, sizeof(msg), 
                 "Trend change: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\nVol: %s",
                 fromTrend, toTrend, ret_2h, ret_30m_value, volText);
        
        sendNotification(title, msg, colorTag);
        lastTrendChangeNotification = now;
        
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[Trend] Trend change notificatie verzonden: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"), 
                     fromTrend, toTrend, ret_2h, ret_30m_value, volText);
        #endif
    }
    
    // Update previous trend state
    this->previousTrendState = this->trendState;
    
    // Update globale variabelen voor backward compatibility
    extern TrendState trendState;
    extern TrendState previousTrendState;
    trendState = this->trendState;
    previousTrendState = this->previousTrendState;
}


