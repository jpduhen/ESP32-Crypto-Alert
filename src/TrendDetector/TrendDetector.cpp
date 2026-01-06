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
    longTermTrendState = TREND_SIDEWAYS;
    previousLongTermTrendState = TREND_SIDEWAYS;
    lastTrendChangeNotification = 0;
    lastLongTermTrendChangeNotification = 0;
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

<<<<<<< HEAD
// Lange termijn trend detection op basis van 4h en 1d returns
TrendState TrendDetector::determineLongTermTrendState(float ret_4h_value, float ret_1d_value, float longTermThreshold) {
    // Gebruik een hogere threshold voor lange termijn (default 2.0%)
    // Lange termijn trends zijn minder gevoelig voor korte termijn fluctuaties
    
    // UP: beide 4h en 1d moeten positief zijn, en minstens één moet boven threshold zijn
    if (ret_4h_value >= longTermThreshold && ret_1d_value >= 0.0f) {
        return TREND_UP;
    }
    if (ret_1d_value >= longTermThreshold && ret_4h_value >= 0.0f) {
        return TREND_UP;
    }
    
    // DOWN: beide 4h en 1d moeten negatief zijn, en minstens één moet onder -threshold zijn
    if (ret_4h_value <= -longTermThreshold && ret_1d_value <= 0.0f) {
        return TREND_DOWN;
    }
    if (ret_1d_value <= -longTermThreshold && ret_4h_value <= 0.0f) {
        return TREND_DOWN;
    }
    
    // SIDEWAYS: alles daartussenin
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
        char msg[240];  // Verkleind van 256 naar 240 bytes (bespaart 16 bytes DRAM)
        char timestamp[32];
        getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
        
        // Vertaal trends naar juiste taal (KT voor NL, ST voor EN)
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = getText("KT+", "ST+");
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = getText("KT-", "ST-");
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = getText("KT=", "ST=");
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = getText("KT+", "ST+");
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = getText("KT-", "ST-");
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = getText("KT=", "ST=");
        
        // VolText is al in Nederlands (getVolatilityText geeft "Rustig", "Gemiddeld", "Volatiel")
        // Vertaal naar Engels indien nodig
        const char* volTextTranslated = volText;
        if (language == 1) {  // English
            if (strcmp(volText, "Rustig") == 0) volTextTranslated = "Low";
            else if (strcmp(volText, "Gemiddeld") == 0) volTextTranslated = "Normal";
            else if (strcmp(volText, "Volatiel") == 0) volTextTranslated = "High";
        }
        
        // Bepaal lange termijn trend voor notificatie
        extern bool hasRet4h;
        extern bool hasRet1d;
        const char* longTermTrendText = "";
        if (hasRet4h && hasRet1d) {
            extern float ret_4h;
            extern float ret_1d;
            const float longTermThreshold = 2.0f;
            TrendState longTermTrend = this->determineLongTermTrendState(ret_4h, ret_1d, longTermThreshold);
            switch (longTermTrend) {
                case TREND_UP:
                    longTermTrendText = getText("LT+", "LT+");
                    break;
                case TREND_DOWN:
                    longTermTrendText = getText("LT-", "LT-");
                    break;
                case TREND_SIDEWAYS:
                default:
                    longTermTrendText = getText("LT=", "LT=");
                    break;
            }
        } else {
            longTermTrendText = "--";
        }
        
        snprintf(title, sizeof(title), "%s %s", 
                 binanceSymbol, getText("Trend Wijziging", "Trend Change"));
        snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\n%s: %s\n%s: %s",
                 prices[0], timestamp,
                 getText("Trend change", "Trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_2h, ret_30m_value,
                 getText("Volatiliteit", "Volatility"), volTextTranslated,
                 getText("Lange termijn", "Long term"), longTermTrendText);
        
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

<<<<<<< HEAD
// Lange termijn trend change detection en notificatie
void TrendDetector::checkLongTermTrendChange(float ret_4h_value, float ret_1d_value, float longTermThreshold) {
    unsigned long now = millis();
    
    // Bepaal nieuwe lange termijn trend state
    TrendState newLongTermTrend = determineLongTermTrendState(ret_4h_value, ret_1d_value, longTermThreshold);
    
    // Check of trend state is veranderd
    if (newLongTermTrend == this->longTermTrendState) {
        return; // Geen change, skip rest
    }
    
    // Geconsolideerde checks: cooldown en data validiteit
    bool cooldownPassed = (lastLongTermTrendChangeNotification == 0 || 
                          (now - lastLongTermTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
    bool hasValidData = (ret_4h_value != 0.0f && ret_1d_value != 0.0f);
    
    if (cooldownPassed && hasValidData) {
        // Geoptimaliseerd: gebruik helper functies
        const char* fromTrend = getTrendName(this->longTermTrendState);
        const char* toTrend = getTrendName(newLongTermTrend);
        const char* colorTag = getTrendColorTag(newLongTermTrend);
        
        // Gebruik lokale buffers (stack geheugen i.p.v. DRAM)
        char title[64];
        char msg[240];  // Verkleind van 256 naar 240 bytes (bespaart 16 bytes DRAM)
        char timestamp[32];
        getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
        
        // Vertaal trends naar juiste taal (LT voor beide talen)
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = "LT+";
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = "LT-";
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = "LT=";
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = "LT+";
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = "LT-";
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = "LT=";
        
        // Bepaal korte termijn trend voor context
        extern TrendState trendState;
        const char* shortTermTrendText = "";
        switch (trendState) {
            case TREND_UP:
                shortTermTrendText = getText("KT+", "ST+");
                break;
            case TREND_DOWN:
                shortTermTrendText = getText("KT-", "ST-");
                break;
            case TREND_SIDEWAYS:
            default:
                shortTermTrendText = getText("KT=", "ST=");
                break;
        }
        
        snprintf(title, sizeof(title), "%s %s", 
                 binanceSymbol, getText("LT Trend Wijziging", "LT Trend Change"));
        snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n4h: %+.2f%%\n1d: %+.2f%%\n%s: %s",
                 prices[0], timestamp,
                 getText("LT trend change", "LT trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_4h_value, ret_1d_value,
                 getText("Korte termijn", "Short term"), shortTermTrendText);
        
        // FASE X.2: Gebruik throttling wrapper voor Long Term Trend Change
        // Note: We gebruiken ALERT2H_TREND_CHANGE type omdat er geen apart type is voor LT
        // De throttling logica zal dit behandelen als een trend change notificatie
        if (AlertEngine::send2HNotification(ALERT2H_TREND_CHANGE, title, msg, colorTag)) {
            lastLongTermTrendChangeNotification = now;
        }
        
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[LT Trend] Lange termijn trend change notificatie verzonden: %s → %s (4h: %.2f%%, 1d: %.2f%%)\n"), 
                     fromTrend, toTrend, ret_4h_value, ret_1d_value);
        #endif
    }
    
    // Update previous long term trend state
    this->previousLongTermTrendState = this->longTermTrendState;
    this->longTermTrendState = newLongTermTrend;
}
