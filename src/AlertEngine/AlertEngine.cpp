#include "AlertEngine.h"
#include <WiFi.h>
#include <time.h>  // Voor getLocalTime()
#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../RegimeEngine/RegimeEngine.h"
// platform_config.h trekt PINS_*.h mee (globale bus/gfx); alleen main .ino, anders duplicate symbols bij linken.
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

#include "../PriceFormat/QuotePriceFormat.h"

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

// PriceData module (voor fiveMinutePrices getter)
#include "../PriceData/PriceData.h"
extern PriceData priceData;  // Voor getFiveMinutePrices()

#include "../AlertAudit.h"

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr);
extern char bitvavoSymbol[];  // Bitvavo market (bijv. "BTC-EUR")
extern float prices[];  // Fase 6.1.4: Voor formatNotificationMessage
extern float latestKnownPrice;
extern SemaphoreHandle_t dataMutex;
extern bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* context);
extern void safeMutexGive(SemaphoreHandle_t mutex, const char* context);
extern uint8_t language;  // Taalinstelling (0 = Nederlands, 1 = English)
extern const char* getText(const char* nlText, const char* enText);  // Taalvertaling functie
extern bool isValidPrice(float price);  // Voor price validatie
void getFormattedTimestamp(char* buffer, size_t bufferSize);  // Fase 6.1.4: Voor formatNotificationMessage
void getFormattedTimestampForNotification(char* buffer, size_t bufferSize);  // Nieuwe functie voor notificaties met slash formaat
extern bool smartConfluenceEnabled;  // Fase 6.1.9: Voor checkAndSendConfluenceAlert
extern bool nightModeEnabled;
extern uint8_t nightModeStartHour;
extern uint8_t nightModeEndHour;
extern bool autoVolatilityEnabled;
extern float autoVolatilityMinMultiplier;
extern float autoVolatilityMaxMultiplier;
extern float nightSpike5mThreshold;
extern float nightMove5mAlertThreshold;
extern float nightMove30mThreshold;
extern uint16_t nightCooldown5mSec;
extern float nightAutoVolMinMultiplier;
extern float nightAutoVolMaxMultiplier;
void safeStrncpy(char *dest, const char *src, size_t destSize);  // FASE X.5: Voor coalescing
extern KlineMetrics lastKline1m;
extern KlineMetrics lastKline5m;

// Forward declaration voor computeTwoHMetrics()
TwoHMetrics computeTwoHMetrics();

// Persistent runtime state voor 2h notificaties
static Alert2HState gAlert2H;

// Fase C: null-veilige C-string voor %s (snprintf/Serial); voorkomt LoadProhibited bij nullptr.
static inline const char* safeFmtStr(const char* p) {
    return (p != nullptr) ? p : "--";
}

// Fase 6.1.10: Forward declarations voor checkAndNotify dependencies
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
void logVolatilityStatus(const EffectiveThresholds& eff);
extern float *fiveMinutePrices;

// Fase 6.1.10: Thresholds zijn #define macros die verwijzen naar struct velden
// We gebruiken de structs direct i.p.v. de macro namen
extern AlertThresholds alertThresholds;
extern NotificationCooldowns notificationCooldowns;

// Regime Fase B: multipliers uit settings (gesynchroniseerd via loadSettings)
extern bool regimeEngineEnabled;
extern float regimeSlapSpike1mMult;
extern float regimeSlapMove5mAlertMult;
extern float regimeSlapMove30mMult;
extern float regimeSlapCooldown1mMult;
extern float regimeSlapCooldown5mMult;
extern float regimeSlapCooldown30mMult;
extern float regimeGeladenSpike1mMult;
extern float regimeGeladenMove5mAlertMult;
extern float regimeGeladenMove30mMult;
extern float regimeGeladenCooldown1mMult;
extern float regimeGeladenCooldown5mMult;
extern float regimeGeladenCooldown30mMult;
extern float regimeEnergiekSpike1mMult;
extern float regimeEnergiekMove5mAlertMult;
extern float regimeEnergiekMove30mMult;
extern float regimeEnergiekCooldown1mMult;
extern float regimeEnergiekCooldown5mMult;
extern float regimeEnergiekCooldown30mMult;
extern bool regimeEnergiekAllowStandalone1mBurst;
extern float regimeEnergiekStandalone1mFactor;
extern float regimeEnergiekMinDirectionStrength;
extern bool hasRet30m;
extern bool getWsSecondLastClosedQuality(uint32_t& tickCount, float& spreadMax, bool& valid, bool& fresh);

#ifndef ENABLE_WS_SECOND_QUALITY_GUARD_1M
#define ENABLE_WS_SECOND_QUALITY_GUARD_1M 1
#endif

namespace {

// Lokale helper (Fase B: schaling — geen gating)
struct RegimeRuntimeMultipliers {
    float spike1m;
    float move5mAlert;
    float move30m;
    float cooldown1m;
    float cooldown5m;
    float cooldown30m;
};

static RegimeRuntimeMultipliers selectRegimeRuntimeMultipliers()
{
    RegimeRuntimeMultipliers m;
    m.spike1m = 1.0f;
    m.move5mAlert = 1.0f;
    m.move30m = 1.0f;
    m.cooldown1m = 1.0f;
    m.cooldown5m = 1.0f;
    m.cooldown30m = 1.0f;
    if (!regimeEngineEnabled) {
        return m;
    }
    // Alleen committed regime (geen proposed / transient)
    const RegimeKind committedRegime = regimeEngineGetSnapshot().committedRegime;
    switch (committedRegime) {
        case REGIME_SLAP:
            m.spike1m = regimeSlapSpike1mMult;
            m.move5mAlert = regimeSlapMove5mAlertMult;
            m.move30m = regimeSlapMove30mMult;
            m.cooldown1m = regimeSlapCooldown1mMult;
            m.cooldown5m = regimeSlapCooldown5mMult;
            m.cooldown30m = regimeSlapCooldown30mMult;
            break;
        case REGIME_ENERGIEK:
            m.spike1m = regimeEnergiekSpike1mMult;
            m.move5mAlert = regimeEnergiekMove5mAlertMult;
            m.move30m = regimeEnergiekMove30mMult;
            m.cooldown1m = regimeEnergiekCooldown1mMult;
            m.cooldown5m = regimeEnergiekCooldown5mMult;
            m.cooldown30m = regimeEnergiekCooldown30mMult;
            break;
        case REGIME_GELADEN:
        default:
            m.spike1m = regimeGeladenSpike1mMult;
            m.move5mAlert = regimeGeladenMove5mAlertMult;
            m.move30m = regimeGeladenMove30mMult;
            m.cooldown1m = regimeGeladenCooldown1mMult;
            m.cooldown5m = regimeGeladenCooldown5mMult;
            m.cooldown30m = regimeGeladenCooldown30mMult;
            break;
    }
    return m;
}

// Fase C Patch 1: guards voor ENERGIEK standalone 1m (geen 5m confirm).
// 30m-takken alleen als hasRet30mData true (geen impliciete flat bij ret_30m==0 zonder data).
static bool energiekStandalone1mGuardsOk(float directionScore,
                                        float minDirStrength,
                                        bool hasRet30mData,
                                        float ret_1m,
                                        float ret_30m)
{
    if (fabsf(directionScore) >= minDirStrength) {
        return true;
    }
    if (!hasRet30mData) {
        return false;
    }
    if (fabsf(ret_30m) < 0.15f) {
        return true;
    }
    if ((ret_1m > 0.0f && ret_30m > 0.0f) || (ret_1m < 0.0f && ret_30m < 0.0f)) {
        return true;
    }
    return false;
}

}  // namespace

// Constants
#define CONFLUENCE_TIME_WINDOW_MS 300000UL  // 5 minuten tijdshorizon voor confluence
#define MAX_1M_ALERTS_PER_HOUR 3
#define MAX_30M_ALERTS_PER_HOUR 2
#define MAX_5M_ALERTS_PER_HOUR 3
#define SECONDS_PER_5MINUTES 300
#define VOLUME_CONFIRM_MULTIPLIER 1.5f
#define RANGE_CONFIRM_MIN_PCT 0.2f
#define VOLUME_EVENT_COOLDOWN_MS 60000UL
#define VOLUME_EMA_WINDOW_1M 20
#define VOLUME_EMA_WINDOW_5M 20
static const uint32_t WS_SECOND_QUALITY_MIN_TICKS_1M = 2U;
static const float WS_SECOND_QUALITY_MAX_SPREAD_1M = 35.0f;
// Nachtstand thresholds (instelbaar)

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

#if DEBUG_ALERT_TRACE
extern unsigned long latestKnownPriceMs;
extern uint8_t latestKnownPriceSource;

static uint32_t alertTraceAllocId()
{
    static uint32_t s_seq = 0;
    if (++s_seq == 0) {
        s_seq = 1;
    }
    return s_seq;
}

static const char* alertTraceNormAuditSrc(const char* auditTag)
{
    if (auditTag == nullptr || auditTag[0] == '\0') {
        return "unknown";
    }
    if (strcmp(auditTag, "WS") == 0) {
        return "WS";
    }
    if (strcmp(auditTag, "REST") == 0) {
        return "REST";
    }
    if (strcmp(auditTag, "FALLBACK") == 0) {
        return "prices0";
    }
    return "unknown";
}

static void alertTraceCandTrigger(float* outPri, const char** outNormSrc, uint32_t* outAgeMs)
{
    const char* raw = "UNKNOWN";
    alertAuditPriceSnapshot(outPri, &raw, outAgeMs);
    *outNormSrc = alertTraceNormAuditSrc(raw);
}

static void alertTraceFmtEpochS(char* buf, size_t bufSz, unsigned long epochS, bool valid)
{
    if (bufSz == 0) {
        return;
    }
    if (!valid || epochS == 0UL) {
        safeStrncpy(buf, "na", bufSz);
    } else {
        snprintf(buf, bufSz, "%lu", (unsigned long)epochS);
    }
    buf[bufSz - 1] = '\0';
}

// Zelfde keuze als snapshotNotifDisplayPrice + bron/leeftijd voor trace (alleen logging).
static void alertTraceDispNotifSnap(uint32_t nowMs, float* outDisp, const char** outSrc, char* ageBuf,
                                    size_t ageBufSz)
{
    float p = prices[0];
    *outDisp = p;
    *outSrc = "na";
    if (ageBuf != nullptr && ageBufSz > 0) {
        safeStrncpy(ageBuf, "na", ageBufSz);
        ageBuf[ageBufSz - 1] = '\0';
    }
    if (ageBuf == nullptr || ageBufSz == 0) {
        return;
    }
    if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "alert trace disp")) {
        const float lk = latestKnownPrice;
        const float px = prices[0];
        const uint8_t src = latestKnownPriceSource;
        const unsigned long lkMs = latestKnownPriceMs;
        safeMutexGive(dataMutex, "alert trace disp");
        if (lk > 0.0f) {
            *outDisp = lk;
            if (src == 2U) {
                *outSrc = "WS";
            } else if (src == 1U) {
                *outSrc = "REST";
            } else {
                *outSrc = "unknown";
            }
            if (lkMs != 0UL && nowMs >= lkMs) {
                snprintf(ageBuf, ageBufSz, "%lu", (unsigned long)(nowMs - lkMs));
            }
        } else {
            *outDisp = px;
            *outSrc = "prices0";
            safeStrncpy(ageBuf, "na", ageBufSz);
        }
        ageBuf[ageBufSz - 1] = '\0';
    }
}
#endif

// Alleen voor weergave in NTFY-teksten: live snapshot, zelfde discipline als UI
static float snapshotNotifDisplayPrice(void)
{
    float p = prices[0];
    if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "alert notif price")) {
        float lk = latestKnownPrice;
        float px = prices[0];
        safeMutexGive(dataMutex, "alert notif price");
        p = (lk > 0.0f) ? lk : px;
    }
    return p;
}

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
                     safeFmtStr(alertType), alertsThisHour, maxAlertsPerHour);
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
        // Alleen UTF-8 emoji (geen ntfy shortcodes — die zijn lelijk in web-UI log)
        return (absRet >= strongThreshold) ? "\xE2\x8F\xAB" /* ⏫ */ : "\xF0\x9F\x94\xBC" /* 🔼 */;
    } else {
        return (absRet >= strongThreshold) ? "\xE2\x8F\xAC" /* ⏬ */ : "\xF0\x9F\x94\xBD" /* 🔽 */;
    }
}

// Eén plek voor 1m Spike / 5m Move / 30m Move titels: 🟦(op)/🟧(neer) + 🔼/🔽 + symbool + band + soortlabel.
// ⏫/⏬ (sterk) blijft alleen in ntfy Tags via determineColorTag(), niet in de titelregel.
static void buildDirectionalMinuteAlertTitle(char* dest, size_t destSz, bool up,
                                             const char* symbol, const char* band,
                                             const char* kindLabel)
{
    if (dest == nullptr || destSz == 0) {
        return;
    }
    const char* sq = up ? "\xF0\x9F\x9F\xA6" /* 🟦 */ : "\xF0\x9F\x9F\xA7" /* 🟧 */;
    const char* ar = up ? "\xF0\x9F\x94\xBC" /* 🔼 */ : "\xF0\x9F\x94\xBD" /* 🔽 */;
    snprintf(dest, destSz, "%s %s %s %s %s", sq, ar, symbol, band, kindLabel);
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

// scopeLine: optioneel (bijv. 30m-alert: volume/range komt uit 5m-candle); nullptr = alleen vol/range-regel
static void appendVolumeRangeInfo(char* msg, size_t msgSize, const VolumeRangeStatus& status,
                                  const char* scopeLine = nullptr)
{
    if (!status.valid) {
        return;
    }
    
    size_t len = strnlen(msg, msgSize);
    if (len >= msgSize - 1) {
        return;
    }
    
    const char* vsAvgText = safeFmtStr(getText("vs gem", "vs avg"));
    const char* rangeText = safeFmtStr(getText("range", "range"));
    char extra[72];
    snprintf(extra, sizeof(extra), "vol%+.0f%% %s | %s %.2f%%",
             status.volumeDeltaPct, vsAvgText, rangeText, status.rangePct);
    if (scopeLine != nullptr && scopeLine[0] != '\0') {
        snprintf(msg + len, msgSize - len, "\n%s\n%s", scopeLine, extra);
    } else {
        snprintf(msg + len, msgSize - len, "\n%s", extra);
    }
}

// Helper: Format notification message with timestamp, price, and min/max
// Fase 6.1.4: Verplaatst naar AlertEngine (parallel implementatie)
// Note: Static functie, gebruikt lokale buffer (kan geen instance members gebruiken)
void AlertEngine::formatNotificationMessage(char* msg, size_t msgSize, float ret, const char* direction, 
                                            float minVal, float maxVal)
{
    // FASE 8.1: Notificatie waarden verificatie logging
    #if DEBUG_CALCULATIONS
    {
        char _dmin[32], _dmax[32], _dpx[32];
        formatQuotePriceEur(_dmin, sizeof(_dmin), minVal);
        formatQuotePriceEur(_dmax, sizeof(_dmax), maxVal);
        formatQuotePriceEur(_dpx, sizeof(_dpx), snapshotNotifDisplayPrice());
        Serial.printf(F("[Notify][Format] ret=%.2f%%, direction=%s, minVal=%s, maxVal=%s, price=%s\n"),
                     ret, direction, _dmin, _dmax, _dpx);
    }
    #endif
    
    // Static functie: gebruik lokale buffer (kan geen instance members gebruiken)
    char timestamp[32];
    getFormattedTimestamp(timestamp, sizeof(timestamp));
    
    char priceStr[32], minStr[32], maxStr[32];
    formatQuotePriceEur(priceStr, sizeof(priceStr), snapshotNotifDisplayPrice());
    formatQuotePriceEur(minStr, sizeof(minStr), minVal);
    formatQuotePriceEur(maxStr, sizeof(maxStr), maxVal);
    if (ret >= 0) {
        snprintf(msg, msgSize, 
                "%s UP %s: +%.2f%%\nPrijs %s: %s\nTop: %s Dal: %s", 
                direction, direction, ret, timestamp, priceStr, maxStr, minStr);
    } else {
        snprintf(msg, msgSize, 
                "%s DOWN %s: %.2f%%\nPrijs %s: %s\nTop: %s Dal: %s", 
                direction, direction, ret, timestamp, priceStr, maxStr, minStr);
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
    
    // FASE 8.1: Notificatie waarden verificatie logging
    #if DEBUG_CALCULATIONS
    {
        char _dmin[32], _dmax[32];
        formatQuotePriceEur(_dmin, sizeof(_dmin), minVal);
        formatQuotePriceEur(_dmax, sizeof(_dmax), maxVal);
        Serial.printf(F("[Notify][Send] %s alert: ret=%.2f%%, threshold=%.2f%%, minVal=%s, maxVal=%s, direction=%s\n"),
                     alertType, ret, threshold, _dmin, _dmax, direction);
    }
    #endif
    
    // Static functie: gebruik lokale buffers (kan geen instance members gebruiken)
    char msg[256];
    formatNotificationMessage(msg, sizeof(msg), ret, direction, minVal, maxVal);
    
    const char* colorTag = determineColorTag(ret, threshold, strongThreshold);
    
    char title[48];
    snprintf(title, sizeof(title), "%s %s Alert", bitvavoSymbol, alertType);
    
    char seqAud[41];
    ntfyBuildSequenceId(title, msg, seqAud, sizeof(seqAud));
    float pa = 0.0f;
    const char* sa = "UNKNOWN";
    uint32_t ag = 0;
    alertAuditPriceSnapshot(&pa, &sa, &ag);
    char ruleAud[48];
    snprintf(ruleAud, sizeof(ruleAud), "generic_%s", alertType);
    char c1Aud[24];
    char c2Aud[20];
    snprintf(c1Aud, sizeof(c1Aud), "ret=%.4g", (double)ret);
    safeStrncpy(c2Aud, direction, sizeof(c2Aud));
    c2Aud[sizeof(c2Aud) - 1] = '\0';
    alertAuditLog(ruleAud, (seqAud[0] != '\0') ? seqAud : nullptr, pa, sa, ag, "ret_pct", threshold, c1Aud, c2Aud);

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

// Helper: Check if night window is active (23:00-07:00)
static inline bool isNightModeWindowActive()
{
    tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    int hour = timeinfo.tm_hour;
    uint8_t startHour = nightModeStartHour;
    uint8_t endHour = nightModeEndHour;
    if (startHour > 23) startHour = 23;
    if (endHour > 23) endHour = 23;
    if (startHour == endHour) {
        return true;  // 24h actief
    }
    if (startHour < endHour) {
        return (hour >= startHour && hour < endHour);
    }
    return (hour >= startHour || hour < endHour);
}

// Helper: Nachtstand filter voor 5m alerts (richting-match met 30m)
static inline bool nightModeAllows5mMove(float ret5m, float ret30m)
{
    if (!nightModeEnabled) return true;
    if (!isNightModeWindowActive()) return true;
    if (ret5m > 0.0f) return (ret30m >= 0.0f);
    if (ret5m < 0.0f) return (ret30m <= 0.0f);
    return true;
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
    snprintf(titleBuffer, sizeof(titleBuffer), "%s %s (1m+5m+Trend)", bitvavoSymbol, 
             getText("Samenloop", "Confluence"));
    
    // Vertaal trend naar juiste taal
    const char* trendTextTranslated = trendText;
    if (strcmp(trendText, "UP") == 0) trendTextTranslated = getText("OP", "UP");
    else if (strcmp(trendText, "DOWN") == 0) trendTextTranslated = getText("NEER", "DOWN");
    else if (strcmp(trendText, "SIDEWAYS") == 0) trendTextTranslated = getText("ZIJWAARTS", "SIDEWAYS");
    
    char cfPriceStr[32];
    formatQuotePriceEur(cfPriceStr, sizeof(cfPriceStr), snapshotNotifDisplayPrice());
    if (direction == EVENT_UP) {
        snprintf(msgBuffer, sizeof(msgBuffer),
                 "%s: %s (%s)\n%s\n1m: +%.2f%%\n5m: +%.2f%%\n30m %s: %s (%+.2f%%)",
                 safeFmtStr(getText("Live prijs", "Live price")),
                 cfPriceStr, timestampBuffer,
                 getText("Eensgezind OMHOOG", "Confluence UP"),
                 last1mEvent.magnitude,
                 last5mEvent.magnitude,
                 getText("Trend", "Trend"), trendTextTranslated, ret_30m);
    } else {
        snprintf(msgBuffer, sizeof(msgBuffer),
                 "%s: %s (%s)\n%s\n1m: %.2f%%\n5m: %.2f%%\n30m %s: %s (%.2f%%)",
                 safeFmtStr(getText("Live prijs", "Live price")),
                 cfPriceStr, timestampBuffer,
                 getText("Eensgezind OMLAAG", "Confluence DOWN"),
                 -last1mEvent.magnitude,
                 -last5mEvent.magnitude,
                 getText("Trend", "Trend"), trendTextTranslated, ret_30m);
    }
    appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange);
    
    const char* colorTag = (direction == EVENT_UP) ? "\xF0\x9F\x93\x88" /* 📈 */ : "\xF0\x9F\x93\x89" /* 📉 */;
    const char* ruleCf = (direction == EVENT_UP) ? "confluence_up" : "confluence_down";
    char seqCf[41];
    ntfyBuildSequenceId(titleBuffer, msgBuffer, seqCf, sizeof(seqCf));
    float pCf = 0.0f;
    const char* srcCf = "UNKNOWN";
    uint32_t ageCf = 0;
    alertAuditPriceSnapshot(&pCf, &srcCf, &ageCf);
    char c1Cf[36];
    char c2Cf[36];
    snprintf(c1Cf, sizeof(c1Cf), "1m=%.3f 5m=%.3f", (double)last1mEvent.magnitude, (double)last5mEvent.magnitude);
    snprintf(c2Cf, sizeof(c2Cf), "30m=%.3f", (double)ret_30m);
    alertAuditLog(ruleCf, (seqCf[0] != '\0') ? seqCf : nullptr, pCf, srcCf, ageCf, "confluence", 0.0f, c1Cf, c2Cf);
    bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
    if (sent) {
        lastVolumeEventMs = now;
    
        // Mark events as used
        last1mEvent.usedInConfluence = true;
        lastConfluenceAlert = now;
    }
    
    #if !DEBUG_BUTTON_ONLY
    Serial_printf(F("[Confluence] Alert verzonden: 1m=%.2f%%, 5m=%.2f%%, trend=%s, ret_30m=%.2f%%\n"),
                  (direction == EVENT_UP ? last1mEvent.magnitude : -last1mEvent.magnitude),
                  (direction == EVENT_UP ? last5mEvent.magnitude : -last5mEvent.magnitude),
                  safeFmtStr(trendText), ret_30m);
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
    // FASE 8.1: Notificatie waarden verificatie logging (internal)
    #if DEBUG_CALCULATIONS
    {
        char _dmin[32], _dmax[32], _dpx[32];
        formatQuotePriceEur(_dmin, sizeof(_dmin), minVal);
        formatQuotePriceEur(_dmax, sizeof(_dmax), maxVal);
        formatQuotePriceEur(_dpx, sizeof(_dpx), snapshotNotifDisplayPrice());
        Serial.printf(F("[Notify][FormatInternal] timeframe=%s, ret=%.2f%%, direction=%s, minVal=%s, maxVal=%s, price=%s\n"),
                     timeframe, ret, direction, _dmin, _dmax, _dpx);
    }
    #endif
    
    getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer));
    
    char priceStr[32], minStr[32], maxStr[32];
    formatQuotePriceEur(priceStr, sizeof(priceStr), snapshotNotifDisplayPrice());
    formatQuotePriceEur(minStr, sizeof(minStr), minVal);
    formatQuotePriceEur(maxStr, sizeof(maxStr), maxVal);
    if (ret >= 0) {
        snprintf(msgBuffer, sizeof(msgBuffer), 
                "%s UP %s: +%.2f%%\nPrijs %s: %s\nTop: %s Dal: %s", 
                timeframe, direction, ret, timestampBuffer, priceStr, maxStr, minStr);
    } else {
        snprintf(msgBuffer, sizeof(msgBuffer), 
                "%s DOWN %s: %.2f%%\nPrijs %s: %s\nTop: %s Dal: %s", 
                timeframe, direction, ret, timestampBuffer, priceStr, maxStr, minStr);
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
    bool confluenceSentThisRound = false;
    
    // Update volume EMA en status altijd, ook als returns 0 zijn
    updateVolumeEmaIfNewCandle(lastKline1m, volumeEma1m, volumeEma1mInitialized,
                               lastKline1mOpenTime, VOLUME_EMA_WINDOW_1M);
    updateVolumeEmaIfNewCandle(lastKline5m, volumeEma5m, volumeEma5mInitialized,
                               lastKline5mOpenTime, VOLUME_EMA_WINDOW_5M);
    
    VolumeRangeStatus volumeRange1m = evaluateVolumeRange(lastKline1m, volumeEma1m);
    VolumeRangeStatus volumeRange5m = evaluateVolumeRange(lastKline5m, volumeEma5m);
    lastVolumeRange1m = volumeRange1m;
    lastVolumeRange5m = volumeRange5m;

    // Early return: als alle returns 0 zijn, skip alerts (volume status is al bijgewerkt)
    if (ret_1m == 0.0f && ret_5m == 0.0f && ret_30m == 0.0f) {
        return;
    }
    
    // Nachtstand overrides (alleen thresholds, geen algoritme)
    bool nightActive = nightModeEnabled && isNightModeWindowActive();
    float spike5mThreshold = nightActive ? nightSpike5mThreshold : alertThresholds.spike5m;
    float baseMove5mAlert = nightActive ? nightMove5mAlertThreshold : alertThresholds.move5mAlert;
    float baseMove30m = nightActive ? nightMove30mThreshold : alertThresholds.move30m;
    uint32_t cooldown5mMs = nightActive ? (static_cast<uint32_t>(nightCooldown5mSec) * 1000UL) : notificationCooldowns.cooldown5MinMs;
    
    // Nachtstand: forceer auto-volatility + min/max
    bool autoVolEnabledOriginal = autoVolatilityEnabled;
    float autoVolMinOriginal = autoVolatilityMinMultiplier;
    float autoVolMaxOriginal = autoVolatilityMaxMultiplier;
    if (nightActive) {
        autoVolatilityEnabled = true;
        autoVolatilityMinMultiplier = nightAutoVolMinMultiplier;
        autoVolatilityMaxMultiplier = nightAutoVolMaxMultiplier;
    }
    
    // Update volatility window met nieuwe 1m return (Auto-Volatility Mode)
    if (ret_1m != 0.0f) {
        volatilityTracker.updateVolatilityWindow(ret_1m);
    }
    
    // Bereken effective thresholds (Auto-Volatility Mode)
    // Fase 6.1.10: Gebruik struct velden direct i.p.v. #define macros
    EffectiveThresholds effThresh = volatilityTracker.calculateEffectiveThresholds(
        alertThresholds.spike1m, 
        baseMove5mAlert, 
        baseMove30m);
    
    // Restore auto-volatility globals als nachtstand overrides actief waren
    if (nightActive) {
        autoVolatilityEnabled = autoVolEnabledOriginal;
        autoVolatilityMinMultiplier = autoVolMinOriginal;
        autoVolatilityMaxMultiplier = autoVolMaxOriginal;
    }

    // Fase B: regime-multipliers op effThresh (na auto-vol + nachtstand); geen gating wijziging
    const auto rm = selectRegimeRuntimeMultipliers();
    const float finalSpike1mThreshold = fmaxf(effThresh.spike1m * rm.spike1m, 0.05f);
    const float finalMove5mThreshold = fmaxf(effThresh.move5m * rm.move5mAlert, 0.05f);
    const float finalMove30mThreshold = fmaxf(effThresh.move30m * rm.move30m, 0.10f);
    static const float kMinCooldown1mMs = 30000.0f;
    static const float kMinCooldown5mMs = 60000.0f;
    static const float kMinCooldown30mMs = 120000.0f;
    const uint32_t finalCooldown1mMs = (uint32_t)fmaxf(
        (float)notificationCooldowns.cooldown1MinMs * rm.cooldown1m, kMinCooldown1mMs);
    const uint32_t finalCooldown5mMs =
        (uint32_t)fmaxf((float)cooldown5mMs * rm.cooldown5m, kMinCooldown5mMs);
    const uint32_t finalCooldown30mMs = (uint32_t)fmaxf(
        (float)notificationCooldowns.cooldown30MinMs * rm.cooldown30m, kMinCooldown30mMs);

    #if !DEBUG_BUTTON_ONLY
    static unsigned long s_lastRegimePhaseBLogMs = 0;
    if (regimeEngineEnabled) {
        if (now - s_lastRegimePhaseBLogMs >= 60000UL) {
            s_lastRegimePhaseBLogMs = now;
            // Finale regime-geschaalde drempels/cooldowns (niet effThresh); committed alleen
            const RegimeSnapshot snap = regimeEngineGetSnapshot();
            Serial.printf(
                "[RegimeB] committed=%u sp1=%.3f m5=%.3f m30=%.3f cd1=%lu cd5=%lu cd30=%lu\n",
                (unsigned)snap.committedRegime,
                finalSpike1mThreshold,
                finalMove5mThreshold,
                finalMove30mThreshold,
                (unsigned long)finalCooldown1mMs,
                (unsigned long)finalCooldown5mMs,
                (unsigned long)finalCooldown30mMs);
        }
    }
    #endif
    
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
    
    
    // ===== 1-MINUUT SPIKE ALERT =====
    // Voorwaarde: |ret_1m| >= effectiveSpike1mThreshold EN |ret_5m| >= spike5mThreshold in dezelfde richting
    if (ret_1m != 0.0f && ret_5m != 0.0f)
    {
        // Gebruik gecachte absolute waarden
        float absRet1m = cachedAbsRet1m;
        float absRet5m = cachedAbsRet5m;
        
        // Early return: check thresholds eerst (sneller)
        if (absRet1m < finalSpike1mThreshold || absRet5m < spike5mThreshold) {
            // Thresholds niet gehaald, skip rest
        } else {
        // Check of beide in dezelfde richting zijn (beide positief of beide negatief)
        bool sameDirection = ((ret_1m > 0 && ret_5m > 0) || (ret_1m < 0 && ret_5m < 0));
        
        // Threshold check: ret_1m >= effectiveSpike1mThreshold EN ret_5m >= spike5mThreshold
        // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
            bool spikeDetected = sameDirection;
            // Als volume/range data niet beschikbaar is, onderdruk alerts niet
            bool volumeRangeOk1m = (!volumeRange1m.valid) || (volumeRange1m.volumeOk && volumeRange1m.rangeOk);
        
            // Update 1m event state voor Smart Confluence Mode (alleen bij volume/range confirmatie)
            if (spikeDetected && volumeRangeOk1m) {
            update1mEvent(ret_1m, now, finalSpike1mThreshold);
        }
        
        // Debug logging alleen bij spike detectie
        if (spikeDetected) {
#if DEBUG_ALERT_TRACE
                const uint32_t tr1m = alertTraceAllocId();
                const uint32_t tr1m_det_ms = now;
                bool tr_ws_qfail = false;
#if ENABLE_WS_SECOND_QUALITY_GUARD_1M
                {
                    uint32_t wsTrTicks = 0;
                    float wsTrSpread = 0.0f;
                    bool wsTrValid = false;
                    bool wsTrFresh = false;
                    if (getWsSecondLastClosedQuality(wsTrTicks, wsTrSpread, wsTrValid, wsTrFresh) && wsTrValid &&
                        wsTrFresh) {
                        tr_ws_qfail = (wsTrTicks < WS_SECOND_QUALITY_MIN_TICKS_1M) ||
                                      (wsTrSpread > WS_SECOND_QUALITY_MAX_SPREAD_1M);
                    }
                }
#endif
                {
                    const RegimeSnapshot trSnap1m = regimeEngineGetSnapshot();
                    float trP = 0.0f;
                    const char* trS = "unknown";
                    uint32_t trAge = 0;
                    alertTraceCandTrigger(&trP, &trS, &trAge);
                    char c1s[20];
                    char c5s[20];
                    alertTraceFmtEpochS(c1s, sizeof(c1s), lastKline1m.openTime, lastKline1m.valid);
                    alertTraceFmtEpochS(c5s, sizeof(c5s), lastKline5m.openTime, lastKline5m.valid);
                    Serial.printf(
                        "[ALERT_TRACE] id=%lu type=1m_spike phase=candidate ms=%lu r1=%.3f r5=%.3f r30=%.3f th1=%.3f th5f=%.3f vr_ok=%d cf_en=%d ws_qfail=%d night=%d reg_en=%d reg=%u "
                        "trig_pri=%.2f trig_src=%s trig_age_ms=%lu snap1m_open_s=%s snap5m_open_s=%s\n",
                        (unsigned long)tr1m, (unsigned long)tr1m_det_ms, (double)ret_1m, (double)ret_5m,
                        (double)ret_30m, (double)finalSpike1mThreshold, (double)spike5mThreshold,
                        volumeRangeOk1m ? 1 : 0, smartConfluenceEnabled ? 1 : 0, tr_ws_qfail ? 1 : 0,
                        nightActive ? 1 : 0, regimeEngineEnabled ? 1 : 0, (unsigned)trSnap1m.committedRegime,
                        (double)trP, trS, (unsigned long)trAge, c1s, c5s);
                }
#endif
                #if !DEBUG_BUTTON_ONLY
            Serial_printf(F("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%\n"), ret_1m, ret_5m);
                #endif
            
                if (!volumeRangeOk1m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 1m spike onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
#if DEBUG_ALERT_TRACE
                    Serial.printf("[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=volume_range_fail\n",
                                  (unsigned long)tr1m);
#endif
                } else {
            bool wsQualitySuppress1m = false;
#if ENABLE_WS_SECOND_QUALITY_GUARD_1M
            {
                uint32_t wsSecTicks = 0;
                float wsSecSpreadMax = 0.0f;
                bool wsSecValid = false;
                bool wsSecFresh = false;
                if (getWsSecondLastClosedQuality(wsSecTicks, wsSecSpreadMax, wsSecValid, wsSecFresh) && wsSecValid && wsSecFresh) {
                    wsQualitySuppress1m =
                        (wsSecTicks < WS_SECOND_QUALITY_MIN_TICKS_1M) ||
                        (wsSecSpreadMax > WS_SECOND_QUALITY_MAX_SPREAD_1M);
                    if (wsQualitySuppress1m) {
                        static unsigned long s_lastWsQualitySuppressLogMs = 0;
                        if ((now - s_lastWsQualitySuppressLogMs) >= 5000UL) {
                            s_lastWsQualitySuppressLogMs = now;
                            Serial_printf(F("[1m][WS quality guard] Suppress spike\n"));
                        }
                    }
                }
            }
#endif
            if (wsQualitySuppress1m) {
                // Extra kwaliteitsfilter voor 1m spike; bestaande thresholds/cooldowns ongewijzigd.
#if DEBUG_ALERT_TRACE
                Serial.printf("[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=ws_quality_fail\n",
                              (unsigned long)tr1m);
#endif
            } else {
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
                if (confluenceFound) {
                    confluenceSentThisRound = true;
                }
            }
            
            // Als confluence werd gevonden, skip individuele 1m alert (Phase 1A/1B/1C)
            if (confluenceFound) {
                        #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Notify] 1m spike onderdrukt (gebruikt in confluence alert)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                Serial.printf("[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=confluence_used\n",
                              (unsigned long)tr1m);
#endif
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                            #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 1m spike onderdrukt (al gebruikt in confluence)\n"));
                            #endif
#if DEBUG_ALERT_TRACE
                    Serial.printf("[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=confluence_used\n",
                                  (unsigned long)tr1m);
#endif
                        } else if (!volumeEventCooldownOk(now)) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 1m spike onderdrukt (volume-event cooldown)\n"));
                            #endif
#if DEBUG_ALERT_TRACE
                            Serial.printf(
                                "[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=volume_event_cooldown\n",
                                (unsigned long)tr1m);
#endif
                } else {
                    // Bereken min en max uit secondPrices buffer
                    float minVal, maxVal;
                    findMinMaxInSecondPrices(minVal, maxVal);
                    
                            // FASE 8.1: Notificatie waarden verificatie logging (1m spike)
                            #if DEBUG_CALCULATIONS
                            {
                                char _dmin[32], _dmax[32], _dpx[32];
                                formatQuotePriceEur(_dmin, sizeof(_dmin), minVal);
                                formatQuotePriceEur(_dmax, sizeof(_dmax), maxVal);
                                formatQuotePriceEur(_dpx, sizeof(_dpx), snapshotNotifDisplayPrice());
                                Serial.printf(F("[Notify][1mSpike] ret_1m=%.2f%%, ret_5m=%.2f%%, minVal=%s, maxVal=%s, price=%s\n"),
                                             ret_1m, ret_5m, _dmin, _dmax, _dpx);
                            }
                            #endif
                            
                            // Format message met hergebruik van class buffer
                            getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                    char spikePriceStr[32], spikeMinStr[32], spikeMaxStr[32];
                    formatQuotePriceEur(spikePriceStr, sizeof(spikePriceStr), snapshotNotifDisplayPrice());
                    formatQuotePriceEur(spikeMinStr, sizeof(spikeMinStr), minVal);
                    formatQuotePriceEur(spikeMaxStr, sizeof(spikeMaxStr), maxVal);
                    if (ret_1m >= 0) {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%s: %s (%s)\n1m %s spike: +%.2f%% (5m: +%.2f%%)\n1m %s: %s\n1m %s: %s",
                                         safeFmtStr(getText("Live prijs", "Live price")),
                                         spikePriceStr, timestampBuffer,
                                         getText("OP", "UP"), ret_1m, ret_5m,
                                         getText("Top", "Top"), spikeMaxStr,
                                         getText("Dal", "Low"), spikeMinStr);
                    } else {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%s: %s (%s)\n1m %s spike: %.2f%% (5m: %.2f%%)\n1m %s: %s\n1m %s: %s",
                                         safeFmtStr(getText("Live prijs", "Live price")),
                                         spikePriceStr, timestampBuffer,
                                         getText("NEER", "DOWN"), ret_1m, ret_5m,
                                         getText("Top", "Top"), spikeMaxStr,
                                         getText("Dal", "Low"), spikeMinStr);
                            }
                            appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange1m);
                    
                    const char* colorTag = determineColorTag(ret_1m, finalSpike1mThreshold, finalSpike1mThreshold * 1.5f);
                            buildDirectionalMinuteAlertTitle(titleBuffer, sizeof(titleBuffer), ret_1m >= 0.0f,
                                                             bitvavoSymbol, "1m", getText("Spike", "Spike"));
                    
                    // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    {
                        const bool ok1mSend = checkAlertConditions(now, lastNotification1Min, finalCooldown1mMs,
                                                                   alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR,
                                                                   "1m spike");
#if DEBUG_ALERT_TRACE
                        if (!ok1mSend) {
                            const char* rsn =
                                (lastNotification1Min != 0 && (now - lastNotification1Min < finalCooldown1mMs))
                                    ? "cooldown"
                                    : "hour_cap";
                            Serial.printf("[ALERT_TRACE] id=%lu type=1m_spike phase=suppress reason=%s\n",
                                          (unsigned long)tr1m, rsn);
                        }
#endif
                        if (ok1mSend) {
                                const char* rule1m = (ret_1m >= 0.0f) ? "1m_up" : "1m_down";
                                char seq1m[41];
                                ntfyBuildSequenceId(titleBuffer, msgBuffer, seq1m, sizeof(seq1m));
                                float p1m = 0.0f;
                                const char* s1m = "UNKNOWN";
                                uint32_t a1m = 0;
                                alertAuditPriceSnapshot(&p1m, &s1m, &a1m);
                                char x1m[28];
                                char x2m[28];
                                snprintf(x1m, sizeof(x1m), "ret_1m=%.3f", (double)ret_1m);
                                snprintf(x2m, sizeof(x2m), "ret_5m=%.3f", (double)ret_5m);
                                alertAuditLog(rule1m, (seq1m[0] != '\0') ? seq1m : nullptr, p1m, s1m, a1m, "ret_1m",
                                              finalSpike1mThreshold, x1m, x2m);
#if DEBUG_ALERT_TRACE
                                {
                                    const uint32_t msn = millis();
                                    float dP = 0.0f;
                                    const char* dS = "na";
                                    char dAge[20];
                                    alertTraceDispNotifSnap(msn, &dP, &dS, dAge, sizeof(dAge));
                                    {
                                        char _td[32], _tn[32], _tx[32];
                                        formatQuotePriceEur(_td, sizeof(_td), snapshotNotifDisplayPrice());
                                        formatQuotePriceEur(_tn, sizeof(_tn), minVal);
                                        formatQuotePriceEur(_tx, sizeof(_tx), maxVal);
                                        Serial.printf(
                                        "[ALERT_TRACE] id=%lu type=1m_spike phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%s max=%s det_ms=%lu\n",
                                        (unsigned long)tr1m, (unsigned long)msn,
                                        _td, dS, dAge, _tn, _tx,
                                        (unsigned long)tr1m_det_ms);
                                    }
                                }
#endif
                                bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
#if DEBUG_ALERT_TRACE
                                Serial.printf(
                                    "[ALERT_TRACE] id=%lu type=1m_spike phase=send_done ms=%lu sent=%d note=enqueue_return\n",
                                    (unsigned long)tr1m, (unsigned long)millis(), sent ? 1 : 0);
#endif
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
        }
    }
    
    // ===== Fase C Patch 1: ENERGIEK standalone 1m burst (geen 5m confirm) =====
    if (regimeEngineEnabled && regimeEnergiekAllowStandalone1mBurst && ret_1m != 0.0f) {
        const RegimeSnapshot snapEn = regimeEngineGetSnapshot();
        if (snapEn.committedRegime == REGIME_ENERGIEK) {
            const float standalone1mTh = fmaxf(finalSpike1mThreshold * regimeEnergiekStandalone1mFactor, 0.05f);
            const float absRet1mStandalone = cachedAbsRet1m;
            const bool normal1mSpikeDetected =
                (ret_1m != 0.0f && ret_5m != 0.0f) &&
                (cachedAbsRet1m >= finalSpike1mThreshold) && (cachedAbsRet5m >= spike5mThreshold) &&
                (((ret_1m > 0.0f && ret_5m > 0.0f) || (ret_1m < 0.0f && ret_5m < 0.0f)));
            bool volumeRangeOk1mS = (!volumeRange1m.valid) || (volumeRange1m.volumeOk && volumeRange1m.rangeOk);

            if (absRet1mStandalone >= standalone1mTh) {
                if (!volumeRangeOk1mS) {
                    // zelfde volume/range policy als 1m spike
                } else if (normal1mSpikeDetected && volumeRangeOk1mS) {
                    // Standaard 1m+5m-spikepad hierboven heeft prioriteit (geen dubbele confluence/standalone).
                } else {
#if DEBUG_ALERT_TRACE
                    const uint32_t tr1e = alertTraceAllocId();
                    const uint32_t tr1e_det_ms = now;
                    {
                        const RegimeSnapshot trSnapE = regimeEngineGetSnapshot();
                        float trP = 0.0f;
                        const char* trS = "unknown";
                        uint32_t trAge = 0;
                        alertTraceCandTrigger(&trP, &trS, &trAge);
                        char c1s[20];
                        char c5s[20];
                        alertTraceFmtEpochS(c1s, sizeof(c1s), lastKline1m.openTime, lastKline1m.valid);
                        alertTraceFmtEpochS(c5s, sizeof(c5s), lastKline5m.openTime, lastKline5m.valid);
                        Serial.printf(
                            "[ALERT_TRACE] id=%lu type=1m_energiek phase=candidate ms=%lu r1=%.3f r5=%.3f r30=%.3f th_std=%.3f th_st=%.3f vr_ok=%d cf_en=%d night=%d reg=%u "
                            "trig_pri=%.2f trig_src=%s trig_age_ms=%lu snap1m_open_s=%s snap5m_open_s=%s\n",
                            (unsigned long)tr1e, (unsigned long)tr1e_det_ms, (double)ret_1m, (double)ret_5m,
                            (double)ret_30m, (double)finalSpike1mThreshold, (double)standalone1mTh,
                            volumeRangeOk1mS ? 1 : 0, smartConfluenceEnabled ? 1 : 0, nightActive ? 1 : 0,
                            (unsigned)trSnapE.committedRegime, (double)trP, trS, (unsigned long)trAge, c1s, c5s);
                    }
#endif
                    update1mEvent(ret_1m, now, standalone1mTh);
                    bool confluenceFoundS = false;
                    if (smartConfluenceEnabled) {
                        confluenceFoundS = checkAndSendConfluenceAlert(now, ret_30m);
                        if (confluenceFoundS) {
                            confluenceSentThisRound = true;
                        }
                    }
                    if (confluenceFoundS) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 1m ENERGIEK standalone onderdrukt (confluence)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                        Serial.printf("[ALERT_TRACE] id=%lu type=1m_energiek phase=suppress reason=confluence_used\n",
                                      (unsigned long)tr1e);
#endif
                    } else if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 1m ENERGIEK standalone onderdrukt (confluence)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                        Serial.printf("[ALERT_TRACE] id=%lu type=1m_energiek phase=suppress reason=confluence_used\n",
                                      (unsigned long)tr1e);
#endif
                    } else if (!volumeEventCooldownOk(now)) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 1m ENERGIEK standalone onderdrukt (volume-event cooldown)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                        Serial.printf(
                            "[ALERT_TRACE] id=%lu type=1m_energiek phase=suppress reason=volume_event_cooldown\n",
                            (unsigned long)tr1e);
#endif
                    } else if (!energiekStandalone1mGuardsOk(snapEn.directionScore,
                                                            regimeEnergiekMinDirectionStrength,
                                                            hasRet30m,
                                                            ret_1m,
                                                            ret_30m)) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial.println(F("[PhaseC] suppressed_energiek_1m_low_direction"));
                        #endif
#if DEBUG_ALERT_TRACE
                        Serial.printf("[ALERT_TRACE] id=%lu type=1m_energiek phase=suppress reason=direction_guard\n",
                                      (unsigned long)tr1e);
#endif
                    } else {
                        // Minimaal, self-contained bericht: geen ret_5m/min-max/appendVolumeRangeInfo
                        // (minder stack en geen hergebruik van 5m-velden in de tekst).
                        #if !DEBUG_BUTTON_ONLY
                        Serial.println(F("[PhaseC] standalone_enter"));
                        #endif
                        getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                        char spikePriceStrS[32];
                        formatQuotePriceEur(spikePriceStrS, sizeof(spikePriceStrS), snapshotNotifDisplayPrice());
                        const char* lineStandalone = safeFmtStr(getText(
                            "Standalone (geen 5m bevestiging)", "Standalone (no 5m confirmation)"));
                        const char* tsS = safeFmtStr(timestampBuffer);
                        const char* symS = safeFmtStr(bitvavoSymbol);
                        const char* spikeLblS = safeFmtStr(getText("Spike", "Spike"));
                        #if !DEBUG_BUTTON_ONLY
                        Serial.println(F("[PhaseC] standalone_before_format"));
                        #endif
                        snprintf(msgBuffer, sizeof(msgBuffer),
                                 "%s: %s (%s)\n%s\n1m: %+.2f%%",
                                 safeFmtStr(getText("Live prijs", "Live price")),
                                 spikePriceStrS, tsS, lineStandalone, ret_1m);
                        const char* colorTagS =
                            determineColorTag(ret_1m, standalone1mTh, standalone1mTh * 1.5f);
                        buildDirectionalMinuteAlertTitle(titleBuffer, sizeof(titleBuffer), ret_1m >= 0.0f,
                                                         symS, "1m", spikeLblS);
                        {
                            const bool ok1eSend = checkAlertConditions(
                                now, lastNotification1Min, finalCooldown1mMs, alerts1MinThisHour,
                                MAX_1M_ALERTS_PER_HOUR, "1m spike ENERGIEK standalone");
#if DEBUG_ALERT_TRACE
                            if (!ok1eSend) {
                                const char* rsn =
                                    (lastNotification1Min != 0 && (now - lastNotification1Min < finalCooldown1mMs))
                                        ? "cooldown"
                                        : "hour_cap";
                                Serial.printf("[ALERT_TRACE] id=%lu type=1m_energiek phase=suppress reason=%s\n",
                                              (unsigned long)tr1e, rsn);
                            }
#endif
                            if (ok1eSend) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial.println(F("[PhaseC] standalone_before_send"));
                            #endif
                            const char* rule1s = (ret_1m >= 0.0f) ? "1m_up" : "1m_down";
                            char seq1s[41];
                            ntfyBuildSequenceId(titleBuffer, msgBuffer, seq1s, sizeof(seq1s));
                            float p1s = 0.0f;
                            const char* s1s = "UNKNOWN";
                            uint32_t a1s = 0;
                            alertAuditPriceSnapshot(&p1s, &s1s, &a1s);
                            char u1s[28];
                            snprintf(u1s, sizeof(u1s), "ret_1m=%.3f", (double)ret_1m);
                            alertAuditLog(rule1s, (seq1s[0] != '\0') ? seq1s : nullptr, p1s, s1s, a1s, "ret_1m",
                                          standalone1mTh, u1s, "standalone");
#if DEBUG_ALERT_TRACE
                            {
                                const uint32_t mse = millis();
                                float dP = 0.0f;
                                const char* dS = "na";
                                char dAge[20];
                                alertTraceDispNotifSnap(mse, &dP, &dS, dAge, sizeof(dAge));
                                {
                                    char _ed[32];
                                    formatQuotePriceEur(_ed, sizeof(_ed), snapshotNotifDisplayPrice());
                                    Serial.printf(
                                    "[ALERT_TRACE] id=%lu type=1m_energiek phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%s max=%s det_ms=%lu\n",
                                    (unsigned long)tr1e, (unsigned long)mse,
                                    _ed, dS, dAge, _ed, _ed,
                                    (unsigned long)tr1e_det_ms);
                                }
                            }
#endif
                            bool sentS = sendNotification(titleBuffer, msgBuffer, safeFmtStr(colorTagS));
#if DEBUG_ALERT_TRACE
                            Serial.printf(
                                "[ALERT_TRACE] id=%lu type=1m_energiek phase=send_done ms=%lu sent=%d note=enqueue_return\n",
                                (unsigned long)tr1e, (unsigned long)millis(), sentS ? 1 : 0);
#endif
                            if (sentS) {
                                lastNotification1Min = now;
                                alerts1MinThisHour++;
                                lastVolumeEventMs = now;
                                #if !DEBUG_BUTTON_ONLY
                                Serial_printf(
                                    F("[Notify] 1m ENERGIEK standalone notificatie verstuurd (%d/%d dit uur)\n"),
                                    alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
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
        if (absRet30m < finalMove30mThreshold || absRet5m < alertThresholds.move5m) {
            // Thresholds niet gehaald, skip rest
        } else {
        // Check of beide in dezelfde richting zijn
        bool sameDirection = ((ret_30m > 0 && ret_5m > 0) || (ret_30m < 0 && ret_5m < 0));
        
        // Threshold check: ret_30m >= effectiveMove30mThreshold EN ret_5m >= move5mThreshold
        // Note: move5mThreshold is de filter threshold, niet de alert threshold
        // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
            bool moveDetected = sameDirection;
            // Als volume/range data niet beschikbaar is, onderdruk alerts niet
            bool volumeRangeOk5m = (!volumeRange5m.valid) || (volumeRange5m.volumeOk && volumeRange5m.rangeOk);
        
        // Debug logging alleen bij move detectie
        if (moveDetected) {
#if DEBUG_ALERT_TRACE
                const uint32_t tr30 = alertTraceAllocId();
                const uint32_t tr30_det_ms = now;
                {
                    const RegimeSnapshot trSnap30 = regimeEngineGetSnapshot();
                    float trP = 0.0f;
                    const char* trS = "unknown";
                    uint32_t trAge = 0;
                    alertTraceCandTrigger(&trP, &trS, &trAge);
                    char c1s[20];
                    char c5s[20];
                    alertTraceFmtEpochS(c1s, sizeof(c1s), lastKline1m.openTime, lastKline1m.valid);
                    alertTraceFmtEpochS(c5s, sizeof(c5s), lastKline5m.openTime, lastKline5m.valid);
                    Serial.printf(
                        "[ALERT_TRACE] id=%lu type=30m_move phase=candidate ms=%lu r1=%.3f r5=%.3f r30=%.3f th30=%.3f th5f=%.3f vr_ok=%d cf_en=%d ws_qfail=0 night=%d reg_en=%d reg=%u "
                        "trig_pri=%.2f trig_src=%s trig_age_ms=%lu snap1m_open_s=%s snap5m_open_s=%s\n",
                        (unsigned long)tr30, (unsigned long)tr30_det_ms, (double)ret_1m, (double)ret_5m,
                        (double)ret_30m, (double)finalMove30mThreshold, (double)alertThresholds.move5m,
                        volumeRangeOk5m ? 1 : 0, smartConfluenceEnabled ? 1 : 0, nightActive ? 1 : 0,
                        regimeEngineEnabled ? 1 : 0, (unsigned)trSnap30.committedRegime, (double)trP, trS,
                        (unsigned long)trAge, c1s, c5s);
                }
#endif
                #if !DEBUG_BUTTON_ONLY
            Serial_printf(F("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%\n"), ret_30m, ret_5m);
                #endif
            
                if (!volumeRangeOk5m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 30m move onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
#if DEBUG_ALERT_TRACE
                    Serial.printf("[ALERT_TRACE] id=%lu type=30m_move phase=suppress reason=volume_range_fail\n",
                                  (unsigned long)tr30);
#endif
                } else {
            // Bereken min en max uit laatste 30 minuten van minuteAverages buffer
            float minVal, maxVal;
            findMinMaxInLast30Minutes(minVal, maxVal);
            
                    // Format message met hergebruik van class buffer
                    getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
            char movePriceStr[32], moveMinStr[32], moveMaxStr[32];
            formatQuotePriceEur(movePriceStr, sizeof(movePriceStr), snapshotNotifDisplayPrice());
            formatQuotePriceEur(moveMinStr, sizeof(moveMinStr), minVal);
            formatQuotePriceEur(moveMaxStr, sizeof(moveMaxStr), maxVal);
            if (ret_30m >= 0) {
                        snprintf(msgBuffer, sizeof(msgBuffer), 
                                 "%s: %s (%s)\n30m %s move: +%.2f%% (5m: +%.2f%%)\n30m %s: %s\n30m %s: %s",
                                 safeFmtStr(getText("Live prijs", "Live price")),
                                 movePriceStr, timestampBuffer,
                                 getText("OP", "UP"), ret_30m, ret_5m,
                                 getText("Top", "Top"), moveMaxStr,
                                 getText("Dal", "Low"), moveMinStr);
            } else {
                        snprintf(msgBuffer, sizeof(msgBuffer), 
                                 "%s: %s (%s)\n30m %s move: %.2f%% (5m: %.2f%%)\n30m %s: %s\n30m %s: %s",
                                 safeFmtStr(getText("Live prijs", "Live price")),
                                 movePriceStr, timestampBuffer,
                                 getText("NEER", "DOWN"), ret_30m, ret_5m,
                                 getText("Top", "Top"), moveMaxStr,
                                 getText("Dal", "Low"), moveMinStr);
                    }
                    appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange5m,
                                          safeFmtStr(getText("5m candle — volume/range",
                                                            "5m candle — volume/range")));
            
            const char* colorTag = determineColorTag(ret_30m, finalMove30mThreshold, finalMove30mThreshold * 1.5f);
                    buildDirectionalMinuteAlertTitle(titleBuffer, sizeof(titleBuffer), ret_30m >= 0.0f,
                                                     bitvavoSymbol, "30m", getText("Move", "Move"));
            
            // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    if (!volumeEventCooldownOk(now)) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 30m move onderdrukt (volume-event cooldown)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                        Serial.printf(
                            "[ALERT_TRACE] id=%lu type=30m_move phase=suppress reason=volume_event_cooldown\n",
                            (unsigned long)tr30);
#endif
                    } else {
                        const bool ok30Send = checkAlertConditions(now, lastNotification30Min, finalCooldown30mMs,
                                                                   alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR,
                                                                   "30m move");
#if DEBUG_ALERT_TRACE
                        if (!ok30Send) {
                            const char* rsn =
                                (lastNotification30Min != 0 && (now - lastNotification30Min < finalCooldown30mMs))
                                    ? "cooldown"
                                    : "hour_cap";
                            Serial.printf("[ALERT_TRACE] id=%lu type=30m_move phase=suppress reason=%s\n",
                                          (unsigned long)tr30, rsn);
                        }
#endif
                        if (ok30Send) {
                        char seq30[41];
                        ntfyBuildSequenceId(titleBuffer, msgBuffer, seq30, sizeof(seq30));
                        float p30 = 0.0f;
                        const char* s30 = "UNKNOWN";
                        uint32_t a30 = 0;
                        alertAuditPriceSnapshot(&p30, &s30, &a30);
                        char u30a[32];
                        char u30b[32];
                        snprintf(u30a, sizeof(u30a), "ret_30m=%.3f", (double)ret_30m);
                        snprintf(u30b, sizeof(u30b), "ret_5m=%.3f", (double)ret_5m);
                        alertAuditLog("30m_move", (seq30[0] != '\0') ? seq30 : nullptr, p30, s30, a30, "ret_30m",
                                      finalMove30mThreshold, u30a, u30b);
#if DEBUG_ALERT_TRACE
                        {
                            const uint32_t ms3 = millis();
                            float dP = 0.0f;
                            const char* dS = "na";
                            char dAge[20];
                            alertTraceDispNotifSnap(ms3, &dP, &dS, dAge, sizeof(dAge));
                            {
                                char _30d[32], _30n[32], _30x[32];
                                formatQuotePriceEur(_30d, sizeof(_30d), snapshotNotifDisplayPrice());
                                formatQuotePriceEur(_30n, sizeof(_30n), minVal);
                                formatQuotePriceEur(_30x, sizeof(_30x), maxVal);
                                Serial.printf(
                                "[ALERT_TRACE] id=%lu type=30m_move phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%s max=%s det_ms=%lu\n",
                                (unsigned long)tr30, (unsigned long)ms3,
                                _30d, dS, dAge, _30n, _30x,
                                (unsigned long)tr30_det_ms);
                            }
                        }
#endif
                        bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
#if DEBUG_ALERT_TRACE
                        Serial.printf(
                            "[ALERT_TRACE] id=%lu type=30m_move phase=send_done ms=%lu sent=%d note=enqueue_return\n",
                            (unsigned long)tr30, (unsigned long)millis(), sent ? 1 : 0);
#endif
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
    }
    
    // ===== 5-MINUTEN MOVE ALERT =====
    // Voorwaarde: |ret_5m| >= effectiveMove5mThreshold
    if (ret_5m != 0.0f)
    {
        // Gebruik gecachte absolute waarde
        float absRet5m = cachedAbsRet5m;
        
        // Early return: check threshold eerst (sneller)
        if (absRet5m < finalMove5mThreshold) {
            // Threshold niet gehaald, skip rest
        } else {
        // Threshold check: ret_5m >= effectiveMove5mThreshold
            bool move5mDetected = true;
            // Als volume/range data niet beschikbaar is, onderdruk alerts niet
            bool volumeRangeOk5m = (!volumeRange5m.valid) || (volumeRange5m.volumeOk && volumeRange5m.rangeOk);
        
            // Update 5m event state voor Smart Confluence Mode (alleen bij volume/range confirmatie)
            if (volumeRangeOk5m) {
            update5mEvent(ret_5m, now, finalMove5mThreshold);
        }
        
        // Debug logging alleen bij move detectie
        if (move5mDetected) {
#if DEBUG_ALERT_TRACE
                const uint32_t tr5 = alertTraceAllocId();
                const uint32_t tr5_det_ms = now;
                {
                    const RegimeSnapshot trSnap5 = regimeEngineGetSnapshot();
                    float trP = 0.0f;
                    const char* trS = "unknown";
                    uint32_t trAge = 0;
                    alertTraceCandTrigger(&trP, &trS, &trAge);
                    char c1s[20];
                    char c5s[20];
                    alertTraceFmtEpochS(c1s, sizeof(c1s), lastKline1m.openTime, lastKline1m.valid);
                    alertTraceFmtEpochS(c5s, sizeof(c5s), lastKline5m.openTime, lastKline5m.valid);
                    Serial.printf(
                        "[ALERT_TRACE] id=%lu type=5m_move phase=candidate ms=%lu r1=%.3f r5=%.3f r30=%.3f th5=%.3f vr_ok=%d cf_en=%d ws_qfail=0 night=%d reg_en=%d reg=%u night5m_ok=%d "
                        "trig_pri=%.2f trig_src=%s trig_age_ms=%lu snap1m_open_s=%s snap5m_open_s=%s\n",
                        (unsigned long)tr5, (unsigned long)tr5_det_ms, (double)ret_1m, (double)ret_5m,
                        (double)ret_30m, (double)finalMove5mThreshold, volumeRangeOk5m ? 1 : 0,
                        smartConfluenceEnabled ? 1 : 0, nightActive ? 1 : 0, regimeEngineEnabled ? 1 : 0,
                        (unsigned)trSnap5.committedRegime, nightModeAllows5mMove(ret_5m, ret_30m) ? 1 : 0,
                        (double)trP, trS, (unsigned long)trAge, c1s, c5s);
                }
#endif
                #if !DEBUG_BUTTON_ONLY
            Serial_printf(F("[Notify] 5m move: ret_5m=%.2f%%\n"), ret_5m);
                #endif
            
                if (!volumeRangeOk5m) {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_printf(F("[Notify] 5m move onderdrukt (volume/range confirmatie fail)\n"));
                    #endif
#if DEBUG_ALERT_TRACE
                    Serial.printf("[ALERT_TRACE] id=%lu type=5m_move phase=suppress reason=volume_range_fail\n",
                                  (unsigned long)tr5);
#endif
                } else {
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
                if (confluenceFound) {
                    confluenceSentThisRound = true;
                }
            }
            
            // Phase 1C: laat 5m doorgaan als er deze ronde een confluence alert is verstuurd
            // (zowel suppress via usedInConfluence als volume-event cooldown worden dan overgeslagen)
            // Check of dit event al gebruikt is in confluence (suppress individuele alert, behalve bij Phase 1C-case)
            if (smartConfluenceEnabled && last5mEvent.usedInConfluence && !confluenceSentThisRound) {
                        #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Notify] 5m move onderdrukt (al gebruikt in confluence)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                Serial.printf("[ALERT_TRACE] id=%lu type=5m_move phase=suppress reason=confluence_used\n",
                              (unsigned long)tr5);
#endif
            } else if (!confluenceSentThisRound && !volumeEventCooldownOk(now)) {
                        #if !DEBUG_BUTTON_ONLY
                        Serial_printf(F("[Notify] 5m move onderdrukt (volume-event cooldown)\n"));
                        #endif
#if DEBUG_ALERT_TRACE
                Serial.printf(
                    "[ALERT_TRACE] id=%lu type=5m_move phase=suppress reason=volume_event_cooldown\n",
                    (unsigned long)tr5);
#endif
            } else if (!nightModeAllows5mMove(ret_5m, ret_30m)) {
                            #if !DEBUG_BUTTON_ONLY
                            Serial_printf(F("[Notify] 5m move onderdrukt (nachtstand, 30m richting mismatch)\n"));
                            #endif
#if DEBUG_ALERT_TRACE
                            Serial.printf(
                                "[ALERT_TRACE] id=%lu type=5m_move phase=suppress reason=night_direction_mismatch\n",
                                (unsigned long)tr5);
#endif
                } else {
                            // Bereken min en max uit fiveMinutePrices buffer (geoptimaliseerde versie)
                            float minVal, maxVal;
                            findMinMaxInFiveMinutePrices(minVal, maxVal);

#if !DEBUG_BUTTON_ONLY
                            {
                                static uint32_t s_lastAlert5mDirLogMs = 0;
                                constexpr uint32_t kAlert5mDirLogMinMs = 8000UL;
                                if (now - s_lastAlert5mDirLogMs >= kAlert5mDirLogMinMs ||
                                    now < s_lastAlert5mDirLogMs) {
                                    s_lastAlert5mDirLogMs = now;
                                    const float* fp = priceData.getFiveMinutePrices();
                                    uint16_t fi = priceData.getFiveMinuteIndex();
                                    bool fFilled = priceData.getFiveMinuteArrayFilled();
                                    float refOldest = 0.0f;
                                    if (fp != nullptr) {
                                        if (fFilled) {
                                            refOldest = fp[fi % SECONDS_PER_5MINUTES];
                                        } else if (fi > 0) {
                                            refOldest = fp[0];
                                        }
                                    }
                                    const float dispPx = snapshotNotifDisplayPrice();
                                    const RegimeSnapshot rs5 = regimeEngineGetSnapshot();
                                    const char* dirLbl = (ret_5m >= 0.0f) ? "OP" : "NEER";
                                    char _5d[32], _5r[32], _5n[32], _5x[32];
                                    formatQuotePriceEur(_5d, sizeof(_5d), dispPx);
                                    formatQuotePriceEur(_5r, sizeof(_5r), refOldest);
                                    formatQuotePriceEur(_5n, sizeof(_5n), minVal);
                                    formatQuotePriceEur(_5x, sizeof(_5x), maxVal);
                                    Serial.printf(
                                        "[ALERT_5M_DIR] signed_r5=%.4f abs_r5=%.4f th_final=%.4f eff_move5m_base=%.4f "
                                        "reg_m5=%.4f reg_id=%u disp=%s ref_oldest_5m=%s rng_min=%s rng_max=%s "
                                        "dir=%s rule=sign_ret_5m\n",
                                        (double)ret_5m, (double)absRet5m, (double)finalMove5mThreshold,
                                        (double)effThresh.move5m, (double)rm.move5mAlert,
                                        (unsigned)rs5.committedRegime, _5d, _5r, _5n, _5x, dirLbl);
                                }
                            }
#endif
                            
                            // Format message met hergebruik van class buffer
                            getFormattedTimestampForNotification(timestampBuffer, sizeof(timestampBuffer));
                            // ret_30m is beschikbaar in checkAndNotify scope
                    char move5mPriceStr[32], move5mMinStr[32], move5mMaxStr[32];
                    formatQuotePriceEur(move5mPriceStr, sizeof(move5mPriceStr), snapshotNotifDisplayPrice());
                    formatQuotePriceEur(move5mMinStr, sizeof(move5mMinStr), minVal);
                    formatQuotePriceEur(move5mMaxStr, sizeof(move5mMaxStr), maxVal);
                    if (ret_5m >= 0) {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%s: %s (%s)\n5m %s move: +%.2f%% (30m: %+.2f%%)\n5m %s: %s\n5m %s: %s",
                                         safeFmtStr(getText("Live prijs", "Live price")),
                                         move5mPriceStr, timestampBuffer,
                                         getText("OP", "UP"), ret_5m, ret_30m,
                                         getText("Top", "Top"), move5mMaxStr,
                                         getText("Dal", "Low"), move5mMinStr);
                    } else {
                                snprintf(msgBuffer, sizeof(msgBuffer), 
                                         "%s: %s (%s)\n5m %s move: %.2f%% (30m: %+.2f%%)\n5m %s: %s\n5m %s: %s",
                                         safeFmtStr(getText("Live prijs", "Live price")),
                                         move5mPriceStr, timestampBuffer,
                                         getText("NEER", "DOWN"), ret_5m, ret_30m,
                                         getText("Top", "Top"), move5mMaxStr,
                                         getText("Dal", "Low"), move5mMinStr);
                            }
                            appendVolumeRangeInfo(msgBuffer, sizeof(msgBuffer), volumeRange5m);
                    
                    const char* colorTag = (ret_5m >= 0.0f) ? "\xF0\x9F\x9F\xA6" /* 🟦 */ : "\xF0\x9F\x9F\xA7" /* 🟧 */;
                            buildDirectionalMinuteAlertTitle(titleBuffer, sizeof(titleBuffer), ret_5m >= 0.0f,
                                                             bitvavoSymbol, "5m", getText("Move", "Move"));
                    
                    // Fase 6.1.10: Gebruik struct veld direct i.p.v. #define macro
                    {
                        const bool ok5Send = checkAlertConditions(now, lastNotification5Min, finalCooldown5mMs,
                                                                  alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR,
                                                                  "5m move");
#if DEBUG_ALERT_TRACE
                        if (!ok5Send) {
                            const char* rsn =
                                (lastNotification5Min != 0 && (now - lastNotification5Min < finalCooldown5mMs))
                                    ? "cooldown"
                                    : "hour_cap";
                            Serial.printf("[ALERT_TRACE] id=%lu type=5m_move phase=suppress reason=%s\n",
                                          (unsigned long)tr5, rsn);
                        }
#endif
                        if (ok5Send) {
                                const char* rule5 = (ret_5m >= 0.0f) ? "5m_up" : "5m_down";
                                char seq5[41];
                                ntfyBuildSequenceId(titleBuffer, msgBuffer, seq5, sizeof(seq5));
                                float p5 = 0.0f;
                                const char* s5 = "UNKNOWN";
                                uint32_t a5 = 0;
                                alertAuditPriceSnapshot(&p5, &s5, &a5);
                                char u5a[28];
                                char u5b[28];
                                snprintf(u5a, sizeof(u5a), "ret_5m=%.3f", (double)ret_5m);
                                snprintf(u5b, sizeof(u5b), "ret_30m=%.3f", (double)ret_30m);
                                alertAuditLog(rule5, (seq5[0] != '\0') ? seq5 : nullptr, p5, s5, a5, "ret_5m",
                                              finalMove5mThreshold, u5a, u5b);
#if DEBUG_ALERT_TRACE
                                {
                                    const uint32_t ms5 = millis();
                                    float dP = 0.0f;
                                    const char* dS = "na";
                                    char dAge[20];
                                    alertTraceDispNotifSnap(ms5, &dP, &dS, dAge, sizeof(dAge));
                                    {
                                        char _5td[32], _5tn[32], _5tx[32];
                                        formatQuotePriceEur(_5td, sizeof(_5td), snapshotNotifDisplayPrice());
                                        formatQuotePriceEur(_5tn, sizeof(_5tn), minVal);
                                        formatQuotePriceEur(_5tx, sizeof(_5tx), maxVal);
                                        Serial.printf(
                                        "[ALERT_TRACE] id=%lu type=5m_move phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%s max=%s det_ms=%lu\n",
                                        (unsigned long)tr5, (unsigned long)ms5,
                                        _5td, dS, dAge, _5tn, _5tx,
                                        (unsigned long)tr5_det_ms);
                                    }
                                }
#endif
                                bool sent = sendNotification(titleBuffer, msgBuffer, colorTag);
#if DEBUG_ALERT_TRACE
                                Serial.printf(
                                    "[ALERT_TRACE] id=%lu type=5m_move phase=send_done ms=%lu sent=%d note=enqueue_return\n",
                                    (unsigned long)tr5, (unsigned long)millis(), sent ? 1 : 0);
#endif
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

// Rate-limited: max. ~1 log per secondary type per interval (geen spam bij langdurige throttle).
static constexpr uint32_t k2hSecondaryThrottlePrecheckLogIntervalMs = 10000UL;

static void logSecondary2hThrottlePrecheckRateLimited(Alert2HType t, uint32_t nowMs)
{
#if DEBUG_BUTTON_ONLY
    (void)t;
    (void)nowMs;
#else
    static uint32_t s_lastLogCompressMs;
    static uint32_t s_lastLogMeanMs;
    static uint32_t s_lastLogAnchorMs;
    uint32_t* lastMs = nullptr;
    const char* tag = "";
    switch (t) {
        case ALERT2H_COMPRESS:
            lastMs = &s_lastLogCompressMs;
            tag = "Compress";
            break;
        case ALERT2H_MEAN_TOUCH:
            lastMs = &s_lastLogMeanMs;
            tag = "Mean Touch";
            break;
        case ALERT2H_ANCHOR_CTX:
            lastMs = &s_lastLogAnchorMs;
            tag = "Anchor Context";
            break;
        default:
            return;
    }
    if (*lastMs != 0U && (nowMs - *lastMs) < k2hSecondaryThrottlePrecheckLogIntervalMs) {
        return;
    }
    *lastMs = nowMs;
    Serial_printf(F("[2h secondary] precheck throttle: %s (skip title/msg build)\n"), tag);
#endif
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
    float breakMargin = alert2HThresholds.breakMarginPct;
    float breakThresholdUp = metrics.high2h * (1.0f + breakMargin / 100.0f);
    float breakThresholdDown = metrics.low2h * (1.0f - breakMargin / 100.0f);
    
    // Static functie: gebruik lokale buffers (title 80 voor emoji-prefix)
    char title[80];
    char msg[280];  // Ruimte voor dynamisch EUR-format (meerdere prijzen per regel)
    char timestamp[32];
    
    #if DEBUG_2H_ALERTS
    // Rate-limited debug logging (1x per 60 sec)
    static uint32_t lastDebugLogMs = 0;
    static constexpr uint32_t DEBUG_LOG_INTERVAL_MS = 60UL * 1000UL; // 60 sec
    
    if ((now - lastDebugLogMs) >= DEBUG_LOG_INTERVAL_MS) {
        // Gebruik reeds berekende thresholds
        bool condBreakUp = lastPrice > breakThresholdUp;
        bool condBreakDown = lastPrice < breakThresholdDown;
        bool condCompress = metrics.rangePct < alert2HThresholds.compressThresholdPct;
        float distPct = absf((lastPrice - metrics.avg2h) / metrics.avg2h * 100.0f);
        bool condTouch = distPct <= alert2HThresholds.meanTouchBandPct;
        
        {
            char _lp[32], _av[32], _hi[32], _lo[32], _an[32];
            formatQuotePriceEur(_lp, sizeof(_lp), lastPrice);
            formatQuotePriceEur(_av, sizeof(_av), metrics.avg2h);
            formatQuotePriceEur(_hi, sizeof(_hi), metrics.high2h);
            formatQuotePriceEur(_lo, sizeof(_lo), metrics.low2h);
            formatQuotePriceEur(_an, sizeof(_an), activeAnchorPrice);
            Serial.printf("[2H-DBG] price=%s avg2h=%s high2h=%s low2h=%s rangePct=%.2f%% anchor=%s\n",
                         _lp, _av, _hi, _lo, metrics.rangePct, _an);
        }
        Serial.printf("[2H-DBG] cond: breakUp=%d breakDown=%d compress=%d touch=%d\n",
                     condBreakUp ? 1 : 0, condBreakDown ? 1 : 0, condCompress ? 1 : 0, condTouch ? 1 : 0);
        lastDebugLogMs = now;
    }
    #endif // DEBUG_2H_ALERTS
    
    // === A) 2h Breakout Up ===
    {
        bool condUp = lastPrice > breakThresholdUp;
        bool cooldownOk = (now - gAlert2H.lastBreakoutUpMs) >= alert2HThresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutUpArmed() && condUp && cooldownOk) {
            send2HBreakoutNotification(true, lastPrice, breakThresholdUp, metrics, now);
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
        bool condDown = lastPrice < breakThresholdDown;
        bool cooldownOk = (now - gAlert2H.lastBreakoutDownMs) >= alert2HThresholds.breakCooldownMs;
        
        if (gAlert2H.getBreakoutDownArmed() && condDown && cooldownOk) {
            send2HBreakoutNotification(false, lastPrice, breakThresholdDown, metrics, now);
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
            if (shouldThrottle2HAlert(ALERT2H_COMPRESS, now)) {
                logSecondary2hThrottlePrecheckRateLimited(ALERT2H_COMPRESS, now);
            } else {
            #if DEBUG_2H_ALERTS
            {
                char _ca[32], _ch[32], _cl[32];
                formatQuotePriceEur(_ca, sizeof(_ca), metrics.avg2h);
                formatQuotePriceEur(_ch, sizeof(_ch), metrics.high2h);
                formatQuotePriceEur(_cl, sizeof(_cl), metrics.low2h);
                Serial.printf("[ALERT2H] range_compress sent: range=%.2f%% < %.2f%% (avg=%s high=%s low=%s)\n",
                             metrics.rangePct, alert2HThresholds.compressThresholdPct,
                             _ca, _ch, _cl);
            }
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            // 🟨 ↕️ 👀 BTC-EUR 2h Compressie
            snprintf(title, sizeof(title), "\xF0\x9F\x9F\xA8 \xE2\x86\x95\xEF\xB8\x8F \xF0\x9F\x91\x80 %s 2h %s",
                     bitvavoSymbol, getText("Compressie", "Compression"));
            char priceStr[32], highStr[32], avgStr[32], lowStr[32];
            formatQuotePriceEur(priceStr, sizeof(priceStr), lastPrice);
            formatQuotePriceEur(highStr, sizeof(highStr), metrics.high2h);
            formatQuotePriceEur(avgStr, sizeof(avgStr), metrics.avg2h);
            formatQuotePriceEur(lowStr, sizeof(lowStr), metrics.low2h);
            snprintf(msg, sizeof(msg), "%s (%s)\n%s: %.2f%% (<%.2f%%)\n2h %s: %s\n2h %s: %s\n2h %s: %s",
                     priceStr, timestamp,
                     getText("Band", "Range"), metrics.rangePct, alert2HThresholds.compressThresholdPct,
                     getText("Top", "High"), highStr,
                     getText("Gem", "Avg"), avgStr,
                     getText("Dal", "Low"), lowStr);
            // FASE X.2: dispatch2HNotification geeft expliciet pending vs blocked (bool-wrapper blijft voor legacy)
            {
                const Alert2HDispatchResult dr = dispatch2HNotification(
                    ALERT2H_COMPRESS, title, msg, "\xF0\x9F\x9F\xA8", metrics.rangePct,
                    alert2HThresholds.compressThresholdPct, "range_pct");
                if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                    dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
            gAlert2H.lastCompressMs = now;
            gAlert2H.setCompressArmed(false);
                }
            }
            }
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
            if (shouldThrottle2HAlert(ALERT2H_MEAN_TOUCH, now)) {
                logSecondary2hThrottlePrecheckRateLimited(ALERT2H_MEAN_TOUCH, now);
            } else {
            const char* direction = (gAlert2H.getMeanFarSide() > 0) ? "from above" : "from below";
            #if DEBUG_2H_ALERTS
            {
                char _mp[32], _ma[32];
                formatQuotePriceEur(_mp, sizeof(_mp), lastPrice);
                formatQuotePriceEur(_ma, sizeof(_ma), metrics.avg2h);
                Serial.printf("[ALERT2H] mean_touch sent: price=%s touched avg2h=%s after %.2f%% away (%s)\n",
                             _mp, _ma, distPct, direction);
            }
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            // from above: 🟦 ⤵️ 👀 ... ; from below: 🟦 ⤴️ 👀 ...
            if (gAlert2H.getMeanFarSide() > 0) {
                snprintf(title, sizeof(title), "\xF0\x9F\x9F\xA6 \xE2\xA4\xB5\xEF\xB8\x8F \xF0\x9F\x91\x80 %s 2h %s",
                         bitvavoSymbol, getText("Raakt Gemiddelde", "Mean Touch"));
            } else {
                snprintf(title, sizeof(title), "\xF0\x9F\x9F\xA6 \xE2\xA4\xB4\xEF\xB8\x8F \xF0\x9F\x91\x80 %s 2h %s",
                         bitvavoSymbol, getText("Raakt Gemiddelde", "Mean Touch"));
            }
            const char* directionText = (gAlert2H.getMeanFarSide() > 0) ? 
                                        getText("van boven", "from above") : 
                                        getText("van onderen", "from below");
            char priceStrMt[32];
            formatQuotePriceEur(priceStrMt, sizeof(priceStrMt), lastPrice);
            snprintf(msg, sizeof(msg), "%s (%s)\n%s 2h %s %s\n%s %.2f%% %s",
                     priceStrMt, timestamp,
                     getText("Raakt", "Touched"), getText("gem.", "avg"), directionText,
                     getText("na", "after"), distPct, getText("verwijdering", "away"));
            // FASE X.2: expliciet dispatch-result (pending = event geconsumeerd voor family-state)
            {
                const Alert2HDispatchResult dr = dispatch2HNotification(
                    ALERT2H_MEAN_TOUCH, title, msg, "\xF0\x9F\x9F\xA6", distPct,
                    alert2HThresholds.meanTouchBandPct, "dist_pct");
                if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                    dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
            gAlert2H.lastMeanMs = now;
            gAlert2H.setMeanArmed(false);
            gAlert2H.setMeanWasFar(false);
            gAlert2H.setMeanFarSide(0);
                }
            }
            }
        }
        
        // Reset arm zodra prijs weer ver genoeg weg is
        if (!gAlert2H.getMeanArmed() && distPct > (alert2HThresholds.meanTouchBandPct * 2.0f)) {
            gAlert2H.setMeanArmed(true);
        }
    }
    
    // === E) Anchor context ===
    // Alleen checken als anchor actief is (activeAnchorPrice > 0)
    if (activeAnchorPrice > 0.0f) {
        float anchorMargin = alert2HThresholds.anchorOutsideMarginPct;
        float anchorHighThreshold = metrics.high2h * (1.0f + anchorMargin / 100.0f);
        float anchorLowThreshold = metrics.low2h * (1.0f - anchorMargin / 100.0f);
        bool condAnchorHigh = activeAnchorPrice > anchorHighThreshold;
        bool condAnchorLow = activeAnchorPrice < anchorLowThreshold;
        bool cooldownOk = (now - gAlert2H.lastAnchorCtxMs) >= alert2HThresholds.anchorCooldownMs;
        
        if (gAlert2H.getAnchorCtxArmed() && cooldownOk && (condAnchorHigh || condAnchorLow)) {
            if (shouldThrottle2HAlert(ALERT2H_ANCHOR_CTX, now)) {
                logSecondary2hThrottlePrecheckRateLimited(ALERT2H_ANCHOR_CTX, now);
            } else {
            #if DEBUG_2H_ALERTS
            {
                char _aa[32], _al[32], _ah[32], _aav[32];
                formatQuotePriceEur(_aa, sizeof(_aa), activeAnchorPrice);
                formatQuotePriceEur(_al, sizeof(_al), metrics.low2h);
                formatQuotePriceEur(_ah, sizeof(_ah), metrics.high2h);
                formatQuotePriceEur(_aav, sizeof(_aav), metrics.avg2h);
                Serial.printf("[ALERT2H] anchor_context sent: anchor=%s outside 2h [%s..%s] (avg=%s)\n",
                             _aa, _al, _ah, _aav);
            }
            #endif
            getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
            // anchor above band: 🟫 ⏫️ 👀 ... ; anchor below band: 🟫 ⏬️ 👀 ...
            if (condAnchorHigh) {
                snprintf(title, sizeof(title), "\xF0\x9F\x9F\xAB \xE2\x8F\xAB\xEF\xB8\x8F \xF0\x9F\x91\x80 %s %s 2h",
                         bitvavoSymbol, getText("Anker buiten", "Anchor outside"));
            } else {
                snprintf(title, sizeof(title), "\xF0\x9F\x9F\xAB \xE2\x8F\xAC\xEF\xB8\x8F \xF0\x9F\x91\x80 %s %s 2h",
                         bitvavoSymbol, getText("Anker buiten", "Anchor outside"));
            }
            char acP[32], acA[32], acH[32], acG[32], acL[32];
            formatQuotePriceEur(acP, sizeof(acP), lastPrice);
            formatQuotePriceEur(acA, sizeof(acA), activeAnchorPrice);
            formatQuotePriceEur(acH, sizeof(acH), metrics.high2h);
            formatQuotePriceEur(acG, sizeof(acG), metrics.avg2h);
            formatQuotePriceEur(acL, sizeof(acL), metrics.low2h);
            snprintf(msg, sizeof(msg), "%s (%s)\n%s %s %s 2h\n2h %s: %s\n2h %s: %s\n2h %s: %s",
                     acP, timestamp,
                     getText("Anker", "Anchor"), acA, getText("outside", "outside"),
                     getText("Top", "High"), acH,
                     getText("Gem", "Avg"), acG,
                     getText("Dal", "Low"), acL);
            // FASE X.2: expliciet dispatch-result
            {
                const Alert2HDispatchResult dr = dispatch2HNotification(
                    ALERT2H_ANCHOR_CTX, title, msg, "\xE2\x9A\x93" /* ⚓ */, lastPrice,
                    (float)alert2HThresholds.anchorOutsideMarginPct, "anchor_ctx");
                if (dr == Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED ||
                    dr == Alert2HDispatchResult::DISPATCH_SENT_NOW) {
            gAlert2H.lastAnchorCtxMs = now;
            gAlert2H.setAnchorCtxArmed(false);
                }
            }
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
    // Static functie: gebruik lokale buffers (title 80 voor emoji-prefix)
    char title[80];
    char msg[280];  // Ruimte voor dynamisch EUR-format (meerdere prijzen per regel)
    char timestamp[32];
    
    // Gebruik timestamp voor notificatie formaat
    getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
    
    if (isUp) {
        #if DEBUG_2H_ALERTS
        {
            char _bp[32], _bh[32], _ba[32];
            formatQuotePriceEur(_bp, sizeof(_bp), lastPrice);
            formatQuotePriceEur(_bh, sizeof(_bh), metrics.high2h);
            formatQuotePriceEur(_ba, sizeof(_ba), metrics.avg2h);
            Serial.printf("[ALERT2H] breakout_up sent: price=%s > high2h=%s (avg=%s, range=%.2f%%)\n",
                         _bp, _bh, _ba, metrics.rangePct);
        }
        #endif
        // 🟪 ⏫️ ⚠️ BTC-EUR 2h breakout
        snprintf(title, sizeof(title), "\xF0\x9F\x9F\xAA \xE2\x8F\xAB\xEF\xB8\x8F \xE2\x9A\xA0\xEF\xB8\x8F %s 2h %s",
                 bitvavoSymbol, getText("breakout", "breakout"));
        char buP[32], buH[32], buA[32];
        formatQuotePriceEur(buP, sizeof(buP), lastPrice);
        formatQuotePriceEur(buH, sizeof(buH), metrics.high2h);
        formatQuotePriceEur(buA, sizeof(buA), metrics.avg2h);
        snprintf(msg, sizeof(msg), "%s (%s)\n%s > 2h %s %s\n%s: %s %s: %.2f%%",
                 buP, timestamp,
                 getText("Prijs", "Price"), getText("Top", "High"), buH,
                 getText("Gem", "Avg"), buA, getText("Band", "Range"), metrics.rangePct);
        // FASE X.2: Gebruik throttling wrapper (Breakout mag altijd door). Tag congruent met title: 🟪
        send2HNotification(ALERT2H_BREAKOUT_UP, title, msg, "\xF0\x9F\x9F\xAA", lastPrice, threshold, "break_lvl");  // 🟪
    } else {
        #if DEBUG_2H_ALERTS
        {
            char _dp[32], _dl[32], _da[32];
            formatQuotePriceEur(_dp, sizeof(_dp), lastPrice);
            formatQuotePriceEur(_dl, sizeof(_dl), metrics.low2h);
            formatQuotePriceEur(_da, sizeof(_da), metrics.avg2h);
            Serial.printf("[ALERT2H] breakdown_down sent: price=%s < low2h=%s (avg=%s, range=%.2f%%)\n",
                         _dp, _dl, _da, metrics.rangePct);
        }
        #endif
        // 🟥 ⏬️ ⚠️ BTC-EUR 2h breakdown
        snprintf(title, sizeof(title), "\xF0\x9F\x9F\xA5 \xE2\x8F\xAC\xEF\xB8\x8F \xE2\x9A\xA0\xEF\xB8\x8F %s 2h %s",
                 bitvavoSymbol, getText("breakdown", "breakdown"));
        char bdP[32], bdL[32], bdA[32];
        formatQuotePriceEur(bdP, sizeof(bdP), lastPrice);
        formatQuotePriceEur(bdL, sizeof(bdL), metrics.low2h);
        formatQuotePriceEur(bdA, sizeof(bdA), metrics.avg2h);
        snprintf(msg, sizeof(msg), "%s (%s)\n%s < 2h %s: %s\n%s: %s %s: %.2f%%",
                 bdP, timestamp,
                 getText("Prijs", "Price"), getText("Dal", "Low"), bdL,
                 getText("Gem", "Avg"), bdA, getText("Band", "Range"), metrics.rangePct);
        // FASE X.2: Gebruik throttling wrapper (Breakdown mag altijd door). Tag congruent met title: 🟥
        send2HNotification(ALERT2H_BREAKOUT_DOWN, title, msg, "\xF0\x9F\x9F\xA5", lastPrice, threshold, "break_lvl");  // 🟥
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

// Log-coalescing (alleen logging): herhaalde 2h mean-touch secondary throttling (detail=secondary_incoming).
static bool mt2hMeanTouchThrottleBurst = false;
static uint32_t mt2hMeanTouchThrottleBurstStartMs = 0;
static uint32_t mt2hMeanTouchThrottleBurstExtra = 0;

static void mt2hMeanTouchThrottleBurstEnd(uint32_t nowMs, const char* detailReason) {
    if (!mt2hMeanTouchThrottleBurst) {
        return;
    }
    const uint32_t extra = mt2hMeanTouchThrottleBurstExtra;
    uint32_t durMs = 0;
    if (mt2hMeanTouchThrottleBurstStartMs > 0 && nowMs >= mt2hMeanTouchThrottleBurstStartMs) {
        durMs = nowMs - mt2hMeanTouchThrottleBurstStartMs;
    }
    mt2hMeanTouchThrottleBurst = false;
    mt2hMeanTouchThrottleBurstStartMs = 0;
    mt2hMeanTouchThrottleBurstExtra = 0;
    if (extra == 0) {
        return;
    }
#if DEBUG_ALERT_TRACE
    Serial.printf(
        "[ALERT_TRACE] type=2h_mean_touch phase=coalesce_summary suppressed_extra=%lu burst_ms=%lu detail=%s\n",
        (unsigned long)extra, (unsigned long)durMs, detailReason ? detailReason : "end");
#elif !DEBUG_BUTTON_ONLY
    Serial_printf(F("[2h throttled] Mean Touch coalesce_summary: suppressed_extra=%lu burst_ms=%lu (%s)\n"),
                  (unsigned long)extra, (unsigned long)durMs, detailReason ? detailReason : "end");
#endif
}

#if DEBUG_ALERT_TRACE
static uint32_t pendingSecondaryTraceDetMs = 0;
static float pendingSecondaryTraceTrigPrice = 0.0f;
static char pendingSecondaryTraceTrigSrc[12] = {0};
static uint32_t pendingSecondaryTraceAlertId = 0;

static void alertTracePendingTraceSave(uint32_t detMs, uint32_t alertTraceId)
{
    float tgp = 0.0f;
    const char* tgr = "UNKNOWN";
    uint32_t tga = 0;
    alertAuditPriceSnapshot(&tgp, &tgr, &tga);
    (void)tga;
    pendingSecondaryTraceDetMs = detMs;
    pendingSecondaryTraceTrigPrice = tgp;
    safeStrncpy(pendingSecondaryTraceTrigSrc, alertTraceNormAuditSrc(tgr), sizeof(pendingSecondaryTraceTrigSrc));
    pendingSecondaryTraceTrigSrc[sizeof(pendingSecondaryTraceTrigSrc) - 1] = '\0';
    pendingSecondaryTraceAlertId = alertTraceId;
}

static void alertTracePendingTraceClear()
{
    pendingSecondaryTraceDetMs = 0;
    pendingSecondaryTraceTrigPrice = 0.0f;
    pendingSecondaryTraceTrigSrc[0] = '\0';
    pendingSecondaryTraceAlertId = 0;
}
#endif

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
    // PRIMARY alerts mogen altijd door (override throttling)
    if (isPrimary2HAlert(alertType)) {
        return false;  // Geen throttling
    }
    
    // FASE X.5: Check global cooldown voor SECONDARY alerts (hard cap)
    if (lastSecondarySentMillis > 0) {
        uint32_t timeSinceLastSecondary = (now - lastSecondarySentMillis) / 1000UL;  // in seconden
        uint32_t globalCooldownSec = alert2HThresholds.twoHSecondaryGlobalCooldownSec;
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
            if (alertType == ALERT2H_TREND_CHANGE && timeSinceLastAlert < alert2HThresholds.throttlingTrendChangeMs) {
                return true;  // Suppress
            }
            // Trend Change → Mean Touch: suppress volgens instelling
            if (alertType == ALERT2H_MEAN_TOUCH && timeSinceLastAlert < alert2HThresholds.throttlingTrendToMeanMs) {
                return true;  // Suppress
            }
            break;
            
        case ALERT2H_MEAN_TOUCH:
            // Mean Touch → Mean Touch: suppress volgens instelling
            if (alertType == ALERT2H_MEAN_TOUCH && timeSinceLastAlert < alert2HThresholds.throttlingMeanTouchMs) {
                return true;  // Suppress
            }
            break;
            
        case ALERT2H_COMPRESS:
            // Compress → Compress: suppress volgens instelling
            if (alertType == ALERT2H_COMPRESS && timeSinceLastAlert < alert2HThresholds.throttlingCompressMs) {
                return true;  // Suppress
            }
            break;
            
        default:
            // Andere combinaties: geen suppressie
            break;
    }
    
    return false;  // Geen throttling
}

static const char* alert2HRuleTag(Alert2HType t) {
    switch (t) {
        case ALERT2H_BREAKOUT_UP:
            return "2h_break_up";
        case ALERT2H_BREAKOUT_DOWN:
            return "2h_break_down";
        case ALERT2H_COMPRESS:
            return "2h_compress";
        case ALERT2H_MEAN_TOUCH:
            return "2h_mean_touch";
        case ALERT2H_ANCHOR_CTX:
            return "2h_anchor_ctx";
        case ALERT2H_TREND_CHANGE:
            return "2h_trend";
        default:
            return "2h_unknown";
    }
}

static const char* alert2HDefaultMetric(Alert2HType t) {
    switch (t) {
        case ALERT2H_BREAKOUT_UP:
        case ALERT2H_BREAKOUT_DOWN:
            return "break_lvl";
        case ALERT2H_COMPRESS:
            return "range_pct";
        case ALERT2H_MEAN_TOUCH:
            return "dist_pct";
        case ALERT2H_ANCHOR_CTX:
            return "anchor_ctx";
        case ALERT2H_TREND_CHANGE:
            return "trend";
        default:
            return "2h";
    }
}

static void alertAuditEmit2hSend(Alert2HType alertType, const char* title, const char* msg,
                                 float auditPrimary, float auditThreshold, const char* auditMetricTag) {
    const char* metric = (auditMetricTag != nullptr && auditMetricTag[0] != '\0') ? auditMetricTag
                                                                                  : alert2HDefaultMetric(alertType);
    char seq[41];
    ntfyBuildSequenceId(title, msg, seq, sizeof(seq));
    float p = 0.0f;
    const char* src = "UNKNOWN";
    uint32_t age = 0;
    alertAuditPriceSnapshot(&p, &src, &age);
    char c1[40] = {0};
    const char* pc1 = "-";
    if (auditMetricTag != nullptr && auditMetricTag[0] != '\0') {
        snprintf(c1, sizeof(c1), "cur=%.4g", (double)auditPrimary);
        pc1 = c1;
    }
    alertAuditLog(alert2HRuleTag(alertType), (seq[0] != '\0') ? seq : nullptr, p, src, age, metric,
                  auditThreshold, pc1, "-");
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
#if DEBUG_ALERT_TRACE
        Serial.printf(
            "[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_throttled detail=pending_flush_drop ms=%lu "
            "orig_det_ms=%lu orig_trig_pri=%.2f orig_trig_src=%s orig_trace_id=%lu\n",
            (unsigned long)alertTraceAllocId(), alert2HRuleTag(pendingSecondaryType), (unsigned long)now,
            (unsigned long)pendingSecondaryTraceDetMs, (double)pendingSecondaryTraceTrigPrice,
            (pendingSecondaryTraceTrigSrc[0] != '\0') ? pendingSecondaryTraceTrigSrc : "na",
            (unsigned long)pendingSecondaryTraceAlertId);
        alertTracePendingTraceClear();
#endif
        pendingSecondaryType = ALERT2H_NONE;
        pendingSecondaryCreatedMillis = 0;
        return false;
    }
    
    // Verstuur pending alert (titel ongewijzigd; bevat al emoji/context, geen [Context]-prefix)
    alertAuditEmit2hSend(pendingSecondaryType, pendingSecondaryTitle, pendingSecondaryMsg, 0.0f, 0.0f, nullptr);
#if DEBUG_ALERT_TRACE
    const uint32_t trFl = alertTraceAllocId();
    const uint32_t pendAge = (pendingSecondaryCreatedMillis > 0) ? (now - pendingSecondaryCreatedMillis) : 0;
    const uint32_t msF = millis();
    float dP = 0.0f;
    const char* dS = "na";
    char dAge[20];
    alertTraceDispNotifSnap(msF, &dP, &dS, dAge, sizeof(dAge));
    {
        char _fd[32];
        formatQuotePriceEur(_fd, sizeof(_fd), snapshotNotifDisplayPrice());
        Serial.printf(
        "[ALERT_TRACE] id=%lu type=%s phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%.4g max=%.4g "
        "orig_det_ms=%lu orig_trig_pri=%.2f orig_trig_src=%s orig_trace_id=%lu pend_created_ms=%lu pend_age_ms=%lu detail=2h_flush_pending\n",
        (unsigned long)trFl, alert2HRuleTag(pendingSecondaryType), (unsigned long)msF,
        _fd, dS, dAge, 0.0, 0.0,
        (unsigned long)pendingSecondaryTraceDetMs, (double)pendingSecondaryTraceTrigPrice,
        (pendingSecondaryTraceTrigSrc[0] != '\0') ? pendingSecondaryTraceTrigSrc : "na",
        (unsigned long)pendingSecondaryTraceAlertId, (unsigned long)pendingSecondaryCreatedMillis,
        (unsigned long)pendAge);
    }
#endif
    bool result = sendNotification(pendingSecondaryTitle, pendingSecondaryMsg, pendingSecondaryColorTag);
#if DEBUG_ALERT_TRACE
    Serial.printf(
        "[ALERT_TRACE] id=%lu type=%s phase=send_done ms=%lu sent=%d note=enqueue_return detail=2h_flush_pending\n",
        (unsigned long)trFl, alert2HRuleTag(pendingSecondaryType), (unsigned long)millis(), result ? 1 : 0);
#endif
    
    // Update throttling state: bij success normaal, bij failure korte backoff om spam te voorkomen
    if (result) {
        last2HAlertType = pendingSecondaryType;
        last2HAlertTimestamp = now;
        lastSecondaryType = pendingSecondaryType;
        lastSecondarySentMillis = now;
    } else {
        // Zet cooldown ook bij failure om herhaalde send-spam te voorkomen
        last2HAlertType = pendingSecondaryType;
        last2HAlertTimestamp = now;
        lastSecondaryType = pendingSecondaryType;
        lastSecondarySentMillis = now;
    }
    
    // Reset pending state
    pendingSecondaryType = ALERT2H_NONE;
    pendingSecondaryCreatedMillis = 0;
#if DEBUG_ALERT_TRACE
    alertTracePendingTraceClear();
#endif
    
    return result;
}

// FASE X.2: Wrapper voor sendNotification() met 2h throttling
// FASE X.3: PRIMARY alerts override throttling, SECONDARY alerts onderhevig aan throttling
// FASE X.5: Uitgebreid met coalescing voor SECONDARY alerts
Alert2HDispatchResult AlertEngine::dispatch2HNotification(Alert2HType alertType, const char* title, const char* msg,
                                                          const char* colorTag,
                                                          float auditPrimary, float auditThreshold,
                                                          const char* auditMetricTag) {
    uint32_t now = millis();
    
    // FASE X.3: PRIMARY alerts override throttling (altijd door, geen coalescing)
    bool isPrimary = isPrimary2HAlert(alertType);
    // Eén evaluatie voor secondary: zelfde uitkomst als herhaalde shouldThrottle2HAlert (geen state-mutatie in die functie).
    bool throttle2hForSecondary = false;
    if (!isPrimary) {
        throttle2hForSecondary = shouldThrottle2HAlert(alertType, now);
    }
    const bool secMeanTouchThrottle = (alertType == ALERT2H_MEAN_TOUCH && throttle2hForSecondary);
    if (isPrimary || alertType != ALERT2H_MEAN_TOUCH) {
        mt2hMeanTouchThrottleBurstEnd(now, "route_change");
    } else if (!secMeanTouchThrottle) {
        mt2hMeanTouchThrottleBurstEnd(now, "mean_touch_not_throttled");
    }
#if DEBUG_ALERT_TRACE
    const uint32_t tr2h = alertTraceAllocId();
    const uint32_t tr2h_det_ms = now;
    {
        const bool skipMeanTouchCand =
            (alertType == ALERT2H_MEAN_TOUCH && !isPrimary && secMeanTouchThrottle && mt2hMeanTouchThrottleBurst);
        if (!skipMeanTouchCand) {
            float trP = 0.0f;
            const char* trS = "unknown";
            uint32_t trAge = 0;
            alertTraceCandTrigger(&trP, &trS, &trAge);
            Serial.printf(
                "[ALERT_TRACE] id=%lu type=%s phase=candidate ms=%lu primary=%d audit_pri=%.4g audit_th=%.4g "
                "trig_pri=%.2f trig_src=%s trig_age_ms=%lu snap1m_open_s=na snap5m_open_s=na\n",
                (unsigned long)tr2h, alert2HRuleTag(alertType), (unsigned long)tr2h_det_ms, isPrimary ? 1 : 0,
                (double)auditPrimary, (double)auditThreshold, (double)trP, trS, (unsigned long)trAge);
        }
    }
#endif
    
    if (isPrimary) {
        // PRIMARY: direct versturen, flush pending SECONDARY eerst
#if DEBUG_ALERT_TRACE
        Serial.printf("[ALERT_TRACE] id=%lu type=%s phase=2h_route decision=primary_flush_pending\n",
                      (unsigned long)tr2h, alert2HRuleTag(alertType));
#endif
        flushPendingSecondaryAlertInternal(now);
        
        // Check throttling (voor PRIMARY is dit altijd false, maar voor consistentie)
        if (shouldThrottle2HAlert(alertType, now)) {
#if DEBUG_ALERT_TRACE
            Serial.printf("[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_throttled detail=primary_matrix\n",
                          (unsigned long)tr2h, alert2HRuleTag(alertType));
#endif
            return Alert2HDispatchResult::DISPATCH_BLOCKED;
        }
        
        // Verstuur PRIMARY alert (titel ongewijzigd, geen [PRIMARY]-prefix)
        alertAuditEmit2hSend(alertType, title, msg, auditPrimary, auditThreshold, auditMetricTag);
#if DEBUG_ALERT_TRACE
        {
            const uint32_t msP = millis();
            float dP = 0.0f;
            const char* dS = "na";
            char dAge[20];
            alertTraceDispNotifSnap(msP, &dP, &dS, dAge, sizeof(dAge));
            {
                char _pd[32];
                formatQuotePriceEur(_pd, sizeof(_pd), snapshotNotifDisplayPrice());
                Serial.printf(
                "[ALERT_TRACE] id=%lu type=%s phase=send_start ms=%lu disp=%s disp_src=%s disp_age_ms=%s min=%.4g max=%.4g det_ms=%lu detail=2h_primary_direct\n",
                (unsigned long)tr2h, alert2HRuleTag(alertType), (unsigned long)msP,
                _pd, dS, dAge, (double)auditPrimary,
                (double)auditThreshold, (unsigned long)tr2h_det_ms);
            }
        }
#endif
        bool result = sendNotification(title, msg, colorTag);
#if DEBUG_ALERT_TRACE
        Serial.printf(
            "[ALERT_TRACE] id=%lu type=%s phase=send_done ms=%lu sent=%d note=enqueue_return detail=2h_primary_direct\n",
            (unsigned long)tr2h, alert2HRuleTag(alertType), (unsigned long)millis(), result ? 1 : 0);
#endif
        
        // Update throttling state: bij failure ook cooldown zodat we niet spam-retryen
        if (result) {
            last2HAlertType = alertType;
            last2HAlertTimestamp = now;
            // PRIMARY alerts tellen niet mee voor secondary global cooldown
        } else {
            last2HAlertType = alertType;
            last2HAlertTimestamp = now;
        }
        
        return result ? Alert2HDispatchResult::DISPATCH_SENT_NOW : Alert2HDispatchResult::DISPATCH_BLOCKED;
    }
    
    // SECONDARY alerts: coalescing logica
    // Check throttling eerst (late vangnet; zelfde regels als shouldThrottle2HAlert elders)
    if (throttle2hForSecondary) {
        if (alertType == ALERT2H_MEAN_TOUCH) {
            if (mt2hMeanTouchThrottleBurst) {
                mt2hMeanTouchThrottleBurstExtra++;
            } else {
                mt2hMeanTouchThrottleBurst = true;
                mt2hMeanTouchThrottleBurstStartMs = now;
                mt2hMeanTouchThrottleBurstExtra = 0;
#if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[2h throttled] %s: %s\n"), "Mean Touch", title);
#endif
#if DEBUG_ALERT_TRACE
                Serial.printf(
                    "[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_throttled detail=secondary_incoming\n",
                    (unsigned long)tr2h, alert2HRuleTag(alertType));
#endif
            }
        } else {
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
#if DEBUG_ALERT_TRACE
            Serial.printf("[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_throttled detail=secondary_incoming\n",
                          (unsigned long)tr2h, alert2HRuleTag(alertType));
#endif
        }
        return Alert2HDispatchResult::DISPATCH_BLOCKED;
    }
    
    // Coalescing: check of er al een pending SECONDARY alert is
    uint32_t coalesceWindowMs = alert2HThresholds.twoHSecondaryCoalesceWindowSec * 1000UL;
    
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
#if DEBUG_ALERT_TRACE
                Serial.printf(
                    "[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_coalesced detail=replace_pending pend_type=%s pend_pri=%u new_pri=%u "
                    "old_orig_det_ms=%lu old_orig_trig_pri=%.2f old_orig_trig_src=%s old_orig_trace_id=%lu\n",
                    (unsigned long)tr2h, alert2HRuleTag(alertType), alert2HRuleTag(pendingSecondaryType),
                    (unsigned)pendingPriority, (unsigned)newPriority, (unsigned long)pendingSecondaryTraceDetMs,
                    (double)pendingSecondaryTraceTrigPrice,
                    (pendingSecondaryTraceTrigSrc[0] != '\0') ? pendingSecondaryTraceTrigSrc : "na",
                    (unsigned long)pendingSecondaryTraceAlertId);
#endif
                // Update pending met nieuwe (hogere prioriteit) alert
                pendingSecondaryType = alertType;
                safeStrncpy(pendingSecondaryTitle, title, sizeof(pendingSecondaryTitle));
                safeStrncpy(pendingSecondaryMsg, msg, sizeof(pendingSecondaryMsg));
                safeStrncpy(pendingSecondaryColorTag, colorTag ? colorTag : "", sizeof(pendingSecondaryColorTag));
                pendingSecondaryCreatedMillis = now;
#if DEBUG_ALERT_TRACE
                alertTracePendingTraceSave(tr2h_det_ms, tr2h);
#endif
                return Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED;
            }
#if DEBUG_ALERT_TRACE
            else {
                const uint32_t pendAgeK = (pendingSecondaryCreatedMillis > 0 && now >= pendingSecondaryCreatedMillis)
                                              ? (now - pendingSecondaryCreatedMillis)
                                              : 0;
                Serial.printf(
                    "[ALERT_TRACE] id=%lu type=%s phase=suppress reason=2h_coalesced detail=keep_pending pend_type=%s pend_pri=%u new_pri=%u "
                    "orig_det_ms=%lu orig_trig_pri=%.2f orig_trig_src=%s orig_trace_id=%lu pend_age_ms=%lu\n",
                    (unsigned long)tr2h, alert2HRuleTag(alertType), alert2HRuleTag(pendingSecondaryType),
                    (unsigned)pendingPriority, (unsigned)newPriority, (unsigned long)pendingSecondaryTraceDetMs,
                    (double)pendingSecondaryTraceTrigPrice,
                    (pendingSecondaryTraceTrigSrc[0] != '\0') ? pendingSecondaryTraceTrigSrc : "na",
                    (unsigned long)pendingSecondaryTraceAlertId, (unsigned long)pendAgeK);
            }
#endif
            // Anders: behoud bestaande pending (hogere prioriteit)
            return Alert2HDispatchResult::DISPATCH_BLOCKED;
        }
        // Buiten coalesce window: flush pending eerst
        flushPendingSecondaryAlertInternal(now);
    }
    
    // Geen pending of pending geflusht: start nieuw pending
    pendingSecondaryType = alertType;
    safeStrncpy(pendingSecondaryTitle, title, sizeof(pendingSecondaryTitle));
    safeStrncpy(pendingSecondaryMsg, msg, sizeof(pendingSecondaryMsg));
    safeStrncpy(pendingSecondaryColorTag, colorTag ? colorTag : "", sizeof(pendingSecondaryColorTag));
    pendingSecondaryCreatedMillis = now;
#if DEBUG_ALERT_TRACE
    alertTracePendingTraceSave(tr2h_det_ms, tr2h);
    Serial.printf(
        "[ALERT_TRACE] id=%lu type=%s phase=2h_pending_stored win_ms=%lu orig_det_ms=%lu orig_trig_pri=%.2f orig_trig_src=%s orig_trace_id=%lu\n",
        (unsigned long)tr2h, alert2HRuleTag(alertType), (unsigned long)coalesceWindowMs,
        (unsigned long)pendingSecondaryTraceDetMs, (double)pendingSecondaryTraceTrigPrice,
        (pendingSecondaryTraceTrigSrc[0] != '\0') ? pendingSecondaryTraceTrigSrc : "na",
        (unsigned long)pendingSecondaryTraceAlertId);
#endif
    
    return Alert2HDispatchResult::DISPATCH_PENDING_ACCEPTED;
}

bool AlertEngine::send2HNotification(Alert2HType alertType, const char* title, const char* msg, const char* colorTag,
                                     float auditPrimary, float auditThreshold, const char* auditMetricTag) {
    const Alert2HDispatchResult r = dispatch2HNotification(alertType, title, msg, colorTag, auditPrimary,
                                                           auditThreshold, auditMetricTag);
    // Historisch: alleen PRIMARY true bij echte send; SECONDARY altijd false uit bool-wrapper
    if (isPrimary2HAlert(alertType)) {
        return (r == Alert2HDispatchResult::DISPATCH_SENT_NOW);
    }
    return false;
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
    uint8_t mode = alert2HThresholds.anchorSourceMode;
    
    if (mode == 0) {  // MANUAL
        return manualAnchorPrice;
    } else if (mode == 1) {  // AUTO
        float autoAnchor = alert2HThresholds.autoAnchorLastValue;
        if (autoAnchor > 0.0f) {
            return autoAnchor;
        }
        return manualAnchorPrice;
    } else if (mode == 2) {  // AUTO_FALLBACK
        float autoAnchor = alert2HThresholds.autoAnchorLastValue;
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
    extern SettingsStore settingsStore;
    extern bool wsConnected;
    extern bool wsAnchorEma4hValid;
    extern bool wsAnchorEma1dValid;
    extern float wsAnchorEma4hLive;
    extern float wsAnchorEma1dLive;
    // Skip auto anchor tijdens WS handshake (voorkomt connect issues)
    extern bool wsConnecting;
    extern unsigned long wsConnectStartMs;
    if (wsConnecting && (millis() - wsConnectStartMs) < 15000UL) {
        Serial.println("[ANCHOR][AUTO] Skip: WS handshake actief");
        return false;
    }
    
    Serial.printf("[ANCHOR][AUTO] maybeUpdateAutoAnchor called: force=%d mode=%d symbol=%s\n", 
                  force, alert2HThresholds.anchorSourceMode, bitvavoSymbol);
    
    uint8_t mode = alert2HThresholds.anchorSourceMode;
    if (mode == 0 || mode == 3) {  // MANUAL of OFF
        Serial.printf("[ANCHOR][AUTO] Auto anchor disabled (mode=%d, expected 1 or 2)\n", mode);
        return false;
    }
    
    // Backoff bij eerdere fetch failures (voorkomt herhaaldelijke connect errors)
    static unsigned long lastAutoAnchorFailMs = 0;
    if (!force && lastAutoAnchorFailMs > 0 && (millis() - lastAutoAnchorFailMs) < (30UL * 60UL * 1000UL)) {
        Serial.println("[ANCHOR][AUTO] Skip: backoff na fetch failure");
        return false;
    }

    // Check tijd sinds laatste update
    uint32_t nowEpoch = 0;
    if (!force) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            nowEpoch = mktime(&timeinfo);
        }
        
        if (nowEpoch > 0 && alert2HThresholds.autoAnchorLastUpdateEpoch > 0) {
            uint32_t minutesSinceUpdate = (nowEpoch - alert2HThresholds.autoAnchorLastUpdateEpoch) / 60;
            if (minutesSinceUpdate < alert2HThresholds.autoAnchorUpdateMinutes) {
                Serial.printf("[ANCHOR][AUTO] Too soon to update (%lu minutes since last update)\n", minutesSinceUpdate);
                return false;
            }
        }
    }
    
    // Forward declaration voor fetchBitvavoCandles (gedefinieerd in .ino)
    extern int fetchBitvavoCandles(const char* symbol, const char* interval, uint16_t limit, float* prices, unsigned long* timestamps, uint16_t maxCount, float* highs = nullptr, float* lows = nullptr, float* volumes = nullptr);
    
    float ema4hValue = 0.0f;
    float ema1dValue = 0.0f;
    bool useWsEma = (wsAnchorEma4hValid && wsAnchorEma1dValid);

    if (!force && wsConnected && !useWsEma) {
        Serial.println("[ANCHOR][AUTO] Skip: WS connected but EMA not ready");
        return false;
    }
    
    if (useWsEma) {
        ema4hValue = wsAnchorEma4hLive;
        ema1dValue = wsAnchorEma1dLive;
        static unsigned long lastWsEmaLogMs = 0;
        unsigned long nowMs = millis();
        if (nowMs - lastWsEmaLogMs >= 60000UL) {
            lastWsEmaLogMs = nowMs;
            {
                char _e4[32], _e1[32];
                formatQuotePriceEur(_e4, sizeof(_e4), ema4hValue);
                formatQuotePriceEur(_e1, sizeof(_e1), ema1dValue);
                Serial.printf("[ANCHOR][AUTO] WS EMA used: ema4h=%s ema1d=%s\n", _e4, _e1);
            }
        }
    } else {
        // Fetch 4h candles
        uint8_t count4h = alert2HThresholds.autoAnchor4hCandles;
        const char* interval4h = get4hIntervalStr();
        float tempPrices[12];  // Herbruikbare buffer (max 12 candles)
        
        int fetched4h = fetchBitvavoCandles(bitvavoSymbol, interval4h, count4h, tempPrices, nullptr, 12);
        Serial.printf("[ANCHOR][AUTO] 4h fetch result: %d candles\n", fetched4h);
        
        if (fetched4h < 2) {
            Serial.println("[ANCHOR][AUTO] Not enough 4h candles");
            lastAutoAnchorFailMs = millis();
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
        
        ema4hValue = ema4h.get();
        
        // Fetch 1d candles (hergebruik tempPrices buffer)
        uint8_t count1d = alert2HThresholds.autoAnchor1dCandles;
        const char* interval1d = get1dIntervalStr();
        
        int fetched1d = fetchBitvavoCandles(bitvavoSymbol, interval1d, count1d, tempPrices, nullptr, 12);
        Serial.printf("[ANCHOR][AUTO] 1d fetch result: %d candles\n", fetched1d);
        
        if (fetched1d < 2) {
            Serial.println("[ANCHOR][AUTO] Not enough 1d candles");
            lastAutoAnchorFailMs = millis();
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
        
        ema1dValue = ema1d.get();
        // Seed WS EMA values so next updates can use WS path without REST
        wsAnchorEma4hLive = ema4hValue;
        wsAnchorEma1dLive = ema1dValue;
        wsAnchorEma4hValid = true;
        wsAnchorEma1dValid = true;
    }
    
    // Combineer EMA's met adaptieve weging (gebruik helper methods voor compacte types)
    float trendDeltaPct = fabsf(ema4hValue - ema1dValue) / ema1dValue * 100.0f;
    float trendPivotPct = alert2HThresholds.getAutoAnchorTrendPivotPct();
    float trendFactor = fminf(trendDeltaPct / trendPivotPct, 1.0f);
    float w4hBase = alert2HThresholds.getAutoAnchorW4hBase();
    float w4hTrendBoost = alert2HThresholds.getAutoAnchorW4hTrendBoost();
    float w4h = w4hBase + trendFactor * w4hTrendBoost;
    float w1d = 1.0f - w4h;
    float newAutoAnchor = w4h * ema4hValue + w1d * ema1dValue;
    
    // Cap t.o.v. huidige koers: EMA's kunnen nafloeken (bijv. na daling), waardoor het anker
    // ver boven de koers komt en onbruikbaar is. Max 2% boven en max 3% onder huidige prijs.
    static const float AUTO_ANCHOR_MAX_ABOVE_CURRENT_PCT = 2.0f;
    static const float AUTO_ANCHOR_MAX_BELOW_CURRENT_PCT = 3.0f;
    float currentPrice = prices[0];
    if (isValidPrice(currentPrice) && currentPrice > 0.0f) {
        float maxAnchor = currentPrice * (1.0f + AUTO_ANCHOR_MAX_ABOVE_CURRENT_PCT / 100.0f);
        float minAnchor = currentPrice * (1.0f - AUTO_ANCHOR_MAX_BELOW_CURRENT_PCT / 100.0f);
        if (newAutoAnchor > maxAnchor) {
            newAutoAnchor = maxAnchor;
            {
                char _na[32], _cp[32];
                formatQuotePriceEur(_na, sizeof(_na), newAutoAnchor);
                formatQuotePriceEur(_cp, sizeof(_cp), currentPrice);
                Serial.printf("[ANCHOR][AUTO] Gecapped naar max %s (2%% boven koers %s)\n", _na, _cp);
            }
        } else if (newAutoAnchor < minAnchor) {
            newAutoAnchor = minAnchor;
            {
                char _na[32], _cp[32];
                formatQuotePriceEur(_na, sizeof(_na), newAutoAnchor);
                formatQuotePriceEur(_cp, sizeof(_cp), currentPrice);
                Serial.printf("[ANCHOR][AUTO] Gecapped naar min %s (3%% onder koers %s)\n", _na, _cp);
            }
        }
    }
    
    // Hysterese check
    float lastAutoAnchor = alert2HThresholds.autoAnchorLastValue;
    bool shouldCommit = force;
    
    if (!shouldCommit && lastAutoAnchor > 0.0f) {
        float minUpdatePct = alert2HThresholds.getAutoAnchorMinUpdatePct();
        float changePct = fabsf(newAutoAnchor - lastAutoAnchor) / lastAutoAnchor * 100.0f;
        if (changePct >= minUpdatePct) {
            shouldCommit = true;
        }
    } else if (lastAutoAnchor <= 0.0f) {
        shouldCommit = true;
    }
    
    // Check force update interval
    if (!shouldCommit && nowEpoch > 0 && alert2HThresholds.autoAnchorLastUpdateEpoch > 0) {
        uint32_t minutesSinceUpdate = (nowEpoch - alert2HThresholds.autoAnchorLastUpdateEpoch) / 60;
        if (minutesSinceUpdate >= alert2HThresholds.autoAnchorForceUpdateMinutes) {
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
                {
                    char _as[32];
                    formatQuotePriceEur(_as, sizeof(_as), newAutoAnchor);
                    Serial.printf("[ANCHOR][AUTO] Anchor ingesteld: %s (take profit/max loss worden getoond)\n", _as);
                }
                
                // Stuur optionele notificatie als enabled
                if (alert2HThresholds.getAutoAnchorNotifyEnabled()) {
                    char timestamp[32];
                    char title[40];
                    char msg[120];  // Verhoogd van 80 naar 120 bytes voor langere notificaties
                    getFormattedTimestampForNotification(timestamp, sizeof(timestamp));
                    snprintf(title, sizeof(title), "%s %s", bitvavoSymbol, getText("Auto Anker", "Auto Anchor"));
                    char anchorStr[32];
                    formatQuotePriceEur(anchorStr, sizeof(anchorStr), newAutoAnchor);
                    snprintf(msg, sizeof(msg), "%s (%s)\n%s: %s", 
                             anchorStr, timestamp,
                             getText("Bijgewerkt", "Updated"), anchorStr);
                    bool sent = sendNotification(title, msg, "\xF0\x9F\x9F\xAB");  // 🟫
                    if (!sent) {
                        Serial.println("[ANCHOR][AUTO] WARN: Auto anchor notificatie niet verstuurd");
                    }
                }
            } else {
                Serial.println("[ANCHOR][AUTO] WARN: Kon anchor niet instellen");
            }
        }
        
        {
            char _e4[32], _e1[32], _nw[32], _ls[32];
            formatQuotePriceEur(_e4, sizeof(_e4), ema4hValue);
            formatQuotePriceEur(_e1, sizeof(_e1), ema1dValue);
            formatQuotePriceEur(_nw, sizeof(_nw), newAutoAnchor);
            formatQuotePriceEur(_ls, sizeof(_ls), lastAutoAnchor);
            Serial.printf("[ANCHOR][AUTO] ema4h=%s ema1d=%s trend=%.2f%% w4h=%.2f new=%s last=%s decision=COMMIT\n",
                         _e4, _e1, trendDeltaPct, w4h, _nw, _ls);
        }
        return true;
    } else {
        {
            char _e4[32], _e1[32], _nw[32], _ls[32];
            formatQuotePriceEur(_e4, sizeof(_e4), ema4hValue);
            formatQuotePriceEur(_e1, sizeof(_e1), ema1dValue);
            formatQuotePriceEur(_nw, sizeof(_nw), newAutoAnchor);
            formatQuotePriceEur(_ls, sizeof(_ls), lastAutoAnchor);
            Serial.printf("[ANCHOR][AUTO] ema4h=%s ema1d=%s trend=%.2f%% w4h=%.2f new=%s last=%s decision=SKIP\n",
                         _e4, _e1, trendDeltaPct, w4h, _nw, _ls);
        }
        return false;
    }
}
