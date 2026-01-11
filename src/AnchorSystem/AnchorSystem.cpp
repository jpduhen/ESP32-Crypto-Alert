#include "AnchorSystem.h"

// Forward declarations voor dependencies (worden later via modules)
extern float openPrices[];  // Voor setAnchorPrice
extern uint8_t language;  // Taalinstelling (0 = Nederlands, 1 = English)
extern const char* getText(const char* nlText, const char* enText);  // Taalvertaling functie
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
    anchorPriceInv = 0.0f;
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
    
    // Buffers verwijderd om DRAM overflow te voorkomen - gebruik lokale buffers
}

// Begin - synchroniseer state met globale variabelen (parallel implementatie)
void AnchorSystem::begin() {
    syncStateFromGlobals();
}

// Sync state: Update AnchorSystem state met globale variabelen
void AnchorSystem::syncStateFromGlobals() {
    const TickType_t timeout = pdMS_TO_TICKS(200);
    if (!safeMutexTake(dataMutex, timeout, "syncStateFromGlobals")) {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (!safeMutexTake(dataMutex, timeout, "syncStateFromGlobals retry")) {
            #if !DEBUG_BUTTON_ONLY
            static uint32_t lastSyncWarnMs = 0;
            uint32_t now = millis();
            if (now - lastSyncWarnMs > 2000) {
                Serial_println("[Anchor] WARN: Mutex timeout bij syncStateFromGlobals");
                lastSyncWarnMs = now;
            }
            #endif
            return;
        }
    }
    // Fase 6.2.6: Synchroniseer anchor state variabelen met globale variabelen (parallel implementatie)
    extern float anchorPrice;
    extern float anchorMax;
    extern float anchorMin;
    extern unsigned long anchorTime;
    extern bool anchorActive;
    extern bool anchorTakeProfitSent;
    extern bool anchorMaxLossSent;
    extern float anchorTakeProfit;
    extern float anchorMaxLoss;
    extern bool trendAdaptiveAnchorsEnabled;
    extern float uptrendMaxLossMultiplier;
    extern float uptrendTakeProfitMultiplier;
    extern float downtrendMaxLossMultiplier;
    extern float downtrendTakeProfitMultiplier;

    float localAnchorPrice = anchorPrice;
    float localAnchorMax = anchorMax;
    float localAnchorMin = anchorMin;
    unsigned long localAnchorTime = anchorTime;
    bool localAnchorActive = anchorActive;
    bool localAnchorTakeProfitSent = anchorTakeProfitSent;
    bool localAnchorMaxLossSent = anchorMaxLossSent;
    float localAnchorTakeProfit = anchorTakeProfit;
    float localAnchorMaxLoss = anchorMaxLoss;
    bool localTrendAdaptiveEnabled = trendAdaptiveAnchorsEnabled;
    float localUptrendMaxLossMultiplier = uptrendMaxLossMultiplier;
    float localUptrendTakeProfitMultiplier = uptrendTakeProfitMultiplier;
    float localDowntrendMaxLossMultiplier = downtrendMaxLossMultiplier;
    float localDowntrendTakeProfitMultiplier = downtrendTakeProfitMultiplier;

    safeMutexGive(dataMutex, "syncStateFromGlobals");

    // Kopieer waarden van globale variabelen naar AnchorSystem state (buiten mutex)
    this->anchorPrice = localAnchorPrice;
    this->anchorPriceInv = isValidPrice(localAnchorPrice) ? (1.0f / localAnchorPrice) : 0.0f;
    this->anchorMax = localAnchorMax;
    this->anchorMin = localAnchorMin;
    this->anchorTime = localAnchorTime;
    this->anchorActive = localAnchorActive;
    this->anchorTakeProfitSent = localAnchorTakeProfitSent;
    this->anchorMaxLossSent = localAnchorMaxLossSent;
    this->anchorTakeProfit = localAnchorTakeProfit;
    this->anchorMaxLoss = localAnchorMaxLoss;
    this->trendAdaptiveAnchorsEnabled = localTrendAdaptiveEnabled;
    this->uptrendMaxLossMultiplier = localUptrendMaxLossMultiplier;
    this->uptrendTakeProfitMultiplier = localUptrendTakeProfitMultiplier;
    this->downtrendMaxLossMultiplier = localDowntrendMaxLossMultiplier;
    this->downtrendTakeProfitMultiplier = localDowntrendTakeProfitMultiplier;
}

// Calculate effective anchor thresholds based on trend
// Fase 6.2.3: Verplaatst naar AnchorSystem (parallel implementatie)
// Geoptimaliseerd: early return, geconsolideerde clamp operaties
AnchorConfigEffective AnchorSystem::calculateEffectiveAnchorThresholds(TrendState trend, float baseMaxLoss, float baseTakeProfit)
{
    AnchorConfigEffective eff{baseMaxLoss, baseTakeProfit};
    
    // Bepaal multipliers (geoptimaliseerd: direct berekenen zonder switch indien mogelijk)
    if (this->trendAdaptiveAnchorsEnabled) {
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
                break;
        }
    }
    
    // Geconsolideerde clamp operaties (één keer, voor beide paden)
    if (eff.maxLossPct < -6.0f) eff.maxLossPct = -6.0f;
    else if (eff.maxLossPct > -1.0f) eff.maxLossPct = -1.0f;
    if (eff.takeProfitPct < 2.0f) eff.takeProfitPct = 2.0f;
    else if (eff.takeProfitPct > 10.0f) eff.takeProfitPct = 10.0f;
    
    return eff;
}

// Legacy functie voor backward compatibility
// Fase 6.2.3: Verplaatst naar AnchorSystem (parallel implementatie)
AnchorConfigEffective AnchorSystem::calcEffectiveAnchor(float baseMaxLoss, float baseTakeProfit, TrendState trend)
{
    return calculateEffectiveAnchorThresholds(trend, baseMaxLoss, baseTakeProfit);
}

// Helper: Format notification message (gebruikt lokale buffers)
// Geoptimaliseerd: gebruik lokale stack buffers i.p.v. instance members (bespaart DRAM)
// Geoptimaliseerd: enum i.p.v. string vergelijkingen (sneller)
void AnchorSystem::formatAnchorNotification(AnchorEventType eventType, float anchorPct, 
                                            const AnchorConfigEffective& effAnchor, 
                                            TrendState trend,
                                            char* msgBuffer, size_t msgSize,
                                            char* titleBuffer, size_t titleSize,
                                            char* timestampBuffer, size_t timestampSize) {
    getFormattedTimestampForNotification(timestampBuffer, timestampSize);
    
    // Vertaal trend naar juiste taal
    const char* trendNameTranslated = getTrendName(trend);
    switch (trend) {
        case TREND_UP:
            trendNameTranslated = getText("OP", "UP");
            break;
        case TREND_DOWN:
            trendNameTranslated = getText("NEER", "DOWN");
            break;
        case TREND_SIDEWAYS:
            trendNameTranslated = getText("ZIJWAARTS", "SIDEWAYS");
            break;
        default:
            break;
    }
    
    // Geoptimaliseerd: enum switch i.p.v. strcmp (sneller, geen string vergelijkingen)
    if (eventType == ANCHOR_EVENT_TAKE_PROFIT) {
        snprintf(titleBuffer, titleSize, "%s %s: %s", binanceSymbol, 
                 getText("Anker", "Anchor"), getText("Winstpakker", "Take Profit"));
        if (this->trendAdaptiveAnchorsEnabled) {
            snprintf(msgBuffer, msgSize, 
                     "%.2f (%s)\n%s: %s\n%s: %.2f | %s: +%.2f\nTP: +%.2f%% (%s: +%.2f%%, %s: +%.2f%%)",
                     prices[0], timestampBuffer,
                     getText("Trend", "Trend"), trendNameTranslated,
                     getText("Anker", "Anchor"), this->anchorPrice,
                     getText("Winst", "Profit"), prices[0] - this->anchorPrice,
                     anchorPct, getText("thr", "thr"), effAnchor.takeProfitPct,
                     getText("basis", "basis"), this->anchorTakeProfit);
        } else {
            snprintf(msgBuffer, msgSize, 
                     "%.2f (%s)\n%s: %.2f | %s: +%.2f\nTP: +%.2f%% (%s: +%.2f%%)",
                     prices[0], timestampBuffer,
                     getText("Anker", "Anchor"), this->anchorPrice,
                     getText("Winst", "Profit"), prices[0] - this->anchorPrice,
                     anchorPct, getText("thr", "thr"), effAnchor.takeProfitPct);
        }
    } else {  // ANCHOR_EVENT_MAX_LOSS
        snprintf(titleBuffer, titleSize, "%s %s: %s", binanceSymbol,
                 getText("Anker", "Anchor"), getText("Verliesbeperker", "Max Loss"));
        if (this->trendAdaptiveAnchorsEnabled) {
            snprintf(msgBuffer, msgSize, 
                     "%.2f (%s)\n%s: %s\n%s: %.2f | %s: %.2f\nML: %.2f%% (%s: %.2f%%, %s: %.2f%%)",
                     prices[0], timestampBuffer,
                     getText("Trend", "Trend"), trendNameTranslated,
                     getText("Anker", "Anchor"), this->anchorPrice,
                     getText("Verlies", "Loss"), prices[0] - this->anchorPrice,
                     anchorPct, getText("thr", "thr"), effAnchor.maxLossPct,
                     getText("basis", "basis"), this->anchorMaxLoss);
        } else {
            snprintf(msgBuffer, msgSize, 
                     "%.2f (%s)\n%s: %.2f | %s: %.2f\nML: %.2f%% (%s: %.2f%%)",
                     prices[0], timestampBuffer,
                     getText("Anker", "Anchor"), this->anchorPrice,
                     getText("Verlies", "Loss"), prices[0] - this->anchorPrice,
                     anchorPct, getText("thr", "thr"), effAnchor.maxLossPct);
        }
    }
}

// Helper: Send anchor alert notification (consolideert take profit en max loss logica)
// Geoptimaliseerd: elimineert code duplicatie, gebruikt lokale buffers
void AnchorSystem::sendAnchorAlert(AnchorEventType eventType, float anchorPct, 
                                   const AnchorConfigEffective& effAnchor, 
                                   TrendState trend) {
    // Gebruik lokale buffers (stack geheugen i.p.v. DRAM)
    char timestamp[32];
    char title[40];
    char msg[140];
    
    formatAnchorNotification(eventType, anchorPct, effAnchor, trend,
                           msg, sizeof(msg), title, sizeof(title), timestamp, sizeof(timestamp));
    
    // Bepaal color tag en MQTT event type op basis van event type
    const char* colorTag = (eventType == ANCHOR_EVENT_TAKE_PROFIT) ? "green_square,💰" : "red_square,⚠️";
    const char* mqttEventType = (eventType == ANCHOR_EVENT_TAKE_PROFIT) ? "take_profit" : "max_loss";
    
    sendNotification(title, msg, colorTag);
    
    // Update flags en globale variabelen
    if (eventType == ANCHOR_EVENT_TAKE_PROFIT) {
        this->anchorTakeProfitSent = true;
        extern bool anchorTakeProfitSent;
        anchorTakeProfitSent = true;
    } else {
        this->anchorMaxLossSent = true;
        extern bool anchorMaxLossSent;
        anchorMaxLossSent = true;
    }
    
    #if !DEBUG_BUTTON_ONLY
    const char* eventName = (eventType == ANCHOR_EVENT_TAKE_PROFIT) ? "Take profit" : "Max loss";
    float threshold = (eventType == ANCHOR_EVENT_TAKE_PROFIT) ? effAnchor.takeProfitPct : effAnchor.maxLossPct;
    float baseThreshold = (eventType == ANCHOR_EVENT_TAKE_PROFIT) ? this->anchorTakeProfit : this->anchorMaxLoss;
    const char* trendName = getTrendName(trend);
    Serial_printf(F("[Anchor] %s notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n"), 
                 eventName, anchorPct, threshold, baseThreshold, trendName, this->anchorPrice, prices[0]);
    #endif
    
    // Publiceer event naar MQTT
    publishMqttAnchorEvent(this->anchorPrice, mqttEventType);
}

// Check anchor take profit / max loss alerts
// Fase 6.2.4: Verplaatst naar AnchorSystem (parallel implementatie)
// Geoptimaliseerd: cache waarden, early returns, hergebruik buffers, validatie
void AnchorSystem::checkAnchorAlerts()
{
    // Geconsolideerde early return: check alle voorwaarden in één keer (sneller, minder branches)
    if (!this->anchorActive || 
        !isValidPrice(this->anchorPrice) || !isValidPrice(prices[0]) ||
        this->anchorPriceInv <= 0.0f) {
        return; // Geen actieve anchor, geen prijs data, of ongeldige waarden
    }
    
    // Bereken anchor percentage direct (cache verwijderd om geheugen te besparen)
    float anchorPct = (prices[0] - this->anchorPrice) * this->anchorPriceInv * 100.0f;
    
    // Haal trend state op (geen cache om geheugen te besparen)
    TrendState currentTrend = trendDetector.getTrendState();
    
    // Bereken dynamische anchor-waarden op basis van trend
    AnchorConfigEffective effAnchor = calculateEffectiveAnchorThresholds(currentTrend, this->anchorMaxLoss, this->anchorTakeProfit);
    
    // Check take profit met dynamische waarde (geconsolideerd met helper functie)
    if (!this->anchorTakeProfitSent && anchorPct >= effAnchor.takeProfitPct) {
        sendAnchorAlert(ANCHOR_EVENT_TAKE_PROFIT, anchorPct, effAnchor, currentTrend);
    }
    
    // Check max loss met dynamische waarde (geconsolideerd met helper functie)
    if (!this->anchorMaxLossSent && anchorPct <= effAnchor.maxLossPct) {
        sendAnchorAlert(ANCHOR_EVENT_MAX_LOSS, anchorPct, effAnchor, currentTrend);
    }
}

// Update anchor min/max wanneer prijs verandert
// Fase 6.2: Nieuwe methode om anchor tracking te synchroniseren
// Geoptimaliseerd: early returns, validatie, geoptimaliseerde min/max updates
void AnchorSystem::updateAnchorMinMax(float currentPrice)
{
    // Early return: check voorwaarden eerst (sneller)
    if (!this->anchorActive || !isValidPrice(this->anchorPrice) || !isValidPrice(currentPrice)) {
        return; // Geen actieve anchor of geen geldige prijs
    }
    
    // Geoptimaliseerde min/max updates (geen onnodige assignments)
    bool updated = false;
    if (currentPrice > this->anchorMax) {
        this->anchorMax = currentPrice;
        updated = true;
    }
    if (currentPrice < this->anchorMin) {
        this->anchorMin = currentPrice;
        updated = true;
    }
    
    // Alleen globale variabelen updaten als er iets veranderd is
    if (updated) {
        const TickType_t timeout = pdMS_TO_TICKS(200);
        if (!safeMutexTake(dataMutex, timeout, "updateAnchorMinMax")) {
            vTaskDelay(pdMS_TO_TICKS(5));
            if (!safeMutexTake(dataMutex, timeout, "updateAnchorMinMax retry")) {
                #if !DEBUG_BUTTON_ONLY
                static uint32_t lastMinMaxWarnMs = 0;
                uint32_t now = millis();
                if (now - lastMinMaxWarnMs > 2000) {
                    Serial_println("[Anchor] WARN: Mutex timeout bij updateAnchorMinMax");
                    lastMinMaxWarnMs = now;
                }
                #endif
                return;
            }
        }
        // Fase 6.2: Update ook globale variabelen voor backward compatibility
        extern float anchorMax;
        extern float anchorMin;
        anchorMax = this->anchorMax;
        anchorMin = this->anchorMin;
        safeMutexGive(dataMutex, "updateAnchorMinMax");
    }
}

// Helper functie om anchor in te stellen (thread-safe)
// Fase 6.2.5: Verplaatst naar AnchorSystem (parallel implementatie)
bool AnchorSystem::setAnchorPrice(float anchorValue, bool shouldUpdateUI, bool skipNotifications)
{
    // Timeout: korter voor web server thread (100ms), langer voor auto anchor updates (2000ms), normaal voor andere (500ms)
    TickType_t timeout;
    if (skipNotifications && !shouldUpdateUI) {
        // Auto anchor update (skipNotifications=true, shouldUpdateUI=false): langere timeout
        timeout = pdMS_TO_TICKS(2000);
    } else if (skipNotifications && shouldUpdateUI) {
        // Web server thread: korte timeout
        timeout = pdMS_TO_TICKS(100);
    } else {
        // Normale update: medium timeout
        timeout = pdMS_TO_TICKS(500);
    }
    
    // Probeer mutex te krijgen (geen retry, gewoon langere timeout)
    if (safeMutexTake(dataMutex, timeout, "setAnchorPrice")) {
        float priceToSet = anchorValue;
        
        // Geconsolideerde validatie: check alles in één keer
        // Als anchorValue 0 is of ongeldig, gebruik huidige prijs
        if (priceToSet <= 0.0f || !isValidPrice(priceToSet)) {
            // Probeer huidige prijs te gebruiken
            if (isValidPrice(prices[0])) {
                priceToSet = prices[0];
                #if !DEBUG_BUTTON_ONLY
                Serial_printf(F("[Anchor] Gebruik huidige prijs als anchor: %.2f\n"), priceToSet);
                #endif
            } else {
                #if !DEBUG_BUTTON_ONLY
                Serial_println("[Anchor] WARN: Geen geldige prijs beschikbaar voor anchor");
                #endif
                safeMutexGive(dataMutex, "setAnchorPrice");
                return false;
            }
        }
        
        // Finale validatie: check dat priceToSet nu geldig is
        if (!isValidPrice(priceToSet) || priceToSet <= 0.0f) {
            #if !DEBUG_BUTTON_ONLY
            Serial_println("[Anchor] WARN: Prijs ongeldig na validatie");
            #endif
            safeMutexGive(dataMutex, "setAnchorPrice");
            return false;
        }
        
        // Set anchor price (atomisch binnen mutex)
        // Geoptimaliseerd: initialiseer alles in één keer
        unsigned long now = millis();
        this->anchorPrice = priceToSet;
        this->anchorPriceInv = 1.0f / priceToSet;
        this->anchorMax = priceToSet;  // Initialiseer max/min met anchor prijs
        this->anchorMin = priceToSet;
        this->anchorTime = now;
        this->anchorActive = true;
        this->anchorTakeProfitSent = false;
        this->anchorMaxLossSent = false;
        
        // Update openPrices (voor backward compatibility)
        openPrices[0] = priceToSet;
        
        // Fase 6.2: Update ook globale variabelen voor backward compatibility
        // (andere code gebruikt nog de globale variabelen, zoals UI updates)
        // Geoptimaliseerd: update in één blok
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
        anchorTime = now;
        anchorActive = true;
        anchorTakeProfitSent = false;
        anchorMaxLossSent = false;
        
        #if !DEBUG_BUTTON_ONLY
        Serial_printf(F("[Anchor] Anchor set: anchorPrice = %.2f\n"), this->anchorPrice);
        #endif
        
        safeMutexGive(dataMutex, "setAnchorPrice");
        
        // Reset UI cache variabelen zodat UI de nieuwe anchor waarde zal tonen
        // Dit moet BUITEN de mutex gebeuren om deadlocks te voorkomen
        extern float lastAnchorMaxValue;
        extern float lastAnchorValue;
        extern float lastAnchorMinValue;
        lastAnchorMaxValue = -1.0f;  // Force UI update voor anchor max
        lastAnchorValue = -1.0f;     // Force UI update voor anchor
        lastAnchorMinValue = -1.0f;  // Force UI update voor anchor min
        
        // Publiceer anchor event naar MQTT en stuur notificatie alleen als niet overgeslagen
        // Doe dit BUITEN de mutex om blocking operaties te voorkomen
        if (!skipNotifications) {
            publishMqttAnchorEvent(this->anchorPrice, "anchor_set");
            
            // Stuur NTFY notificatie met lokale buffers (stack geheugen i.p.v. DRAM)
            char timestamp[32];
            char title[40];
            char msg[80];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
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
    #if !DEBUG_BUTTON_ONLY
    Serial_println("[Anchor] WARN: Mutex timeout bij setAnchorPrice");
    #endif
    return false;
}
