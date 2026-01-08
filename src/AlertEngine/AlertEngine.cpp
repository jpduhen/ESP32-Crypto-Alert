#include "AlertEngine.h"
#include <WiFi.h>
#include <time.h>  // Voor getLocalTime()

// SettingsStore module (Fase 6.1.10: voor AlertThresholds en NotificationCooldowns structs)
#include "../SettingsStore/SettingsStore.h"

// VolatilityTracker module (Fase 6.1.10: voor checkAndNotify - moet eerst voor EffectiveThresholds)
#include "../VolatilityTracker/VolatilityTracker.h"
extern VolatilityTracker volatilityTracker;  // Fase 6.1.10: Voor checkAndNotify

// TrendDetector module (Fase 6.1.9: voor trendSupportsDirection)
#include "../TrendDetector/TrendDetector.h"
extern TrendDetector trendDetector;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert

// AnchorSystem module (voor auto anchor updates)
#include "../AnchorSystem/AnchorSystem.h"
extern AnchorSystem anchorSystem;

// Alert2HThresholds module (voor 2h alert thresholds)
#include "Alert2HThresholds.h"

// Extern declaration voor 2h alert thresholds (wordt geladen vanuit settings)
extern Alert2HThresholds alert2HThresholds;
extern SettingsStore settingsStore;

// PriceData module (voor fiveMinutePrices getter)
#include "../PriceData/PriceData.h"
extern PriceData priceData;  // Voor getFiveMinutePrices()

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char binanceSymbol[];
extern float prices[];  // Fase 6.1.4: Voor formatNotificationMessage
extern uint8_t language;  // Taalinstelling (0 = Nederlands, 1 = English)
extern const char* getText(const char* nlText, const char* enText);  // Taalvertaling functie
extern bool isValidPrice(float price);  // Voor price validatie
void getFormattedTimestamp(char* buffer, size_t bufferSize);  // Fase 6.1.4: Voor formatNotificationMessage
void getFormattedTimestampForNotification(char* buffer, size_t bufferSize);  // Nieuwe functie voor notificaties met slash formaat
extern bool smartConfluenceEnabled;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert
void safeStrncpy(char *dest, const char *src, size_t destSize);  // FASE X.5: Voor coalescing
extern KlineMetrics lastKline1m;
extern KlineMetrics lastKline5m;

// Forward declaration voor computeTwoHMetrics()
TwoHMetrics computeTwoHMetrics();

// Persistent runtime state voor 2h notificaties
static Alert2HState gAlert2H;
static const Alert2HThresholds* gAlert2HThresholdsPtr = nullptr;
static bool gAlert2HThresholdsReady = false;

// Fase 6.1.10: Forward declarations voor checkAndNotify dependencies
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
void logVolatilityStatus(const EffectiveThresholds& eff);
// Fase 6.1.10: fiveMinutePrices kan pointer zijn (CYD/TTGO) of array (andere platforms)
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
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
#define VOLUME_CONFIRM_MULTIPLIER 1.5f
#define RANGE_CONFIRM_MIN_PCT 0.2f
#define VOLUME_EVENT_COOLDOWN_MS 120000UL
#define VOLUME_EMA_WINDOW_1M 20
#define VOLUME_EMA_WINDOW_5M 20

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
    
    // Initialiseer buffers
    msgBuffer[0] = '\0';
    titleBuffer[0] = '\0';
    timestampBuffer[0] = '\0';
    
    // Initialiseer cache
    cachedAbsRet1m = 0.0f;
    cachedAbsRet5m = 0.0f;
    cachedAbsRet30m = 0.0f;
    valuesCached = false;
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void AlertEngine::begin() {
    // Fase 6.1.1: Basis structuur - sync wordt incrementeel geïmplementeerd
    syncStateFromGlobals();
}

void AlertEngine::onSettingsLoaded(const Alert2HThresholds& thresholds) {
    gAlert2HThresholdsPtr = &thresholds;
    gAlert2HThresholdsReady = true;
}

bool AlertEngine::are2HThresholdsReady() {
    return gAlert2HThresholdsReady || settingsStore.isLoaded();
}

const Alert2HThresholds& AlertEngine::getAlert2HThresholds() {
    return gAlert2HThresholdsPtr ? *gAlert2HThresholdsPtr : alert2HThresholds;
}

// Helper: Check if cooldown has passed and hourly limit is OK
// Fase 6.1.2: Verplaatst naar AlertEngine (parallel implementatie)
// Geoptimaliseerd: early returns, minder logging
bool AlertEngine::checkAlertConditions(unsigned long now, unsigned long& lastNotification, unsigned long cooldownMs, 
                                       uint8_t& alertsThisHour, uint8_t maxAlertsPerHour, const char* alertType)
{
    // Early return: check cooldown eerst (sneller)
    if (lastNotification != 0 && (now - lastNotification < cooldownMs)) {
        return false;
    }
    
    // Check hourly limit
    if (alertsThisHour >= maxAlertsPerHour) {
        #if !DEBUG_BUTTON_ONLY
        Serial_printf("[Notify] %s gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", 
                     alertType, alertsThisHour, maxAlertsPerHour);
        #endif
        return false;
    }
    
    return true;
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

static EmaAccumulator volumeEma1m;
static EmaAccumulator volumeEma5m;
static bool volumeEma1mInitialized = false;
static bool volumeEma5mInitialized = false;
static unsigned long lastVolumeEventMs = 0;
static unsigned long lastKline1mOpenTime = 0;
static unsigned long lastKline5mOpenTime = 0;
extern VolumeRangeStatus lastVolumeRange1m;
extern VolumeRangeStatus lastVolumeRange5m;

static void updateVolumeEmaIfNewCandle(const KlineMetrics& kline, EmaAccumulator& ema, bool& initialized,
                                      unsigned long& lastOpenTime, uint16_t windowSize)
{
    if (!kline.valid || kline.volume <= 0.0f) {
        return;
    }
    
    if (!initialized) {
        ema.begin(windowSize);
        initialized = true;
    }
    
    if (kline.openTime != 0 && kline.openTime != lastOpenTime) {
        ema.push(kline.volume);
        lastOpenTime = kline.openTime;
    }
}

static VolumeRangeStatus evaluateVolumeRange(const KlineMetrics& kline, const EmaAccumulator& ema)
{
    VolumeRangeStatus status;
    if (!kline.valid || kline.close <= 0.0f || kline.high < kline.low) {
        return status;
    }
    
    status.valid = true;
    status.rangePct = ((kline.high - kline.low) / kline.close) * 100.0f;
    status.rangeOk = status.rangePct >= RANGE_CONFIRM_MIN_PCT;
    
    if (ema.isValid() && ema.get() > 0.0f) {
        status.volumeDeltaPct = ((kline.volume / ema.get()) - 1.0f) * 100.0f;
        status.volumeOk = kline.volume >= (ema.get() * VOLUME_CONFIRM_MULTIPLIER);
    }
    
    return status;
}

static bool volumeEventCooldownOk(unsigned long now)
{
    return (lastVolumeEventMs == 0 || (now - lastVolumeEventMs) >= VOLUME_EVENT_COOLDOWN_MS);
}

static void appendVolumeRangeInfo(char* msg, size_t msgSize, const VolumeRangeStatus& status)
{
    if (!status.valid) {
        return;
    }
    
    size_t len = strnlen(msg, msgSize);
    if (len >= msgSize - 1) {
        return;
    }
    
    const char* vsAvgText = getText("vs gem", "vs avg");
    const char* rangeText = getText("range", "range");
    char extra[72];
    snprintf(extra, sizeof(extra), "vol%+.0f%% %s | %s %.2f%%",
             status.volumeDeltaPct, vsAvgText, rangeText, status.rangePct);
    snprintf(msg + len, msgSize - len, "\n%s", extra);
}

// Helper: Format notification message with timestamp, price, and min/max
// Fase 6.1.4: Verplaatst naar AlertEngine (parallel implementatie)
// Note: Static functie, gebruikt lokale buffer (kan geen instance members gebruiken)
void AlertEngine::formatNotificationMessage(char* msg, size_t msgSize, float ret, const char* direction, 
                                            float minVal, float maxVal)
{
    // Static functie: gebruik lokale buffer (kan geen instance members gebruiken)
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
    
    // Static functie: gebruik lokale buffers (kan geen instance members gebruiken)
    char msg[256];
    formatNotificationMessage(msg, sizeof(msg), ret, direction, minVal, maxVal);
    
    const char* colorTag = determineColorTag(ret, threshold, strongThreshold);
    
    char title[48];
    snprintf(title, sizeof(title), "%s %s Alert", binanceSymbol, alertType);
    
    bool sent = sendNotification(title, msg, colorTag);
    if (sent) {
        lastNotification = now;
        alertsThisHour++;
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[Notify] %s notificatie verstuurd (%d/%d dit uur)\n"), alertType, alertsThisHour, maxAlertsPerHour);
        #endif
    }
    
    return sent;
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
// Geoptimaliseerd: gebruik gecachte absolute waarde indien beschikbaar
void AlertEngine::update1mEvent(float ret_1m, unsigned long timestamp, float threshold)
{
    // Gebruik gecachte absolute waarde indien beschikbaar (voorkomt herhaalde fabsf)
    float absRet1m = valuesCached ? cachedAbsRet1m : fabsf(ret_1m);
    if (absRet1m >= threshold) {
        last1mEvent.direction = (ret_1m > 0) ? EVENT_UP : EVENT_DOWN;
        last1mEvent.timestamp = timestamp;
        last1mEvent.magnitude = absRet1m;
        last1mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Update 5m event state voor Smart Confluence Mode
// Fase 6.1.8: Verplaatst naar AlertEngine (parallel implementatie)
// Geoptimaliseerd: gebruik gecachte absolute waarde indien beschikbaar
void AlertEngine::update5mEvent(float ret_5m, unsigned long timestamp, float threshold)
{
    // Gebruik gecachte absolute waarde indien beschikbaar (voorkomt herhaalde fabsf)
    float absRet5m = valuesCached ? cachedAbsRet5m : fabsf(ret_5m);
    if (absRet5m >= threshold) {
        last5mEvent.direction = (ret_5m > 0) ? EVENT_UP : EVENT_DOWN;
        last5mEvent.timestamp = timestamp;
        last5mEvent.magnitude = absRet5m;
        last5mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Helper: Check if two events are within confluence time window
// Geoptimaliseerd: inline, early return
static inline bool eventsWithinTimeWindow(unsigned long timestamp1, unsigned long timestamp2, unsigned long now)
{
    // Early return: check validiteit eerst
    if (timestamp1 == 0 || timestamp2 == 0) return false;
    
    // Bereken absolute tijdverschil (geoptimaliseerd: voorkomt signed overflow)
    unsigned long timeDiff;
    if (timestamp1 > timestamp2) {
        timeDiff = timestamp1 - timestamp2;
    } else {
        timeDiff = timestamp2 - timestamp1;
    }
    
    return (timeDiff <= CONFLUENCE_TIME_WINDOW_MS);
}

// Helper: Check if 30m trend supports the direction (UP/DOWN)
// Geoptimaliseerd: inline, early returns
static inline bool trendSupportsDirection(EventDirection direction)
{
    // Fase 6.1.9: Gebruik TrendDetector module getter
    TrendState currentTrend = trendDetector.getTrendState();
    
    // Early returns voor snellere checks
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
// Geoptimaliseerd: early returns, minder checks
bool AlertEngine::checkAndSendConfluenceAlert(unsigned long now, float ret_30m)
{
    // Early returns: check voorwaarden eerst (sneller)
    if (!smartConfluenceEnabled) return false;
    
    // Check if we have valid 1m and 5m events
    if (last1mEvent.direction == EVENT_NONE || last5mEvent.direction == EVENT_NONE) {
        return false;
    }
    
    // Check if events are already used in confluence
    if (last1mEvent.usedInConfluence || last5mEvent.usedInConfluence) {
        return false;
    }
    
    // Check if both events are in the same direction (sneller dan time window check)
    if (last1mEvent.direction != last5mEvent.direction) {
        return false;
    }
    
    // Check if events are within time window
    if (!eventsWithinTimeWindow(last1mEvent.timestamp, last5mEvent.timestamp, now)) {
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

    VolumeRangeStatus volumeRange = evaluateVolumeRange(lastKline5m, volumeEma5m);
    if (!volumeRange.valid || !volumeRange.volumeOk || !volumeRange.rangeOk) {
        return false;
    }
    
    if (!volumeEventCooldownOk(now)) {
        return false;
    }
    
    // Confluence detected! Send combined alert
    EventDirection direction = last1mEvent.direction;
    const char* directionText = (direction == EVENT_UP) ? "UP" : "DOWN";
    // Fase 6.1.9: Gebruik TrendDetector module getter + inline helper (geoptimaliseerd)
    TrendState currentTrend = trendDetector.getTrendState();
    const char* trendText = getTrendName(currentTrend);
    
    // Hergebruik class buffers i.p.v. lokale stack allocaties
    getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
    snprintf(titleBuffer, sizeof(titleBuffer), "%s %s (1m+5m+Trend)", binanceSymbol, 
             getText("Samenloop", "Confluence"));
    
    // Vertaal trend naar juiste taal
    const char* trendTextTranslated = trendText;
    if (strcmp(trendText, "UP") == 0) trendTextTranslated = getText("OP", "UP");
    else if (strcmp(trendText, "DOWN") == 0) trendTextTranslated = getText("NEER", "DOWN");
    else if (strcmp(trendText, "SIDEWAYS") == 0) trendTextTranslated = getText("ZIJWAARTS", "SIDEWAYS");
    
    if (direction == EVENT_UP) {
        snprintf(msgBuffer, sizeof(msgBuffer),
                 "%.2f (%s)\n%s\n1m: +%.2f%%\n5m: +%.2f%%\n30m %s: %s (%+.2f%%)",
                 prices[0], timestampBuffer,
                 getText("Eensgezind OMHOOG", "Confluence UP"),
                 last1mEvent.magnitude,
                 last5mEvent.magnitude,
                 getText("Trend", "Trend"), trendTextTranslated, ret_30m);
    } else {
        snprintf(msgBuffer, sizeof(msgBuffer),
                 "%.2f (%s)\n%s\n1m: %.2f%%\n5m: %.2f%%\n30m %s: %s (%.2f%%)",
                 prices[0], timestampBuffer,
                 getText("Eensgezind OMLAAG", "Confluence DOWN"),
                 -last1mEvent.magnitude,
                 -last5mEvent.magnitude,
                 getText("Trend", "Trend"), trendTextTranslated, ret_30m);
    }
    appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange);
    
    const char* colorTag = (direction == EVENT_UP) ? "green_square,📈" : "red_square,📉";
    bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
    if (sent) {
        lastVolumeEventMs = now;
        
        // Mark events as used
        last1mEvent.usedInConfluence = true;
        last5mEvent.usedInConfluence = true;
        lastConfluenceAlert = now;
    }
    
    #if !DEBUG_BUTTON_ONLY
    Serial_printf(F("[Confluence] Alert verzonden: 1m=%.2f%%, 5m=%.2f%%, trend=%s, ret_30m=%.2f%%\n"),
                  (direction == EVENT_UP ? last1mEvent.magnitude : -last1mEvent.magnitude),
                  (direction == EVENT_UP ? last5mEvent.magnitude : -last5mEvent.magnitude),
                  trendText, ret_30m);
    #endif
    
    return sent;
}

// Helper: Cache absolute waarden (voorkomt herhaalde fabsf calls)
void AlertEngine::cacheAbsoluteValues(float ret_1m, float ret_5m, float ret_30m) {
    cachedAbsRet1m = (ret_1m != 0.0f) ? fabsf(ret_1m) : 0.0f;
    cachedAbsRet5m = (ret_5m != 0.0f) ? fabsf(ret_5m) : 0.0f;
    cachedAbsRet30m = (ret_30m != 0.0f) ? fabsf(ret_30m) : 0.0f;
    valuesCached = true;
}

// Helper: Bereken min/max uit fiveMinutePrices (gebruikt zelfde logica als 1m en 30m)
// Fase: Extra sanity-range om corrupte waarden te filteren
bool AlertEngine::findMinMaxInFiveMinutePrices(float& minVal, float& maxVal) {
    // Gebruik PriceData getters (consistent met 1m implementatie)
    const float* fiveMinPrices = priceData.getFiveMinutePrices();
    if (fiveMinPrices == nullptr) {
        // Fallback naar huidige prijs
        if (prices[0] > 0.0f) {
            minVal = prices[0];
            maxVal = prices[0];
        } else {
            minVal = 0.0f;
            maxVal = 0.0f;
        }
        return false;
    }
    
    uint16_t fiveMinIndex = priceData.getFiveMinuteIndex();
    bool fiveMinArrayFilled = priceData.getFiveMinuteArrayFilled();

    uint16_t available = fiveMinArrayFilled ? SECONDS_PER_5MINUTES : fiveMinIndex;
    if (available == 0) {
        // Geen data beschikbaar, fallback naar huidige prijs
        if (isValidPrice(prices[0])) {
            minVal = prices[0];
            maxVal = prices[0];
        } else {
            minVal = 0.0f;
            maxVal = 0.0f;
        }
        return false;
    }

    float currentPrice = prices[0];
    bool useSanityRange = isValidPrice(currentPrice);
    float minAllowed = useSanityRange ? currentPrice * 0.1f : 0.0f;
    float maxAllowed = useSanityRange ? currentPrice * 10.0f : 0.0f;

    bool firstValid = false;
    for (uint16_t i = 1; i <= available; i++) {
        uint16_t idx = (fiveMinIndex - i + SECONDS_PER_5MINUTES) % SECONDS_PER_5MINUTES;
        float value = fiveMinPrices[idx];
        if (!isValidPrice(value)) {
            continue;
        }
        if (useSanityRange && (value < minAllowed || value > maxAllowed)) {
            continue;
        }
        if (!firstValid) {
            minVal = value;
            maxVal = value;
            firstValid = true;
        } else {
            if (value < minVal) minVal = value;
            if (value > maxVal) maxVal = value;
        }
    }

    if (!firstValid) {
        if (isValidPrice(currentPrice)) {
            minVal = currentPrice;
            maxVal = currentPrice;
        } else {
            minVal = 0.0f;
            maxVal = 0.0f;
        }
    }

    return firstValid;
}

// Helper: Format notification message (gebruikt class buffers)
void AlertEngine::formatNotificationMessageInternal(float ret, const char* direction, 
                                                    float minVal, float maxVal, const char* timeframe) {
    getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer));
    
    if (ret >= 0) {
        snprintf(msgBuffer, sizeof(msgBuffer), 
                "%s UP %s: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                timeframe, direction, ret, timestampBuffer, prices[0], maxVal, minVal);
    } else {
        snprintf(msgBuffer, sizeof(msgBuffer), 
                "%s DOWN %s: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                timeframe, direction, ret, timestampBuffer, prices[0], maxVal, minVal);
    }
}

// Main alert checking function
// Fase 6.1.10: Verplaatst naar AlertEngine (parallel implementatie)
// Geoptimaliseerd: cache waarden, hergebruik buffers, early returns, validatie
void AlertEngine::checkAndNotify(float ret_1m, float ret_5m, float ret_30m)
{
    // Validatie: check voor NaN en Inf waarden
    if (isnan(ret_1m) || isinf(ret_1m) || isnan(ret_5m) || isinf(ret_5m) || isnan(ret_30m) || isinf(ret_30m)) {
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[AlertEngine] WARN: NaN/Inf waarde gedetecteerd, skip checks\n"));
        #endif
        return;
    }
    
    unsigned long now = millis();
    
    // Early return: als alle returns 0 zijn, skip checks
    if (ret_1m == 0.0f && ret_5m == 0.0f && ret_30m == 0.0f) {
        return;
    }
    
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
    
    // Log volatility status (voor debug) - alleen als DEBUG_BUTTON_ONLY niet actief is
    #if !DEBUG_BUTTON_ONLY
    logVolatilityStatus(effThresh);
    #endif
    
    // Reset tellers elk uur
    if (hourStartTime == 0 || (now - hourStartTime >= 3600000UL)) { // 1 uur = 3600000 ms
        alerts1MinThisHour = 0;
        alerts30MinThisHour = 0;
        alerts5MinThisHour = 0;
        hourStartTime = now;
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[Notify] Uur-tellers gereset\n"));
        #endif
    }
    
    // Cache absolute waarden (voorkomt herhaalde fabsf calls)
    cacheAbsoluteValues(ret_1m, ret_5m, ret_30m);
    
    updateVolumeEmaIfNewCandle(lastKline1m, volumeEma1m, volumeEma1mInitialized,
                               lastKline1mOpenTime, VOLUME_EMA_WINDOW_1M);
    updateVolumeEmaIfNewCandle(lastKline5m, volumeEma5m, volumeEma5mInitialized,
                               lastKline5mOpenTime, VOLUME_EMA_WINDOW_5M);

    VolumeRangeStatus volumeRange1m = evaluateVolumeRange(lastKline1m, volumeEma1m);
    VolumeRangeStatus volumeRange5m = evaluateVolumeRange(lastKline5m, volumeEma5m);
    lastVolumeRange1m = volumeRange1m;
    lastVolumeRange5m = volumeRange5m;

    
    // ===== 1-MINUUT SPIKE ALERT =====
    // Voorwaarde: |ret_1m| >= effectiveSpike1mThreshold EN |ret_5m| >= spike5mThreshold in dezelfde richting
    if (ret_1m != 0.0f && ret_5m != 0.0f)
    {
        // Gebruik gecachte absolute waarden
        float absRet1m = cachedAbsRet1m;
        float absRet5m = cachedAbsRet5m;
        
        // Early return: check thresholds eerst (sneller)
        if (absRet1m < effThresh.spike1m || absRet5m < alertThresholds.spike5m) {
            // Thresholds niet gehaald, skip rest
        } else {
            // Check of beide in dezelfde richting zijn (beide positief of beide negatief)
            bool sameDirection = ((ret_1m > 0 && ret_5m > 0) || (ret_1m < 0 && ret_5m < 0));
            
            // Threshold check: ret_1m >= effectiveSpike1mThreshold EN ret_5m >= spike5mThreshold
            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
            bool spikeDetected = sameDirection;
            bool volumeRangeOk1m = (volumeRange1m.valid && volumeRange1m.volumeOk && volumeRange1m.rangeOk);
            
            // Update 1m event state voor Smart Confluence Mode (alleen bij volume/range confirmatie)
            if (spikeDetected && volumeRangeOk1m) {
                update1mEvent(ret_1m, now, effThresh.spike1m);
            }
            
            // Debug logging alleen bij spike detectie
            if (spikeDetected) {
                #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%\n"), ret_1m, ret_5m);
                #endif
                
                if (!volumeRangeOk1m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 1m spike onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
                } else {
                    // Check for confluence first (Smart Confluence Mode)
                    bool confluenceFound = false;
                    if (smartConfluenceEnabled) {
                        confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
                    }
                    
                    // Als confluence werd gevonden, skip individuele alert
                    if (confluenceFound) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 1m spike onderdrukt (gebruikt in confluence alert)\n"));
                        #endif
                    } else {
                        // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                        if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 1m spike onderdrukt (al gebruikt in confluence)\n"));
                            #endif
                        } else if (!volumeEventCooldownOk(now)) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 1m spike onderdrukt (volume-event cooldown)\n"));
                            #endif
                        } else {
                            // Bereken min en max uit secondPrices buffer
                            float minVal, maxVal;
                            findMinMaxInSecondPrices(minVal, maxVal);
                            
                            // Format message met hergebruik van class buffer
                            getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                            if (ret_1m >= 0) {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%.2f (%s)\n1m %s spike: +%.2f%% (5m: +%.2f%%)\n1m %s: %.2f\n1m %s: %.2f", 
                                         prices[0], timestampBuffer, 
                                         getText("OP", "UP"), ret_1m, ret_5m,
                                         getText("Top", "Top"), maxVal,
                                         getText("Dal", "Low"), minVal);
                            } else {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%.2f (%s)\n1m %s spike: %.2f%% (5m: %.2f%%)\n1m %s: %.2f\n1m %s: %.2f", 
                                         prices[0], timestampBuffer,
                                         getText("NEER", "DOWN"), ret_1m, ret_5m,
                                         getText("Top", "Top"), maxVal,
                                         getText("Dal", "Low"), minVal);
                            }
                            appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange1m);
                            
                            const char* colorTag = determineColorTag(ret_1m, effThresh.spike1m, effThresh.spike1m * 1.5f);
                            snprintf(titleBuffer, sizeof(titleBuffer), "%s 1m %s", binanceSymbol, getText("Spike", "Spike"));
                            
                            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                            if (checkAlertConditions(now, lastNotification1Min, notificationCooldowns.cooldown1MinMs, 
                                                     alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR, "1m spike")) {
                                bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
                                if (sent) {
                                    lastNotification1Min = now;
                                    alerts1MinThisHour++;
                                    lastVolumeEventMs = now;
                                    #if !DEBUG_BUTTON_ONLY
                                    Serial_printf(F("[Notify] 1m spike notificatie verstuurd (%d/%d dit uur)\n"), alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
                                    #endif
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ===== 30-MINUTEN TREND MOVE ALERT =====
    // Voorwaarde: |ret_30m| >= effectiveMove30mThreshold EN |ret_5m| >= move5mThreshold in dezelfde richting
    if (ret_30m != 0.0f && ret_5m != 0.0f)
    {
        // Gebruik gecachte absolute waarden
        float absRet30m = cachedAbsRet30m;
        float absRet5m = cachedAbsRet5m;
        
        // Early return: check thresholds eerst (sneller)
        if (absRet30m < effThresh.move30m || absRet5m < alertThresholds.move5m) {
            // Thresholds niet gehaald, skip rest
        } else {
            // Check of beide in dezelfde richting zijn
            bool sameDirection = ((ret_30m > 0 && ret_5m > 0) || (ret_30m < 0 && ret_5m < 0));
            
            // Threshold check: ret_30m >= effectiveMove30mThreshold EN ret_5m >= move5mThreshold
            // Note: move5mThreshold is de filter threshold, niet de alert threshold
            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
            bool moveDetected = sameDirection;
            bool volumeRangeOk5m = (volumeRange5m.valid && volumeRange5m.volumeOk && volumeRange5m.rangeOk);
            
            // Debug logging alleen bij move detectie
            if (moveDetected) {
                #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%\n"), ret_30m, ret_5m);
                #endif
                
                if (!volumeRangeOk5m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 30m move onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
                } else {
                    // Bereken min en max uit laatste 30 minuten van minuteAverages buffer
                    float minVal, maxVal;
                    findMinMaxInLast30Minutes(minVal, maxVal);
                    
                    // Format message met hergebruik van class buffer
                    getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                    if (ret_30m >= 0) {
                        snprintf(msgBuffer, sizeof(msgBuffer), 
                                 "%.2f (%s)\n30m %s move: +%.2f%% (5m: +%.2f%%)\n30m %s: %.2f\n30m %s: %.2f", 
                                 prices[0], timestampBuffer,
                                 getText("OP", "UP"), ret_30m, ret_5m,
                                 getText("Top", "Top"), maxVal,
                                 getText("Dal", "Low"), minVal);
                    } else {
                        snprintf(msgBuffer, sizeof(msgBuffer), 
                                 "%.2f (%s)\n30m %s move: %.2f%% (5m: %.2f%%)\n30m %s: %.2f\n30m %s: %.2f", 
                                 prices[0], timestampBuffer,
                                 getText("NEER", "DOWN"), ret_30m, ret_5m,
                                 getText("Top", "Top"), maxVal,
                                 getText("Dal", "Low"), minVal);
                    }
                    appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange5m);
                    
                    const char* colorTag = determineColorTag(ret_30m, effThresh.move30m, effThresh.move30m * 1.5f);
                    snprintf(titleBuffer, sizeof(titleBuffer), "%s 30m %s", binanceSymbol, getText("Move", "Move"));
                    
                    // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    if (!volumeEventCooldownOk(now)) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 30m move onderdrukt (volume-event cooldown)\n"));
                        #endif
                    } else if (checkAlertConditions(now, lastNotification30Min, notificationCooldowns.cooldown30MinMs, 
                                                    alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR, "30m move")) {
                        bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
                        if (sent) {
                            lastNotification30Min = now;
                            alerts30MinThisHour++;
                            lastVolumeEventMs = now;
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 30m move notificatie verstuurd (%d/%d dit uur)\n"), alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
                            #endif
                        }
                    }
                }
            }
        }
    }
    
    // ===== 5-MINUTEN MOVE ALERT =====
    // Voorwaarde: |ret_5m| >= effectiveMove5mThreshold
    if (ret_5m != 0.0f)
    {
        // Gebruik gecachte absolute waarde
        float absRet5m = cachedAbsRet5m;
        
        // Early return: check threshold eerst (sneller)
        if (absRet5m < effThresh.move5m) {
            // Threshold niet gehaald, skip rest
        } else {
            // Threshold check: ret_5m >= effectiveMove5mThreshold
            bool move5mDetected = true;
            bool volumeRangeOk5m = (volumeRange5m.valid && volumeRange5m.volumeOk && volumeRange5m.rangeOk);
            
            // Update 5m event state voor Smart Confluence Mode (alleen bij volume/range confirmatie)
            if (volumeRangeOk5m) {
                update5mEvent(ret_5m, now, effThresh.move5m);
            }
            
            // Debug logging alleen bij move detectie
            if (move5mDetected) {
                #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Notify] 5m move: ret_5m=%.2f%%\n"), ret_5m);
                #endif
                
                if (!volumeRangeOk5m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 5m move onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
                } else {
                    // Check for confluence first (Smart Confluence Mode)
                    bool confluenceFound = false;
                    if (smartConfluenceEnabled) {
                        confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
                    }
                    
                    // Als confluence werd gevonden, skip individuele alert
                    if (confluenceFound) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 5m move onderdrukt (gebruikt in confluence alert)\n"));
                        #endif
                    } else {
                        // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                        if (smartConfluenceEnabled && last5mEvent.usedInConfluence) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 5m move onderdrukt (al gebruikt in confluence)\n"));
                            #endif
                        } else if (!volumeEventCooldownOk(now)) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 5m move onderdrukt (volume-event cooldown)\n"));
                            #endif
                        } else {
                            // Bereken min en max uit fiveMinutePrices buffer (geoptimaliseerde versie)
                            float minVal, maxVal;
                            findMinMaxInFiveMinutePrices(minVal, maxVal);
                            
                            // Format message met hergebruik van class buffer
                            getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                            // ret_30m is beschikbaar in checkAndNotify scope
                            if (ret_5m >= 0) {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%.2f (%s)\n5m %s move: +%.2f%% (30m: %+.2f%%)\n5m %s: %.2f\n5m %s: %.2f", 
                                         prices[0], timestampBuffer,
                                         getText("OP", "UP"), ret_5m, ret_30m,
                                         getText("Top", "Top"), maxVal,
                                         getText("Dal", "Low"), minVal);
                            } else {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%.2f (%s)\n5m %s move: %.2f%% (30m: %+.2f%%)\n5m %s: %.2f\n5m %s: %.2f", 
                                         prices[0], timestampBuffer,
                                         getText("NEER", "DOWN"), ret_5m, ret_30m,
                                         getText("Top", "Top"), maxVal,
                                         getText("Dal", "Low"), minVal);
                            }
                            appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange5m);
                            
                            const char* colorTag = determineColorTag(ret_5m, effThresh.move5m, effThresh.move5m * 1.5f);
                            snprintf(titleBuffer, sizeof(titleBuffer), "%s 5m %s", binanceSymbol, getText("Move", "Move"));
                            
                            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                            if (checkAlertConditions(now, lastNotification5Min, notificationCooldowns.cooldown5MinMs, 
                                                     alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR, "5m move")) {
                                bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
                                if (sent) {
                                    lastNotification5Min = now;
                                    alerts5MinThisHour++;
                                    lastVolumeEventMs = now;
                                    #if !DEBUG_BUTTON_ONLY
                                    Serial_printf(F("[Notify] 5m move notificatie verstuurd (%d/%d dit uur)\n"), alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
                                    #endif
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// Check 2-hour notifications (breakout, breakdown, compression, mean reversion, anchor context)
// Wordt aangeroepen na elke price update
// Geoptimaliseerd: early returns, hergebruik buffers, minder berekeningen, validatie
void AlertEngine::check2HNotifications(float lastPrice, float anchorPrice)
{
    // Geconsolideerde validatie: check alle voorwaarden in één keer (sneller, minder branches)
    if (isnan(lastPrice) || isinf(lastPrice) || isnan(anchorPrice) || isinf(anchorPrice) ||
        WiFi.status() != WL_CONNECTED || lastPrice <= 0.0f) {
        // FASE X.5: Flush pending alert ook bij invalid state (voorkomt hangende alerts)
        flushPendingSecondaryAlert();
        return;  // Skip checks bij ongeldige waarden of geen WiFi
    }

    if (!are2HThresholdsReady()) {
        static bool loggedNotReady = false;
        if (!loggedNotReady) {
            Serial_println(F("[AlertEngine] WARN: 2h thresholds nog niet geladen, skip 2h checks"));
            loggedNotReady = true;
        }
        return;
    }

    const Alert2HThresholds& thresholds = getAlert2HThresholds();
    
    // Auto Anchor: gebruik actieve anchor price (kan auto anchor zijn)
    float activeAnchorPrice = getActiveAnchorPrice(anchorPrice);
    
    // Bereken 2h metrics
    TwoHMetrics metrics = computeTwoHMetrics();
    
    // Early return: check validiteit van metrics
    if (!metrics.valid) {
        // FASE X.5: Flush pending alert ook bij invalid metrics
        flushPendingSecondaryAlert();
        return;
    }
    
    uint32_t now = millis();
    
    // FASE X.5: Flush pending SECONDARY alert als coalesce window is verstreken
    flushPendingSecondaryAlert();
    
    // Geconsolideerde berekeningen: bereken breakMargin en thresholds één keer
    float breakMargin = thresholds.breakMarginPct;
    float breakThresholdUp = metrics.high2h * (1.0f + breakMargin / 100.0f);
    float breakThresholdDown = metrics.low2h * (1.0f - breakMargin / 100.0f);
    
    // Static functie: gebruik lokale buffers (kan geen instance members gebruiken)
    char title[32];
    char msg[200];  // Verhoogd om volledige notificatieteksten te ondersteunen
    char timestamp[32];
    
    #if DEBUG_2H_ALERTS
    // Rate-limited debug logging (1x per 60 sec)
    static uint32_t lastDebugLogMs = 0;
    static constexpr uint32_t DEBUG_LOG_INTERVAL_MS = 60UL * 1000UL; // 60 sec
    
    if ((now - lastDebugLogMs) >= DEBUG_LOG_INTERVAL_MS) {
        // Gebruik reeds berekende thresholds
        bool condBreakUp = lastPrice > breakThresholdUp;
        bool condBreakDown = lastPrice < breakThresholdDown;
        bool condCompress = metrics.rangePct < thresholds.compressThresholdPct;
        float distPct = absf((lastPrice - metrics.avg2h) / metrics.avg2h * 100.0f);
        bool condTouch = distPct <= thresholds.meanTouchBandPct;
        
        Serial.printf("[2H-DBG] price=%.2f avg2h=%.2f high2h=%.2f low2h=%.2f rangePct=%.2f%% anchor=%.2f\n",
                     lastPrice, metrics.avg2h, metrics.high2h, metrics.low2h, metrics.rangePct, activeAnchorPrice);
        Serial.printf("[2H-DBG] cond: breakUp=%d breakDown=%d compress=%d touch=%d\n",
                     condBreakUp ? 1 : 0, condBreakDown ? 1 : 0, condCompress ? 1 : 0, condTouch ? 1 : 0);
        lastDebugLogMs = now;
    }
    #endif // DEBUG_2H_ALERTS
    
    // === A) 2h Breakout Up ===
    {
        bool condUp = lastPrice > breakThresholdUp;
        bool cooldownOk = (now - gAlert2H.lastBreakoutUpMs) >= thresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutUpArmed() && condUp && cooldownOk) {
            send2HBreakoutNotification(true, lastPrice, breakThresholdUp, metrics, now);
            gAlert2H.lastBreakoutUpMs = now;
            gAlert2H.setBreakoutUpArmed(false);
        }
        
        // Reset arm zodra prijs weer onder reset threshold komt
        float resetThreshold = metrics.high2h * (1.0f - thresholds.breakResetMarginPct / 100.0f);
        if (!gAlert2H.getBreakoutUpArmed() && lastPrice < resetThreshold) {
            gAlert2H.setBreakoutUpArmed(true);
        }
    }
    
    // === B) 2h Breakdown Down ===
    {
        bool condDown = lastPrice < breakThresholdDown;
        bool cooldownOk = (now - gAlert2H.lastBreakoutDownMs) >= thresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutDownArmed() && condDown && cooldownOk) {
            send2HBreakoutNotification(false, lastPrice, breakThresholdDown, metrics, now);
            gAlert2H.lastBreakoutDownMs = now;
            gAlert2H.setBreakoutDownArmed(false);
        }
        
        // Reset arm zodra prijs weer boven reset threshold komt
        float resetThreshold = metrics.low2h * (1.0f + thresholds.breakResetMarginPct / 100.0f);
        if (!gAlert2H.getBreakoutDownArmed() && lastPrice > resetThreshold) {
            gAlert2H.setBreakoutDownArmed(true);
        }
    }
    
    // === C) Range compression ===
    {
        bool condComp = metrics.rangePct < thresholds.compressThresholdPct;
        bool cooldownOk = (now - gAlert2H.lastCompressMs) >= thresholds.compressCooldownMs;
        
        if (gAlert2H.getCompressArmed() && condComp && cooldownOk) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] range_compress sent: range=%.2f%% < %.2f%% (avg=%.2f high=%.2f low=%.2f)\n",
                         metrics.rangePct, thresholds.compressThresholdPct,
                         metrics.avg2h, metrics.high2h, metrics.low2h);
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            snprintf(title, sizeof(title), "%s 2h %s", 
                     binanceSymbol, getText("Compressie", "Compression"));
            snprintf(msg, sizeof(msg), "%.2f (%s)\n%s: %.2f%% (<%.2f%%)\n2h %s: %.2f\n2h %s: %.2f\n2h %s: %.2f",
                     lastPrice, timestamp,
                     getText("Band", "Range"), metrics.rangePct, thresholds.compressThresholdPct,
                     getText("Top", "High"), metrics.high2h,
                     getText("Gem", "Avg"), metrics.avg2h,
                     getText("Dal", "Low"), metrics.low2h);
            // FASE X.2: Gebruik throttling wrapper
            if (send2HNotification(ALERT2H_COMPRESS, title, msg, "yellow_square,📉")) {
                gAlert2H.lastCompressMs = now;
                gAlert2H.setCompressArmed(false);
            }
        }
        
        // Reset arm zodra range weer boven reset threshold komt
        if (!gAlert2H.getCompressArmed() && metrics.rangePct > thresholds.compressResetPct) {
            gAlert2H.setCompressArmed(true);
        }
    }
    
    // === D) Mean reversion touch to avg2h ===
    {
        float distPct = absf((lastPrice - metrics.avg2h) / metrics.avg2h * 100.0f);
        
        // Update state: zijn we ver genoeg weg?
        if (distPct >= thresholds.meanMinDistancePct) {
            gAlert2H.setMeanWasFar(true);
            gAlert2H.setMeanFarSide((lastPrice >= metrics.avg2h) ? +1 : -1);
        }
        
        // Check touch
        bool touch = distPct <= thresholds.meanTouchBandPct;
        bool cooldownOk = (now - gAlert2H.lastMeanMs) >= thresholds.meanCooldownMs;
        
        if (gAlert2H.getMeanArmed() && gAlert2H.getMeanWasFar() && touch && cooldownOk) {
            const char* direction = (gAlert2H.getMeanFarSide() > 0) ? "from above" : "from below";
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] mean_touch sent: price=%.2f touched avg2h=%.2f after %.2f%% away (%s)\n",
                         lastPrice, metrics.avg2h, distPct, direction);
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            snprintf(title, sizeof(title), "%s 2h %s", 
                     binanceSymbol, getText("Raakt Gemiddelde", "Mean Touch"));
            const char* directionText = (gAlert2H.getMeanFarSide() > 0) ? 
                                        getText("van boven", "from above") : 
                                        getText("van onderen", "from below");
            snprintf(msg, sizeof(msg), "%.2f (%s)\n%s 2h %s %s\n%s %.2f%% %s",
                     lastPrice, timestamp,
                     getText("Raakt", "Touched"), getText("gem.", "avg"), directionText,
                     getText("na", "after"), distPct, getText("verwijdering", "away"));
            // FASE X.2: Gebruik throttling wrapper
            if (send2HNotification(ALERT2H_MEAN_TOUCH, title, msg, "green_square,📊")) {
                gAlert2H.lastMeanMs = now;
                gAlert2H.setMeanArmed(false);
                gAlert2H.setMeanWasFar(false);
                gAlert2H.setMeanFarSide(0);
            }
        }
        
        // Reset arm zodra prijs weer ver genoeg weg is
        if (!gAlert2H.getMeanArmed() && distPct > (thresholds.meanTouchBandPct * 2.0f)) {
            gAlert2H.setMeanArmed(true);
        }
    }
    
    // === E) Anchor context ===
    // Alleen checken als anchor actief is (activeAnchorPrice > 0)
    if (activeAnchorPrice > 0.0f) {
        float anchorMargin = thresholds.anchorOutsideMarginPct;
        float anchorHighThreshold = metrics.high2h * (1.0f + anchorMargin / 100.0f);
        float anchorLowThreshold = metrics.low2h * (1.0f - anchorMargin / 100.0f);
        bool condAnchorHigh = activeAnchorPrice > anchorHighThreshold;
        bool condAnchorLow = activeAnchorPrice < anchorLowThreshold;
        bool cooldownOk = (now - gAlert2H.lastAnchorCtxMs) >= thresholds.anchorCooldownMs;
        
        if (gAlert2H.getAnchorCtxArmed() && cooldownOk && (condAnchorHigh || condAnchorLow)) {
            #if DEBUG_2H_ALERTS
            Serial.printf("[ALERT2H] anchor_context sent: anchor=%.2f outside 2h [%.2f..%.2f] (avg=%.2f)\n",
                         activeAnchorPrice, metrics.low2h, metrics.high2h, metrics.avg2h);
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            snprintf(title, sizeof(title), "%s %s 2h", 
                     binanceSymbol, 
                     getText("Anker buiten", "Anchor outside"));
            snprintf(msg, sizeof(msg), "%.2f (%s)\n%s %.2f %s 2h\n2h %s: %.2f\n2h %s: %.2f\n2h %s: %.2f",
                     lastPrice, timestamp,
                     getText("Anker", "Anchor"), activeAnchorPrice, getText("outside", "outside"),
                     getText("Top", "High"), metrics.high2h,
                     getText("Gem", "Avg"), metrics.avg2h,
                     getText("Dal", "Low"), metrics.low2h);
            // FASE X.2: Gebruik throttling wrapper
            if (send2HNotification(ALERT2H_ANCHOR_CTX, title, msg, "purple_square,⚓")) {
                gAlert2H.lastAnchorCtxMs = now;
                gAlert2H.setAnchorCtxArmed(false);
            }
        }
        
        // Reset arm zodra anchor weer binnen range komt (inclusief marge)
        if (!gAlert2H.getAnchorCtxArmed() && 
            anchorPrice <= anchorHighThreshold && 
            anchorPrice >= anchorLowThreshold) {
            gAlert2H.setAnchorCtxArmed(true);
        }
    }
}

// Helper: Send 2h breakout/breakdown notification (consolideert up/down logica)
// Geoptimaliseerd: elimineert code duplicatie tussen breakout up en breakdown down
void AlertEngine::send2HBreakoutNotification(bool isUp, float lastPrice, float threshold, 
                                             const TwoHMetrics& metrics, uint32_t now) {
    // Static functie: gebruik lokale buffers
    char title[32];
    char msg[200];  // Verhoogd om volledige notificatieteksten te ondersteunen
    char timestamp[32];
    
    // Gebruik timestamp voor notificatie formaat
    getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
    
    if (isUp) {
        #if DEBUG_2H_ALERTS
        Serial.printf("[ALERT2H] breakout_up sent: price=%.2f > high2h=%.2f (avg=%.2f, range=%.2f%%)\n",
                     lastPrice, metrics.high2h, metrics.avg2h, metrics.rangePct);
        #endif
        snprintf(title, sizeof(title), "%s 2h %s ↑", 
                 binanceSymbol, getText("breakout", "breakout"));
        snprintf(msg, sizeof(msg), "%.2f (%s)\n%s > 2h %s %.2f\n%s: %.2f %s: %.2f%%",
                 lastPrice, timestamp,
                 getText("Prijs", "Price"), getText("Top", "High"), metrics.high2h,
                 getText("Gem", "Avg"), metrics.avg2h, getText("Band", "Range"), metrics.rangePct);
        // FASE X.2: Gebruik throttling wrapper (Breakout mag altijd door)
        send2HNotification(ALERT2H_BREAKOUT_UP, title, msg, "blue_square,🔼");
    } else {
        #if DEBUG_2H_ALERTS
        Serial.printf("[ALERT2H] breakdown_down sent: price=%.2f < low2h=%.2f (avg=%.2f, range=%.2f%%)\n",
                     lastPrice, metrics.low2h, metrics.avg2h, metrics.rangePct);
        #endif
        snprintf(title, sizeof(title), "%s 2h %s ↓", 
                 binanceSymbol, getText("breakdown", "breakdown"));
        snprintf(msg, sizeof(msg), "%.2f (%s)\n%s < 2h %s: %.2f\n%s: %.2f %s: %.2f%%",
                 lastPrice, timestamp,
                 getText("Prijs", "Price"), getText("Dal", "Low"), metrics.low2h,
                 getText("Gem", "Avg"), metrics.avg2h, getText("Band", "Range"), metrics.rangePct);
        // FASE X.2: Gebruik throttling wrapper (Breakdown mag altijd door)
        send2HNotification(ALERT2H_BREAKOUT_DOWN, title, msg, "orange_square,🔽");
    }
}

// FASE X.2: 2h alert throttling matrix - static state voor throttling
static Alert2HType last2HAlertType = ALERT2H_NONE;
static uint32_t last2HAlertTimestamp = 0;

// FASE X.5: Secondary global cooldown state
static Alert2HType lastSecondaryType = ALERT2H_NONE;
static uint32_t lastSecondarySentMillis = 0;

// FASE X.5: Coalescing state voor burst-demping
static Alert2HType pendingSecondaryType = ALERT2H_NONE;
static uint32_t pendingSecondaryCreatedMillis = 0;
static char pendingSecondaryTitle[64];
static char pendingSecondaryMsg[256];
static char pendingSecondaryColorTag[32];

// FASE X.3: Check of alert PRIMARY is (override throttling)
bool AlertEngine::isPrimary2HAlert(Alert2HType alertType) {
    // PRIMARY: Breakout/Breakdown (regime-veranderingen)
    return (alertType == ALERT2H_BREAKOUT_UP || alertType == ALERT2H_BREAKOUT_DOWN);
}

// FASE X.5: Bepaal prioriteit van SECONDARY alert (voor coalescing)
// Hogere waarde = hogere prioriteit
static uint8_t getSecondaryAlertPriority(Alert2HType alertType) {
    switch (alertType) {
        case ALERT2H_TREND_CHANGE: return 4;  // Hoogste prioriteit
        case ALERT2H_ANCHOR_CTX: return 3;
        case ALERT2H_MEAN_TOUCH: return 2;
        case ALERT2H_COMPRESS: return 1;      // Laagste prioriteit
        default: return 0;  // PRIMARY alerts hebben geen prioriteit (worden niet gecoalesced)
    }
}

// FASE X.5: Bepaal cooldown op basis van matrix (in seconden)
// Eerst specifieke pair-regels, dan fallback regels, dan default
static uint32_t getSecondaryCooldownSec(Alert2HType lastType, Alert2HType nextType) {
    // Specifieke pair-regels (hardcoded defaults, later uitbreidbaar)
    switch (lastType) {
        case ALERT2H_TREND_CHANGE:
            if (nextType == ALERT2H_TREND_CHANGE) return 180UL * 60UL;  // 180 min
            if (nextType == ALERT2H_MEAN_TOUCH) return 60UL * 60UL;     // 60 min (bestaat)
            if (nextType == ALERT2H_ANCHOR_CTX) return 120UL * 60UL;    // 120 min
            if (nextType == ALERT2H_COMPRESS) return 120UL * 60UL;      // 120 min
            break;
        case ALERT2H_MEAN_TOUCH:
            if (nextType == ALERT2H_MEAN_TOUCH) return 60UL * 60UL;      // 60 min (bestaat)
            if (nextType == ALERT2H_COMPRESS) return 90UL * 60UL;       // 90 min
            break;
        case ALERT2H_COMPRESS:
            if (nextType == ALERT2H_COMPRESS) return 120UL * 60UL;      // 120 min (bestaat)
            if (nextType == ALERT2H_MEAN_TOUCH) return 90UL * 60UL;     // 90 min
            break;
        case ALERT2H_ANCHOR_CTX:
            if (nextType == ALERT2H_ANCHOR_CTX) return 180UL * 60UL;     // 180 min
            break;
        default:
            break;
    }
    
    // Fallback regels: "Any Secondary → nextType"
    switch (nextType) {
        case ALERT2H_ANCHOR_CTX: return 120UL * 60UL;  // 120 min
        case ALERT2H_COMPRESS: return 120UL * 60UL;    // 120 min
        case ALERT2H_MEAN_TOUCH: return 60UL * 60UL;   // 60 min
        case ALERT2H_TREND_CHANGE: return 180UL * 60UL; // 180 min
        default: return 120UL * 60UL;  // Default fallback: 120 min
    }
}

// FASE X.2: Check of 2h alert gesuppresseerd moet worden volgens throttling matrix
// FASE X.3: PRIMARY alerts override throttling (altijd door)
// FASE X.5: Uitgebreid met global cooldown en uitgebreide matrix
bool AlertEngine::shouldThrottle2HAlert(Alert2HType alertType, uint32_t now) {
    const Alert2HThresholds& thresholds = getAlert2HThresholds();
    // PRIMARY alerts mogen altijd door (override throttling)
    if (isPrimary2HAlert(alertType)) {
        return false;  // Geen throttling
    }
    
    // FASE X.5: Check global cooldown voor SECONDARY alerts (hard cap)
    if (lastSecondarySentMillis > 0) {
        uint32_t timeSinceLastSecondary = (now - lastSecondarySentMillis) / 1000UL;  // in seconden
        uint32_t globalCooldownSec = thresholds.twoHSecondaryGlobalCooldownSec;
        if (timeSinceLastSecondary < globalCooldownSec) {
            #ifdef DEBUG_ALERT_THROTTLE
            Serial_printf(F("[2h throttled] SECONDARY dropped by global cooldown (%lu < %lu sec)\n"),
                         timeSinceLastSecondary, globalCooldownSec);
            #endif
            return true;  // Suppress door global cooldown
        }
    }
    
    // Geen vorige alert = altijd door (na global cooldown check)
    if (last2HAlertType == ALERT2H_NONE || last2HAlertTimestamp == 0) {
        return false;  // Geen throttling
    }
    
    uint32_t timeSinceLastAlert = now - last2HAlertTimestamp;
    
    // FASE X.5: Gebruik uitgebreide matrix met getSecondaryCooldownSec()
    // Bepaal cooldown op basis van matrix (in milliseconden)
    uint32_t matrixCooldownSec = getSecondaryCooldownSec(last2HAlertType, alertType);
    uint32_t matrixCooldownMs = matrixCooldownSec * 1000UL;
    
    // Check matrix cooldown (maar global cooldown heeft voorrang als die langer is)
    if (timeSinceLastAlert < matrixCooldownMs) {
        #ifdef DEBUG_ALERT_THROTTLE
        Serial_printf(F("[2h throttled] SECONDARY allowed by matrix cooldown (%lu < %lu ms)\n"),
                     timeSinceLastAlert, matrixCooldownMs);
        #endif
        return true;  // Suppress door matrix cooldown
    }
    
    // Legacy matrix checks (voor backward compatibility met instelbare settings)
    // Deze worden alleen gebruikt als getSecondaryCooldownSec geen match geeft
    switch (last2HAlertType) {
        case ALERT2H_TREND_CHANGE:
            // Trend Change → Trend Change: suppress volgens instelling
            if (alertType == ALERT2H_TREND_CHANGE && timeSinceLastAlert < thresholds.throttlingTrendChangeMs) {
                return true;  // Suppress
            }
            // Trend Change → Mean Touch: suppress volgens instelling
            if (alertType == ALERT2H_MEAN_TOUCH && timeSinceLastAlert < thresholds.throttlingTrendToMeanMs) {
                return true;  // Suppress
            }
            break;
            
        case ALERT2H_MEAN_TOUCH:
            // Mean Touch → Mean Touch: suppress volgens instelling
            if (alertType == ALERT2H_MEAN_TOUCH && timeSinceLastAlert < thresholds.throttlingMeanTouchMs) {
                return true;  // Suppress
            }
            break;
            
        case ALERT2H_COMPRESS:
            // Compress → Compress: suppress volgens instelling
            if (alertType == ALERT2H_COMPRESS && timeSinceLastAlert < thresholds.throttlingCompressMs) {
                return true;  // Suppress
            }
            break;
            
        default:
            // Andere combinaties: geen suppressie
            break;
    }
    
    return false;  // Geen throttling
}

// FASE X.5: Flush pending SECONDARY alert (verstuur als er een pending is)
// Interne helper functie (gebruikt uint32_t now parameter)
static bool flushPendingSecondaryAlertInternal(uint32_t now) {
    if (pendingSecondaryType == ALERT2H_NONE || pendingSecondaryCreatedMillis == 0) {
        return false;  // Geen pending alert
    }
    
    // Check of pending alert nog steeds binnen throttling window valt
    // Gebruik AlertEngine::shouldThrottle2HAlert omdat het een member functie is
    if (AlertEngine::shouldThrottle2HAlert(pendingSecondaryType, now)) {
        // Pending alert is nu gesuppresseerd, reset pending state
        pendingSecondaryType = ALERT2H_NONE;
        pendingSecondaryCreatedMillis = 0;
        return false;
    }
    
    // Verstuur pending alert
        char titleWithClass[64];
        snprintf(titleWithClass, sizeof(titleWithClass), "[%s] %s", 
                 getText("Context", "Context"), pendingSecondaryTitle);
        bool result = sendNotification(titleWithClass, pendingSecondaryMsg, pendingSecondaryColorTag);
    
    // Update throttling state alleen als notificatie succesvol is verstuurd
    if (result) {
        last2HAlertType = pendingSecondaryType;
        last2HAlertTimestamp = now;
        lastSecondaryType = pendingSecondaryType;
        lastSecondarySentMillis = now;
    }
    
    // Reset pending state
    pendingSecondaryType = ALERT2H_NONE;
    pendingSecondaryCreatedMillis = 0;
    
    return result;
}

// FASE X.2: Wrapper voor sendNotification() met 2h throttling
// FASE X.3: PRIMARY alerts override throttling, SECONDARY alerts onderhevig aan throttling
// FASE X.5: Uitgebreid met coalescing voor SECONDARY alerts
bool AlertEngine::send2HNotification(Alert2HType alertType, const char* title, const char* msg, const char* colorTag) {
    uint32_t now = millis();
    const Alert2HThresholds& thresholds = getAlert2HThresholds();
    
    // FASE X.3: PRIMARY alerts override throttling (altijd door, geen coalescing)
    bool isPrimary = isPrimary2HAlert(alertType);
    
    if (isPrimary) {
        // PRIMARY: direct versturen, flush pending SECONDARY eerst
        flushPendingSecondaryAlertInternal(now);
        
        // Check throttling (voor PRIMARY is dit altijd false, maar voor consistentie)
        if (shouldThrottle2HAlert(alertType, now)) {
            return false;
        }
        
        // Verstuur PRIMARY alert
        char titleWithClass[64];
        snprintf(titleWithClass, sizeof(titleWithClass), "[%s] %s", 
                 getText("PRIMAIR", "PRIMARY"), title);
        bool result = sendNotification(titleWithClass, msg, colorTag);
        
        // Update throttling state alleen als notificatie succesvol is verstuurd
        if (result) {
            last2HAlertType = alertType;
            last2HAlertTimestamp = now;
            // PRIMARY alerts tellen niet mee voor secondary global cooldown
        }
        
        return result;
    }
    
    // SECONDARY alerts: coalescing logica
    // Check throttling eerst
    if (shouldThrottle2HAlert(alertType, now)) {
        #if !DEBUG_BUTTON_ONLY
        const char* alertTypeName = "";
        switch (alertType) {
            case ALERT2H_TREND_CHANGE: alertTypeName = "Trend Change"; break;
            case ALERT2H_MEAN_TOUCH: alertTypeName = "Mean Touch"; break;
            case ALERT2H_COMPRESS: alertTypeName = "Compress"; break;
            case ALERT2H_ANCHOR_CTX: alertTypeName = "Anchor Context"; break;
            default: alertTypeName = "Unknown"; break;
        }
        Serial_printf(F("[2h throttled] %s: %s\n"), alertTypeName, title);
        #endif
        return false;  // Alert gesuppresseerd
    }
    
    // Coalescing: check of er al een pending SECONDARY alert is
    uint32_t coalesceWindowMs = thresholds.twoHSecondaryCoalesceWindowSec * 1000UL;
    
    if (pendingSecondaryType != ALERT2H_NONE && pendingSecondaryCreatedMillis > 0) {
        uint32_t timeSincePending = now - pendingSecondaryCreatedMillis;
        
        if (timeSincePending < coalesceWindowMs) {
            // Binnen coalesce window: update pending als nieuwe type hogere prioriteit heeft
            uint8_t pendingPriority = getSecondaryAlertPriority(pendingSecondaryType);
            uint8_t newPriority = getSecondaryAlertPriority(alertType);
            
            if (newPriority > pendingPriority) {
                #ifdef DEBUG_ALERT_THROTTLE
                Serial_printf(F("[2h coalesced] old->new: %d->%d (priority %d->%d)\n"),
                             pendingSecondaryType, alertType, pendingPriority, newPriority);
                #endif
                // Update pending met nieuwe (hogere prioriteit) alert
                pendingSecondaryType = alertType;
                safeStrncpy(pendingSecondaryTitle, title, sizeof(pendingSecondaryTitle));
                safeStrncpy(pendingSecondaryMsg, msg, sizeof(pendingSecondaryMsg));
                safeStrncpy(pendingSecondaryColorTag, colorTag ? colorTag : "", sizeof(pendingSecondaryColorTag));
                pendingSecondaryCreatedMillis = now;
            }
            // Anders: behoud bestaande pending (hogere prioriteit)
            return false;  // Alert gecoalesced, niet direct verstuurd
        } else {
            // Buiten coalesce window: flush pending eerst
            flushPendingSecondaryAlertInternal(now);
        }
    }
    
    // Geen pending of pending geflusht: start nieuw pending
    // (We versturen niet direct, maar wachten op coalesce window of flush)
    pendingSecondaryType = alertType;
    safeStrncpy(pendingSecondaryTitle, title, sizeof(pendingSecondaryTitle));
    safeStrncpy(pendingSecondaryMsg, msg, sizeof(pendingSecondaryMsg));
    safeStrncpy(pendingSecondaryColorTag, colorTag ? colorTag : "", sizeof(pendingSecondaryColorTag));
    pendingSecondaryCreatedMillis = now;
    
    // Verstuur direct als er geen andere pending was (eerste alert in nieuwe window)
    // Dit voorkomt dat de eerste alert blijft hangen
    return false;  // Alert wordt gecoalesced, wordt later geflusht
}

// FASE X.5: Publieke flush functie (gebruikt millis() intern)
void AlertEngine::flushPendingSecondaryAlert() {
    flushPendingSecondaryAlertInternal(millis());
}

// Auto Anchor: Interval helpers
const char* AlertEngine::get4hIntervalStr() {
    return "4h";  // Bitvavo ondersteunt "4h"
}

const char* AlertEngine::get1dIntervalStr() {
    return "1d";  // Bitvavo ondersteunt "1d"
}

// Auto Anchor: Get active anchor price based on mode
float AlertEngine::getActiveAnchorPrice(float manualAnchorPrice) {
    const Alert2HThresholds& thresholds = getAlert2HThresholds();
    uint8_t mode = thresholds.anchorSourceMode;
    
    if (mode == 0) {  // MANUAL
        return manualAnchorPrice;
    } else if (mode == 1) {  // AUTO
        float autoAnchor = thresholds.autoAnchorLastValue;
        if (autoAnchor > 0.0f) {
            return autoAnchor;
        }
        return manualAnchorPrice;
    } else if (mode == 2) {  // AUTO_FALLBACK
        float autoAnchor = thresholds.autoAnchorLastValue;
        if (autoAnchor > 0.0f) {
            return autoAnchor;
        }
        return manualAnchorPrice;
    } else {  // OFF (mode 3)
        return manualAnchorPrice;
    }
}

// Auto Anchor: Update auto anchor value (called from apiTask)
bool AlertEngine::maybeUpdateAutoAnchor(bool force) {
    const Alert2HThresholds& thresholds = getAlert2HThresholds();
    Serial.printf("[ANCHOR][AUTO] maybeUpdateAutoAnchor called: force=%d mode=%d symbol=%s\n", 
                  force, thresholds.anchorSourceMode, binanceSymbol);
    
    uint8_t mode = thresholds.anchorSourceMode;
    if (mode == 0 || mode == 3) {  // MANUAL of OFF
        Serial.printf("[ANCHOR][AUTO] Auto anchor disabled (mode=%d, expected 1 or 2)\n", mode);
        return false;
    }
    
    // Check tijd sinds laatste update
    uint32_t nowEpoch = 0;
    if (!force) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            nowEpoch = mktime(&timeinfo);
        }
        
        if (nowEpoch > 0 && thresholds.autoAnchorLastUpdateEpoch > 0) {
            uint32_t minutesSinceUpdate = (nowEpoch - thresholds.autoAnchorLastUpdateEpoch) / 60;
            if (minutesSinceUpdate < thresholds.autoAnchorUpdateMinutes) {
                Serial.printf("[ANCHOR][AUTO] Too soon to update (%lu minutes since last update)\n", minutesSinceUpdate);
                return false;
            }
        }
    }
    
    // Forward declaration voor fetchBinanceKlines (gedefinieerd in .ino)
    extern int fetchBinanceKlines(const char* symbol, const char* interval, uint16_t limit, float* prices, unsigned long* timestamps, uint16_t maxCount, float* highs = nullptr, float* lows = nullptr, float* volumes = nullptr);
    
    // Fetch 4h candles
    uint8_t count4h = thresholds.autoAnchor4hCandles;
    const char* interval4h = get4hIntervalStr();
    float tempPrices[12];  // Herbruikbare buffer (max 12 candles)
    
    int fetched4h = fetchBinanceKlines(binanceSymbol, interval4h, count4h, tempPrices, nullptr, 12);
    Serial.printf("[ANCHOR][AUTO] 4h fetch result: %d candles\n", fetched4h);
    
    if (fetched4h < 2) {
        Serial.println("[ANCHOR][AUTO] Not enough 4h candles");
        return false;
    }
    
    // Bereken 4h EMA
    EmaAccumulator ema4h;
    ema4h.begin(count4h);
    for (int i = 0; i < fetched4h; i++) {
        ema4h.push(tempPrices[i]);
    }
    
    if (!ema4h.isValid()) {
        Serial.println("[ANCHOR][AUTO] 4h EMA invalid");
        return false;
    }
    
    float ema4hValue = ema4h.get();
    
    // Fetch 1d candles (hergebruik tempPrices buffer)
    uint8_t count1d = thresholds.autoAnchor1dCandles;
    const char* interval1d = get1dIntervalStr();
    
    int fetched1d = fetchBinanceKlines(binanceSymbol, interval1d, count1d, tempPrices, nullptr, 12);
    Serial.printf("[ANCHOR][AUTO] 1d fetch result: %d candles\n", fetched1d);
    
    if (fetched1d < 2) {
        Serial.println("[ANCHOR][AUTO] Not enough 1d candles");
        return false;
    }
    
    // Bereken 1d EMA
    EmaAccumulator ema1d;
    ema1d.begin(count1d);
    for (int i = 0; i < fetched1d; i++) {
        ema1d.push(tempPrices[i]);
    }
    
    if (!ema1d.isValid()) {
        Serial.println("[ANCHOR][AUTO] 1d EMA invalid");
        return false;
    }
    
    float ema1dValue = ema1d.get();
    
    // Combineer EMA's met adaptieve weging (gebruik helper methods voor compacte types)
    float trendDeltaPct = fabsf(ema4hValue - ema1dValue) / ema1dValue * 100.0f;
    float trendPivotPct = thresholds.getAutoAnchorTrendPivotPct();
    float trendFactor = fminf(trendDeltaPct / trendPivotPct, 1.0f);
    float w4hBase = thresholds.getAutoAnchorW4hBase();
    float w4hTrendBoost = thresholds.getAutoAnchorW4hTrendBoost();
    float w4h = w4hBase + trendFactor * w4hTrendBoost;
    float w1d = 1.0f - w4h;
    float newAutoAnchor = w4h * ema4hValue + w1d * ema1dValue;
    
    // Hysterese check
    float lastAutoAnchor = thresholds.autoAnchorLastValue;
    bool shouldCommit = force;
    
    if (!shouldCommit && lastAutoAnchor > 0.0f) {
        float minUpdatePct = thresholds.getAutoAnchorMinUpdatePct();
        float changePct = fabsf(newAutoAnchor - lastAutoAnchor) / lastAutoAnchor * 100.0f;
        if (changePct >= minUpdatePct) {
            shouldCommit = true;
        }
    } else if (lastAutoAnchor <= 0.0f) {
        shouldCommit = true;
    }
    
    // Check force update interval
    if (!shouldCommit && nowEpoch > 0 && thresholds.autoAnchorLastUpdateEpoch > 0) {
        uint32_t minutesSinceUpdate = (nowEpoch - thresholds.autoAnchorLastUpdateEpoch) / 60;
        if (minutesSinceUpdate >= thresholds.autoAnchorForceUpdateMinutes) {
            shouldCommit = true;
        }
    }
    
    if (shouldCommit) {
        // Update settings
        CryptoMonitorSettings settings = settingsStore.load();
        settings.alert2HThresholds.autoAnchorLastValue = newAutoAnchor;
        if (nowEpoch > 0) {
            settings.alert2HThresholds.autoAnchorLastUpdateEpoch = nowEpoch;
        } else {
            settings.alert2HThresholds.autoAnchorLastUpdateEpoch = millis() / 1000UL;
        }
        settingsStore.save(settings);
        
        // Update lokale alert2HThresholds
        alert2HThresholds.autoAnchorLastValue = newAutoAnchor;
        alert2HThresholds.autoAnchorLastUpdateEpoch = settings.alert2HThresholds.autoAnchorLastUpdateEpoch;
        
        // Stel auto anchor in als actieve anchor
        uint8_t currentMode = alert2HThresholds.anchorSourceMode;
        if (currentMode == 1 || currentMode == 2) {
            // BELANGRIJK: shouldUpdateUI=false en skipNotifications=true om deadlocks te voorkomen
            bool anchorSet = anchorSystem.setAnchorPrice(newAutoAnchor, false, true);
            if (anchorSet) {
                Serial.printf("[ANCHOR][AUTO] Anchor ingesteld: %.2f (take profit/max loss worden getoond)\n", newAutoAnchor);
                
                // Stuur optionele notificatie als enabled
                if (alert2HThresholds.getAutoAnchorNotifyEnabled()) {
                    char timestamp[32];
                    char title[40];
                    char msg[120];  // Verhoogd van 80 naar 120 bytes voor langere notificaties
                    getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
                    snprintf(title, sizeof(title), "%s %s", binanceSymbol, getText("Auto Anker", "Auto Anchor"));
                    snprintf(msg, sizeof(msg), "%.2f (%s)\n%s: %.2f", 
                             newAutoAnchor, timestamp,
                             getText("Bijgewerkt", "Updated"), newAutoAnchor);
                    bool sent = sendNotification(title, msg, "anchor");
                    if (!sent) {
                        Serial.println("[ANCHOR][AUTO] WARN: Auto anchor notificatie niet verstuurd");
                    }
                }
            } else {
                Serial.println("[ANCHOR][AUTO] WARN: Kon anchor niet instellen");
            }
        }
        
        Serial.printf("[ANCHOR][AUTO] ema4h=%.2f ema1d=%.2f trend=%.2f%% w4h=%.2f new=%.2f last=%.2f decision=COMMIT\n",
                     ema4hValue, ema1dValue, trendDeltaPct, w4h, newAutoAnchor, lastAutoAnchor);
        return true;
    } else {
        Serial.printf("[ANCHOR][AUTO] ema4h=%.2f ema1d=%.2f trend=%.2f%% w4h=%.2f new=%.2f last=%.2f decision=SKIP\n",
                     ema4hValue, ema1dValue, trendDeltaPct, w4h, newAutoAnchor, lastAutoAnchor);
        return false;
    }
}
