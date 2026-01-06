#ifndef ALERTENGINE_H
#define ALERTENGINE_H

#include <Arduino.h>

// TrendDetector module (voor getTrendName helper)
#include "../TrendDetector/TrendDetector.h"

// EventDirection enum - gebruikt voor Smart Confluence Mode
enum EventDirection {
    EVENT_UP,
    EVENT_DOWN,
    EVENT_NONE
};

// LastOneMinuteEvent struct - voor Smart Confluence Mode
struct LastOneMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_1m|
    bool usedInConfluence;  // Flag om te voorkomen dat dit event dubbel wordt gebruikt
};

// LastFiveMinuteEvent struct - voor Smart Confluence Mode
struct LastFiveMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_5m|
    bool usedInConfluence;  // Flag om te voorkomen dat dit event dubbel wordt gebruikt
};

// TwoHMetrics struct - uniforme 2-uur metrics voor alert detection
struct TwoHMetrics {
    float avg2h = 0.0f;
    float high2h = 0.0f;
    float low2h = 0.0f;
    float rangePct = 0.0f;
    bool valid = false;
};

// Streaming EMA accumulator (heap-safe)
struct EmaAccumulator {
    float ema;
    uint16_t count;
    float alpha;
    bool valid;
    
    void begin(uint16_t n) {
        count = 0;
        ema = 0.0f;
        alpha = 2.0f / (n + 1.0f);
        valid = false;
    }
    
    void push(float value) {
        if (count == 0) {
            ema = value;
        } else {
            ema = alpha * value + (1.0f - alpha) * ema;
        }
        count++;
        if (count >= 2) {
            valid = true;
        }
    }
    
    float get() const { return ema; }
    bool isValid() const { return valid; }
};

// FASE X.2: 2h alert types voor throttling matrix
enum Alert2HType {
    ALERT2H_NONE = 0,
    ALERT2H_TREND_CHANGE,
    ALERT2H_MEAN_TOUCH,
    ALERT2H_COMPRESS,
    ALERT2H_BREAKOUT_UP,
    ALERT2H_BREAKOUT_DOWN,
    ALERT2H_ANCHOR_CTX
};

// Alert2HState struct - persistent runtime state voor 2h notificaties
// Geoptimaliseerd met bitfields en compacte layout om geheugen te besparen (24 bytes i.p.v. 32 bytes)
struct Alert2HState {
    // Timestamps eerst (20 bytes totaal)
    uint32_t lastBreakoutUpMs = 0;
    uint32_t lastBreakoutDownMs = 0;
    uint32_t lastCompressMs = 0;
    uint32_t lastMeanMs = 0;
    uint32_t lastAnchorCtxMs = 0;
    
    // Bitfields aan het einde om padding te minimaliseren (1 byte totaal)
    uint8_t flags;  // Bitfield container: breakoutUpArmed(0), breakoutDownArmed(1), compressArmed(2), 
                     // meanArmed(3), anchorCtxArmed(4), meanWasFar(5), meanFarSide(6-7)
    
    // Helper methods voor bitfield access
    bool getBreakoutUpArmed() const { return (flags & 0x01) != 0; }
    void setBreakoutUpArmed(bool v) { flags = v ? (flags | 0x01) : (flags & ~0x01); }
    bool getBreakoutDownArmed() const { return (flags & 0x02) != 0; }
    void setBreakoutDownArmed(bool v) { flags = v ? (flags | 0x02) : (flags & ~0x02); }
    bool getCompressArmed() const { return (flags & 0x04) != 0; }
    void setCompressArmed(bool v) { flags = v ? (flags | 0x04) : (flags & ~0x04); }
    bool getMeanArmed() const { return (flags & 0x08) != 0; }
    void setMeanArmed(bool v) { flags = v ? (flags | 0x08) : (flags & ~0x08); }
    bool getAnchorCtxArmed() const { return (flags & 0x10) != 0; }
    void setAnchorCtxArmed(bool v) { flags = v ? (flags | 0x10) : (flags & ~0x10); }
    bool getMeanWasFar() const { return (flags & 0x20) != 0; }
    void setMeanWasFar(bool v) { flags = v ? (flags | 0x20) : (flags & ~0x20); }
    int8_t getMeanFarSide() const { return ((flags >> 6) & 0x03) - 1; } // -1, 0, +1
    void setMeanFarSide(int8_t v) { flags = (flags & 0x3F) | (((v + 1) & 0x03) << 6); }
    
    Alert2HState() : flags(0x1F) {} // Alle armed flags op true (0x1F = 0b00011111)
};

// AlertEngine class - beheert alert detection en notificaties
class AlertEngine {
public:
    AlertEngine();
    void begin();
    
    // Main alert checking function
    // Fase 6.1.10: Verplaatst naar AlertEngine (parallel implementatie)
    void checkAndNotify(float ret_1m, float ret_5m, float ret_30m);
    
    // Helper: Check if cooldown has passed and hourly limit is OK
    // Fase 6.1.2: Verplaatst naar AlertEngine (parallel implementatie)
    static bool checkAlertConditions(unsigned long now, unsigned long& lastNotification, unsigned long cooldownMs, 
                                     uint8_t& alertsThisHour, uint8_t maxAlertsPerHour, const char* alertType);
    
    // Helper: Determine color tag based on return value and threshold
    // Fase 6.1.3: Verplaatst naar AlertEngine (parallel implementatie)
    static const char* determineColorTag(float ret, float threshold, float strongThreshold);
    
    // Helper: Format notification message with timestamp, price, and min/max
    // Fase 6.1.4: Verplaatst naar AlertEngine (parallel implementatie)
    static void formatNotificationMessage(char* msg, size_t msgSize, float ret, const char* direction, 
                                          float minVal, float maxVal);
    
    // Helper: Send alert notification with all checks
    // Fase 6.1.5: Verplaatst naar AlertEngine (parallel implementatie)
    static bool sendAlertNotification(float ret, float threshold, float strongThreshold, 
                                      unsigned long now, unsigned long& lastNotification, 
                                      unsigned long cooldownMs, uint8_t& alertsThisHour, 
                                      uint8_t maxAlertsPerHour, const char* alertType, 
                                      const char* direction, float minVal, float maxVal);
    
    // Update 1m event state voor Smart Confluence Mode
    // Fase 6.1.8: Verplaatst naar AlertEngine (parallel implementatie)
    void update1mEvent(float ret_1m, unsigned long timestamp, float threshold);
    
    // Update 5m event state voor Smart Confluence Mode
    // Fase 6.1.8: Verplaatst naar AlertEngine (parallel implementatie)
    void update5mEvent(float ret_5m, unsigned long timestamp, float threshold);
    
    // Check for confluence and send combined alert if found
    // Fase 6.1.9: Verplaatst naar AlertEngine (parallel implementatie)
    bool checkAndSendConfluenceAlert(unsigned long now, float ret_30m);
    
    // Check 2-hour notifications (breakout, breakdown, compression, mean reversion, anchor context)
    // Wordt aangeroepen na elke price update
    static void check2HNotifications(float lastPrice, float anchorPrice);
    
    // Auto Anchor methods
    static float getActiveAnchorPrice(float manualAnchorPrice);
    static bool maybeUpdateAutoAnchor(bool force);
    static const char* get4hIntervalStr();
    static const char* get1dIntervalStr();
    
    // Helper: Get trend name string (inline voor performance)
    static inline const char* getTrendName(TrendState trend) {
        switch (trend) {
            case TREND_UP: return "UP";
            case TREND_DOWN: return "DOWN";
            case TREND_SIDEWAYS: return "SIDEWAYS";
            default: return "UNKNOWN";
        }
    }
    
    // Helper: Send 2h breakout/breakdown notification (consolideert up/down logica)
    static void send2HBreakoutNotification(bool isUp, float lastPrice, float threshold, 
                                             const TwoHMetrics& metrics, uint32_t now);
    
    // FASE X.5: Flush pending SECONDARY alert (aanroepen periodiek of na check2HNotifications)
    static void flushPendingSecondaryAlert();
    
    // FASE X.2: 2h alert throttling - check of alert gesuppresseerd moet worden
    // Returns true als alert moet worden gesuppresseerd, false als alert door mag
    static bool shouldThrottle2HAlert(Alert2HType alertType, uint32_t now);
    
    // FASE X.3: Check of alert PRIMARY is (override throttling)
    // PRIMARY: Breakout/Breakdown (regime-veranderingen)
    // SECONDARY: Mean Touch, Compress, Trend Change (context-signalen)
    static bool isPrimary2HAlert(Alert2HType alertType);
    
    // FASE X.2: Wrapper voor sendNotification() met 2h throttling
    // FASE X.3: PRIMARY alerts override throttling, SECONDARY alerts onderhevig aan throttling
    // Gebruik deze functie in plaats van direct sendNotification() voor 2h alerts
    static bool send2HNotification(Alert2HType alertType, const char* title, const char* msg, const char* colorTag);
    
    // Sync state: Update AlertEngine state met globale variabelen (voor parallel implementatie)
    void syncStateFromGlobals();
    
private:
    // Alert state variabelen (Fase 6.1.6: verplaatst naar AlertEngine)
    unsigned long lastNotification1Min;
    unsigned long lastNotification30Min;
    unsigned long lastNotification5Min;
    uint8_t alerts1MinThisHour;
    uint8_t alerts30MinThisHour;
    uint8_t alerts5MinThisHour;
    unsigned long hourStartTime;
    
    // Smart Confluence Mode state (Fase 6.1.7: verplaatst naar AlertEngine)
    LastOneMinuteEvent last1mEvent;
    LastFiveMinuteEvent last5mEvent;
    unsigned long lastConfluenceAlert;
    
    // Geheugen optimalisatie: hergebruik buffers i.p.v. lokale stack allocaties
    // Vergroot om volledige notificatieteksten te ondersteunen
    char msgBuffer[256];      // Hergebruik voor alle notification messages
    char titleBuffer[64];     // Hergebruik voor alle notification titles
    char timestampBuffer[32]; // Hergebruik voor timestamp formatting
    
    // CPU optimalisatie: cache berekende waarden
    float cachedAbsRet1m;
    float cachedAbsRet5m;
    float cachedAbsRet30m;
    bool valuesCached;
    
    // Helper: Cache absolute waarden (voorkomt herhaalde fabsf calls)
    void cacheAbsoluteValues(float ret_1m, float ret_5m, float ret_30m);
    
    // Helper: Bereken min/max uit fiveMinutePrices (geoptimaliseerde versie)
    bool findMinMaxInFiveMinutePrices(float& minVal, float& maxVal);
    
    // Helper: Format notification message (gebruikt class buffers)
    void formatNotificationMessageInternal(float ret, const char* direction, 
                                           float minVal, float maxVal, const char* timeframe);
    
    // Note: Globale arrays en settings worden via extern declarations in .cpp file gebruikt
    // (parallel implementatie - arrays blijven globaal in .ino)
};

#endif // ALERTENGINE_H
