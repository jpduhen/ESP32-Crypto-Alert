#include "WebServer.h"
#include <WebServer.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "../SettingsStore/SettingsStore.h"

// Forward declarations voor dependencies
extern WebServer server;  // Globale WebServer instance (gedefinieerd in .ino)
extern TrendDetector trendDetector;
extern VolatilityTracker volatilityTracker;
extern AnchorSystem anchorSystem;

// Externe variabelen en functies die nodig zijn voor web server
extern SemaphoreHandle_t dataMutex;
extern float prices[];
extern bool anchorActive;
extern float anchorPrice;
extern char ntfyTopic[];
extern char binanceSymbol[];
extern uint8_t language;
extern char mqttHost[];
extern char mqttUser[];
extern char mqttPass[];
extern uint16_t mqttPort;
extern bool mqttConnected;
extern PubSubClient mqttClient;
extern unsigned long lastMqttReconnectAttempt;
extern uint8_t mqttReconnectAttemptCount;

// Settings variabelen
// Note: spike1mThreshold, spike5mThreshold, etc. zijn macro's (gedefinieerd hieronder)
extern float trendThreshold;
extern float volatilityLowThreshold;
extern float volatilityHighThreshold;
extern float anchorTakeProfit;
extern float anchorMaxLoss;
extern bool trendAdaptiveAnchorsEnabled;
extern float uptrendMaxLossMultiplier;
extern float uptrendTakeProfitMultiplier;
extern float downtrendMaxLossMultiplier;
extern float downtrendTakeProfitMultiplier;
extern bool smartConfluenceEnabled;
extern bool warmStartEnabled;
extern uint8_t warmStart1mExtraCandles;
extern uint8_t warmStart5mCandles;
extern uint8_t warmStart30mCandles;
extern uint8_t warmStart2hCandles;
extern bool autoVolatilityEnabled;
extern uint8_t autoVolatilityWindowMinutes;
extern float autoVolatilityBaseline1mStdPct;
extern float autoVolatilityMinMultiplier;
extern float autoVolatilityMaxMultiplier;
// Note: notificationCooldown1MinMs, etc. zijn macro's (gedefinieerd hieronder)
extern char symbolsArray[][16];

// Helper functies
extern const char* getText(const char* dutch, const char* english);
extern void logHeap(const char* context);
extern void formatIPAddress(IPAddress ip, char* buffer, size_t bufferSize);
extern void generateDefaultNtfyTopic(char* buffer, size_t bufferSize);
extern SettingsStore settingsStore;
extern AlertThresholds alertThresholds;
extern NotificationCooldowns notificationCooldowns;
extern void saveSettings();
extern bool safeAtof(const char* str, float& result);
extern bool safeSecondsToMs(int seconds, uint32_t& resultMs);
extern bool safeStrncpy(char* dest, const char* src, size_t destSize);
extern bool isValidPrice(float price);
extern bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* caller);
extern void safeMutexGive(SemaphoreHandle_t mutex, const char* caller);
extern bool queueAnchorSetting(float value, bool useCurrentPrice);

// Serial_printf en Serial_println zijn macro's, geen functies
// Definieer ze hier als macro's (zonder DEBUG_BUTTON_ONLY check, altijd aan)
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif
#ifndef Serial_println
#define Serial_println Serial.println
#endif
extern float calculateReturn1Minute();
extern float calculateReturn5Minutes();
extern float calculateReturn30Minutes();
extern bool sendNotification(const char* title, const char* message, const char* colorTag);

// Macro's voor backward compatibility (verwijzen naar structs)
// Deze moeten NA de extern declaraties komen
#define spike1mThreshold alertThresholds.spike1m
#define spike5mThreshold alertThresholds.spike5m
#define move30mThreshold alertThresholds.move30m
#define move5mThreshold alertThresholds.move5m
#define move5mAlertThreshold alertThresholds.move5mAlert
#define notificationCooldown1MinMs notificationCooldowns.cooldown1MinMs
#define notificationCooldown30MinMs notificationCooldowns.cooldown30MinMs
#define notificationCooldown5MinMs notificationCooldowns.cooldown5MinMs

// TrendDetector en VolatilityTracker includes
#include "../TrendDetector/TrendDetector.h"
#include "../VolatilityTracker/VolatilityTracker.h"
#include "../AnchorSystem/AnchorSystem.h"

// WebServerModule implementation
// Fase 9: Web Interface Module refactoring

WebServerModule::WebServerModule() {
    // Initialiseer server pointer naar nullptr (wordt later ingesteld)
    server = nullptr;
}

void WebServerModule::begin() {
    // Fase 9.1.2: Basis initialisatie
    // Server pointer wordt ingesteld in setupWebServer()
}

// Fase 9.1.2: setupWebServer() verplaatst naar WebServerModule
void WebServerModule::setupWebServer() {
    // Gebruik globale server instance
    server = &::server;
    
    Serial.println(F("[WebServer] Routes registreren..."));
    server->on("/", [this]() { this->handleRoot(); });
    Serial.println("[WebServer] Route '/' geregistreerd");
    server->on("/save", HTTP_POST, [this]() { this->handleSave(); });
    Serial.println(F("[WebServer] Route '/save' geregistreerd"));
    server->on("/anchor/set", HTTP_POST, [this]() { this->handleAnchorSet(); });
    Serial.println("[WebServer] Route '/anchor/set' geregistreerd");
    server->on("/ntfy/reset", HTTP_POST, [this]() { this->handleNtfyReset(); });
    Serial.println(F("[WebServer] Route '/ntfy/reset' geregistreerd"));
    server->onNotFound([this]() { this->handleNotFound(); }); // 404 handler
    Serial.println(F("[WebServer] 404 handler geregistreerd"));
    server->begin();
    Serial.println("[WebServer] Server gestart");
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char ipBuffer[16];
    formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
    Serial.printf("[WebServer] Gestart op http://%s\n", ipBuffer);
}

// Fase 9.1.4: handleClient() voor webTask
void WebServerModule::handleClient() {
    if (server != nullptr) {
        server->handleClient();
    }
}

// Fase 9.1.3: renderSettingsHTML() verplaatst vanuit .ino
// Refactored: chunked HTML rendering (geen grote String in heap)
void WebServerModule::renderSettingsHTML() {
    if (server == nullptr) return;
    
    // Haal huidige status op (thread-safe)
    float currentPrice = 0.0f;
    float currentRet1m = 0.0f;
    float currentRet5m = 0.0f;
    float currentRet30m = 0.0f;
    TrendState currentTrend = TREND_SIDEWAYS;
    VolatilityState currentVol = VOLATILITY_MEDIUM;
    bool currentAnchorActive = false;
    float currentAnchorPrice = 0.0f;
    float currentAnchorPct = 0.0f;
    
    // Haal alle status data op binnen één mutex lock
    if (safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "getSettingsHTML status")) {
        if (isValidPrice(prices[0])) {
            currentPrice = prices[0];
        }
        // Fase 5.3.14: Gebruik TrendDetector module getter i.p.v. globale variabele
        currentTrend = trendDetector.getTrendState();
        // Fase 5.3.13: Gebruik VolatilityTracker module getter i.p.v. globale variabele
        currentVol = volatilityTracker.getVolatilityState();
        currentAnchorActive = anchorActive;
        if (anchorActive && isValidPrice(anchorPrice)) {
            currentAnchorPrice = anchorPrice;
            if (isValidPrice(prices[0]) && anchorPrice > 0.0f) {
                currentAnchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
            }
        }
        // Bereken returns binnen dezelfde mutex lock
        currentRet1m = calculateReturn1Minute();
        currentRet5m = calculateReturn5Minutes();
        currentRet30m = calculateReturn30Minutes();
        safeMutexGive(dataMutex, "getSettingsHTML status");
    }
    
    // Bepaal platform naam
    const char* platformName = "";
    #ifdef PLATFORM_TTGO
        platformName = "TTGO";
    #elif defined(PLATFORM_CYD24)
        platformName = "CYD24";
    #elif defined(PLATFORM_CYD28)
        platformName = "CYD28";
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        platformName = "ESP32-S3";
    #else
        platformName = "Unknown";
    #endif
    
    // Start chunked output
    sendHtmlHeader(platformName, ntfyTopic);
    
    // Temporary buffer voor variabele waarden
    char tmpBuf[256];
    char valueBuf[64];
    
    // Form start (voor anchor instellen)
    server->sendContent(F("<form method='POST' action='/save'>"));
    
    // Anchor instellen - helemaal bovenaan
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", 
             (currentPrice > 0.0f) ? currentPrice : 
             ((currentAnchorActive && currentAnchorPrice > 0.0f) ? currentAnchorPrice : 0.0f));
    
    server->sendContent(F("<div style='background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:15px 0;'>"));
    snprintf(tmpBuf, sizeof(tmpBuf), "<label style='display:block;margin-top:0;margin-bottom:8px;color:#fff;font-weight:bold;'>%s (EUR):</label>", 
             getText("Referentieprijs (Anchor)", "Reference price (Anchor)"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<input type='number' step='0.01' id='anchorValue' value='%s' min='0.01' lang='en' style='width:100%%;padding:8px;margin-bottom:10px;border:1px solid #444;background:#1a1a1a;color:#fff;border-radius:4px;box-sizing:border-box;'>",
             valueBuf);
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<button type='button' id='anchorBtn' style='width:100%%;background:#4CAF50;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;font-weight:bold;'>%s</button>",
             getText("Stel Anchor in", "Set Anchor"));
    server->sendContent(tmpBuf);
    server->sendContent(F("</div>"));
    
    // Status box
    server->sendContent(F("<div class='status-box'>"));
    if (currentPrice > 0.0f) {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f EUR", currentPrice);
        sendStatusRow(getText("Huidige Prijs", "Current Price"), valueBuf);
    } else {
        sendStatusRow(getText("Huidige Prijs", "Current Price"), getText("--", "--"));
    }
    
    const char* trendText = "";
    switch (currentTrend) {
        case TREND_UP: trendText = getText("OMHOOG", "UP"); break;
        case TREND_DOWN: trendText = getText("OMLAAG", "DOWN"); break;
        case TREND_SIDEWAYS: default: trendText = getText("VLAK", "SIDEWAYS"); break;
    }
    sendStatusRow(getText("Trend", "Trend"), trendText);
    
    const char* volText = "";
    switch (currentVol) {
        case VOLATILITY_LOW: volText = getText("Laag", "Low"); break;
        case VOLATILITY_MEDIUM: volText = getText("Gemiddeld", "Medium"); break;
        case VOLATILITY_HIGH: volText = getText("Hoog", "High"); break;
    }
    sendStatusRow(getText("Volatiliteit", "Volatility"), volText);
    
    if (currentRet1m != 0.0f) {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f%%", currentRet1m);
        sendStatusRow(getText("1m Return", "1m Return"), valueBuf);
    }
    if (currentRet30m != 0.0f) {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f%%", currentRet30m);
        sendStatusRow(getText("30m Return", "30m Return"), valueBuf);
    }
    
    if (currentAnchorActive && currentAnchorPrice > 0.0f) {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f EUR", currentAnchorPrice);
        sendStatusRow(getText("Anchor", "Anchor"), valueBuf);
        if (currentAnchorPct != 0.0f) {
            snprintf(valueBuf, sizeof(valueBuf), "%.2f%%", currentAnchorPct);
            sendStatusRow(getText("Anchor Delta", "Anchor Delta"), valueBuf);
        }
    }
    server->sendContent(F("</div>"));
    
    // Basis & Connectiviteit sectie
    sendSectionHeader(getText("Basis & Connectiviteit", "Basic & Connectivity"), "basic", true);
    sendSectionDesc(getText("Basisinstellingen voor symbol, notificaties en connectiviteit", "Basic settings for symbol, notifications and connectivity"));
    
    // NTFY Topic met reset knop (onder input veld, net als anchor)
    snprintf(tmpBuf, sizeof(tmpBuf), "<label>%s:", 
             getText("NTFY Topic", "NTFY Topic"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<input type='text' name='ntfytopic' value='%s' maxlength='63' style='width:100%%;padding:8px;margin-bottom:10px;border:1px solid #444;background:#1a1a1a;color:#fff;border-radius:4px;box-sizing:border-box;'>", ntfyTopic);
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<button type='button' id='ntfyResetBtn' style='width:100%%;background:#2196F3;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;font-weight:bold;'>%s</button>", 
             getText("Standaard uniek NTFY-topic", "Default unique NTFY topic"));
    server->sendContent(tmpBuf);
    server->sendContent(F("</label>"));
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='info'>%s</div>", 
             getText("NTFY.sh topic voor notificaties", "NTFY.sh topic for notifications"));
    server->sendContent(tmpBuf);
    sendInputRow(getText("Binance Symbol", "Binance Symbol"), "binancesymbol", "text", binanceSymbol, 
                 getText("Bijv. BTCEUR, ETHBTC", "E.g. BTCEUR, ETHBTC"));
    sendInputRow(getText("Taal", "Language"), "language", "number", (language == 0) ? "0" : "1", 
                 getText("0 = Nederlands, 1 = English", "0 = Dutch, 1 = English"), 0, 1, 1);
    
    sendSectionFooter();
    
    // Anchor & Risicokader sectie
    sendSectionHeader(getText("Anchor & Risicokader", "Anchor & Risk Framework"), "anchor", true);
    sendSectionDesc(getText("Anchor prijs instellingen en risicobeheer", "Anchor price settings and risk management"));
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", anchorTakeProfit);
    sendInputRow(getText("Take Profit", "Take Profit"), "anchorTP", "number", 
                 valueBuf, getText("Take profit percentage boven anchor", "Take profit percentage above anchor"), 
                 0.1f, 100.0f, 0.1f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", anchorMaxLoss);
    sendInputRow(getText("Max Loss", "Max Loss"), "anchorML", "number", 
                 valueBuf, getText("Max loss percentage onder anchor (negatief)", "Max loss percentage below anchor (negative)"), 
                 -100.0f, -0.1f, 0.1f);
    
    sendSectionFooter();
    
    // Signaalgeneratie sectie
    sendSectionHeader(getText("Signaalgeneratie", "Signal Generation"), "signals", false);
    sendSectionDesc(getText("Thresholds voor spike en move detectie", "Thresholds for spike and move detection"));
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", spike1mThreshold);
    sendInputRow(getText("1m Spike Threshold", "1m Spike Threshold"), "spike1m", "number", 
                 valueBuf, getText("Minimum 1m return voor spike alert", "Minimum 1m return for spike alert"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", spike5mThreshold);
    sendInputRow(getText("5m Spike Threshold", "5m Spike Threshold"), "spike5m", "number", 
                 valueBuf, getText("Minimum 5m return voor spike confirmatie", "Minimum 5m return for spike confirmation"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", move5mAlertThreshold);
    sendInputRow(getText("5m Move Threshold", "5m Move Threshold"), "move5mAlert", "number", 
                 valueBuf, getText("Minimum 5m return voor move alert", "Minimum 5m return for move alert"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", move5mThreshold);
    sendInputRow(getText("5m Move Filter", "5m Move Filter"), "move5m", "number", 
                 valueBuf, getText("Minimum 5m return voor 30m move confirmatie", "Minimum 5m return for 30m move confirmation"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", move30mThreshold);
    sendInputRow(getText("30m Move Threshold", "30m Move Threshold"), "move30m", "number", 
                 valueBuf, getText("Minimum 30m return voor move alert", "Minimum 30m return for move alert"), 
                 0.01f, 20.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", trendThreshold);
    sendInputRow(getText("Trend Threshold", "Trend Threshold"), "trendTh", "number", 
                 valueBuf, getText("Minimum 2h return voor trend detectie", "Minimum 2h return for trend detection"), 
                 0.1f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.4f", volatilityLowThreshold);
    sendInputRow(getText("Volatiliteit Laag", "Volatility Low"), "volLow", "number", 
                 valueBuf, getText("Threshold voor lage volatiliteit", "Threshold for low volatility"), 
                 0.01f, 1.0f, 0.0001f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.4f", volatilityHighThreshold);
    sendInputRow(getText("Volatiliteit Hoog", "Volatility High"), "volHigh", "number", 
                 valueBuf, getText("Threshold voor hoge volatiliteit", "Threshold for high volatility"), 
                 0.01f, 1.0f, 0.0001f);
    
    sendSectionFooter();
    
    // Slimme logica & filters sectie
    sendSectionHeader(getText("Slimme logica & filters", "Smart Logic & Filters"), "smart", false);
    sendSectionDesc(getText("Trend-adaptive anchors, Confluence Mode en Auto-Volatility", "Trend-adaptive anchors, Confluence Mode and Auto-Volatility"));
    
    sendCheckboxRow(getText("Trend-Adaptive Anchors", "Trend-Adaptive Anchors"), "trendAdapt", trendAdaptiveAnchorsEnabled);
    
    if (trendAdaptiveAnchorsEnabled) {
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", uptrendMaxLossMultiplier);
        sendInputRow(getText("UP Trend Max Loss Multiplier", "UP Trend Max Loss Multiplier"), "upMLMult", "number", 
                     valueBuf, getText("Multiplier voor max loss bij UP trend", "Multiplier for max loss in UP trend"), 
                     0.5f, 2.0f, 0.01f);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", uptrendTakeProfitMultiplier);
        sendInputRow(getText("UP Trend Take Profit Multiplier", "UP Trend Take Profit Multiplier"), "upTPMult", "number", 
                     valueBuf, getText("Multiplier voor take profit bij UP trend", "Multiplier for take profit in UP trend"), 
                     0.5f, 2.0f, 0.01f);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", downtrendMaxLossMultiplier);
        sendInputRow(getText("DOWN Trend Max Loss Multiplier", "DOWN Trend Max Loss Multiplier"), "downMLMult", "number", 
                     valueBuf, getText("Multiplier voor max loss bij DOWN trend", "Multiplier for max loss in DOWN trend"), 
                     0.5f, 2.0f, 0.01f);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", downtrendTakeProfitMultiplier);
        sendInputRow(getText("DOWN Trend Take Profit Multiplier", "DOWN Trend Take Profit Multiplier"), "downTPMult", "number", 
                     valueBuf, getText("Multiplier voor take profit bij DOWN trend", "Multiplier for take profit in DOWN trend"), 
                     0.5f, 2.0f, 0.01f);
    }
    
    sendCheckboxRow(getText("Smart Confluence Mode", "Smart Confluence Mode"), "smartConf", smartConfluenceEnabled);
    
    sendCheckboxRow(getText("Auto-Volatility Mode", "Auto-Volatility Mode"), "autoVol", autoVolatilityEnabled);
    
    if (autoVolatilityEnabled) {
        snprintf(valueBuf, sizeof(valueBuf), "%u", autoVolatilityWindowMinutes);
        sendInputRow(getText("Volatility Window (min)", "Volatility Window (min)"), "autoVolWin", "number", 
                     valueBuf, getText("Aantal minuten voor volatiliteit berekening", "Number of minutes for volatility calculation"), 
                     10, 120, 1);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.4f", autoVolatilityBaseline1mStdPct);
        sendInputRow(getText("Baseline σ (1m)", "Baseline σ (1m)"), "autoVolBase", "number", 
                     valueBuf, getText("Baseline standaarddeviatie voor 1m returns", "Baseline standard deviation for 1m returns"), 
                     0.01f, 1.0f, 0.0001f);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", autoVolatilityMinMultiplier);
        sendInputRow(getText("Min Multiplier", "Min Multiplier"), "autoVolMin", "number", 
                     valueBuf, getText("Minimum volatility multiplier", "Minimum volatility multiplier"), 
                     0.1f, 1.0f, 0.01f);
        
        snprintf(valueBuf, sizeof(valueBuf), "%.2f", autoVolatilityMaxMultiplier);
        sendInputRow(getText("Max Multiplier", "Max Multiplier"), "autoVolMax", "number", 
                     valueBuf, getText("Maximum volatility multiplier", "Maximum volatility multiplier"), 
                     1.0f, 3.0f, 0.01f);
    }
    
    sendSectionFooter();
    
    // Cooldowns sectie
    sendSectionHeader(getText("Cooldowns", "Cooldowns"), "cooldowns", false);
    sendSectionDesc(getText("Tijdsintervallen tussen alerts", "Time intervals between alerts"));
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", notificationCooldown1MinMs / 1000UL);
    sendInputRow(getText("1m Cooldown (sec)", "1m Cooldown (sec)"), "cd1min", "number", 
                 valueBuf, getText("Cooldown tussen 1m spike alerts in seconden", "Cooldown between 1m spike alerts in seconds"), 
                 1, 3600, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", notificationCooldown5MinMs / 1000UL);
    sendInputRow(getText("5m Cooldown (sec)", "5m Cooldown (sec)"), "cd5min", "number", 
                 valueBuf, getText("Cooldown tussen 5m move alerts in seconden", "Cooldown between 5m move alerts in seconds"), 
                 1, 3600, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", notificationCooldown30MinMs / 1000UL);
    sendInputRow(getText("30m Cooldown (sec)", "30m Cooldown (sec)"), "cd30min", "number", 
                 valueBuf, getText("Cooldown tussen 30m move alerts in seconden", "Cooldown between 30m move alerts in seconds"), 
                 1, 3600, 1);
    
    sendSectionFooter();
    
    // Warm-Start sectie
    sendSectionHeader(getText("Warm-Start", "Warm-Start"), "warmstart", false);
    sendSectionDesc(getText("Binance historische data voor snelle initialisatie", "Binance historical data for fast initialization"));
    
    sendCheckboxRow(getText("Warm-Start Ingeschakeld", "Warm-Start Enabled"), "warmStart", warmStartEnabled);
    
    if (warmStartEnabled) {
        snprintf(valueBuf, sizeof(valueBuf), "%u", warmStart1mExtraCandles);
        sendInputRow(getText("1m Extra Candles", "1m Extra Candles"), "ws1mExtra", "number", 
                     valueBuf, getText("Extra 1m candles bovenop volatility window", "Extra 1m candles on top of volatility window"), 
                     0, 100, 1);
        
        snprintf(valueBuf, sizeof(valueBuf), "%u", warmStart5mCandles);
        sendInputRow(getText("5m Candles", "5m Candles"), "ws5m", "number", 
                     valueBuf, getText("Aantal 5m candles", "Number of 5m candles"), 
                     2, 200, 1);
        
        snprintf(valueBuf, sizeof(valueBuf), "%u", warmStart30mCandles);
        sendInputRow(getText("30m Candles", "30m Candles"), "ws30m", "number", 
                     valueBuf, getText("Aantal 30m candles", "Number of 30m candles"), 
                     2, 200, 1);
        
        snprintf(valueBuf, sizeof(valueBuf), "%u", warmStart2hCandles);
        sendInputRow(getText("2h Candles", "2h Candles"), "ws2h", "number", 
                     valueBuf, getText("Aantal 2h candles", "Number of 2h candles"), 
                     2, 200, 1);
    }
    
    sendSectionFooter();
    
    // MQTT sectie
    sendSectionHeader(getText("Integratie", "Integration"), "mqtt", false);
    sendSectionDesc(getText("MQTT instellingen voor Home Assistant", "MQTT settings for Home Assistant"));
    
    sendInputRow(getText("MQTT Host", "MQTT Host"), "mqtthost", "text", mqttHost, 
                 getText("MQTT broker hostname of IP", "MQTT broker hostname or IP"));
    
    snprintf(valueBuf, sizeof(valueBuf), "%u", mqttPort);
    sendInputRow(getText("MQTT Port", "MQTT Port"), "mqttport", "number", 
                 valueBuf, getText("MQTT broker poort", "MQTT broker port"), 
                 1, 65535, 1);
    
    sendInputRow(getText("MQTT User", "MQTT User"), "mqttuser", "text", mqttUser, 
                 getText("MQTT gebruikersnaam (optioneel)", "MQTT username (optional)"));
    
    sendInputRow(getText("MQTT Password", "MQTT Password"), "mqttpass", "text", mqttPass, 
                 getText("MQTT wachtwoord (optioneel)", "MQTT password (optional)"));
    
    sendSectionFooter();
    
    // Submit button
    snprintf(tmpBuf, sizeof(tmpBuf), "<button type='submit'>%s</button>", 
             getText("Opslaan", "Save"));
    server->sendContent(tmpBuf);
    
    server->sendContent(F("</form>"));
    
    // Footer
    sendHtmlFooter();
}

// Fase 9.1.4: Web handlers verplaatst vanuit .ino
void WebServerModule::handleRoot() {
    if (server == nullptr) {
        return;
    }
    
    // M1: Rate-limited heap telemetry in web server (alleen bij "/")
    logHeap("WEB_ROOT");
    
    renderSettingsHTML();
}

void WebServerModule::handleSave() {
    if (server == nullptr) {
        return;
    }
    
    // M1: Rate-limited heap telemetry in web server (alleen bij "/save")
    logHeap("WEB_SAVE");
    
    // Handle language setting
    if (server->hasArg("language")) {
        uint8_t newLanguage = server->arg("language").toInt();
        if (newLanguage == 0 || newLanguage == 1) {
            language = newLanguage;
        }
    }
    
    if (server->hasArg("ntfytopic")) {
        String topic = server->arg("ntfytopic");
        topic.trim();
        // Allow empty topic (will use default) or valid length topic
        if (topic.length() == 0 || (topic.length() > 0 && topic.length() < 64)) {
            if (topic.length() == 0) {
                // Generate default topic if empty
                generateDefaultNtfyTopic(ntfyTopic, 64);
            } else {
                topic.toCharArray(ntfyTopic, 64);
            }
        }
    }
    if (server->hasArg("binancesymbol")) {
        String symbol = server->arg("binancesymbol");
        symbol.trim();
        symbol.toUpperCase(); // Binance symbolen zijn altijd uppercase
        if (symbol.length() > 0 && symbol.length() < 16) {  // binanceSymbol is 16 bytes
            symbol.toCharArray(binanceSymbol, 16);
            // Update symbols array
            safeStrncpy(symbolsArray[0], binanceSymbol, 16);  // symbolsArray[0] is 16 bytes
        }
    }
    if (server->hasArg("spike1m")) {
        float val;
        if (safeAtof(server->arg("spike1m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            spike1mThreshold = val;
        }
    }
    if (server->hasArg("spike5m")) {
        float val;
        if (safeAtof(server->arg("spike5m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            spike5mThreshold = val;
        }
    }
    if (server->hasArg("move30m")) {
        float val;
        if (safeAtof(server->arg("move30m").c_str(), val) && val >= 0.01f && val <= 20.0f) {
            move30mThreshold = val;
        }
    }
    if (server->hasArg("move5m")) {
        float val;
        if (safeAtof(server->arg("move5m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            move5mThreshold = val;
        }
    }
    if (server->hasArg("move5mAlert")) {
        float val;
        if (safeAtof(server->arg("move5mAlert").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            move5mAlertThreshold = val;
        }
    }
    if (server->hasArg("cd1min")) {
        int seconds = server->arg("cd1min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown1MinMs = resultMs;
        }
    }
    if (server->hasArg("cd30min")) {
        int seconds = server->arg("cd30min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown30MinMs = resultMs;
        }
    }
    if (server->hasArg("cd5min")) {
        int seconds = server->arg("cd5min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown5MinMs = resultMs;
        }
    }
    
    // MQTT settings
    if (server->hasArg("mqtthost")) {
        String host = server->arg("mqtthost");
        host.trim();
        if (host.length() > 0 && host.length() < 64) {  // mqttHost is 64 bytes
            host.toCharArray(mqttHost, 64);
        }
    }
    if (server->hasArg("mqttport")) {
        uint16_t port = server->arg("mqttport").toInt();
        if (port > 0 && port <= 65535) {
            mqttPort = port;
        }
    }
    if (server->hasArg("mqttuser")) {
        String user = server->arg("mqttuser");
        user.trim();
        if (user.length() > 0 && user.length() < 64) {  // mqttUser is 64 bytes
            user.toCharArray(mqttUser, 64);
        }
    }
    if (server->hasArg("mqttpass")) {
        String pass = server->arg("mqttpass");
        pass.trim();
        if (pass.length() > 0 && pass.length() < 64) {  // mqttPass is 64 bytes
            pass.toCharArray(mqttPass, 64);
        }
    }
    
    // Trend and volatility settings
    if (server->hasArg("trendTh")) {
        float val;
        if (safeAtof(server->arg("trendTh").c_str(), val) && val >= 0.1f && val <= 10.0f) {
            trendThreshold = val;
        }
    }
    if (server->hasArg("volLow")) {
        float val;
        if (safeAtof(server->arg("volLow").c_str(), val) && val >= 0.01f && val <= 1.0f) {
            volatilityLowThreshold = val;
        }
    }
    if (server->hasArg("volHigh")) {
        float val;
        if (safeAtof(server->arg("volHigh").c_str(), val) && val >= 0.01f && val <= 1.0f && val > volatilityLowThreshold) {
            volatilityHighThreshold = val;
        }
    }
    
    // Anchor settings - NIET vanuit web server thread verwerken om crashes te voorkomen
    // Anchor setting wordt verwerkt via een aparte route /anchor/set die sneller is
    if (server->hasArg("anchorTP")) {
        float val;
        if (safeAtof(server->arg("anchorTP").c_str(), val) && val >= 0.1f && val <= 100.0f) {
            anchorTakeProfit = val;
        }
    }
    if (server->hasArg("anchorML")) {
        float val;
        if (safeAtof(server->arg("anchorML").c_str(), val) && val >= -100.0f && val <= -0.1f) {
            anchorMaxLoss = val;
        }
    }
    
    // Trend-adaptive anchor settings
    trendAdaptiveAnchorsEnabled = server->hasArg("trendAdapt");
    if (server->hasArg("upMLMult")) {
        float val;
        if (safeAtof(server->arg("upMLMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            uptrendMaxLossMultiplier = val;
        }
    }
    if (server->hasArg("upTPMult")) {
        float val;
        if (safeAtof(server->arg("upTPMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            uptrendTakeProfitMultiplier = val;
        }
    }
    if (server->hasArg("downMLMult")) {
        float val;
        if (safeAtof(server->arg("downMLMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            downtrendMaxLossMultiplier = val;
        }
    }
    if (server->hasArg("downTPMult")) {
        float val;
        if (safeAtof(server->arg("downTPMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            downtrendTakeProfitMultiplier = val;
        }
    }
    
    // Smart Confluence Mode settings
    smartConfluenceEnabled = server->hasArg("smartConf");
    
    // Warm-Start settings
    warmStartEnabled = server->hasArg("warmStart");
    if (server->hasArg("ws1mExtra")) {
        uint8_t val = server->arg("ws1mExtra").toInt();
        if (val >= 0 && val <= 100) {
            warmStart1mExtraCandles = val;
        }
    }
    if (server->hasArg("ws5m")) {
        uint8_t val = server->arg("ws5m").toInt();
        if (val >= 2 && val <= 200) {
            warmStart5mCandles = val;
        }
    }
    if (server->hasArg("ws30m")) {
        uint8_t val = server->arg("ws30m").toInt();
        if (val >= 2 && val <= 200) {
            warmStart30mCandles = val;
        }
    }
    if (server->hasArg("ws2h")) {
        uint8_t val = server->arg("ws2h").toInt();
        if (val >= 2 && val <= 200) {
            warmStart2hCandles = val;
        }
    }
    
    // Auto-Volatility Mode settings
    autoVolatilityEnabled = server->hasArg("autoVol");
    if (server->hasArg("autoVolWin")) {
        uint8_t val = server->arg("autoVolWin").toInt();
        if (val >= 10 && val <= 120) {
            autoVolatilityWindowMinutes = val;
        }
    }
    if (server->hasArg("autoVolBase")) {
        float val;
        if (safeAtof(server->arg("autoVolBase").c_str(), val) && val >= 0.01f && val <= 1.0f) {
            autoVolatilityBaseline1mStdPct = val;
        }
    }
    if (server->hasArg("autoVolMin")) {
        float val;
        if (safeAtof(server->arg("autoVolMin").c_str(), val) && val >= 0.1f && val <= 1.0f) {
            autoVolatilityMinMultiplier = val;
        }
    }
    if (server->hasArg("autoVolMax")) {
        float val;
        if (safeAtof(server->arg("autoVolMax").c_str(), val) && val >= 1.0f && val <= 3.0f) {
            autoVolatilityMaxMultiplier = val;
        }
    }
    
    saveSettings();
    
    // Herconnect MQTT als instellingen zijn gewijzigd
    if (mqttConnected) {
        mqttClient.disconnect();
        mqttConnected = false;
        lastMqttReconnectAttempt = 0;
        mqttReconnectAttemptCount = 0; // Reset counter bij disconnect
    }
    
    // Chunked HTML output (geen String in heap)
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html; charset=utf-8", "");
    
    server->sendContent(F("<!DOCTYPE html><html><head>"));
    server->sendContent(F("<meta http-equiv='refresh' content='2;url=/'><meta charset='UTF-8'>"));
    server->sendContent(F("<title>Opgeslagen</title>"));
    server->sendContent(F("<style>"));
    server->sendContent(F("body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff;text-align:center;}"));
    server->sendContent(F("h1{color:#4CAF50;}"));
    server->sendContent(F("</style></head><body>"));
    
    // Dynamische tekst via kleine buffer
    char tmpBuf[128];
    snprintf(tmpBuf, sizeof(tmpBuf), "<h1>%s</h1>", getText("Instellingen opgeslagen!", "Settings saved!"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<p>%s</p>", getText("Terug naar instellingen...", "Returning to settings..."));
    server->sendContent(tmpBuf);
    
    server->sendContent(F("</body></html>"));
}

void WebServerModule::handleNotFound() {
    if (server == nullptr) {
        return;
    }
    
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server->uri();
    message += "\nMethod: ";
    message += (server->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server->args();
    message += "\n";
    for (uint8_t i = 0; i < server->args(); i++) {
        message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
    }
    server->send(404, "text/plain", message);
    Serial_printf(F("[WebServer] 404: %s\n"), server->uri().c_str());
}

// Handler voor anchor set (aparte route om crashes te voorkomen)
// Gebruikt een queue om asynchroon te verwerken vanuit main loop
// Thread-safe: schrijft naar volatile variabelen die worden gelezen vanuit uiTask
// C1: Network-safe anchor-set handler (geen HTTPS in web thread)
// Zet alleen flags, verwerking gebeurt in apiTask waar HTTPS calls al zijn
void WebServerModule::handleAnchorSet() {
    if (server == nullptr) {
        return;  // Server niet geïnitialiseerd
    }
    
    // Zorg ervoor dat er altijd een response wordt gestuurd
    bool responseSent = false;
    
    if (server->hasArg("value")) {
        // Gebruik char array i.p.v. String om heap fragmentatie te voorkomen
        char valueBuffer[32] = {0};
        String anchorValueStr = server->arg("value");
        
        // Kopieer naar buffer (met lengte check)
        size_t len = anchorValueStr.length();
        if (len > 0 && len < sizeof(valueBuffer)) {
            strncpy(valueBuffer, anchorValueStr.c_str(), sizeof(valueBuffer) - 1);
            valueBuffer[sizeof(valueBuffer) - 1] = '\0';
        }
        
        // Trim whitespace
        char* start = valueBuffer;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        bool valid = false;
        
        if (strlen(start) > 0) {
            float val;
            if (safeAtof(start, val) && val > 0.0f && isValidPrice(val)) {
                // Valide waarde - zet in queue voor asynchrone verwerking in apiTask
                valid = queueAnchorSetting(val, false);
                if (valid) {
                    Serial_printf("[Web] Anchor setting queued: %.2f\n", val);
                }
            } else {
                Serial_printf(F("[Web] WARN: Ongeldige anchor waarde opgegeven: '%s'\n"), start);
            }
        } else {
            // Leeg veld = gebruik huidige prijs (wordt opgehaald in apiTask)
            valid = queueAnchorSetting(0.0f, true);
            if (valid) {
                Serial_println(F("[Web] Anchor setting queued: gebruik huidige prijs"));
            }
        }
        
        // C1: Return HTTP 200 direct (geen Binance fetch hier, gebeurt in apiTask)
        if (valid) {
            server->send(200, "text/plain", "OK");
            responseSent = true;
        } else {
            server->send(400, "text/plain", "ERROR: Invalid anchor value");
            responseSent = true;
        }
    } else {
        server->send(400, "text/plain", "ERROR: Missing 'value' parameter");
        responseSent = true;
    }
    
    // Fallback: als er om wat voor reden dan ook geen response is gestuurd
    if (!responseSent) {
        server->send(500, "text/plain", "ERROR: Internal server error");
    }
}

void WebServerModule::handleNtfyReset() {
    if (server == nullptr) return;
    
    // Genereer standaard topic en sla op
    generateDefaultNtfyTopic(ntfyTopic, 64);  // ntfyTopic is 64 bytes groot
    
    // Sla op via SettingsStore
    CryptoMonitorSettings settings = settingsStore.load();
    safeStrncpy(settings.ntfyTopic, ntfyTopic, sizeof(settings.ntfyTopic));
    settingsStore.save(settings);
    
    Serial_printf(F("[Web] NTFY topic gereset naar standaard: %s\n"), ntfyTopic);
    
    // Stuur succes response
    server->send(200, "text/plain", "OK");
}

// Fase 9.1.3: HTML helper functies verplaatst vanuit .ino
void WebServerModule::sendHtmlHeader(const char* platformName, const char* ntfyTopic) {
    if (server == nullptr) return;
    
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html; charset=utf-8", "");
    
    // HTML doctype en head (lang='en' om punt als decimaal scheidingsteken te forceren)
    server->sendContent(F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    
    // Title
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), "<title>%s %s %s</title>", 
             getText("Instellingen", "Settings"), platformName, ntfyTopic);
    server->sendContent(titleBuf);
    
    // CSS
    server->sendContent(F("<style>"));
    server->sendContent(F("*{box-sizing:border-box;}"));
    server->sendContent(F("body{font-family:Arial;margin:0;padding:10px;background:#1a1a1a;color:#fff;}"));
    server->sendContent(F(".container{max-width:600px;margin:0 auto;padding:0 10px;}"));
    server->sendContent(F("h1{color:#00BCD4;margin:15px 0;font-size:24px;}"));
    server->sendContent(F("form{max-width:100%;}"));
    server->sendContent(F("label{display:block;margin:15px 0 5px;color:#ccc;}"));
    server->sendContent(F("input[type=number],input[type=text],select{width:100%;padding:8px;border:1px solid #444;background:#2a2a2a;color:#fff;border-radius:4px;box-sizing:border-box;}"));
    server->sendContent(F("button{background:#00BCD4;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;width:100%;}"));
    server->sendContent(F("button:hover{background:#00acc1;}"));
    server->sendContent(F(".info{color:#888;font-size:12px;margin-top:5px;}"));
    server->sendContent(F(".status-box{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:20px 0;max-width:100%;}"));
    server->sendContent(F(".status-row{display:flex;justify-content:space-between;margin:8px 0;padding:8px 0;border-bottom:1px solid #333;flex-wrap:wrap;}"));
    server->sendContent(F(".status-label{color:#888;flex:1;min-width:120px;}"));
    server->sendContent(F(".status-value{color:#fff;font-weight:bold;text-align:right;flex:1;min-width:100px;}"));
    server->sendContent(F(".section-header{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:12px;margin:15px 0 0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"));
    server->sendContent(F(".section-header:hover{background:#333;}"));
    server->sendContent(F(".section-header h3{margin:0;color:#00BCD4;font-size:16px;}"));
    server->sendContent(F(".section-content{display:none;padding:15px;background:#1a1a1a;border:1px solid #444;border-top:none;border-radius:0 0 4px 4px;}"));
    server->sendContent(F(".section-content.active{display:block;}"));
    server->sendContent(F(".section-desc{color:#888;font-size:12px;margin-top:5px;margin-bottom:15px;}"));
    server->sendContent(F(".toggle-icon{color:#00BCD4;font-size:18px;flex-shrink:0;margin-left:10px;}"));
    server->sendContent(F("@media (max-width:600px){"));
    server->sendContent(F("body{padding:5px;}"));
    server->sendContent(F(".container{padding:0 5px;}"));
    server->sendContent(F("h1{font-size:20px;margin:10px 0;}"));
    server->sendContent(F(".status-box{padding:10px;margin:15px 0;}"));
    server->sendContent(F(".status-row{flex-direction:column;padding:6px 0;}"));
    server->sendContent(F(".status-label{min-width:auto;margin-bottom:3px;}"));
    server->sendContent(F(".status-value{text-align:left;min-width:auto;}"));
    server->sendContent(F(".section-header{padding:10px;}"));
    server->sendContent(F(".section-header h3{font-size:14px;}"));
    server->sendContent(F(".section-content{padding:10px;}"));
    server->sendContent(F("button{padding:10px 20px;font-size:14px;}"));
    server->sendContent(F("label{font-size:14px;}"));
    server->sendContent(F("input[type=number],input[type=text],select{font-size:14px;padding:6px;}"));
    server->sendContent(F("}"));
    server->sendContent(F("</style>"));
    
    // JavaScript
    server->sendContent(F("<script type='text/javascript'>"));
    server->sendContent(F("(function(){"));
    server->sendContent(F("function toggleSection(id){"));
    server->sendContent(F("var content=document.getElementById('content-'+id);"));
    server->sendContent(F("var icon=document.getElementById('icon-'+id);"));
    server->sendContent(F("if(!content||!icon)return false;"));
    server->sendContent(F("if(content.classList.contains('active')){"));
    server->sendContent(F("content.classList.remove('active');"));
    server->sendContent(F("icon.innerHTML='&#9654;';"));
    server->sendContent(F("}else{"));
    server->sendContent(F("content.classList.add('active');"));
    server->sendContent(F("icon.innerHTML='&#9660;';"));
    server->sendContent(F("}"));
    server->sendContent(F("return false;"));
    server->sendContent(F("}"));
    server->sendContent(F("function setAnchorBtn(e){"));
    server->sendContent(F("if(e){e.preventDefault();e.stopPropagation();}"));
    server->sendContent(F("var input=document.getElementById('anchorValue');"));
    server->sendContent(F("if(!input){alert('Input not found');return false;}"));
    server->sendContent(F("var val=input.value||'';"));
    server->sendContent(F("var xhr=new XMLHttpRequest();"));
    server->sendContent(F("xhr.open('POST','/anchor/set',true);"));
    server->sendContent(F("xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"));
    server->sendContent(F("xhr.onreadystatechange=function(){"));
    server->sendContent(F("if(xhr.readyState==4){"));
    server->sendContent(F("if(xhr.status==200){"));
    
    char alertBuf[128];
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Anchor ingesteld!", "Anchor set!"));
    server->sendContent(alertBuf);
    
    server->sendContent(F("setTimeout(function(){location.reload();},500);"));
    server->sendContent(F("}else{"));
    
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Fout bij instellen anchor", "Error setting anchor"));
    server->sendContent(alertBuf);
    
    server->sendContent(F("}"));
    server->sendContent(F("}"));
    server->sendContent(F("};"));
    server->sendContent(F("xhr.send('value='+encodeURIComponent(val));"));
    server->sendContent(F("return false;"));
    server->sendContent(F("}"));
    server->sendContent(F("function resetNtfyBtn(e){"));
    server->sendContent(F("if(e){e.preventDefault();e.stopPropagation();}"));
    server->sendContent(F("var xhr=new XMLHttpRequest();"));
    server->sendContent(F("xhr.open('POST','/ntfy/reset',true);"));
    server->sendContent(F("xhr.onreadystatechange=function(){"));
    server->sendContent(F("if(xhr.readyState==4){"));
    server->sendContent(F("if(xhr.status==200){"));
    server->sendContent(F("setTimeout(function(){location.reload();},500);"));
    server->sendContent(F("}else{"));
    
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Fout bij resetten NTFY topic", "Error resetting NTFY topic"));
    server->sendContent(alertBuf);
    
    server->sendContent(F("}"));
    server->sendContent(F("}"));
    server->sendContent(F("};"));
    server->sendContent(F("xhr.send();"));
    server->sendContent(F("return false;"));
    server->sendContent(F("}"));
    server->sendContent(F("window.addEventListener('DOMContentLoaded',function(){"));
    // Fix: converteer komma's naar punten in number inputs (locale fix)
    server->sendContent(F("var numberInputs=document.querySelectorAll('input[type=\"number\"]');"));
    server->sendContent(F("for(var i=0;i<numberInputs.length;i++){"));
    server->sendContent(F("numberInputs[i].addEventListener('input',function(e){"));
    server->sendContent(F("var val=this.value.replace(',','.');"));
    server->sendContent(F("if(val!==this.value){this.value=val;}});"));
    server->sendContent(F("numberInputs[i].addEventListener('blur',function(e){"));
    server->sendContent(F("var val=this.value.replace(',','.');"));
    server->sendContent(F("if(val!==this.value){this.value=val;}});"));
    server->sendContent(F("}"));
    server->sendContent(F("var headers=document.querySelectorAll('.section-header');"));
    server->sendContent(F("for(var i=0;i<headers.length;i++){"));
    server->sendContent(F("headers[i].addEventListener('click',function(e){"));
    server->sendContent(F("var id=this.getAttribute('data-section');"));
    server->sendContent(F("toggleSection(id);"));
    server->sendContent(F("e.preventDefault();"));
    server->sendContent(F("return false;"));
    server->sendContent(F("});"));
    server->sendContent(F("}"));
    server->sendContent(F("var basic=document.getElementById('icon-basic');"));
    server->sendContent(F("var anchor=document.getElementById('icon-anchor');"));
    server->sendContent(F("if(basic)basic.innerHTML='&#9660;';"));
    server->sendContent(F("if(anchor)anchor.innerHTML='&#9660;';"));
    server->sendContent(F("var anchorBtn=document.getElementById('anchorBtn');"));
    server->sendContent(F("if(anchorBtn){"));
    server->sendContent(F("anchorBtn.addEventListener('click',setAnchorBtn);"));
    server->sendContent(F("}"));
    server->sendContent(F("var ntfyResetBtn=document.getElementById('ntfyResetBtn');"));
    server->sendContent(F("if(ntfyResetBtn){"));
    server->sendContent(F("ntfyResetBtn.addEventListener('click',resetNtfyBtn);"));
    server->sendContent(F("}"));
    server->sendContent(F("});"));
    server->sendContent(F("})();"));
    server->sendContent(F("</script>"));
    server->sendContent(F("</head><body>"));
    server->sendContent(F("<div class='container'>"));
    
    // Title
    char h1Buf[128];
    snprintf(h1Buf, sizeof(h1Buf), "<h1>%s %s %s</h1>", 
             getText("Instellingen", "Settings"), platformName, ntfyTopic);
    server->sendContent(h1Buf);
}

void WebServerModule::sendHtmlFooter() {
    if (server == nullptr) return;
    server->sendContent(F("</div>"));
    server->sendContent(F("</body></html>"));
}

void WebServerModule::sendInputRow(const char* label, const char* name, const char* type, const char* value, 
                                   const char* info, float minVal, float maxVal, float step) {
    if (server == nullptr) return;
    
    char buf[256];
    if (strcmp(type, "number") == 0) {
        // Format step met juiste precisie (detecteer aantal decimalen)
        // Gebruik altijd punt als decimaal scheidingsteken (niet komma)
        // Vermijd floating point precisie problemen door expliciete string matching
        char stepStr[16];
        // Check voor veelvoorkomende step waarden eerst (exacte match)
        if (step == 1.0f) {
            strcpy(stepStr, "1");
        } else if (step == 0.1f) {
            strcpy(stepStr, "0.1");
        } else if (step == 0.01f) {
            strcpy(stepStr, "0.01");
        } else if (step == 0.001f) {
            strcpy(stepStr, "0.001");
        } else if (step == 0.0001f) {
            strcpy(stepStr, "0.0001");
        } else if (step >= 1.0f) {
            snprintf(stepStr, sizeof(stepStr), "%.0f", step);
        } else if (step >= 0.1f) {
            snprintf(stepStr, sizeof(stepStr), "%.1f", step);
        } else if (step >= 0.01f) {
            snprintf(stepStr, sizeof(stepStr), "%.2f", step);
        } else if (step >= 0.001f) {
            snprintf(stepStr, sizeof(stepStr), "%.3f", step);
        } else {
            snprintf(stepStr, sizeof(stepStr), "%.4f", step);
        }
        
        // Format min/max met juiste precisie
        char minStr[16], maxStr[16];
        if (minVal != 0 || maxVal != 0) {
            snprintf(minStr, sizeof(minStr), "%.2f", minVal);
            snprintf(maxStr, sizeof(maxStr), "%.2f", maxVal);
            snprintf(buf, sizeof(buf), "<label>%s:<input type='number' step='%s' name='%s' value='%s' min='%s' max='%s' lang='en' inputmode='decimal'></label>",
                     label, stepStr, name, value, minStr, maxStr);
        } else {
            snprintf(buf, sizeof(buf), "<label>%s:<input type='number' step='%s' name='%s' value='%s' lang='en' inputmode='decimal'></label>",
                     label, stepStr, name, value);
        }
    } else if (strcmp(type, "text") == 0) {
        snprintf(buf, sizeof(buf), "<label>%s:<input type='text' name='%s' value='%s' maxlength='63'></label>",
                 label, name, value);
    } else {
        snprintf(buf, sizeof(buf), "<label>%s:<input type='%s' name='%s' value='%s'></label>",
                 label, type, name, value);
    }
    server->sendContent(buf);
    if (info && strlen(info) > 0) {
        snprintf(buf, sizeof(buf), "<div class='info'>%s</div>", info);
        server->sendContent(buf);
    }
}

void WebServerModule::sendCheckboxRow(const char* label, const char* name, bool checked) {
    if (server == nullptr) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "<label><input type='checkbox' name='%s' value='1'%s> %s</label>",
             name, checked ? " checked" : "", label);
    server->sendContent(buf);
}

void WebServerModule::sendStatusRow(const char* label, const char* value) {
    if (server == nullptr) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value'>%s</span></div>",
             label, value);
    server->sendContent(buf);
}

void WebServerModule::sendSectionHeader(const char* title, const char* sectionId, bool expanded) {
    if (server == nullptr) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='section-header' data-section='%s'><h3>%s</h3><span class='toggle-icon' id='icon-%s'>%s</span></div>",
             sectionId, title, sectionId, expanded ? "&#9660;" : "&#9654;");
    server->sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='section-content%s' id='content-%s'>",
             expanded ? " active" : "", sectionId);
    server->sendContent(buf);
}

void WebServerModule::sendSectionFooter() {
    if (server == nullptr) return;
    server->sendContent(F("</div>"));
}

void WebServerModule::sendSectionDesc(const char* desc) {
    if (server == nullptr) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='section-desc'>%s</div>", desc);
    server->sendContent(buf);
}


