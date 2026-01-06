#include "TrendDetector.h"

// FASE X.2: Include AlertEngine voor throttling
#include "../AlertEngine/AlertEngine.h"

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char binanceSymbol[];
extern float prices[];  // Voor notificatie formaat
extern uint8_t language;  // Taalinstelling (0 = Nederlands, 1 = English)
extern const char* getText(const char* nlText, const char* enText);  // Taalvertaling functie
void getFormattedTimestampForNotification(char* buffer, size_t bufferSize);  // Nieuwe functie voor notificaties met slash formaat

// VolatilityState enum (hoort bij VolatilityTracker module - Fase 5.2)
// Forward declaration - VolatilityTracker.h wordt geïncludeerd in .ino
#include "../VolatilityTracker/VolatilityTracker.h"
extern VolatilityState volatilityState;  // Hoort bij VolatilityTracker module

// FASE X.4: Include SettingsStore voor Alert2HThresholds
#include "../SettingsStore/SettingsStore.h"

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
    mediumTrendState = TREND_SIDEWAYS;
    longTrendState = TREND_SIDEWAYS;
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
// FASE X.1: 2h Trend-hysterese (Optie A) - stabiliseer trendstatus om flip-flopping te voorkomen
// FASE X.4: Hysterese factor is nu instelbaar via settings
// Geoptimaliseerd: geconsolideerde checks, early returns
TrendState TrendDetector::determineTrendState(float ret_2h_value, float ret_30m_value, float trendThreshold) {
    // Forward declaration voor alert2HThresholds (uit SettingsStore)
    extern Alert2HThresholds alert2HThresholds;
    
    // Hysterese factor uit settings (default 0.65)
    const float hysteresisFactor = alert2HThresholds.trendHysteresisFactor;
    const float exitThreshold = trendThreshold * hysteresisFactor;
    
    // Gebruik huidige trend state voor hysterese logica
    TrendState currentState = this->trendState;
    
    // Hysterese logica: verschillende thresholds voor binnenkomen en verlaten van trend
    switch (currentState) {
        case TREND_SIDEWAYS:
            // SIDEWAYS → UP: alleen bij ret2h >= +TrendThreshold (en ret_30m >= 0 voor bevestiging)
            if (ret_2h_value >= trendThreshold && ret_30m_value >= 0.0f) {
                return TREND_UP;
            }
            // SIDEWAYS → DOWN: alleen bij ret2h <= -TrendThreshold (en ret_30m <= 0 voor bevestiging)
            if (ret_2h_value <= -trendThreshold && ret_30m_value <= 0.0f) {
                return TREND_DOWN;
            }
            // Blijft SIDEWAYS
            return TREND_SIDEWAYS;
            
        case TREND_UP:
            // UP → SIDEWAYS: pas als ret2h < +(TrendThreshold * 0.65) OF ret_30m < 0
            if (ret_2h_value < exitThreshold || ret_30m_value < 0.0f) {
                return TREND_SIDEWAYS;
            }
            // Blijft UP (ret_2h >= exitThreshold && ret_30m >= 0)
            return TREND_UP;
            
        case TREND_DOWN:
            // DOWN → SIDEWAYS: pas als ret2h > -(TrendThreshold * 0.65) OF ret_30m > 0
            if (ret_2h_value > -exitThreshold || ret_30m_value > 0.0f) {
                return TREND_SIDEWAYS;
            }
            // Blijft DOWN (ret_2h <= -exitThreshold && ret_30m <= 0)
            return TREND_DOWN;
            
        default:
            // Fallback: gebruik originele logica
            if (ret_2h_value >= trendThreshold && ret_30m_value >= 0.0f) {
                return TREND_UP;
            }
            if (ret_2h_value <= -trendThreshold && ret_30m_value <= 0.0f) {
                return TREND_DOWN;
            }
            return TREND_SIDEWAYS;
    }
}

TrendState TrendDetector::determineTrendStateSimple(float ret_value, float trendThreshold, TrendState currentState) {
    extern Alert2HThresholds alert2HThresholds;
    
    const float hysteresisFactor = alert2HThresholds.trendHysteresisFactor;
    const float exitThreshold = trendThreshold * hysteresisFactor;
    
    switch (currentState) {
        case TREND_SIDEWAYS:
            if (ret_value >= trendThreshold) {
                return TREND_UP;
            }
            if (ret_value <= -trendThreshold) {
                return TREND_DOWN;
            }
            return TREND_SIDEWAYS;
            
        case TREND_UP:
            if (ret_value < exitThreshold) {
                return TREND_SIDEWAYS;
            }
            return TREND_UP;
            
        case TREND_DOWN:
            if (ret_value > -exitThreshold) {
                return TREND_SIDEWAYS;
            }
            return TREND_DOWN;
            
        default:
            if (ret_value >= trendThreshold) {
                return TREND_UP;
            }
            if (ret_value <= -trendThreshold) {
                return TREND_DOWN;
            }
            return TREND_SIDEWAYS;
    }
}

void TrendDetector::updateMediumTrendState(float ret_1d_value, float trendThreshold) {
    mediumTrendState = determineTrendStateSimple(ret_1d_value, trendThreshold, mediumTrendState);
}

void TrendDetector::updateLongTrendState(float ret_7d_value, float trendThreshold) {
    longTrendState = determineTrendStateSimple(ret_7d_value, trendThreshold, longTrendState);
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
        char timestamp[32];
        getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
        
        // Vertaal trends naar juiste taal
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = getText("OP", "UP");
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = getText("NEER", "DOWN");
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = getText("ZIJWAARTS", "SIDEWAYS");
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = getText("OP", "UP");
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = getText("NEER", "DOWN");
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = getText("ZIJWAARTS", "SIDEWAYS");
        
        // VolText is al in Nederlands (getVolatilityText geeft "Rustig", "Gemiddeld", "Volatiel")
        // Vertaal naar Engels indien nodig
        const char* volTextTranslated = volText;
        if (language == 1) {  // English
            if (strcmp(volText, "Rustig") == 0) volTextTranslated = "Low";
            else if (strcmp(volText, "Gemiddeld") == 0) volTextTranslated = "Normal";
            else if (strcmp(volText, "Volatiel") == 0) volTextTranslated = "High";
        }
        
        snprintf(title, sizeof(title), "%s %s", 
                 binanceSymbol, getText("Trend Wijziging", "Trend Change"));
        snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\n%s: %s",
                 prices[0], timestamp,
                 getText("Trend change", "Trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_2h, ret_30m_value,
                 getText("Volatiliteit", "Volatility"), volTextTranslated);
        
        // FASE X.2: Gebruik throttling wrapper voor Trend Change
        if (AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, title, msg, colorTag)) {
            lastTrendChangeNotification = now;
        }
        
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

