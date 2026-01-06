#ifndef TRENDDETECTOR_H
#define TRENDDETECTOR_H

#include <Arduino.h>

// VolatilityTracker module (voor VolatilityState enum)
#include "../VolatilityTracker/VolatilityTracker.h"

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
    
    // Medium trend detection
    // Bepaal medium trend state op basis van 1d return
    // ret_4h_value: 4-hour return percentage (legacy, unused)
    // ret_1d_value: 1-day return percentage
    // mediumThreshold: threshold voor 1d trend (default 2.0%)
    TrendState determineMediumTrendState(float ret_4h_value, float ret_1d_value, float mediumThreshold);

    // Lange termijn trend detection
    // Bepaal lange termijn trend state op basis van 7d return
    // ret_7d_value: 7-day return percentage
    // longTermThreshold: threshold voor lange termijn trend (default 2.0%)
    TrendState determineLongTermTrendState(float ret_7d_value, float longTermThreshold);
    
    // Trend change detection en notificatie
    // Check of trend is veranderd en stuur notificatie indien nodig
    // Note: Deze functie synchroniseert eerst TrendDetector state met globale variabelen,
    //       checkt op changes, en update dan zowel TrendDetector als globale state
    // ret_30m_value: 30-minute return voor notificatie
    // ret_2h: 2-hour return voor notificatie (extern, wordt gebruikt in notificatie)
    // minuteArrayFilled: of minute averages array gevuld is
    // minuteIndex: huidige index in minute averages array
    void checkTrendChange(float ret_30m_value, float ret_2h, bool minuteArrayFilled, uint8_t minuteIndex);
    
    // Medium trend state management
    TrendState getMediumTrendState() const { return mediumTrendState; }
    TrendState getPreviousMediumTrendState() const { return previousMediumTrendState; }
    void setMediumTrendState(TrendState state) { mediumTrendState = state; }

    // Lange termijn trend state management
    TrendState getLongTermTrendState() const { return longTermTrendState; }
    TrendState getPreviousLongTermTrendState() const { return previousLongTermTrendState; }
    void setLongTermTrendState(TrendState state) { longTermTrendState = state; }
    
    // Medium trend change detection en notificatie
    // Check of medium trend is veranderd en stuur notificatie indien nodig
    // ret_4h_value: 4-hour return percentage (legacy, unused)
    // ret_1d_value: 1-day return percentage
    // mediumThreshold: threshold voor 1d trend (default 2.0%)
    void checkMediumTrendChange(float ret_4h_value, float ret_1d_value, float mediumThreshold);

    // Lange termijn trend change detection en notificatie
    // Check of lange termijn trend is veranderd en stuur notificatie indien nodig
    // ret_7d_value: 7-day return percentage
    // longTermThreshold: threshold voor lange termijn trend (default 2.0%)
    void checkLongTermTrendChange(float ret_7d_value, float longTermThreshold);
    
    // Sync state: Update TrendDetector state met globale variabelen (voor parallel implementatie)
    void syncStateFromGlobals();
    
    // Helper: Get trend name string (inline voor performance)
    static inline const char* getTrendName(TrendState trend) {
        switch (trend) {
            case TREND_UP: return "UP";
            case TREND_DOWN: return "DOWN";
            case TREND_SIDEWAYS: return "SIDEWAYS";
            default: return "UNKNOWN";
        }
    }
    
    // Helper: Get color tag for trend (geoptimaliseerd: elimineert switch duplicatie)
    static inline const char* getTrendColorTag(TrendState trend) {
        switch (trend) {
            case TREND_UP: return "green_square,📈";
            case TREND_DOWN: return "red_square,📉";
            case TREND_SIDEWAYS:
            default: return "grey_square,➡️";
        }
    }
    
    // Helper: Get volatility text (geoptimaliseerd: elimineert switch duplicatie)
    static inline const char* getVolatilityText(VolatilityState volState);
    
private:
    TrendState trendState;
    TrendState previousTrendState;
    TrendState mediumTrendState;
    TrendState previousMediumTrendState;
    TrendState longTermTrendState;
    TrendState previousLongTermTrendState;
    unsigned long lastTrendChangeNotification;
    unsigned long lastMediumTrendChangeNotification;
    unsigned long lastLongTermTrendChangeNotification;
    
    TrendState determineTrendStateSimple(float ret_value, float trendThreshold, TrendState currentState);
    
    // Forward declarations voor dependencies (worden later via parameters of andere modules)
    // volatilityState - hoort bij VolatilityTracker module (later)
    // sendNotification() - hoort bij AlertEngine module (later)
    // Voor nu gebruiken we extern declarations
};

#endif // TRENDDETECTOR_H
