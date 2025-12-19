#ifndef TRENDDETECTOR_H
#define TRENDDETECTOR_H

#include <Arduino.h>

// TrendState enum - gebruikt voor trend detection
enum TrendState {
    TREND_UP,
    TREND_DOWN,
    TREND_SIDEWAYS
};

// TrendDetector class - beheert trend detection en state management
class TrendDetector {
public:
    TrendDetector();
    void begin();
    
    // Trend state management
    TrendState getTrendState() const { return trendState; }
    TrendState getPreviousTrendState() const { return previousTrendState; }
    void setTrendState(TrendState state) { trendState = state; }
    
    // Trend detection
    // Bepaal trend state op basis van 2h en 30m returns
    // ret_2h_value: 2-hour return percentage
    // ret_30m_value: 30-minute return percentage
    // trendThreshold: threshold voor trend bepaling (default uit settings)
    TrendState determineTrendState(float ret_2h_value, float ret_30m_value, float trendThreshold);
    
    // Trend change detection en notificatie
    // Check of trend is veranderd en stuur notificatie indien nodig
    // Note: Deze functie synchroniseert eerst TrendDetector state met globale variabelen,
    //       checkt op changes, en update dan zowel TrendDetector als globale state
    // ret_30m_value: 30-minute return voor notificatie
    // ret_2h: 2-hour return voor notificatie (extern, wordt gebruikt in notificatie)
    // minuteArrayFilled: of minute averages array gevuld is
    // minuteIndex: huidige index in minute averages array
    void checkTrendChange(float ret_30m_value, float ret_2h, bool minuteArrayFilled, uint8_t minuteIndex);
    
    // Sync state: Update TrendDetector state met globale variabelen (voor parallel implementatie)
    void syncStateFromGlobals();
    
private:
    TrendState trendState;
    TrendState previousTrendState;
    unsigned long lastTrendChangeNotification;
    
    // Forward declarations voor dependencies (worden later via parameters of andere modules)
    // volatilityState - hoort bij VolatilityTracker module (later)
    // sendNotification() - hoort bij AlertEngine module (later)
    // Voor nu gebruiken we extern declarations
};

#endif // TRENDDETECTOR_H


