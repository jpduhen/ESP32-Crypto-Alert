#ifndef ANCHORSYSTEM_H
#define ANCHORSYSTEM_H

#include <Arduino.h>

// Forward declarations voor dependencies (worden later via modules)
extern bool sendNotification(const char *title, const char *message, const char *colorTag);
extern char binanceSymbol[];
extern float prices[];  // Voor anchor price checks
void getFormattedTimestamp(char* buffer, size_t bufferSize);  // Voor notificaties
void publishMqttAnchorEvent(float anchor_price, const char* event_type);  // Voor MQTT events
bool isValidPrice(float price);  // Voor price validatie
bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* caller);  // Voor thread-safe operaties
void safeMutexGive(SemaphoreHandle_t mutex, const char* caller);  // Voor thread-safe operaties
extern SemaphoreHandle_t dataMutex;  // FreeRTOS mutex
void updateUI();  // Voor UI updates

// TrendDetector module (voor trend-adaptive anchors)
#include "../TrendDetector/TrendDetector.h"
extern TrendDetector trendDetector;  // Voor trend state

// Dynamische anchor configuratie op basis van trend
struct AnchorConfigEffective {
    float maxLossPct;      // Effectieve max loss percentage (negatief)
    float takeProfitPct;   // Effectieve take profit percentage (positief)
};

// AnchorSystem class - beheert anchor price tracking en alerts
class AnchorSystem {
public:
    AnchorSystem();
    void begin();
    
    // Anchor price management
    // anchorValue: de waarde om in te stellen (0.0 = gebruik huidige prijs)
    // shouldUpdateUI: true = update UI direct (alleen vanuit main loop thread), false = skip UI update
    // skipNotifications: true = skip NTFY en MQTT (voor web server thread), false = stuur notificaties
    // returns: true als succesvol, false als mislukt
    bool setAnchorPrice(float anchorValue = 0.0f, bool shouldUpdateUI = true, bool skipNotifications = false);
    
    // Calculate effective anchor thresholds based on trend
    // trend: huidige trend state
    // baseMaxLoss: basis max loss threshold
    // baseTakeProfit: basis take profit threshold
    // returns: AnchorConfigEffective met effectieve thresholds
    AnchorConfigEffective calculateEffectiveAnchorThresholds(TrendState trend, float baseMaxLoss, float baseTakeProfit);
    
    // Legacy functie voor backward compatibility
    AnchorConfigEffective calcEffectiveAnchor(float baseMaxLoss, float baseTakeProfit, TrendState trend);
    
    // Check anchor take profit / max loss alerts
    void checkAnchorAlerts();
    
    // Update anchor min/max wanneer prijs verandert (aanroepen vanuit apiTask)
    void updateAnchorMinMax(float currentPrice);
    
    // Getters voor anchor state (voor backward compatibility)
    float getAnchorPrice() const { return anchorPrice; }
    float getAnchorMax() const { return anchorMax; }
    float getAnchorMin() const { return anchorMin; }
    unsigned long getAnchorTime() const { return anchorTime; }
    bool isAnchorActive() const { return anchorActive; }
    bool isAnchorTakeProfitSent() const { return anchorTakeProfitSent; }
    bool isAnchorMaxLossSent() const { return anchorMaxLossSent; }
    
    // Sync state: Update AnchorSystem state met globale variabelen (voor parallel implementatie)
    void syncStateFromGlobals();
    
private:
    // Anchor state variabelen
    float anchorPrice;
    float anchorMax;  // Hoogste prijs sinds anchor
    float anchorMin;  // Laagste prijs sinds anchor
    unsigned long anchorTime;
    bool anchorActive;
    bool anchorTakeProfitSent;
    bool anchorMaxLossSent;
    
    // Anchor settings (worden geladen uit settings)
    float anchorTakeProfit;
    float anchorMaxLoss;
    bool trendAdaptiveAnchorsEnabled;
    float uptrendMaxLossMultiplier;
    float uptrendTakeProfitMultiplier;
    float downtrendMaxLossMultiplier;
    float downtrendTakeProfitMultiplier;
};

#endif // ANCHORSYSTEM_H

