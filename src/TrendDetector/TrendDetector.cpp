#include "TrendDetector.h"

// FASE X.2: Include AlertEngine voor throttling
#include "../AlertEngine/AlertEngine.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char bitvavoSymbol[];  // Bitvavo market (bijv. "BTC-EUR")
extern float prices[];  // Voor notificatie formaat
extern float latestKnownPrice;
extern SemaphoreHandle_t dataMutex;
extern bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* context);
extern void safeMutexGive(SemaphoreHandle_t mutex, const char* context);
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

// Eerste ~45 s na boot: geen trend-wijziging NTFY (minder burst naar ntfy.sh / 429).
#ifndef NTFY_TREND_STARTUP_SUPPRESS_MS
#define NTFY_TREND_STARTUP_SUPPRESS_MS 45000UL
#endif

static float snapshotNotifDisplayPriceTrend(void)
{
    float p = prices[0];
    if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "trend notif price")) {
        float lk = latestKnownPrice;
        float px = prices[0];
        safeMutexGive(dataMutex, "trend notif price");
        p = (lk > 0.0f) ? lk : px;
    }
    return p;
}

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
    previousMediumTrendState = TREND_SIDEWAYS;
    longTermTrendState = TREND_SIDEWAYS;
    previousLongTermTrendState = TREND_SIDEWAYS;
    lastTrendChangeNotification = 0;
    lastMediumTrendChangeNotification = 0;
    lastLongTermTrendChangeNotification = 0;
}

// Helper: Validate return/threshold values
static inline bool isValidReturnValue(float value) {
    return !isnan(value) && !isinf(value);
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
    
    if (!isValidReturnValue(ret_2h_value) || !isValidReturnValue(ret_30m_value) ||
        !isValidReturnValue(trendThreshold) || trendThreshold <= 0.0f) {
        return TREND_SIDEWAYS;
    }
    
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

// Medium trend detection op basis van 1d return
TrendState TrendDetector::determineMediumTrendState(float ret_4h_value, float ret_1d_value, float mediumThreshold) {
    // Gebruik een hogere threshold voor 1d trend (default 2.0%)
    // 1d trends zijn minder gevoelig voor korte termijn fluctuaties
    (void)ret_4h_value;
    
    if (!isValidReturnValue(ret_1d_value) || !isValidReturnValue(mediumThreshold) || mediumThreshold <= 0.0f) {
        return TREND_SIDEWAYS;
    }

    if (ret_1d_value >= mediumThreshold) {
        return TREND_UP;
    }
    
    if (ret_1d_value <= -mediumThreshold) {
        return TREND_DOWN;
    }
    
    // SIDEWAYS: alles daartussenin
    return TREND_SIDEWAYS;
}

// Lange termijn trend detection op basis van 7d return
TrendState TrendDetector::determineLongTermTrendState(float ret_7d_value, float longTermThreshold) {
    if (!isValidReturnValue(ret_7d_value) || !isValidReturnValue(longTermThreshold) || longTermThreshold <= 0.0f) {
        return TREND_SIDEWAYS;
    }
    if (ret_7d_value >= longTermThreshold) {
        return TREND_UP;
    }
    if (ret_7d_value <= -longTermThreshold) {
        return TREND_DOWN;
    }
    return TREND_SIDEWAYS;
}

// Trend change detection en notificatie
void TrendDetector::checkTrendChange(float ret_30m_value, float ret_2h, bool minuteArrayFilled, uint8_t minuteIndex) {
    unsigned long now = millis();
    
    if (!isValidReturnValue(ret_2h) || !isValidReturnValue(ret_30m_value)) {
        return;
    }
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
        
        // Vertaal trends naar juiste taal (2h voor NL/EN)
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = "2h//";
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = "2h\\\\";
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = "2h=";
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = "2h//";
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = "2h\\\\";
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = "2h=";
        
        // VolText is al in Nederlands (getVolatilityText geeft "Rustig", "Gemiddeld", "Volatiel")
        // Vertaal naar Engels indien nodig
        const char* volTextTranslated = volText;
        if (language == 1) {  // English
            if (strcmp(volText, "Rustig") == 0) volTextTranslated = "Low";
            else if (strcmp(volText, "Gemiddeld") == 0) volTextTranslated = "Normal";
            else if (strcmp(volText, "Volatiel") == 0) volTextTranslated = "High";
        }
        
        extern bool hasRet1d;
        const char* mediumTrendText = "";
        if (hasRet1d) {
            extern float ret_1d;
            const float mediumThreshold = 2.0f;
            TrendState mediumTrend = this->determineMediumTrendState(0.0f, ret_1d, mediumThreshold);
            switch (mediumTrend) {
                case TREND_UP:
                    mediumTrendText = "1d//";
                    break;
                case TREND_DOWN:
                    mediumTrendText = "1d\\\\";
                    break;
                case TREND_SIDEWAYS:
                default:
                    mediumTrendText = "1d=";
                    break;
            }
        } else {
            mediumTrendText = "--";
        }

        // Bepaal lange termijn trend (7d) voor notificatie
        extern bool hasRet7d;
        const char* longTermTrendText = "";
        if (hasRet7d) {
            extern float ret_7d;
            const float longTermThreshold = 1.0f;
            TrendState longTermTrend = this->determineLongTermTrendState(ret_7d, longTermThreshold);
            switch (longTermTrend) {
                case TREND_UP:
                    longTermTrendText = "7d//";
                    break;
                case TREND_DOWN:
                    longTermTrendText = "7d\\\\";
                    break;
                case TREND_SIDEWAYS:
                default:
                    longTermTrendText = "7d=";
                    break;
            }
        } else {
            longTermTrendText = "--";
        }
        
        // Titel met richting:
        // UP      → 🟩 ↗️ BTC-EUR Trend Wijziging
        // DOWN    → 🟥 ↘️ BTC-EUR Trend Wijziging
        // SIDEWAYS→ 🟨 ↔️ BTC-EUR Trend Wijziging
        const bool toUp = (strcmp(toTrend, "UP") == 0);
        const bool toDown = (strcmp(toTrend, "DOWN") == 0);
        const char* trendColor = toUp
            ? "\xF0\x9F\x9F\xA9"         // 🟩
            : (toDown ? "\xF0\x9F\x9F\xA5" // 🟥
                      : "\xF0\x9F\x9F\xA8"); // 🟨
        const char* trendArrow = toUp
            ? "\xE2\x86\x97\xEF\xB8\x8F"  // ↗️
            : (toDown ? "\xE2\x86\x98\xEF\xB8\x8F" // ↘️
                      : "\xE2\x86\x94\xEF\xB8\x8F"); // ↔️
        snprintf(title, sizeof(title), "%s %s %s %s",
                 trendColor,
                 trendArrow,
                 bitvavoSymbol,
                 getText("Trend Wijziging", "Trend Change"));
            const float ntfPrice2h = snapshotNotifDisplayPriceTrend();
            snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\n%s: %s\n%s: %s",
                 ntfPrice2h, timestamp,
                 getText("Trend change", "Trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_2h, ret_30m_value,
                 getText("Volatiliteit", "Volatility"), volTextTranslated,
                 getText("1d trend", "1d trend"), mediumTrendText,
                 getText("7d trend", "7d trend"), longTermTrendText);
        
        // FASE X.2: Gebruik throttling wrapper voor Trend Change
        if (millis() < NTFY_TREND_STARTUP_SUPPRESS_MS) {
            #if !DEBUG_BUTTON_ONLY
            unsigned long msLeft = NTFY_TREND_STARTUP_SUPPRESS_MS - millis();
            Serial_printf(F("[Trend] startup: 2h trend NTFY suppressed (%lu ms left)\n"), msLeft);
            #endif
        } else {
            const Alert2HDispatchResult dr =
                AlertEngine::dispatch2HNotification(ALERT2H_TREND_CHANGE, title, msg, colorTag);
            if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                lastTrendChangeNotification = now;
            }
            #if !DEBUG_BUTTON_ONLY
            if (dr == Alert2HDispatchResult::DISPATCH_BLOCKED) {
                Serial_printf(F("[Trend] Trend change niet uitgeleverd (2h throttle/coalesce): %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"),
                              fromTrend, toTrend, ret_2h, ret_30m_value, volText);
            } else if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED) {
                Serial_printf(F("[Trend] Trend change opgenomen in 2h pending: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"),
                              fromTrend, toTrend, ret_2h, ret_30m_value, volText);
            } else if (dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                Serial_printf(F("[Trend] Trend change direct verzonden: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"),
                              fromTrend, toTrend, ret_2h, ret_30m_value, volText);
            }
            #endif
        }
        }
        
        // Update previous trend state
        this->previousTrendState = this->trendState;
        
        // Update globale variabelen voor backward compatibility
        extern TrendState trendState;
        extern TrendState previousTrendState;
        trendState = this->trendState;
        previousTrendState = this->previousTrendState;
    }

// Medium trend change detection en notificatie
void TrendDetector::checkMediumTrendChange(float ret_4h_value, float ret_1d_value, float mediumThreshold) {
    unsigned long now = millis();
    (void)ret_4h_value;
    
    if (!isValidReturnValue(ret_1d_value) || !isValidReturnValue(mediumThreshold) || mediumThreshold <= 0.0f) {
        return;
    }
    
    // Bepaal nieuwe medium trend state
    TrendState newMediumTrend = determineMediumTrendState(ret_4h_value, ret_1d_value, mediumThreshold);
    
    // Check of trend state is veranderd
    if (newMediumTrend == this->mediumTrendState) {
        return; // Geen change, skip rest
    }
    
    // Geconsolideerde checks: cooldown en data validiteit
    bool cooldownPassed = (lastMediumTrendChangeNotification == 0 || 
                          (now - lastMediumTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
    bool hasValidData = (ret_4h_value != 0.0f && ret_1d_value != 0.0f);
    
    if (cooldownPassed && hasValidData) {
        // Geoptimaliseerd: gebruik helper functies
        const char* fromTrend = getTrendName(this->mediumTrendState);
        const char* toTrend = getTrendName(newMediumTrend);
        const char* colorTag = getTrendColorTag(newMediumTrend);
        
        // Gebruik lokale buffers (stack geheugen i.p.v. DRAM)
        char title[64];
        char msg[240];  // Verkleind van 256 naar 240 bytes (bespaart 16 bytes DRAM)
        char timestamp[32];
        getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
        
        // Vertaal trends naar juiste taal (1d voor beide talen, gebruik //, \\, = zoals in UI)
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = "1d//";
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = "1d\\\\";
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = "1d=";
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = "1d//";
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = "1d\\\\";
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = "1d=";
        
        // Bepaal korte termijn trend voor context
        extern TrendState trendState;
        const char* shortTermTrendText = "";
        switch (trendState) {
            case TREND_UP:
                shortTermTrendText = "2h//";
                break;
            case TREND_DOWN:
                shortTermTrendText = "2h\\\\";
                break;
            case TREND_SIDEWAYS:
            default:
                shortTermTrendText = "2h=";
                break;
        }
        
        // 1d Trend change:
        // UP      → 🟩 ↗️ BTC-EUR 1d Trend Wijziging
        // DOWN    → 🟥 ↘️ BTC-EUR 1d Trend Wijziging
        // SIDEWAYS→ 🟨 ↔️ BTC-EUR 1d Trend Wijziging
        const bool toUp = (strcmp(toTrend, "UP") == 0);
        const bool toDown = (strcmp(toTrend, "DOWN") == 0);
        const char* trendColor = toUp
            ? "\xF0\x9F\x9F\xA9"         // 🟩
            : (toDown ? "\xF0\x9F\x9F\xA5" // 🟥
                      : "\xF0\x9F\x9F\xA8"); // 🟨
        const char* trendArrow = toUp
            ? "\xE2\x86\x97\xEF\xB8\x8F"  // ↗️
            : (toDown ? "\xE2\x86\x98\xEF\xB8\x8F" // ↘️
                      : "\xE2\x86\x94\xEF\xB8\x8F"); // ↔️
        snprintf(title, sizeof(title), "%s %s %s %s",
                 trendColor,
                 trendArrow,
                 bitvavoSymbol,
                 getText("1d Trend Wijziging", "1d Trend Change"));
        const float ntfPrice1d = snapshotNotifDisplayPriceTrend();
        snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n1d: %+.2f%%\n%s: %s",
                 ntfPrice1d, timestamp,
                 getText("1d trend change", "1d trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_1d_value,
                 getText("2h trend", "2h trend"), shortTermTrendText);
        
        // FASE X.2: Gebruik throttling wrapper voor 1d Trend Change
        // Note: We gebruiken ALERT2H_TREND_CHANGE type omdat er geen apart type is voor 1d
        // De throttling logica zal dit behandelen als een trend change notificatie
        if (millis() < NTFY_TREND_STARTUP_SUPPRESS_MS) {
            #if !DEBUG_BUTTON_ONLY
            unsigned long msLeft = NTFY_TREND_STARTUP_SUPPRESS_MS - millis();
            Serial_printf(F("[1d Trend] startup: 1d trend NTFY suppressed (%lu ms left)\n"), msLeft);
            #endif
        } else {
            const Alert2HDispatchResult dr =
                AlertEngine::dispatch2HNotification(ALERT2H_TREND_CHANGE, title, msg, colorTag);
            if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                lastMediumTrendChangeNotification = now;
            }
            #if !DEBUG_BUTTON_ONLY
            if (dr == Alert2HDispatchResult::DISPATCH_BLOCKED) {
                Serial_printf(F("[1d Trend] 1d trend change niet uitgeleverd (2h throttle/coalesce): %s → %s (1d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_1d_value);
            } else if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED) {
                Serial_printf(F("[1d Trend] 1d trend change opgenomen in 2h pending: %s → %s (1d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_1d_value);
            } else if (dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                Serial_printf(F("[1d Trend] 1d trend change direct verzonden: %s → %s (1d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_1d_value);
            }
            #endif
        }
    }
    
    // Update previous medium trend state
    this->previousMediumTrendState = this->mediumTrendState;
    this->mediumTrendState = newMediumTrend;
}

// Lange termijn trend change detection en notificatie
void TrendDetector::checkLongTermTrendChange(float ret_7d_value, float longTermThreshold) {
    unsigned long now = millis();
    
    if (!isValidReturnValue(ret_7d_value) || !isValidReturnValue(longTermThreshold) || longTermThreshold <= 0.0f) {
        return;
    }
    
    // Bepaal nieuwe lange termijn trend state
    TrendState newLongTermTrend = determineLongTermTrendState(ret_7d_value, longTermThreshold);
    
    // Check of trend state is veranderd
    if (newLongTermTrend == this->longTermTrendState) {
        return; // Geen change, skip rest
    }
    
    // Geconsolideerde checks: cooldown en data validiteit
    bool cooldownPassed = (lastLongTermTrendChangeNotification == 0 || 
                          (now - lastLongTermTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
    bool hasValidData = (ret_7d_value != 0.0f);
    
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
        
        // Vertaal trends naar juiste taal (7d voor beide talen, gebruik //, \\, = zoals in UI)
        const char* fromTrendTranslated = fromTrend;
        const char* toTrendTranslated = toTrend;
        if (strcmp(fromTrend, "UP") == 0) fromTrendTranslated = "7d//";
        else if (strcmp(fromTrend, "DOWN") == 0) fromTrendTranslated = "7d\\\\";
        else if (strcmp(fromTrend, "SIDEWAYS") == 0) fromTrendTranslated = "7d=";
        if (strcmp(toTrend, "UP") == 0) toTrendTranslated = "7d//";
        else if (strcmp(toTrend, "DOWN") == 0) toTrendTranslated = "7d\\\\";
        else if (strcmp(toTrend, "SIDEWAYS") == 0) toTrendTranslated = "7d=";
        
        // Bepaal korte termijn trend voor context
        extern TrendState trendState;
        const char* shortTermTrendText = "";
        switch (trendState) {
            case TREND_UP:
                shortTermTrendText = "2h//";
                break;
            case TREND_DOWN:
                shortTermTrendText = "2h\\\\";
                break;
            case TREND_SIDEWAYS:
            default:
                shortTermTrendText = "2h=";
                break;
        }
        
        // 7d Trend change:
        // UP      → 🟩 ↗️ BTC-EUR 7d Trend Wijziging
        // DOWN    → 🟥 ↘️ BTC-EUR 7d Trend Wijziging
        // SIDEWAYS→ 🟨 ↔️ BTC-EUR 7d Trend Wijziging
        const bool toUp = (strcmp(toTrend, "UP") == 0);
        const bool toDown = (strcmp(toTrend, "DOWN") == 0);
        const char* trendColor = toUp
            ? "\xF0\x9F\x9F\xA9"         // 🟩
            : (toDown ? "\xF0\x9F\x9F\xA5" // 🟥
                      : "\xF0\x9F\x9F\xA8"); // 🟨
        const char* trendArrow = toUp
            ? "\xE2\x86\x97\xEF\xB8\x8F"  // ↗️
            : (toDown ? "\xE2\x86\x98\xEF\xB8\x8F" // ↘️
                      : "\xE2\x86\x94\xEF\xB8\x8F"); // ↔️
        snprintf(title, sizeof(title), "%s %s %s %s",
                 trendColor,
                 trendArrow,
                 bitvavoSymbol,
                 getText("7d Trend Wijziging", "7d Trend Change"));
        const float ntfPrice7d = snapshotNotifDisplayPriceTrend();
        snprintf(msg, sizeof(msg), 
                 "%.2f (%s)\n%s: %s → %s\n7d: %+.2f%%\n%s: %s",
                 ntfPrice7d, timestamp,
                 getText("7d trend change", "7d trend change"), fromTrendTranslated, toTrendTranslated,
                 ret_7d_value,
                 getText("2h trend", "2h trend"), shortTermTrendText);
        
        // FASE X.2: Gebruik throttling wrapper voor 7d Trend Change
        // Note: We gebruiken ALERT2H_TREND_CHANGE type omdat er geen apart type is voor 7d
        // De throttling logica zal dit behandelen als een trend change notificatie
        if (millis() < NTFY_TREND_STARTUP_SUPPRESS_MS) {
            #if !DEBUG_BUTTON_ONLY
            unsigned long msLeft = NTFY_TREND_STARTUP_SUPPRESS_MS - millis();
            Serial_printf(F("[7d Trend] startup: 7d trend NTFY suppressed (%lu ms left)\n"), msLeft);
            #endif
        } else {
            const Alert2HDispatchResult dr =
                AlertEngine::dispatch2HNotification(ALERT2H_TREND_CHANGE, title, msg, colorTag);
            if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                lastLongTermTrendChangeNotification = now;
            }
            #if !DEBUG_BUTTON_ONLY
            if (dr == Alert2HDispatchResult::DISPATCH_BLOCKED) {
                Serial_printf(F("[7d Trend] 7d trend change niet uitgeleverd (2h throttle/coalesce): %s → %s (7d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_7d_value);
            } else if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED) {
                Serial_printf(F("[7d Trend] 7d trend change opgenomen in 2h pending: %s → %s (7d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_7d_value);
            } else if (dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
                Serial_printf(F("[7d Trend] 7d trend change direct verzonden: %s → %s (7d: %.2f%%)\n"),
                              fromTrend, toTrend, ret_7d_value);
            }
            #endif
        }
    }
    
    // Update previous long term trend state
    this->previousLongTermTrendState = this->longTermTrendState;
    this->longTermTrendState = newLongTermTrend;
}
