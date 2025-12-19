#include "AlertEngine.h"

// SettingsStore module (Fase 6.1.10: voor AlertThresholds en NotificationCooldowns structs)
#include "../SettingsStore/SettingsStore.h"

// VolatilityTracker module (Fase 6.1.10: voor checkAndNotify - moet eerst voor EffectiveThresholds)
#include "../VolatilityTracker/VolatilityTracker.h"
extern VolatilityTracker volatilityTracker;  // Fase 6.1.10: Voor checkAndNotify

// TrendDetector module (Fase 6.1.9: voor trendSupportsDirection)
#include "../TrendDetector/TrendDetector.h"
extern TrendDetector trendDetector;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char binanceSymbol[];
extern float prices[];  // Fase 6.1.4: Voor formatNotificationMessage
void getFormattedTimestamp(char* buffer, size_t bufferSize);  // Fase 6.1.4: Voor formatNotificationMessage
extern bool smartConfluenceEnabled;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert

// Fase 6.1.10: Forward declarations voor checkAndNotify dependencies
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
void logVolatilityStatus(const EffectiveThresholds& eff);
extern float fiveMinutePrices[];

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
    
    char title[64];
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
    char title[80];
    snprintf(title, sizeof(title), "%s Confluence Alert (1m+5m+Trend)", binanceSymbol);
    
    char msg[320];
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
                    char title[64];
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
                    // Bereken min en max uit fiveMinutePrices buffer
                    float minVal = fiveMinutePrices[0];
                    float maxVal = fiveMinutePrices[0];
                    for (int i = 1; i < SECONDS_PER_5MINUTES; i++) {
                        if (fiveMinutePrices[i] > 0.0f) {
                            if (fiveMinutePrices[i] < minVal || minVal <= 0.0f) minVal = fiveMinutePrices[i];
                            if (fiveMinutePrices[i] > maxVal || maxVal <= 0.0f) maxVal = fiveMinutePrices[i];
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
                    char title[64];
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

