#include "AlertEngine.h"
#include <WiFi.h>

// SettingsStore module (Fase 6.1.10: voor AlertThresholds en NotificationCooldowns structs)
#include "../SettingsStore/SettingsStore.h"

// VolatilityTracker module (Fase 6.1.10: voor checkAndNotify - moet eerst voor EffectiveThresholds)
#include "../VolatilityTracker/VolatilityTracker.h"
extern VolatilityTracker volatilityTracker;  // Fase 6.1.10: Voor checkAndNotify

// TrendDetector module (Fase 6.1.9: voor trendSupportsDirection)
#include "../TrendDetector/TrendDetector.h"
extern TrendDetector trendDetector;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert

// Alert2HThresholds module (voor 2h alert thresholds)
#include "Alert2HThresholds.h"

// Extern declaration voor 2h alert thresholds (wordt geladen vanuit settings)
extern Alert2HThresholds alert2HThresholds;

// PriceData module (voor fiveMinutePrices getter)
#include "../PriceData/PriceData.h"
extern PriceData priceData;  // Voor getFiveMinutePrices()

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char binanceSymbol[];
extern float prices[];  // Fase 6.1.4: Voor formatNotificationMessage
void getFormattedTimestamp(char* buffer, size_t bufferSize);  // Fase 6.1.4: Voor formatNotificationMessage
extern bool smartConfluenceEnabled;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert

// Forward declaration voor computeTwoHMetrics()
TwoHMetrics computeTwoHMetrics();

// Persistent runtime state voor 2h notificaties
static Alert2HState gAlert2H;

// Fase 6.1.10: Forward declarations voor checkAndNotify dependencies
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
void logVolatilityStatus(const EffectiveThresholds& eff);
// Fase 6.1.10: fiveMinutePrices kan pointer zijn (CYD) of array (andere platforms)
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
extern float *fiveMinutePrices;
#else
extern float fiveMinutePrices[];
#endif

// Fase 6.1.10: Thresholds zijn #define macros die verwijzen naar struct velden
// We gebruiken de structs direct i.p.v. de macro namen
extern AlertThresholds alertThresholds;
extern NotificationCooldowns notificationCooldowns;

// Constants
#define CONFLUENCE_TIME_WINDOW_MS 300000UL  // 5 minuten tijdshorizon voor confluence
#define MAX_1M_ALERTS_PER_HOUR 3
#define MAX_30M_ALERTS_PER_HOUR 2
#define MAX_5M_ALERTS_PER_HOUR 3
#define SECONDS_PER_5MINUTES 300

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

// Constructor - initialiseer state variabelen
AlertEngine::AlertEngine() {
    // Fase 6.1.6: Initialiseer alert state variabelen
    lastNotification1Min = 0;
    lastNotification30Min = 0;
    lastNotification5Min = 0;
    alerts1MinThisHour = 0;
    alerts30MinThisHour = 0;
    alerts5MinThisHour = 0;
    hourStartTime = 0;
    
    // Fase 6.1.7: Initialiseer Smart Confluence Mode state variabelen
    last1mEvent = {EVENT_NONE, 0, 0.0f, false};
    last5mEvent = {EVENT_NONE, 0, 0.0f, false};
    lastConfluenceAlert = 0;
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void AlertEngine::begin() {
    // Fase 6.1.1: Basis structuur - sync wordt incrementeel geïmplementeerd
    syncStateFromGlobals();
}

// Helper: Check if cooldown has passed and hourly limit is OK
// Fase 6.1.2: Verplaatst naar AlertEngine (parallel implementatie)
bool AlertEngine::checkAlertConditions(unsigned long now, unsigned long& lastNotification, unsigned long cooldownMs, 
                                       uint8_t& alertsThisHour, uint8_t maxAlertsPerHour, const char* alertType)
{
    bool cooldownPassed = (lastNotification == 0 || (now - lastNotification >= cooldownMs));
    bool hourlyLimitOk = (alertsThisHour < maxAlertsPerHour);
    
    if (!hourlyLimitOk) {
        Serial_printf("[Notify] %s gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", 
                     alertType, alertsThisHour, maxAlertsPerHour);
    }
    
    return cooldownPassed && hourlyLimitOk;
}

// Helper: Determine color tag based on return value and threshold
// Fase 6.1.3: Verplaatst naar AlertEngine (parallel implementatie)
const char* AlertEngine::determineColorTag(float ret, float threshold, float strongThreshold)
{
    float absRet = fabsf(ret);
    if (ret > 0) {
        // Stijging: blauw voor normale (🔼), paars voor strong threshold (⏫️)
        return (absRet >= strongThreshold) ? "purple_square,⏫️" : "blue_square,🔼";
    } else {
        // Daling: oranje voor normale (🔽), rood voor strong threshold (⏬️)
        return (absRet >= strongThreshold) ? "red_square,⏬️" : "orange_square,🔽";
    }
}

// Helper: Format notification message with timestamp, price, and min/max
// Fase 6.1.4: Verplaatst naar AlertEngine (parallel implementatie)
void AlertEngine::formatNotificationMessage(char* msg, size_t msgSize, float ret, const char* direction, 
                                            float minVal, float maxVal)
{
    char timestamp[32];
    getFormattedTimestamp(timestamp, sizeof(timestamp));
    
    if (ret >= 0) {
        snprintf(msg, msgSize, 
                "%s UP %s: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                direction, direction, ret, timestamp, prices[0], maxVal, minVal);
    } else {
        snprintf(msg, msgSize, 
                "%s DOWN %s: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                direction, direction, ret, timestamp, prices[0], maxVal, minVal);
    }
}

// Helper: Send alert notification with all checks
// Fase 6.1.5: Verplaatst naar AlertEngine (parallel implementatie)
bool AlertEngine::sendAlertNotification(float ret, float threshold, float strongThreshold, 
                                        unsigned long now, unsigned long& lastNotification, 
                                        unsigned long cooldownMs, uint8_t& alertsThisHour, 
                                        uint8_t maxAlertsPerHour, const char* alertType, 
                                        const char* direction, float minVal, float maxVal)
{
    if (!checkAlertConditions(now, lastNotification, cooldownMs, alertsThisHour, maxAlertsPerHour, alertType)) {
        return false;
    }
    
    char msg[256];
    formatNotificationMessage(msg, sizeof(msg), ret, direction, minVal, maxVal);
    
    const char* colorTag = determineColorTag(ret, threshold, strongThreshold);
    
    char title[48];  // Verkleind van 64 naar 48 bytes
    snprintf(title, sizeof(title), "%s %s Alert", binanceSymbol, alertType);
    
    sendNotification(title, msg, colorTag);
    lastNotification = now;
    alertsThisHour++;
    Serial_printf(F("[Notify] %s notificatie verstuurd (%d/%d dit uur)\n"), alertType, alertsThisHour, maxAlertsPerHour);
    
    return true;
}

// Sync state: Update AlertEngine state met globale variabelen
void AlertEngine::syncStateFromGlobals() {
    // Fase 6.1.6: Synchroniseer alert state variabelen met globale variabelen (parallel implementatie)
    extern unsigned long lastNotification1Min;
    extern unsigned long lastNotification30Min;
    extern unsigned long lastNotification5Min;
    extern uint8_t alerts1MinThisHour;
    extern uint8_t alerts30MinThisHour;
    extern uint8_t alerts5MinThisHour;
    extern unsigned long hourStartTime;
    
    // Kopieer waarden van globale variabelen naar AlertEngine state
    this->lastNotification1Min = lastNotification1Min;
    this->lastNotification30Min = lastNotification30Min;
    this->lastNotification5Min = lastNotification5Min;
    this->alerts1MinThisHour = alerts1MinThisHour;
    this->alerts30MinThisHour = alerts30MinThisHour;
    this->alerts5MinThisHour = alerts5MinThisHour;
    this->hourStartTime = hourStartTime;
    
    // Fase 6.1.7: Synchroniseer Smart Confluence Mode state variabelen
    extern LastOneMinuteEvent last1mEvent;
    extern LastFiveMinuteEvent last5mEvent;
    extern unsigned long lastConfluenceAlert;
    
    this->last1mEvent = last1mEvent;
    this->last5mEvent = last5mEvent;
    this->lastConfluenceAlert = lastConfluenceAlert;
}

// Update 1m event state voor Smart Confluence Mode
// Fase 6.1.8: Verplaatst naar AlertEngine (parallel implementatie)
void AlertEngine::update1mEvent(float ret_1m, unsigned long timestamp, float threshold)
{
    float absRet1m = fabsf(ret_1m);
    if (absRet1m >= threshold) {
        last1mEvent.direction = (ret_1m > 0) ? EVENT_UP : EVENT_DOWN;
        last1mEvent.timestamp = timestamp;
        last1mEvent.magnitude = absRet1m;
        last1mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Update 5m event state voor Smart Confluence Mode
// Fase 6.1.8: Verplaatst naar AlertEngine (parallel implementatie)
void AlertEngine::update5mEvent(float ret_5m, unsigned long timestamp, float threshold)
{
    float absRet5m = fabsf(ret_5m);
    if (absRet5m >= threshold) {
        last5mEvent.direction = (ret_5m > 0) ? EVENT_UP : EVENT_DOWN;
        last5mEvent.timestamp = timestamp;
        last5mEvent.magnitude = absRet5m;
        last5mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Helper: Check if two events are within confluence time window
static bool eventsWithinTimeWindow(unsigned long timestamp1, unsigned long timestamp2, unsigned long now)
{
    if (timestamp1 == 0 || timestamp2 == 0) return false;
    unsigned long timeDiff = (timestamp1 > timestamp2) ? (timestamp1 - timestamp2) : (timestamp2 - timestamp1);
    return (timeDiff <= CONFLUENCE_TIME_WINDOW_MS);
}

// Helper: Check if 30m trend supports the direction (UP/DOWN)
static bool trendSupportsDirection(EventDirection direction)
{
    // Fase 6.1.9: Gebruik TrendDetector module getter
    TrendState currentTrend = trendDetector.getTrendState();
    if (direction == EVENT_UP) {
        // UP-confluence: 30m trend moet UP zijn of op zijn minst niet sterk DOWN
        return (currentTrend == TREND_UP || currentTrend == TREND_SIDEWAYS);
    } else if (direction == EVENT_DOWN) {
        // DOWN-confluence: 30m trend moet DOWN zijn of op zijn minst niet sterk UP
        return (currentTrend == TREND_DOWN || currentTrend == TREND_SIDEWAYS);
    }
    return false;
}

// Check for confluence and send combined alert if found
// Fase 6.1.9: Verplaatst naar AlertEngine (parallel implementatie)
bool AlertEngine::checkAndSendConfluenceAlert(unsigned long now, float ret_30m)
{
    if (!smartConfluenceEnabled) return false;
    
    // Check if we have valid 1m and 5m events
    if (last1mEvent.direction == EVENT_NONE || last5mEvent.direction == EVENT_NONE) {
        return false;
    }
    
    // Check if events are already used in confluence
    if (last1mEvent.usedInConfluence || last5mEvent.usedInConfluence) {
        return false;
    }
    
    // Check if events are within time window
    if (!eventsWithinTimeWindow(last1mEvent.timestamp, last5mEvent.timestamp, now)) {
        return false;
    }
    
    // Check if both events are in the same direction
    if (last1mEvent.direction != last5mEvent.direction) {
        return false;
    }
    
    // Check if 30m trend supports the direction
    if (!trendSupportsDirection(last1mEvent.direction)) {
        return false;
    }
    
    // Check cooldown (prevent spam)
    if (lastConfluenceAlert > 0 && (now - lastConfluenceAlert) < CONFLUENCE_TIME_WINDOW_MS) {
        return false;
    }
    
    // Confluence detected! Send combined alert
    EventDirection direction = last1mEvent.direction;
    const char* directionText = (direction == EVENT_UP) ? "UP" : "DOWN";
    const char* trendText = "";
    // Fase 6.1.9: Gebruik TrendDetector module getter
    TrendState currentTrend = trendDetector.getTrendState();
    switch (currentTrend) {
        case TREND_UP: trendText = "UP"; break;
        case TREND_DOWN: trendText = "DOWN"; break;
        case TREND_SIDEWAYS: trendText = "SIDEWAYS"; break;
    }
    
    char timestamp[32];
    getFormattedTimestamp(timestamp, sizeof(timestamp));
    char title[64];  // Verkleind van 80 naar 64 bytes
    snprintf(title, sizeof(title), "%s Confluence Alert (1m+5m+Trend)", binanceSymbol);
    
    char msg[256];  // Verkleind van 320 naar 256 bytes
    if (direction == EVENT_UP) {
        snprintf(msg, sizeof(msg),
                 "Confluence %s gedetecteerd!\n\n"
                 "1m: +%.2f%%\n"
                 "5m: +%.2f%%\n"
                 "30m Trend: %s (%.2f%%)\n\n"
                 "Prijs %s: %.2f",
                 directionText,
                 last1mEvent.magnitude,
                 last5mEvent.magnitude,
                 trendText, ret_30m,
                 timestamp, prices[0]);
    } else {
        snprintf(msg, sizeof(msg),
                 "Confluence %s gedetecteerd!\n\n"
                 "1m: %.2f%%\n"
                 "5m: %.2f%%\n"
                 "30m Trend: %s (%.2f%%)\n\n"
                 "Prijs %s: %.2f",
                 directionText,
                 -last1mEvent.magnitude,
                 -last5mEvent.magnitude,
                 trendText, ret_30m,
                 timestamp, prices[0]);
    }
    
    const char* colorTag = (direction == EVENT_UP) ? "green_square,📈" : "red_square,📉";
    sendNotification(title, msg, colorTag);
    
    // Mark events as used
    last1mEvent.usedInConfluence = true;
    last5mEvent.usedInConfluence = true;
    lastConfluenceAlert = now;
    
    Serial_printf(F("[Confluence] Alert verzonden: 1m=%.2f%%, 5m=%.2f%%, trend=%s, ret_30m=%.2f%%\n"),
                  (direction == EVENT_UP ? last1mEvent.magnitude : -last1mEvent.magnitude),
                  (direction == EVENT_UP ? last5mEvent.magnitude : -last5mEvent.magnitude),
                  trendText, ret_30m);
    
    return true;
}

// Main alert checking function
// Fase 6.1.10: Verplaatst naar AlertEngine (parallel implementatie)
void AlertEngine::checkAndNotify(float ret_1m, float ret_5m, float ret_30m)
{
    unsigned long now = millis();
    
    // Update volatility window met nieuwe 1m return (Auto-Volatility Mode)
    if (ret_1m != 0.0f) {
        volatilityTracker.updateVolatilityWindow(ret_1m);
    }
    
    // Bereken effective thresholds (Auto-Volatility Mode)
    // Fase 6.1.10: Gebruik struct velden direct i.p.v. #define macros
    EffectiveThresholds effThresh = volatilityTracker.calculateEffectiveThresholds(
        alertThresholds.spike1m, 
        alertThresholds.move5mAlert, 
        alertThresholds.move30m);
    
    // Log volatility status (voor debug)
    logVolatilityStatus(effThresh);
    
    // Reset tellers elk uur
    if (hourStartTime == 0 || (now - hourStartTime >= 3600000UL)) { // 1 uur = 3600000 ms
        alerts1MinThisHour = 0;
        alerts30MinThisHour = 0;
        alerts5MinThisHour = 0;
        hourStartTime = now;
        Serial_printf(F("[Notify] Uur-tellers gereset\n"));
    }
    
    // ===== 1-MINUUT SPIKE ALERT =====
    // Voorwaarde: |ret_1m| >= effectiveSpike1mThreshold EN |ret_5m| >= spike5mThreshold in dezelfde richting
    if (ret_1m != 0.0f && ret_5m != 0.0f)
    {
        float absRet1m = fabsf(ret_1m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn (beide positief of beide negatief)
        bool sameDirection = ((ret_1m > 0 && ret_5m > 0) || (ret_1m < 0 && ret_5m < 0));
        
        // Threshold check: ret_1m >= effectiveSpike1mThreshold EN ret_5m >= spike5mThreshold
        // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
        bool spikeDetected = (absRet1m >= effThresh.spike1m) && (absRet5m >= alertThresholds.spike5m) && sameDirection;
        
        // Update 1m event state voor Smart Confluence Mode
        if (spikeDetected) {
            update1mEvent(ret_1m, now, effThresh.spike1m);
        }
        
        // Debug logging alleen bij spike detectie
        if (spikeDetected) {
            Serial_printf(F("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%\n"), ret_1m, ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf(F("[Notify] 1m spike onderdrukt (gebruikt in confluence alert)\n"));
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                    Serial_printf(F("[Notify] 1m spike onderdrukt (al gebruikt in confluence)\n"));
                } else {
                    // Bereken min en max uit secondPrices buffer
                    float minVal, maxVal;
                    findMinMaxInSecondPrices(minVal, maxVal);
                    
                    // Format message with 5m info
                    char timestamp[32];
                    getFormattedTimestamp(timestamp, sizeof(timestamp));
                    char msg[256];
                    if (ret_1m >= 0) {
                        snprintf(msg, sizeof(msg), 
                                 "1m UP spike: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
                    } else {
                        snprintf(msg, sizeof(msg), 
                                 "1m DOWN spike: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
                    }
                    
                    const char* colorTag = determineColorTag(ret_1m, effThresh.spike1m, effThresh.spike1m * 1.5f);
                    char title[48];  // Verkleind van 64 naar 48 bytes
                    snprintf(title, sizeof(title), "%s 1m Spike Alert", binanceSymbol);
                    
                    // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    if (checkAlertConditions(now, lastNotification1Min, notificationCooldowns.cooldown1MinMs, 
                                             alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR, "1m spike")) {
                        sendNotification(title, msg, colorTag);
                        lastNotification1Min = now;
                        alerts1MinThisHour++;
                        Serial_printf(F("[Notify] 1m spike notificatie verstuurd (%d/%d dit uur)\n"), alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
                    }
                }
            }
        }
    }
    
    // ===== 30-MINUTEN TREND MOVE ALERT =====
    // Voorwaarde: |ret_30m| >= effectiveMove30mThreshold EN |ret_5m| >= move5mThreshold in dezelfde richting
    if (ret_30m != 0.0f && ret_5m != 0.0f)
    {
        float absRet30m = fabsf(ret_30m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn
        bool sameDirection = ((ret_30m > 0 && ret_5m > 0) || (ret_30m < 0 && ret_5m < 0));
        
        // Threshold check: ret_30m >= effectiveMove30mThreshold EN ret_5m >= move5mThreshold
        // Note: move5mThreshold is de filter threshold, niet de alert threshold
        // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
        bool moveDetected = (absRet30m >= effThresh.move30m) && (absRet5m >= alertThresholds.move5m) && sameDirection;
        
        // Debug logging alleen bij move detectie
        if (moveDetected) {
            Serial_printf(F("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%\n"), ret_30m, ret_5m);
            
            // Bereken min en max uit laatste 30 minuten van minuteAverages buffer
            float minVal, maxVal;
            findMinMaxInLast30Minutes(minVal, maxVal);
            
            // Format message with 5m info
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char msg[256];
            if (ret_30m >= 0) {
                snprintf(msg, sizeof(msg), 
                         "30m UP move: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            } else {
                snprintf(msg, sizeof(msg), 
                         "30m DOWN move: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            }
            
            const char* colorTag = determineColorTag(ret_30m, effThresh.move30m, effThresh.move30m * 1.5f);
            char title[64];
            snprintf(title, sizeof(title), "%s 30m Move Alert", binanceSymbol);
            
            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
            if (checkAlertConditions(now, lastNotification30Min, notificationCooldowns.cooldown30MinMs, 
                                     alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR, "30m move")) {
                sendNotification(title, msg, colorTag);
                lastNotification30Min = now;
                alerts30MinThisHour++;
                Serial_printf(F("[Notify] 30m move notificatie verstuurd (%d/%d dit uur)\n"), alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
            }
        }
    }
    
    // ===== 5-MINUTEN MOVE ALERT =====
    // Voorwaarde: |ret_5m| >= effectiveMove5mThreshold
    if (ret_5m != 0.0f)
    {
        float absRet5m = fabsf(ret_5m);
        
        // Threshold check: ret_5m >= effectiveMove5mThreshold
        bool move5mDetected = (absRet5m >= effThresh.move5m);
        
        // Update 5m event state voor Smart Confluence Mode
        if (move5mDetected) {
            update5mEvent(ret_5m, now, effThresh.move5m);
        }
        
        // Debug logging alleen bij move detectie
        if (move5mDetected) {
            Serial_printf(F("[Notify] 5m move: ret_5m=%.2f%%\n"), ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf(F("[Notify] 5m move onderdrukt (gebruikt in confluence alert)\n"));
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last5mEvent.usedInConfluence) {
                    Serial_printf(F("[Notify] 5m move onderdrukt (al gebruikt in confluence)\n"));
                } else {
                    // Bereken min en max uit fiveMinutePrices buffer via PriceData module
                    const float* fiveMinPrices = priceData.getFiveMinutePrices();
                    uint16_t fiveMinIndex = priceData.getFiveMinuteIndex();
                    bool fiveMinArrayFilled = priceData.getFiveMinuteArrayFilled();
                    float minVal = 0.0f;
                    float maxVal = 0.0f;
                    bool foundValidValue = false;
                    
                    // Bepaal hoeveel elementen we moeten checken (alleen gevulde elementen)
                    uint16_t elementsToCheck = fiveMinArrayFilled ? SECONDS_PER_5MINUTES : fiveMinIndex;
                    
                    // Als array leeg is, gebruik huidige prijs
                    if (elementsToCheck == 0) {
                        minVal = prices[0];
                        maxVal = prices[0];
                    } else {
                        // Zoek eerste geldige waarde (> 0.0 en < 1e6 om extreem hoge waarden uit te sluiten) als startwaarde
                        for (uint16_t i = 0; i < elementsToCheck; i++) {
                            if (fiveMinPrices[i] > 0.0f && fiveMinPrices[i] < 1000000.0f) {  // Validatie: prijs moet redelijk zijn
                                minVal = fiveMinPrices[i];
                                maxVal = fiveMinPrices[i];
                                foundValidValue = true;
                                break;
                            }
                        }
                        
                        // Als geen geldige waarde gevonden, gebruik huidige prijs als fallback
                        if (!foundValidValue) {
                            minVal = prices[0];
                            maxVal = prices[0];
                        } else {
                            // Zoek min en max in rest van array met validatie
                            for (uint16_t i = 0; i < elementsToCheck; i++) {
                                if (fiveMinPrices[i] > 0.0f && fiveMinPrices[i] < 1000000.0f) {  // Validatie: prijs moet redelijk zijn
                                    if (fiveMinPrices[i] < minVal) minVal = fiveMinPrices[i];
                                    if (fiveMinPrices[i] > maxVal) maxVal = fiveMinPrices[i];
                                }
                            }
                        }
                        
                        // Extra validatie: als min/max nog steeds ongeldig zijn, gebruik huidige prijs
                        if (minVal <= 0.0f || maxVal <= 0.0f || minVal > 1000000.0f || maxVal > 1000000.0f) {
                            Serial_printf(F("[AlertEngine] WARN: Ongeldige min/max waarden voor 5m move (min=%.2f, max=%.2f), gebruik huidige prijs\n"), minVal, maxVal);
                            minVal = prices[0];
                            maxVal = prices[0];
                        }
                    }
                    
                    // Format message
                    char timestamp[32];
                    getFormattedTimestamp(timestamp, sizeof(timestamp));
                    char msg[256];
                    if (ret_5m >= 0) {
                        snprintf(msg, sizeof(msg), 
                                 "5m UP move: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_5m, timestamp, prices[0], maxVal, minVal);
                    } else {
                        snprintf(msg, sizeof(msg), 
                                 "5m DOWN move: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_5m, timestamp, prices[0], maxVal, minVal);
                    }
                    
                    const char* colorTag = determineColorTag(ret_5m, effThresh.move5m, effThresh.move5m * 1.5f);
                    char title[48];  // Verkleind van 64 naar 48 bytes
                    snprintf(title, sizeof(title), "%s 5m Move Alert", binanceSymbol);
                    
                    // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    if (checkAlertConditions(now, lastNotification5Min, notificationCooldowns.cooldown5MinMs, 
                                             alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR, "5m move")) {
                        sendNotification(title, msg, colorTag);
                        lastNotification5Min = now;
                        alerts5MinThisHour++;
                        Serial_printf(F("[Notify] 5m move notificatie verstuurd (%d/%d dit uur)\n"), alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
                    }
                }
            }
        }
    }
}

// Check 2-hour notifications (breakout, breakdown, compression, mean reversion, anchor context)
// Wordt aangeroepen na elke price update
void AlertEngine::check2HNotifications(float lastPrice, float anchorPrice)
{
    // Check WiFi verbinding
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    // Check validiteit van inputs (anchorPrice kan 0.0f zijn als anchor niet actief is)
    if (lastPrice <= 0.0f) {
        return;
    }
    
    // Bereken 2h metrics
    TwoHMetrics metrics = computeTwoHMetrics();
    
    // Check validiteit van metrics
    if (!metrics.valid) {
        return;
    }
    
    uint32_t now = millis();
    
    // Gedeelde buffers (hergebruik om geheugen te besparen)
    char title[32];  // Verkleind van 48 naar 32 bytes
    char msg[80];    // Verkleind van 128 naar 80 bytes om DRAM overflow te voorkomen (32 bytes bespaard)
    
    #if DEBUG_2H_ALERTS
    // Rate-limited debug logging (1x per 60 sec)
    static uint32_t lastDebugLogMs = 0;
    static constexpr uint32_t DEBUG_LOG_INTERVAL_MS = 60UL * 1000UL; // 60 sec
    
    if ((now - lastDebugLogMs) >= DEBUG_LOG_INTERVAL_MS) {
        // Bereken condities voor logging (deze worden later ook gebruikt in de checks)
        float breakMargin = alert2HThresholds.breakMarginPct;
        float breakThresholdUp = metrics.high2h * (1.0f + breakMargin / 100.0f);
        float breakThresholdDown = metrics.low2h * (1.0f - breakMargin / 100.0f);
        bool condBreakUp = lastPrice > breakThresholdUp;
        bool condBreakDown = lastPrice < breakThresholdDown;
        bool condCompress = metrics.rangePct < alert2HThresholds.compressThresholdPct;
        float distPct = absf((lastPrice - metrics.avg2h) / metrics.avg2h * 100.0f);
        bool condTouch = distPct <= alert2HThresholds.meanTouchBandPct;
        
        Serial.printf("[2H-DBG] price=%.2f avg2h=%.2f high2h=%.2f low2h=%.2f rangePct=%.2f%% anchor=%.2f\n",
                     lastPrice, metrics.avg2h, metrics.high2h, metrics.low2h, metrics.rangePct, anchorPrice);
        Serial.printf("[2H-DBG] cond: breakUp=%d breakDown=%d compress=%d touch=%d\n",
                     condBreakUp ? 1 : 0, condBreakDown ? 1 : 0, condCompress ? 1 : 0, condTouch ? 1 : 0);
        lastDebugLogMs = now;
    }
    #endif // DEBUG_2H_ALERTS
    
    // === A) 2h Breakout Up ===
    {
        float breakMargin = alert2HThresholds.breakMarginPct;
        float breakThreshold = metrics.high2h * (1.0f + breakMargin / 100.0f);
        bool condUp = lastPrice > breakThreshold;
        bool cooldownOk = (now - gAlert2H.lastBreakoutUpMs) >= alert2HThresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutUpArmed() && condUp && cooldownOk) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] breakout_up sent: price=%.2f > high2h=%.2f (avg=%.2f, range=%.2f%%)\n",
                         lastPrice, metrics.high2h, metrics.avg2h, metrics.rangePct);
            #endif
            snprintf(title, sizeof(title), "%s 2h breakout ↑", binanceSymbol);
            snprintf(msg, sizeof(msg), "Price %.2f > 2h high %.2f (avg %.2f, range %.2f%%)",
                     lastPrice, metrics.high2h, metrics.avg2h, metrics.rangePct);
            sendNotification(title, msg, "blue_square,🔼");
            gAlert2H.lastBreakoutUpMs = now;
            gAlert2H.setBreakoutUpArmed(false);
        }
        
        // Reset arm zodra prijs weer onder reset threshold komt
        float resetThreshold = metrics.high2h * (1.0f - alert2HThresholds.breakResetMarginPct / 100.0f);
        if (!gAlert2H.getBreakoutUpArmed() && lastPrice < resetThreshold) {
            gAlert2H.setBreakoutUpArmed(true);
        }
    }
    
    // === B) 2h Breakdown Down ===
    {
        float breakMargin = alert2HThresholds.breakMarginPct;
        float breakThreshold = metrics.low2h * (1.0f - breakMargin / 100.0f);
        bool condDown = lastPrice < breakThreshold;
        bool cooldownOk = (now - gAlert2H.lastBreakoutDownMs) >= alert2HThresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutDownArmed() && condDown && cooldownOk) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] breakdown_down sent: price=%.2f < low2h=%.2f (avg=%.2f, range=%.2f%%)\n",
                         lastPrice, metrics.low2h, metrics.avg2h, metrics.rangePct);
            #endif
            snprintf(title, sizeof(title), "%s 2h breakdown ↓", binanceSymbol);
            snprintf(msg, sizeof(msg), "Price %.2f < 2h low %.2f (avg %.2f, range %.2f%%)",
                     lastPrice, metrics.low2h, metrics.avg2h, metrics.rangePct);
            sendNotification(title, msg, "orange_square,🔽");
            gAlert2H.lastBreakoutDownMs = now;
            gAlert2H.setBreakoutDownArmed(false);
        }
        
        // Reset arm zodra prijs weer boven reset threshold komt
        float resetThreshold = metrics.low2h * (1.0f + alert2HThresholds.breakResetMarginPct / 100.0f);
        if (!gAlert2H.getBreakoutDownArmed() && lastPrice > resetThreshold) {
            gAlert2H.setBreakoutDownArmed(true);
        }
    }
    
    // === C) Range compression ===
    {
        bool condComp = metrics.rangePct < alert2HThresholds.compressThresholdPct;
        bool cooldownOk = (now - gAlert2H.lastCompressMs) >= alert2HThresholds.compressCooldownMs;
        
        if (gAlert2H.getCompressArmed() && condComp && cooldownOk) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] range_compress sent: range=%.2f%% < %.2f%% (avg=%.2f high=%.2f low=%.2f)\n",
                         metrics.rangePct, alert2HThresholds.compressThresholdPct,
                         metrics.avg2h, metrics.high2h, metrics.low2h);
            #endif
            snprintf(title, sizeof(title), "%s 2h range compress", binanceSymbol);
            snprintf(msg, sizeof(msg), "Range %.2f%% (<%.2f%%). avg %.2f high %.2f low %.2f",
                     metrics.rangePct, alert2HThresholds.compressThresholdPct,
                     metrics.avg2h, metrics.high2h, metrics.low2h);
            sendNotification(title, msg, "yellow_square,📉");
            gAlert2H.lastCompressMs = now;
            gAlert2H.setCompressArmed(false);
        }
        
        // Reset arm zodra range weer boven reset threshold komt
        if (!gAlert2H.getCompressArmed() && metrics.rangePct > alert2HThresholds.compressResetPct) {
            gAlert2H.setCompressArmed(true);
        }
    }
    
    // === D) Mean reversion touch to avg2h ===
    {
        float distPct = absf((lastPrice - metrics.avg2h) / metrics.avg2h * 100.0f);
        
        // Update state: zijn we ver genoeg weg?
        if (distPct >= alert2HThresholds.meanMinDistancePct) {
            gAlert2H.setMeanWasFar(true);
            gAlert2H.setMeanFarSide((lastPrice >= metrics.avg2h) ? +1 : -1);
        }
        
        // Check touch
        bool touch = distPct <= alert2HThresholds.meanTouchBandPct;
        bool cooldownOk = (now - gAlert2H.lastMeanMs) >= alert2HThresholds.meanCooldownMs;
        
        if (gAlert2H.getMeanArmed() && gAlert2H.getMeanWasFar() && touch && cooldownOk) {
            const char* direction = (gAlert2H.getMeanFarSide() > 0) ? "from above" : "from below";
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] mean_touch sent: price=%.2f touched avg2h=%.2f after %.2f%% away (%s)\n",
                         lastPrice, metrics.avg2h, distPct, direction);
            #endif
            snprintf(title, sizeof(title), "%s 2h mean touch", binanceSymbol);
            snprintf(msg, sizeof(msg), "Touched 2h avg %.2f after %.2f%% away (%s)",
                     metrics.avg2h, distPct, direction);
            sendNotification(title, msg, "green_square,📊");
            gAlert2H.lastMeanMs = now;
            gAlert2H.setMeanArmed(false);
            gAlert2H.setMeanWasFar(false);
            gAlert2H.setMeanFarSide(0);
        }
        
        // Reset arm zodra prijs weer ver genoeg weg is
        if (!gAlert2H.getMeanArmed() && distPct > (alert2HThresholds.meanTouchBandPct * 2.0f)) {
            gAlert2H.setMeanArmed(true);
        }
    }
    
    // === E) Anchor context ===
    // Alleen checken als anchor actief is (anchorPrice > 0)
    if (anchorPrice > 0.0f) {
        float anchorMargin = alert2HThresholds.anchorOutsideMarginPct;
        float anchorHighThreshold = metrics.high2h * (1.0f + anchorMargin / 100.0f);
        float anchorLowThreshold = metrics.low2h * (1.0f - anchorMargin / 100.0f);
        bool condAnchorHigh = anchorPrice > anchorHighThreshold;
        bool condAnchorLow = anchorPrice < anchorLowThreshold;
        bool cooldownOk = (now - gAlert2H.lastAnchorCtxMs) >= alert2HThresholds.anchorCooldownMs;
        
        if (gAlert2H.getAnchorCtxArmed() && cooldownOk && (condAnchorHigh || condAnchorLow)) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] anchor_context sent: anchor=%.2f outside 2h [%.2f..%.2f] (avg=%.2f)\n",
                         anchorPrice, metrics.low2h, metrics.high2h, metrics.avg2h);
            #endif
            snprintf(title, sizeof(title), "%s Anchor buiten 2h", binanceSymbol);
            snprintf(msg, sizeof(msg), "Anchor %.2f outside 2h [%.2f..%.2f] (avg %.2f)",
                     anchorPrice, metrics.low2h, metrics.high2h, metrics.avg2h);
            sendNotification(title, msg, "purple_square,⚓");
            gAlert2H.lastAnchorCtxMs = now;
            gAlert2H.setAnchorCtxArmed(false);
        }
        
        // Reset arm zodra anchor weer binnen range komt (inclusief marge)
        if (!gAlert2H.getAnchorCtxArmed() && 
            anchorPrice <= anchorHighThreshold && 
            anchorPrice >= anchorLowThreshold) {
            gAlert2H.setAnchorCtxArmed(true);
        }
    }
}

