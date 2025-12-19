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
TrendState TrendDetector::determineTrendState(float ret_2h_value, float ret_30m_value, float trendThreshold) {
    if (ret_2h_value >= trendThreshold && ret_30m_value >= 0.0f)
    {
        return TREND_UP;
    }
    else if (ret_2h_value <= -trendThreshold && ret_30m_value <= 0.0f)
    {
        return TREND_DOWN;
    }
    else
    {
        return TREND_SIDEWAYS;
    }
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
    
    // Check of trend state is veranderd
    if (this->trendState != this->previousTrendState)
    {
        // Check cooldown: max 1 trend-change notificatie per 10 minuten
        bool cooldownPassed = (lastTrendChangeNotification == 0 || 
                               (now - lastTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
        
        // Alleen notificeren als cooldown is verstreken en we hebben geldige data
        if (cooldownPassed && ret_2h != 0.0f && (minuteArrayFilled || minuteIndex >= 120))
        {
            const char* fromTrend = "";
            const char* toTrend = "";
            const char* colorTag = "";
            
            // Bepaal tekst voor vorige trend
            switch (this->previousTrendState)
            {
                case TREND_UP:
                    fromTrend = "UP";
                    break;
                case TREND_DOWN:
                    fromTrend = "DOWN";
                    break;
                case TREND_SIDEWAYS:
                default:
                    fromTrend = "SIDEWAYS";
                    break;
            }
            
            // Bepaal tekst voor nieuwe trend
            switch (this->trendState)
            {
                case TREND_UP:
                    toTrend = "UP";
                    colorTag = "green_square,📈";
                    break;
                case TREND_DOWN:
                    toTrend = "DOWN";
                    colorTag = "red_square,📉";
                    break;
                case TREND_SIDEWAYS:
                default:
                    toTrend = "SIDEWAYS";
                    colorTag = "grey_square,➡️";
                    break;
            }
            
            // Bepaal volatiliteit tekst (extern dependency - later via VolatilityTracker module)
            const char* volText = "";
            switch (volatilityState)
            {
                case VOLATILITY_LOW:
                    volText = "Rustig";
                    break;
                case VOLATILITY_MEDIUM:
                    volText = "Gemiddeld";
                    break;
                case VOLATILITY_HIGH:
                    volText = "Volatiel";
                    break;
            }
            
            char title[64];
            char msg[256];
            snprintf(title, sizeof(title), "%s Trend Change", binanceSymbol);
            snprintf(msg, sizeof(msg), 
                     "Trend change: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\nVol: %s",
                     fromTrend, toTrend, ret_2h, ret_30m_value, volText);
            
            sendNotification(title, msg, colorTag);
            lastTrendChangeNotification = now;
            
            Serial_printf(F("[Trend] Trend change notificatie verzonden: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"), 
                         fromTrend, toTrend, ret_2h, ret_30m_value, volText);
        }
        
        // Update previous trend state
        this->previousTrendState = this->trendState;
        
        // Update globale variabelen voor backward compatibility
        extern TrendState trendState;
        extern TrendState previousTrendState;
        trendState = this->trendState;
        previousTrendState = this->previousTrendState;
    }
}


