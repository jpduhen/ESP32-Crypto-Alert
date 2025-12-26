#include "AnchorSystem.h"

// Forward declarations voor dependencies (worden later via modules)
extern float openPrices[];  // Voor setAnchorPrice
// Fase 8.11.2: updateUI() is verplaatst naar UIController module (header al geïncludeerd via AnchorSystem.h)

// Forward declarations voor Serial macros
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif
#ifndef Serial_println
#define Serial_println Serial.println
#endif

// Constructor - initialiseer state variabelen
AnchorSystem::AnchorSystem() {
    // Initialiseer anchor state variabelen
    anchorPrice = 0.0f;
    anchorMax = 0.0f;
    anchorMin = 0.0f;
    anchorTime = 0;
    anchorActive = false;
    anchorTakeProfitSent = false;
    anchorMaxLossSent = false;
    
    // Initialiseer anchor settings (worden gesynchroniseerd in begin())
    anchorTakeProfit = 0.0f;
    anchorMaxLoss = 0.0f;
    trendAdaptiveAnchorsEnabled = false;
    uptrendMaxLossMultiplier = 1.0f;
    uptrendTakeProfitMultiplier = 1.0f;
    downtrendMaxLossMultiplier = 1.0f;
    downtrendTakeProfitMultiplier = 1.0f;
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void AnchorSystem::begin() {
    syncStateFromGlobals();
}

// Sync state: Update AnchorSystem state met globale variabelen
void AnchorSystem::syncStateFromGlobals() {
    // Fase 6.2.6: Synchroniseer anchor state variabelen met globale variabelen (parallel implementatie)
    extern float anchorPrice;
    extern float anchorMax;
    extern float anchorMin;
    extern unsigned long anchorTime;
    extern bool anchorActive;
    extern bool anchorTakeProfitSent;
    extern bool anchorMaxLossSent;
    
    // Kopieer waarden van globale variabelen naar AnchorSystem state
    this->anchorPrice = anchorPrice;
    this->anchorMax = anchorMax;
    this->anchorMin = anchorMin;
    this->anchorTime = anchorTime;
    this->anchorActive = anchorActive;
    this->anchorTakeProfitSent = anchorTakeProfitSent;
    this->anchorMaxLossSent = anchorMaxLossSent;
    
    // Fase 6.2.6: Synchroniseer anchor settings
    extern float anchorTakeProfit;
    extern float anchorMaxLoss;
    extern bool trendAdaptiveAnchorsEnabled;
    extern float uptrendMaxLossMultiplier;
    extern float uptrendTakeProfitMultiplier;
    extern float downtrendMaxLossMultiplier;
    extern float downtrendTakeProfitMultiplier;
    
    this->anchorTakeProfit = anchorTakeProfit;
    this->anchorMaxLoss = anchorMaxLoss;
    this->trendAdaptiveAnchorsEnabled = trendAdaptiveAnchorsEnabled;
    this->uptrendMaxLossMultiplier = uptrendMaxLossMultiplier;
    this->uptrendTakeProfitMultiplier = uptrendTakeProfitMultiplier;
    this->downtrendMaxLossMultiplier = downtrendMaxLossMultiplier;
    this->downtrendTakeProfitMultiplier = downtrendTakeProfitMultiplier;
}

// Calculate effective anchor thresholds based on trend
// Fase 6.2.3: Verplaatst naar AnchorSystem (parallel implementatie)
AnchorConfigEffective AnchorSystem::calculateEffectiveAnchorThresholds(TrendState trend, float baseMaxLoss, float baseTakeProfit)
{
    AnchorConfigEffective eff;
    
    // Als trend-adaptive uit staat, gebruik basiswaarden
    if (!this->trendAdaptiveAnchorsEnabled) {
        eff.maxLossPct = baseMaxLoss;
        eff.takeProfitPct = baseTakeProfit;
        return eff;
    }
    
    // Pas multipliers toe op basis van trend
    switch (trend) {
        case TREND_UP:
            eff.maxLossPct = baseMaxLoss * this->uptrendMaxLossMultiplier;
            eff.takeProfitPct = baseTakeProfit * this->uptrendTakeProfitMultiplier;
            break;
            
        case TREND_DOWN:
            eff.maxLossPct = baseMaxLoss * this->downtrendMaxLossMultiplier;
            eff.takeProfitPct = baseTakeProfit * this->downtrendTakeProfitMultiplier;
            break;
            
        case TREND_SIDEWAYS:
        default:
            // Basiswaarden (geen aanpassing)
            eff.maxLossPct = baseMaxLoss;
            eff.takeProfitPct = baseTakeProfit;
            break;
    }
    
    // Clamp waarden om extreme situaties te voorkomen
    if (eff.maxLossPct < -6.0f) eff.maxLossPct = -6.0f;
    if (eff.maxLossPct > -1.0f) eff.maxLossPct = -1.0f;
    if (eff.takeProfitPct < 2.0f) eff.takeProfitPct = 2.0f;
    if (eff.takeProfitPct > 10.0f) eff.takeProfitPct = 10.0f;
    
    return eff;
}

// Legacy functie voor backward compatibility
// Fase 6.2.3: Verplaatst naar AnchorSystem (parallel implementatie)
AnchorConfigEffective AnchorSystem::calcEffectiveAnchor(float baseMaxLoss, float baseTakeProfit, TrendState trend)
{
    return calculateEffectiveAnchorThresholds(trend, baseMaxLoss, baseTakeProfit);
}

// Check anchor take profit / max loss alerts
// Fase 6.2.4: Verplaatst naar AnchorSystem (parallel implementatie)
void AnchorSystem::checkAnchorAlerts()
{
    if (!this->anchorActive || !isValidPrice(this->anchorPrice) || !isValidPrice(prices[0])) {
        return; // Geen actieve anchor of geen prijs data
    }
    
    // Bereken dynamische anchor-waarden op basis van trend
    TrendState currentTrend = trendDetector.getTrendState();
    AnchorConfigEffective effAnchor = calculateEffectiveAnchorThresholds(currentTrend, this->anchorMaxLoss, this->anchorTakeProfit);
    
    // Bereken percentage verandering t.o.v. anchor
    float anchorPct = ((prices[0] - this->anchorPrice) / this->anchorPrice) * 100.0f;
    
    // Helper: get trend name
    const char* trendName = "";
    switch (currentTrend) {
        case TREND_UP: trendName = "UP"; break;
        case TREND_DOWN: trendName = "DOWN"; break;
        case TREND_SIDEWAYS: trendName = "SIDEWAYS"; break;
    }
    
    // Check take profit met dynamische waarde
    if (!this->anchorTakeProfitSent && anchorPct >= effAnchor.takeProfitPct) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[320];
        snprintf(title, sizeof(title), "%s Take Profit", binanceSymbol);
        
        // Toon trend en effective thresholds in notificatie
        if (this->trendAdaptiveAnchorsEnabled) {
            snprintf(msg, sizeof(msg), 
                     "Take profit bereikt: +%.2f%%\nTrend: %s, Threshold (eff.): +%.2f%% (basis: +%.2f%%)\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nWinst: +%.2f EUR",
                     anchorPct, trendName, effAnchor.takeProfitPct, this->anchorTakeProfit, this->anchorPrice, timestamp, prices[0], prices[0] - this->anchorPrice);
        } else {
            snprintf(msg, sizeof(msg), 
                     "Take profit bereikt: +%.2f%%\nThreshold: +%.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nWinst: +%.2f EUR",
                     anchorPct, effAnchor.takeProfitPct, this->anchorPrice, timestamp, prices[0], prices[0] - this->anchorPrice);
        }
        sendNotification(title, msg, "green_square,💰");
        this->anchorTakeProfitSent = true;
        
        // Fase 6.2: Update ook globale variabele voor backward compatibility
        extern bool anchorTakeProfitSent;
        anchorTakeProfitSent = true;
        
        Serial_printf(F("[Anchor] Take profit notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n"), 
                     anchorPct, effAnchor.takeProfitPct, this->anchorTakeProfit, trendName, this->anchorPrice, prices[0]);
        
        // Publiceer take profit event naar MQTT
        publishMqttAnchorEvent(this->anchorPrice, "take_profit");
    }
    
    // Check max loss met dynamische waarde
    if (!this->anchorMaxLossSent && anchorPct <= effAnchor.maxLossPct) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[320];
        snprintf(title, sizeof(title), "%s Max Loss", binanceSymbol);
        
        // Toon trend en effective thresholds in notificatie
        if (this->trendAdaptiveAnchorsEnabled) {
            snprintf(msg, sizeof(msg), 
                     "Max loss bereikt: %.2f%%\nTrend: %s, Threshold (eff.): %.2f%% (basis: %.2f%%)\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nVerlies: %.2f EUR",
                     anchorPct, trendName, effAnchor.maxLossPct, this->anchorMaxLoss, this->anchorPrice, timestamp, prices[0], prices[0] - this->anchorPrice);
        } else {
            snprintf(msg, sizeof(msg), 
                     "Max loss bereikt: %.2f%%\nThreshold: %.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nVerlies: %.2f EUR",
                     anchorPct, effAnchor.maxLossPct, this->anchorPrice, timestamp, prices[0], prices[0] - this->anchorPrice);
        }
        sendNotification(title, msg, "red_square,⚠️");
        this->anchorMaxLossSent = true;
        
        // Fase 6.2: Update ook globale variabele voor backward compatibility
        extern bool anchorMaxLossSent;
        anchorMaxLossSent = true;
        
        Serial_printf(F("[Anchor] Max loss notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n"), 
                     anchorPct, effAnchor.maxLossPct, this->anchorMaxLoss, trendName, this->anchorPrice, prices[0]);
        
        // Publiceer max loss event naar MQTT
        publishMqttAnchorEvent(this->anchorPrice, "max_loss");
    }
}

// Update anchor min/max wanneer prijs verandert
// Fase 6.2: Nieuwe methode om anchor tracking te synchroniseren
void AnchorSystem::updateAnchorMinMax(float currentPrice)
{
    if (!this->anchorActive || !isValidPrice(this->anchorPrice) || !isValidPrice(currentPrice)) {
        return; // Geen actieve anchor of geen geldige prijs
    }
    
    // Update min/max binnen module
    if (currentPrice > this->anchorMax) {
        this->anchorMax = currentPrice;
    }
    if (currentPrice < this->anchorMin) {
        this->anchorMin = currentPrice;
    }
    
    // Fase 6.2: Update ook globale variabelen voor backward compatibility
    extern float anchorMax;
    extern float anchorMin;
    anchorMax = this->anchorMax;
    anchorMin = this->anchorMin;
}

// Helper functie om anchor in te stellen (thread-safe)
// Fase 6.2.5: Verplaatst naar AnchorSystem (parallel implementatie)
bool AnchorSystem::setAnchorPrice(float anchorValue, bool shouldUpdateUI, bool skipNotifications)
{
    // Kortere timeout voor web server thread om watchdog te voorkomen
    TickType_t timeout = skipNotifications ? pdMS_TO_TICKS(100) : pdMS_TO_TICKS(500);
    if (safeMutexTake(dataMutex, timeout, "setAnchorPrice")) {
        float priceToSet = anchorValue;
        
        // Als anchorValue 0 is of ongeldig, gebruik huidige prijs
        if (priceToSet <= 0.0f || !isValidPrice(priceToSet)) {
            if (isValidPrice(prices[0])) {
                priceToSet = prices[0];
                Serial_printf(F("[Anchor] Gebruik huidige prijs als anchor: %.2f\n"), priceToSet);
            } else {
                Serial_println("[Anchor] WARN: Geen geldige prijs beschikbaar voor anchor");
                safeMutexGive(dataMutex, "setAnchorPrice");
                return false;
            }
        }
        
        // Valideer dat de prijs nog steeds geldig is na mutex lock
        if (!isValidPrice(priceToSet)) {
            Serial_println("[Anchor] WARN: Prijs ongeldig na validatie");
            safeMutexGive(dataMutex, "setAnchorPrice");
            return false;
        }
        
        // Set anchor price (atomisch binnen mutex)
        this->anchorPrice = priceToSet;
        openPrices[0] = priceToSet;
        this->anchorMax = priceToSet;  // Initialiseer max/min met anchor prijs
        this->anchorMin = priceToSet;
        this->anchorTime = millis();
        this->anchorActive = true;
        this->anchorTakeProfitSent = false;
        this->anchorMaxLossSent = false;
        
        // Fase 6.2: Update ook globale variabelen voor backward compatibility
        // (andere code gebruikt nog de globale variabelen, zoals UI updates)
        extern float anchorPrice;
        extern float anchorMax;
        extern float anchorMin;
        extern unsigned long anchorTime;
        extern bool anchorActive;
        extern bool anchorTakeProfitSent;
        extern bool anchorMaxLossSent;
        
        anchorPrice = priceToSet;
        anchorMax = priceToSet;
        anchorMin = priceToSet;
        anchorTime = this->anchorTime;
        anchorActive = true;
        anchorTakeProfitSent = false;
        anchorMaxLossSent = false;
        
        Serial_printf(F("[Anchor] Anchor set: anchorPrice = %.2f\n"), this->anchorPrice);
        
        safeMutexGive(dataMutex, "setAnchorPrice");
        
        // Publiceer anchor event naar MQTT en stuur notificatie alleen als niet overgeslagen
        // Doe dit BUITEN de mutex om blocking operaties te voorkomen
        if (!skipNotifications) {
            publishMqttAnchorEvent(this->anchorPrice, "anchor_set");
            
            // Stuur NTFY notificatie
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char title[64];
            char msg[128];
            snprintf(title, sizeof(title), "%s Anchor Set", binanceSymbol);
            snprintf(msg, sizeof(msg), "%s: %.2f EUR", timestamp, priceToSet);
            sendNotification(title, msg, "white_check_mark");
        }
        
        // Update UI alleen als gevraagd (niet vanuit web/MQTT threads)
        // Fase 8.11.2: Gebruik module versie
        if (shouldUpdateUI) {
            uiController.updateUI();
        }
        
        return true;
    }
    Serial_println("[Anchor] WARN: Mutex timeout bij setAnchorPrice");
    return false;
}

