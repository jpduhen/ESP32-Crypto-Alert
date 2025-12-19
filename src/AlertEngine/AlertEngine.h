#ifndef ALERTENGINE_H
#define ALERTENGINE_H

#include <Arduino.h>

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
    
    // Note: Globale arrays en settings worden via extern declarations in .cpp file gebruikt
    // (parallel implementatie - arrays blijven globaal in .ino)
};

#endif // ALERTENGINE_H

