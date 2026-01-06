#include "WebServer.h"
#include <WebServer.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP.h>  // Voor ESP.restart()
#include <Arduino_GFX_Library.h>  // Voor Arduino_GFX type
#include "../SettingsStore/SettingsStore.h"
// Platform config voor platform naam detectie
#define MODULE_INCLUDE  // Flag om PINS includes te voorkomen
#include "../../platform_config.h"
#undef MODULE_INCLUDE

// Forward declarations voor dependencies
extern WebServer server;  // Globale WebServer instance (gedefinieerd in .ino)
extern TrendDetector trendDetector;
extern VolatilityTracker volatilityTracker;
extern AnchorSystem anchorSystem;
extern Arduino_GFX* gfx;  // Display object voor rotatie aanpassing

// Externe variabelen en functies die nodig zijn voor web server
extern SemaphoreHandle_t dataMutex;
extern float prices[];
extern bool anchorActive;
extern float anchorPrice;
extern char ntfyTopic[];
extern char binanceSymbol[];
extern uint8_t language;
extern uint8_t displayRotation;
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
extern uint8_t anchorStrategy;
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
extern void getMqttTopicPrefix(char* buffer, size_t bufferSize);  // Forward declaration
extern SettingsStore settingsStore;
extern AlertThresholds alertThresholds;
extern Alert2HThresholds alert2HThresholds;
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

// WEB-PERF-3: Externe variabelen voor /status endpoint
extern float ret_2h;  // 2-hour return (static in .ino, maar extern hier)
extern float ret_30m;  // 30-minute return (static in .ino, maar extern hier)
extern float ret_1d;  // 24-hour return (hourly buffer)
extern float averagePrices[];  // Array met gemiddelde prijzen (index 3 = avg2h)
extern bool hasRet2h;  // Flag: ret_2h beschikbaar
extern bool hasRet30m;  // Flag: ret_30m beschikbaar

// TwoHMetrics struct is gedefinieerd in AlertEngine.h, maar we declareren het hier voor gebruik
#include "../AlertEngine/AlertEngine.h"  // Voor TwoHMetrics struct
extern TwoHMetrics computeTwoHMetrics();

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

// WEB-PERF-3: Static cache variabelen
bool WebServerModule::sPageCacheValid = false;
String WebServerModule::sPageCache = "";

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
    server->on("/status", HTTP_GET, [this]() { this->handleStatus(); });  // WEB-PERF-3: Status endpoint
    Serial.println(F("[WebServer] Route '/status' geregistreerd"));
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
// Performance optimalisatie: debug logging voor ESP32-S3
void WebServerModule::renderSettingsHTML() {
    if (server == nullptr) return;
    
    #if !DEBUG_BUTTON_ONLY
    unsigned long renderStart = millis();
    Serial.println(F("[WEB] render start"));
    #endif
    
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
    
    // Status box - WEB-PERF-3: gebruik placeholders voor live updates via JavaScript
    server->sendContent(F("<div class='status-box'>"));
    // Placeholders met IDs voor client-side updates
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='curPrice'>--</span></div>",
             getText("Huidige Prijs", "Current Price"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='trend'>--</span></div>",
             getText("Trend", "Trend"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='volatility'>--</span></div>",
             getText("Volatiliteit", "Volatility"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='ret1m'>--</span></div>",
             getText("1m Return", "1m Return"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='ret30m'>--</span></div>",
             getText("30m Return", "30m Return"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='anchor'>--</span></div>",
             getText("Anchor", "Anchor"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value' id='anchorDelta'>--</span></div>",
             getText("Anchor Delta", "Anchor Delta"));
    server->sendContent(tmpBuf);
    // API state indicator (klein, onopvallend)
    snprintf(tmpBuf, sizeof(tmpBuf), "<div style='margin-top:10px;font-size:11px;color:#666;' id='apiState'></div>");
    server->sendContent(tmpBuf);
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
    sendInputRow(getText("Display Rotatie", "Display Rotation"), "displayRotation", "number", 
                 (displayRotation == 2) ? "2" : "0", 
                 getText("0 = normaal, 2 = 180 graden gedraaid", "0 = normal, 2 = rotated 180 degrees"), 0, 2, 2);
    
    sendSectionFooter();
    
    // Anchor & Risicokader sectie
    sendSectionHeader(getText("Anchor & Risicokader", "Anchor & Risk Framework"), "anchor", true);
    sendSectionDesc(getText("Anchor prijs instellingen en risicobeheer", "Anchor price settings and risk management"));
    
    // TP/SL Strategie dropdown
    const char* strategyOptions[] = {
        getText("Handmatig", "Manual"),
        getText("Conservatief (TP +1.8%, SL -1.2%)", "Conservative (TP +1.8%, SL -1.2%)"),
        getText("Actief (TP +1.2%, SL -0.9%)", "Active (TP +1.2%, SL -0.9%)")
    };
    sendDropdownRow(getText("TP/SL Strategie", "TP/SL Strategy"), "anchorStrategy", anchorStrategy, strategyOptions, 3);
    
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
    
    // 2-hour Alert Thresholds sectie
    sendSectionHeader(getText("2-uur Alert Thresholds", "2-hour Alert Thresholds"), "2hAlerts", false);
    sendSectionDesc(getText("Thresholds voor 2-uur breakout, breakdown, compressie en mean reversion alerts", "Thresholds for 2-hour breakout, breakdown, compression and mean reversion alerts"));
    
    // Breakout/Breakdown thresholds
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.breakMarginPct);
    sendInputRow(getText("Breakout/Breakdown Margin (%)", "Breakout/Breakdown Margin (%)"), "2hBreakMargin", "number", 
                 valueBuf, getText("Minimaal percentage boven/onder 2h high/low voor breakout", "Minimum percentage above/below 2h high/low for breakout"), 
                 0.01f, 5.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.breakResetMarginPct);
    sendInputRow(getText("Breakout Reset Margin (%)", "Breakout Reset Margin (%)"), "2hBreakReset", "number", 
                 valueBuf, getText("Percentage voor reset van breakout arm", "Percentage for breakout arm reset"), 
                 0.01f, 5.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.breakCooldownMs / 1000UL / 60UL);
    sendInputRow(getText("Breakout Cooldown (min)", "Breakout Cooldown (min)"), "2hBreakCD", "number", 
                 valueBuf, getText("Cooldown in minuten tussen breakout alerts", "Cooldown in minutes between breakout alerts"), 
                 1, 180, 1);
    
    // Mean reversion thresholds
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.meanMinDistancePct);
    sendInputRow(getText("Mean Min Distance (%)", "Mean Min Distance (%)"), "2hMeanMinDist", "number", 
                 valueBuf, getText("Minimaal percentage afstand van avg2h voor mean reversion", "Minimum percentage distance from avg2h for mean reversion"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.meanTouchBandPct);
    sendInputRow(getText("Mean Touch Band (%)", "Mean Touch Band (%)"), "2hMeanTouch", "number", 
                 valueBuf, getText("Band rond avg2h voor 'touch' detectie", "Band around avg2h for 'touch' detection"), 
                 0.01f, 2.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.meanCooldownMs / 1000UL / 60UL);
    sendInputRow(getText("Mean Reversion Cooldown (min)", "Mean Reversion Cooldown (min)"), "2hMeanCD", "number", 
                 valueBuf, getText("Cooldown in minuten tussen mean reversion alerts", "Cooldown in minutes between mean reversion alerts"), 
                 1, 180, 1);
    
    // Range compression thresholds
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.compressThresholdPct);
    sendInputRow(getText("Compress Threshold (%)", "Compress Threshold (%)"), "2hCompressTh", "number", 
                 valueBuf, getText("Maximum range% voor compressie alert", "Maximum range% for compression alert"), 
                 0.01f, 5.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.compressResetPct);
    sendInputRow(getText("Compress Reset (%)", "Compress Reset (%)"), "2hCompressReset", "number", 
                 valueBuf, getText("Range% voor reset van compressie arm", "Range% for compression arm reset"), 
                 0.01f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.compressCooldownMs / 1000UL / 60UL);
    sendInputRow(getText("Compress Cooldown (min)", "Compress Cooldown (min)"), "2hCompressCD", "number", 
                 valueBuf, getText("Cooldown in minuten tussen compressie alerts", "Cooldown in minutes between compression alerts"), 
                 1, 300, 1);
    
    // Anchor context thresholds
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.anchorOutsideMarginPct);
    sendInputRow(getText("Anchor Outside Margin (%)", "Anchor Outside Margin (%)"), "2hAnchorMargin", "number", 
                 valueBuf, getText("Minimaal percentage buiten 2h range voor anchor alert", "Minimum percentage outside 2h range for anchor alert"), 
                 0.01f, 5.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.anchorCooldownMs / 1000UL / 60UL);
    sendInputRow(getText("Anchor Context Cooldown (min)", "Anchor Context Cooldown (min)"), "2hAnchorCD", "number", 
                 valueBuf, getText("Cooldown in minuten tussen anchor context alerts", "Cooldown in minutes between anchor context alerts"), 
                 1, 300, 1);
    
    // FASE X.4: Trend hysteresis en throttling instellingen
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.trendHysteresisFactor);
    sendInputRow(getText("Trend Hysteresis Factor", "Trend Hysteresis Factor"), "2hTrendHyst", "number", 
                 valueBuf, getText("Factor voor trend exit threshold (0.65 = 65% van threshold)", "Factor for trend exit threshold (0.65 = 65% of threshold)"), 
                 0.1f, 1.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.throttlingTrendChangeMs / 1000UL / 60UL);
    sendInputRow(getText("Throttle: Trend Change (min)", "Throttle: Trend Change (min)"), "2hThrottleTC", "number", 
                 valueBuf, getText("Cooldown tussen Trend Change alerts", "Cooldown between Trend Change alerts"), 
                 1, 600, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.throttlingTrendToMeanMs / 1000UL / 60UL);
    sendInputRow(getText("Throttle: Trend→Mean (min)", "Throttle: Trend→Mean (min)"), "2hThrottleTM", "number", 
                 valueBuf, getText("Cooldown na Trend Change voor Mean Touch alert", "Cooldown after Trend Change for Mean Touch alert"), 
                 1, 300, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.throttlingMeanTouchMs / 1000UL / 60UL);
    sendInputRow(getText("Throttle: Mean Touch (min)", "Throttle: Mean Touch (min)"), "2hThrottleMT", "number", 
                 valueBuf, getText("Cooldown tussen Mean Touch alerts", "Cooldown between Mean Touch alerts"), 
                 1, 300, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.throttlingCompressMs / 1000UL / 60UL);
    sendInputRow(getText("Throttle: Compress (min)", "Throttle: Compress (min)"), "2hThrottleComp", "number", 
                 valueBuf, getText("Cooldown tussen Compress alerts", "Cooldown between Compress alerts"), 
                 1, 600, 1);
    
    // FASE X.5: Secondary global cooldown en coalescing instellingen
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.twoHSecondaryGlobalCooldownSec / 60UL);
    sendInputRow(getText("2h Secondary Global Cooldown (min)", "2h Secondary Global Cooldown (min)"), "2hSecGlobalCD", "number", 
                 valueBuf, getText("Globale cooldown voor alle SECONDARY alerts (hard cap)", "Global cooldown for all SECONDARY alerts (hard cap)"), 
                 1, 1440, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%lu", alert2HThresholds.twoHSecondaryCoalesceWindowSec);
    sendInputRow(getText("2h Secondary Coalesce Window (sec)", "2h Secondary Coalesce Window (sec)"), "2hSecCoalesce", "number", 
                 valueBuf, getText("Tijdvenster voor burst-demping (meerdere alerts binnen window = 1 melding)", "Time window for burst suppression (multiple alerts within window = 1 notification)"), 
                 10, 600, 1);
    
    sendSectionFooter();
    
    // Auto Anchor sectie
    sendSectionHeader(getText("Auto Anchor", "Auto Anchor"), "autoAnchor", false);
    sendSectionDesc(getText("Automatische anchor berekening op basis van 4h en 1d EMA", "Automatic anchor calculation based on 4h and 1d EMA"));
    
    // Anchor Source Mode dropdown
    const char* anchorSourceOptions[] = {"MANUAL", "AUTO", "AUTO_FALLBACK", "OFF"};
    sendDropdownRow(getText("Anchor Bron", "Anchor Source"), "anchorSourceMode", 
                    alert2HThresholds.anchorSourceMode, anchorSourceOptions, 4);
    
    // Auto Anchor settings (gebruik helper methods voor compacte types)
    snprintf(valueBuf, sizeof(valueBuf), "%u", alert2HThresholds.autoAnchorUpdateMinutes);
    sendInputRow(getText("Update Interval (min)", "Update Interval (min)"), "autoAnchorUpdateMinutes", "number", 
                 valueBuf, getText("Interval tussen auto anchor updates", "Interval between auto anchor updates"), 
                 10, 1440, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%u", alert2HThresholds.autoAnchorForceUpdateMinutes);
    sendInputRow(getText("Force Update Interval (min)", "Force Update Interval (min)"), "autoAnchorForceUpdateMinutes", "number", 
                 valueBuf, getText("Force update na X minuten (ook bij kleine wijzigingen)", "Force update after X minutes (even for small changes)"), 
                 60, 2880, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%u", alert2HThresholds.autoAnchor4hCandles);
    sendInputRow(getText("4h Candles", "4h Candles"), "autoAnchor4hCandles", "number", 
                 valueBuf, getText("Aantal 4h candles voor EMA berekening", "Number of 4h candles for EMA calculation"), 
                 2, 100, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%u", alert2HThresholds.autoAnchor1dCandles);
    sendInputRow(getText("1d Candles", "1d Candles"), "autoAnchor1dCandles", "number", 
                 valueBuf, getText("Aantal 1d candles voor EMA berekening", "Number of 1d candles for EMA calculation"), 
                 2, 60, 1);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.getAutoAnchorMinUpdatePct());
    sendInputRow(getText("Min Update %", "Min Update %"), "autoAnchorMinUpdatePct", "number", 
                 valueBuf, getText("Minimaal percentage wijziging voor update", "Minimum percentage change for update"), 
                 0.01f, 5.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.getAutoAnchorTrendPivotPct());
    sendInputRow(getText("Trend Pivot %", "Trend Pivot %"), "autoAnchorTrendPivotPct", "number", 
                 valueBuf, getText("Trend pivot percentage voor adaptieve weging", "Trend pivot percentage for adaptive weighting"), 
                 0.1f, 10.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.getAutoAnchorW4hBase());
    sendInputRow(getText("4h Base Weight", "4h Base Weight"), "autoAnchorW4hBase", "number", 
                 valueBuf, getText("Basis gewicht voor 4h EMA", "Base weight for 4h EMA"), 
                 0.0f, 1.0f, 0.01f);
    
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.getAutoAnchorW4hTrendBoost());
    sendInputRow(getText("4h Trend Boost", "4h Trend Boost"), "autoAnchorW4hTrendBoost", "number", 
                 valueBuf, getText("Extra gewicht voor 4h EMA bij sterke trend", "Extra weight for 4h EMA in strong trend"), 
                 0.0f, 1.0f, 0.01f);
    
    // Read-only display voor laatste waarde
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", alert2HThresholds.autoAnchorLastValue);
    sendStatusRow(getText("Laatste Auto Anchor", "Last Auto Anchor"), valueBuf);
    
    // Checkbox voor notificaties
    sendCheckboxRow(getText("Notificatie bij update", "Notify on update"), 
                    "autoAnchorNotifyEnabled", alert2HThresholds.getAutoAnchorNotifyEnabled());
    
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
    
    // MQTT Topic Prefix (read-only, ter referentie)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    snprintf(tmpBuf, sizeof(tmpBuf), "<div style='margin:15px 0;padding:10px;background:#2a2a2a;border:1px solid #444;border-radius:4px;'>");
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<label style='display:block;margin-bottom:5px;color:#ccc;font-weight:bold;'>%s:</label>", 
             getText("MQTT Topic Prefix", "MQTT Topic Prefix"));
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div style='padding:8px;background:#1a1a1a;border:1px solid #555;border-radius:4px;color:#fff;font-family:monospace;font-size:14px;word-break:break-all;'>%s</div>", mqttPrefix);
    server->sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='info' style='margin-top:5px;font-size:12px;color:#888;'>%s</div>", 
             getText("Unieke prefix gebaseerd op NTFY topic (alleen ter referentie)", "Unique prefix based on NTFY topic (reference only)"));
    server->sendContent(tmpBuf);
    server->sendContent(F("</div>"));
    
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
    
    // Handle language setting - Fix: gebruik String object eerst
    if (server->hasArg("language")) {
        String langStr = server->arg("language");
        int langVal = atoi(langStr.c_str());
        if (langVal == 0 || langVal == 1) {
            language = static_cast<uint8_t>(langVal);
        }
    }
    
    // Handle display rotation setting
    if (server->hasArg("displayRotation")) {
        String rotStr = server->arg("displayRotation");
        int rotVal = atoi(rotStr.c_str());
        if (rotVal == 0 || rotVal == 2) {
            displayRotation = static_cast<uint8_t>(rotVal);
            // Wis scherm eerst om residu te voorkomen
            gfx->fillScreen(RGB565_BLACK);
            // Pas rotatie direct toe
            gfx->setRotation(rotVal);
            // Wis scherm opnieuw na rotatie
            gfx->fillScreen(RGB565_BLACK);
        }
    }
    
    // Fix: Gebruik String object eerst (zoals bij anchor) om dangling pointer te voorkomen
    // ESP32-S3 heeft problemen met direct .c_str() op tijdelijke String objecten
    if (server->hasArg("ntfytopic")) {
        String topicStr = server->arg("ntfytopic");  // Bewaar String object eerst
        char topicBuffer[64];
        size_t topicLen = topicStr.length();
        
        if (topicLen >= sizeof(topicBuffer)) {
            topicLen = sizeof(topicBuffer) - 1;
        }
        strncpy(topicBuffer, topicStr.c_str(), topicLen);
        topicBuffer[topicLen] = '\0';
        
        // Trim leading/trailing whitespace
        char* start = topicBuffer;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        topicLen = strlen(start);
        
        // Allow empty topic (will use default) or valid length topic
        if (topicLen == 0) {
                // Generate default topic if empty
                generateDefaultNtfyTopic(ntfyTopic, 64);
        } else if (topicLen < 64) {
            strncpy(ntfyTopic, start, topicLen);
            ntfyTopic[topicLen] = '\0';
        }
    }
    if (server->hasArg("binancesymbol")) {
        // Fix: gebruik String object eerst om dangling pointer te voorkomen
        String symbolStr = server->arg("binancesymbol");
        char symbolBuffer[16];
        size_t symbolLen = symbolStr.length();
        
        if (symbolLen >= sizeof(symbolBuffer)) {
            symbolLen = sizeof(symbolBuffer) - 1;
        }
        strncpy(symbolBuffer, symbolStr.c_str(), symbolLen);
        symbolBuffer[symbolLen] = '\0';
        
        // Trim leading/trailing whitespace
        char* start = symbolBuffer;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        symbolLen = strlen(start);
        
        if (symbolLen > 0 && symbolLen < 16) {  // binanceSymbol is 16 bytes
            // Convert to uppercase
            for (size_t i = 0; i < symbolLen; i++) {
                symbolBuffer[i] = (start[i] >= 'a' && start[i] <= 'z') 
                    ? (start[i] - 'a' + 'A') 
                    : start[i];
            }
            symbolBuffer[symbolLen] = '\0';
            
            // Check if symbol actually changed
            bool symbolChanged = (strcmp(binanceSymbol, symbolBuffer) != 0);
            
            if (symbolChanged) {
                // Symbol changed - reboot for clean state
                Serial_printf(F("[Settings] Binance symbol changed from %s to %s - rebooting\n"), 
                             binanceSymbol, symbolBuffer);
                
                // Update symbol before reboot
                safeStrncpy(binanceSymbol, symbolBuffer, 16);  // binanceSymbol is 16 bytes
                safeStrncpy(symbolsArray[0], binanceSymbol, 16);
                
                // Save settings before reboot
                saveSettings();
                // WEB-PERF-3: Invalideer cache na settings wijziging
                invalidatePageCache();
                
                // Small delay to ensure settings are saved
                delay(100);
                
                // Reboot for clean state
                ESP.restart();
            } else {
                // No change, just update normally
                safeStrncpy(binanceSymbol, symbolBuffer, 16);  // binanceSymbol is 16 bytes
                safeStrncpy(symbolsArray[0], binanceSymbol, 16);
            }
        }
    }
    // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
    float floatVal;
    if (parseFloatArg("spike1m", floatVal, 0.01f, 10.0f)) {
        spike1mThreshold = floatVal;
    }
    if (parseFloatArg("spike5m", floatVal, 0.01f, 10.0f)) {
        spike5mThreshold = floatVal;
    }
    if (parseFloatArg("move30m", floatVal, 0.01f, 20.0f)) {
        move30mThreshold = floatVal;
    }
    if (parseFloatArg("move5m", floatVal, 0.01f, 10.0f)) {
        move5mThreshold = floatVal;
    }
    if (parseFloatArg("move5mAlert", floatVal, 0.01f, 10.0f)) {
        move5mAlertThreshold = floatVal;
    }
    
    // 2-hour alert thresholds - geoptimaliseerd: gebruik helper functies
    if (parseFloatArg("2hBreakMargin", floatVal, 0.01f, 5.0f)) {
        alert2HThresholds.breakMarginPct = floatVal;
    }
    if (parseFloatArg("2hBreakReset", floatVal, 0.01f, 5.0f)) {
        alert2HThresholds.breakResetMarginPct = floatVal;
    }
    int intVal;
    if (parseIntArg("2hBreakCD", intVal, 1, 180)) {
        alert2HThresholds.breakCooldownMs = intVal * 60UL * 1000UL;
    }
    if (parseFloatArg("2hMeanMinDist", floatVal, 0.01f, 10.0f)) {
        alert2HThresholds.meanMinDistancePct = floatVal;
    }
    if (parseFloatArg("2hMeanTouch", floatVal, 0.01f, 2.0f)) {
        alert2HThresholds.meanTouchBandPct = floatVal;
    }
    if (parseIntArg("2hMeanCD", intVal, 1, 180)) {
        alert2HThresholds.meanCooldownMs = intVal * 60UL * 1000UL;
    }
    if (parseFloatArg("2hCompressTh", floatVal, 0.01f, 5.0f)) {
        alert2HThresholds.compressThresholdPct = floatVal;
    }
    if (parseFloatArg("2hCompressReset", floatVal, 0.01f, 10.0f)) {
        alert2HThresholds.compressResetPct = floatVal;
    }
    if (parseIntArg("2hCompressCD", intVal, 1, 300)) {
        alert2HThresholds.compressCooldownMs = intVal * 60UL * 1000UL;
    }
    if (parseFloatArg("2hAnchorMargin", floatVal, 0.01f, 5.0f)) {
        alert2HThresholds.anchorOutsideMarginPct = floatVal;
    }
    if (parseIntArg("2hAnchorCD", intVal, 1, 300)) {
        alert2HThresholds.anchorCooldownMs = intVal * 60UL * 1000UL;
    }
    
    // FASE X.4: Trend hysteresis en throttling instellingen
    if (parseFloatArg("2hTrendHyst", floatVal, 0.1f, 1.0f)) {
        alert2HThresholds.trendHysteresisFactor = floatVal;
    }
    if (parseIntArg("2hThrottleTC", intVal, 1, 600)) {
        alert2HThresholds.throttlingTrendChangeMs = intVal * 60UL * 1000UL;
    }
    if (parseIntArg("2hThrottleTM", intVal, 1, 300)) {
        alert2HThresholds.throttlingTrendToMeanMs = intVal * 60UL * 1000UL;
    }
    if (parseIntArg("2hThrottleMT", intVal, 1, 300)) {
        alert2HThresholds.throttlingMeanTouchMs = intVal * 60UL * 1000UL;
    }
    if (parseIntArg("2hThrottleComp", intVal, 1, 600)) {
        alert2HThresholds.throttlingCompressMs = intVal * 60UL * 1000UL;
    }
    
    // FASE X.5: Secondary global cooldown en coalescing instellingen
    if (parseIntArg("2hSecGlobalCD", intVal, 1, 1440)) {
        alert2HThresholds.twoHSecondaryGlobalCooldownSec = intVal * 60UL;  // Convert minuten naar seconden
    }
    if (parseIntArg("2hSecCoalesce", intVal, 10, 600)) {
        alert2HThresholds.twoHSecondaryCoalesceWindowSec = intVal;  // Al in seconden
    }
    
    // Auto Anchor settings parsing
    if (server->hasArg("anchorSourceMode")) {
        int mode = constrain(server->arg("anchorSourceMode").toInt(), 0, 3);
        alert2HThresholds.anchorSourceMode = static_cast<uint8_t>(mode);
    }
    if (parseIntArg("autoAnchorUpdateMinutes", intVal, 10, 1440)) {
        alert2HThresholds.autoAnchorUpdateMinutes = static_cast<uint16_t>(intVal);
    }
    if (parseIntArg("autoAnchorForceUpdateMinutes", intVal, 60, 2880)) {
        alert2HThresholds.autoAnchorForceUpdateMinutes = static_cast<uint16_t>(intVal);
    }
    if (parseIntArg("autoAnchor4hCandles", intVal, 2, 100)) {
        alert2HThresholds.autoAnchor4hCandles = static_cast<uint8_t>(intVal);
    }
    if (parseIntArg("autoAnchor1dCandles", intVal, 2, 60)) {
        alert2HThresholds.autoAnchor1dCandles = static_cast<uint8_t>(intVal);
    }
    if (parseFloatArg("autoAnchorMinUpdatePct", floatVal, 0.01f, 5.0f)) {
        alert2HThresholds.setAutoAnchorMinUpdatePct(floatVal);
    }
    if (parseFloatArg("autoAnchorTrendPivotPct", floatVal, 0.1f, 10.0f)) {
        alert2HThresholds.setAutoAnchorTrendPivotPct(floatVal);
    }
    if (parseFloatArg("autoAnchorW4hBase", floatVal, 0.0f, 1.0f)) {
        alert2HThresholds.setAutoAnchorW4hBase(floatVal);
    }
    if (parseFloatArg("autoAnchorW4hTrendBoost", floatVal, 0.0f, 1.0f)) {
        alert2HThresholds.setAutoAnchorW4hTrendBoost(floatVal);
    }
    if (server->hasArg("autoAnchorNotifyEnabled")) {
        alert2HThresholds.setAutoAnchorNotifyEnabled(true);
    } else {
        alert2HThresholds.setAutoAnchorNotifyEnabled(false);
    }
    
    // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
    if (parseIntArg("cd1min", intVal, 0, 3600)) {
        uint32_t resultMs;
        if (safeSecondsToMs(intVal, resultMs)) {
            notificationCooldown1MinMs = resultMs;
        }
    }
    if (parseIntArg("cd30min", intVal, 0, 3600)) {
        uint32_t resultMs;
        if (safeSecondsToMs(intVal, resultMs)) {
            notificationCooldown30MinMs = resultMs;
        }
    }
    if (parseIntArg("cd5min", intVal, 0, 3600)) {
        uint32_t resultMs;
        if (safeSecondsToMs(intVal, resultMs)) {
            notificationCooldown5MinMs = resultMs;
        }
    }
    
    // MQTT settings - geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
    parseStringArg("mqtthost", mqttHost, 64);
    if (parseIntArg("mqttport", intVal, 1, 65535)) {
        mqttPort = static_cast<uint16_t>(intVal);
    }
    parseStringArg("mqttuser", mqttUser, 64);
    parseStringArg("mqttpass", mqttPass, 64);
    
    // Trend and volatility settings - geoptimaliseerd: gebruik helper functie
    if (parseFloatArg("trendTh", floatVal, 0.1f, 10.0f)) {
        trendThreshold = floatVal;
    }
    if (parseFloatArg("volLow", floatVal, 0.01f, 1.0f)) {
        volatilityLowThreshold = floatVal;
    }
    if (parseFloatArg("volHigh", floatVal, 0.01f, 1.0f) && floatVal > volatilityLowThreshold) {
        volatilityHighThreshold = floatVal;
    }
    
    // Anchor settings - NIET vanuit web server thread verwerken om crashes te voorkomen
    // Anchor setting wordt verwerkt via een aparte route /anchor/set die sneller is
    if (parseIntArg("anchorStrategy", intVal, 0, 2)) {
        anchorStrategy = static_cast<uint8_t>(intVal);
        // Pas TP/SL automatisch aan op basis van strategie
        if (anchorStrategy == 1) {
            // Conservatief: TP +1.8%, SL -1.2%
            anchorTakeProfit = 1.8f;
            anchorMaxLoss = -1.2f;
        } else if (anchorStrategy == 2) {
            // Actief: TP +1.2%, SL -0.9%
            anchorTakeProfit = 1.2f;
            anchorMaxLoss = -0.9f;
        }
        // anchorStrategy == 0 (handmatig): behoud huidige waarden
        
        // Reset cache variabelen om BTCEUR blok en footer te forceren te updaten
        extern float lastAnchorMaxValue;
        extern float lastAnchorValue;
        extern float lastAnchorMinValue;
        lastAnchorMaxValue = -1.0f;
        lastAnchorValue = -1.0f;
        lastAnchorMinValue = -1.0f;
        
        // Reset footer cache om versie te forceren te updaten
        extern char lastVersionText[32];
        lastVersionText[0] = '\0';
    }
    if (parseFloatArg("anchorTP", floatVal, 0.1f, 100.0f)) {
        anchorTakeProfit = floatVal;
        // Als handmatig aangepast, zet strategie terug naar handmatig
        if (anchorStrategy != 0) {
            anchorStrategy = 0;
        }
        
        // Reset cache variabelen om BTCEUR blok en footer te forceren te updaten
        extern float lastAnchorMaxValue;
        extern float lastAnchorValue;
        extern float lastAnchorMinValue;
        lastAnchorMaxValue = -1.0f;
        lastAnchorValue = -1.0f;
        lastAnchorMinValue = -1.0f;
        
        // Reset footer cache om versie te forceren te updaten
        extern char lastVersionText[];
        lastVersionText[0] = '\0';
    }
    if (parseFloatArg("anchorML", floatVal, -100.0f, -0.1f)) {
        anchorMaxLoss = floatVal;
        // Als handmatig aangepast, zet strategie terug naar handmatig
        if (anchorStrategy != 0) {
            anchorStrategy = 0;
        }
        
        // Reset cache variabelen om BTCEUR blok en footer te forceren te updaten
        extern float lastAnchorMaxValue;
        extern float lastAnchorValue;
        extern float lastAnchorMinValue;
        lastAnchorMaxValue = -1.0f;
        lastAnchorValue = -1.0f;
        lastAnchorMinValue = -1.0f;
        
        // Reset footer cache om versie te forceren te updaten
        extern char lastVersionText[32];
        lastVersionText[0] = '\0';
    }
    
    // Trend-adaptive anchor settings - geoptimaliseerd: gebruik helper functie
    trendAdaptiveAnchorsEnabled = server->hasArg("trendAdapt");
    if (parseFloatArg("upMLMult", floatVal, 0.5f, 2.0f)) {
        uptrendMaxLossMultiplier = floatVal;
    }
    if (parseFloatArg("upTPMult", floatVal, 0.5f, 2.0f)) {
        uptrendTakeProfitMultiplier = floatVal;
    }
    if (parseFloatArg("downMLMult", floatVal, 0.5f, 2.0f)) {
        downtrendMaxLossMultiplier = floatVal;
    }
    if (parseFloatArg("downTPMult", floatVal, 0.5f, 2.0f)) {
        downtrendTakeProfitMultiplier = floatVal;
    }
    
    // Smart Confluence Mode settings
    smartConfluenceEnabled = server->hasArg("smartConf");
    
    // Warm-Start settings - geoptimaliseerd: gebruik helper functie
    warmStartEnabled = server->hasArg("warmStart");
    if (parseIntArg("ws1mExtra", intVal, 0, 100)) {
        warmStart1mExtraCandles = static_cast<uint8_t>(intVal);
    }
    if (parseIntArg("ws5m", intVal, 2, 200)) {
        warmStart5mCandles = static_cast<uint8_t>(intVal);
    }
    if (parseIntArg("ws30m", intVal, 2, 200)) {
        warmStart30mCandles = static_cast<uint8_t>(intVal);
    }
    if (parseIntArg("ws2h", intVal, 2, 200)) {
        warmStart2hCandles = static_cast<uint8_t>(intVal);
    }
    
    // Auto-Volatility Mode settings - geoptimaliseerd: gebruik helper functie
    autoVolatilityEnabled = server->hasArg("autoVol");
    if (parseIntArg("autoVolWin", intVal, 10, 120)) {
        autoVolatilityWindowMinutes = static_cast<uint8_t>(intVal);
    }
    if (parseFloatArg("autoVolBase", floatVal, 0.01f, 1.0f)) {
        autoVolatilityBaseline1mStdPct = floatVal;
    }
    if (parseFloatArg("autoVolMin", floatVal, 0.1f, 1.0f)) {
        autoVolatilityMinMultiplier = floatVal;
    }
    if (parseFloatArg("autoVolMax", floatVal, 1.0f, 3.0f)) {
        autoVolatilityMaxMultiplier = floatVal;
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
    
    // Performance optimalisatie: gebruik char buffer i.p.v. String concatenatie
    // Max URI length + method + args info = ~512 bytes
    char message[512];
    snprintf(message, sizeof(message), "File Not Found\n\nURI: %s\nMethod: %s\nArguments: %d\n",
             server->uri().c_str(), 
             (server->method() == HTTP_GET) ? "GET" : "POST",
             server->args());
    
    // Append argument details
    size_t msgLen = strlen(message);
    for (uint8_t i = 0; i < server->args() && msgLen < sizeof(message) - 50; i++) {
        size_t written = snprintf(message + msgLen, sizeof(message) - msgLen, 
                                  " %s: %s\n", 
                                  server->argName(i).c_str(), 
                                  server->arg(i).c_str());
        if (written > 0 && written < sizeof(message) - msgLen) {
            msgLen += written;
        } else {
            break;  // Buffer vol
        }
    }
    
    server->send(404, "text/plain", message);
    Serial_printf(F("[WebServer] 404: %s\n"), server->uri().c_str());
}

// WEB-PERF-3: Status endpoint - JSON met live waarden (geen heap-allocaties)
void WebServerModule::handleStatus() {
    if (server == nullptr) return;
    
    #if !DEBUG_BUTTON_ONLY
    unsigned long statusStart = millis();
    #endif
    
    // Lokale variabelen voor thread-safe data kopiëren
    float price = 0.0f;
    float ret1m = 0.0f;
    float ret5m = 0.0f;
    float ret30m = 0.0f;
    float ret2h = 0.0f;
    float ret1d = 0.0f;
    TrendState trend = TREND_SIDEWAYS;
    TrendState trendLong = TREND_SIDEWAYS;
    VolatilityState volatility = VOLATILITY_MEDIUM;
    bool anchorActive = false;
    float anchorPrice = 0.0f;
    float anchorDeltaPct = 0.0f;
    float avg2h = 0.0f;
    float high2h = 0.0f;
    float low2h = 0.0f;
    float range2hPct = 0.0f;
    bool hasRet2hFlag = false;
    bool hasRet30mFlag = false;
    
    // Neem kort de dataMutex om globale waarden te kopiëren
    if (safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "handleStatus")) {
        if (isValidPrice(prices[0])) {
            price = prices[0];
        }
        ret1m = calculateReturn1Minute();
        ret5m = calculateReturn5Minutes();
        ret30m = calculateReturn30Minutes();
        ret2h = ::ret_2h;  // Gebruik globale ret_2h
        ret1d = ::ret_1d;
        trend = trendDetector.getTrendState();
        trendLong = trendDetector.getLongTermTrendState();
        volatility = volatilityTracker.getVolatilityState();
        anchorActive = ::anchorActive;
        if (anchorActive && isValidPrice(::anchorPrice)) {
            anchorPrice = ::anchorPrice;
            if (isValidPrice(price) && anchorPrice > 0.0f) {
                anchorDeltaPct = ((price - anchorPrice) / anchorPrice) * 100.0f;
            }
        }
        hasRet2hFlag = ::hasRet2h;
        hasRet30mFlag = ::hasRet30m;
        
        // Bereken 2h metrics (binnen mutex voor thread-safety)
        TwoHMetrics metrics = computeTwoHMetrics();
        avg2h = metrics.avg2h;
        high2h = metrics.high2h;
        low2h = metrics.low2h;
        range2hPct = metrics.rangePct;
        
        safeMutexGive(dataMutex, "handleStatus");
    }
    
    // JSON buffer (900 bytes voor extra trend/return velden)
    char jsonBuf[900];
    size_t written = 0;
    
    // Bouw JSON zonder String-concatenaties (gebruik snprintf met offset)
    written = snprintf(jsonBuf, sizeof(jsonBuf),
        "{"
        "\"symbol\":\"%s\","
        "\"price\":%.2f,"
        "\"trend\":\"%s\","
        "\"volatility\":\"%s\","
        "\"ret1m\":%.2f,"
        "\"ret5m\":%.2f,"
        "\"ret30m\":%.2f,"
        "\"ret2h\":%.2f,"
        "\"ret1d\":%.2f,"
        "\"trendLong\":\"%s\","
        "\"anchor\":%.2f,"
        "\"anchorDeltaPct\":%.2f,"
        "\"avg2h\":%.2f,"
        "\"high2h\":%.2f,"
        "\"low2h\":%.2f,"
        "\"range2hPct\":%.2f,"
        "\"uptimeSec\":%lu,"
        "\"heapFree\":%u,"
        "\"heapLargest\":%u"
        "}",
        binanceSymbol,
        price,
        getTrendText(trend),
        getVolatilityText(volatility),
        ret1m,
        ret5m,
        ret30m,
        ret2h,
        ret1d,
        getTrendText(trendLong),
        anchorPrice,
        anchorDeltaPct,
        avg2h,
        high2h,
        low2h,
        range2hPct,
        millis() / 1000,
        ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
    );
    
    // Verstuur JSON response
    if (written > 0 && written < sizeof(jsonBuf)) {
        server->send(200, "application/json", jsonBuf);
    } else {
        // Fallback bij buffer overflow (zou niet moeten gebeuren)
        server->send(500, "application/json", "{\"error\":\"buffer overflow\"}");
    }
    
    #if !DEBUG_BUTTON_ONLY
    unsigned long statusEnd = millis();
    Serial_printf(F("[WEB] /status in %lu ms\n"), statusEnd - statusStart);
    #endif
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
            strncpy(valueBuffer, anchorValueStr.c_str(), len);
            valueBuffer[len] = '\0';
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
    
    // WEB-PERF-3: Invalideer cache na settings wijziging
    invalidatePageCache();
    
    // Stuur succes response
    server->send(200, "text/plain", "OK");
}

// WEB-PERF-3: Cache invalidation helper
void WebServerModule::invalidatePageCache() {
    sPageCacheValid = false;
    sPageCache = "";  // Free memory
}

// WEB-PERF-3: Get or build settings page (with caching)
String WebServerModule::getOrBuildSettingsPage() {
    // Return cached page if valid
    if (sPageCacheValid && sPageCache.length() > 0) {
        #if !DEBUG_BUTTON_ONLY
        Serial.println(F("[WEB] handleRoot served cached page"));
        #endif
        return sPageCache;
    }
    
    #if !DEBUG_BUTTON_ONLY
    unsigned long buildStart = millis();
    #endif
    
    // Reserve memory voor grote HTML pagina (alleen voor platforms met voldoende geheugen)
    // CYD24/CYD28 zonder PSRAM: skip reserve om DRAM overflow te voorkomen
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // CYD zonder PSRAM: geen reserve (cache nog niet geïmplementeerd anyway)
        sPageCache = "";
    #else
        // ESP32-S3/TTGO met PSRAM: reserve memory
        sPageCache.reserve(16000);
        sPageCache = "";
    #endif
    
    // Build HTML page (we moeten renderSettingsHTML() aanpassen om naar String te schrijven)
    // Voor nu: gebruik renderSettingsHTML() direct en cache de response niet
    // TODO: Refactor renderSettingsHTML() om naar String te kunnen schrijven
    
    // Voor nu: return empty string en gebruik renderSettingsHTML() direct
    // Dit is een placeholder - volledige implementatie vereist refactoring van renderSettingsHTML()
    sPageCacheValid = false;  // Cache nog niet geïmplementeerd
    
    #if !DEBUG_BUTTON_ONLY
    unsigned long buildEnd = millis();
    Serial_printf(F("[WEB] buildSettingsPage: %lu ms, len=%u (not implemented yet)\n"), 
                  buildEnd - buildStart, sPageCache.length());
    #endif
    
    return sPageCache;  // Empty for now
}

// Fase 9.1.3: HTML helper functies verplaatst vanuit .ino
void WebServerModule::sendHtmlHeader(const char* platformName, const char* ntfyTopic) {
    if (server == nullptr) return;
    
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html; charset=utf-8", "");
    
    // HTML doctype en head (lang='en' om punt als decimaal scheidingsteken te forceren)
    server->sendContent(F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    
    // Title - gebruik binanceSymbol in plaats van platformName, voeg versie toe
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), "<title>%s %s %s v%s</title>", 
             getText("Instellingen", "Settings"), binanceSymbol, ntfyTopic, VERSION_STRING);
    server->sendContent(titleBuf);
    
    // CSS - Performance optimalisatie: combineer statische CSS in één PROGMEM string
    // Dit vermindert het aantal sendContent() calls en TCP/IP overhead
    static const char cssBlock[] PROGMEM = 
        "<style>"
        "*{box-sizing:border-box;}"
        "body{font-family:Arial;margin:0;padding:10px;background:#1a1a1a;color:#fff;}"
        ".container{max-width:600px;margin:0 auto;padding:0 10px;}"
        "h1{color:#00BCD4;margin:15px 0;font-size:24px;}"
        "form{max-width:100%;}"
        "label{display:block;margin:15px 0 5px;color:#ccc;}"
        "input[type=number],input[type=text],select{width:100%;padding:8px;border:1px solid #444;background:#2a2a2a;color:#fff;border-radius:4px;box-sizing:border-box;}"
        "button{background:#00BCD4;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;width:100%;}"
        "button:hover{background:#00acc1;}"
        ".info{color:#888;font-size:12px;margin-top:5px;}"
        ".status-box{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:20px 0;max-width:100%;}"
        ".status-row{display:flex;justify-content:space-between;margin:8px 0;padding:8px 0;border-bottom:1px solid #333;flex-wrap:wrap;}"
        ".status-label{color:#888;flex:1;min-width:120px;}"
        ".status-value{color:#fff;font-weight:bold;text-align:right;flex:1;min-width:100px;}"
        ".section-header{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:12px;margin:15px 0 0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"
        ".section-header:hover{background:#333;}"
        ".section-header h3{margin:0;color:#00BCD4;font-size:16px;}"
        ".section-content{display:none;padding:15px;background:#1a1a1a;border:1px solid #444;border-top:none;border-radius:0 0 4px 4px;}"
        ".section-content.active{display:block;}"
        ".section-desc{color:#888;font-size:12px;margin-top:5px;margin-bottom:15px;}"
        ".toggle-icon{color:#00BCD4;font-size:18px;flex-shrink:0;margin-left:10px;}"
        "@media (max-width:600px){"
        "body{padding:5px;}"
        ".container{padding:0 5px;}"
        "h1{font-size:20px;margin:10px 0;}"
        ".status-box{padding:10px;margin:15px 0;}"
        ".status-row{flex-direction:column;padding:6px 0;}"
        ".status-label{min-width:auto;margin-bottom:3px;}"
        ".status-value{text-align:left;min-width:auto;}"
        ".section-header{padding:10px;}"
        ".section-header h3{font-size:14px;}"
        ".section-content{padding:10px;}"
        "button{padding:10px 20px;font-size:14px;}"
        "label{font-size:14px;}"
        "input[type=number],input[type=text],select{font-size:14px;padding:6px;}"
        "}"
        "</style>";
    server->sendContent_P(cssBlock);
    
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
    
    // WEB-PERF-3: Refresh status direct na anchor set (geen full reload)
    server->sendContent(F("refreshStatus();"));
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
    // TP/SL Strategie dropdown JavaScript
    server->sendContent(F("var strategySelect=document.querySelector('select[name=\"anchorStrategy\"]');"));
    server->sendContent(F("if(strategySelect){"));
    server->sendContent(F("strategySelect.addEventListener('change',function(){"));
    server->sendContent(F("var val=parseInt(this.value);"));
    server->sendContent(F("var tpInput=document.querySelector('input[name=\"anchorTP\"]');"));
    server->sendContent(F("var mlInput=document.querySelector('input[name=\"anchorML\"]');"));
    server->sendContent(F("if(val==1){"));
    server->sendContent(F("if(tpInput)tpInput.value='1.8';"));
    server->sendContent(F("if(mlInput)mlInput.value='-1.2';"));
    server->sendContent(F("}else if(val==2){"));
    server->sendContent(F("if(tpInput)tpInput.value='1.2';"));
    server->sendContent(F("if(mlInput)mlInput.value='-0.9';"));
    server->sendContent(F("}"));
    server->sendContent(F("});"));
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
    
    // WEB-PERF-3: Live status updates via /status endpoint
    server->sendContent(F("function refreshStatus(){"));
    server->sendContent(F("fetch('/status').then(function(r){return r.json();}).then(function(d){"));
    server->sendContent(F("var el=document.getElementById('curPrice');if(el)el.textContent=d.price>0?d.price.toFixed(2)+' EUR':'--';"));
    server->sendContent(F("el=document.getElementById('trend');if(el)el.textContent=d.trend||'--';"));
    server->sendContent(F("el=document.getElementById('volatility');if(el)el.textContent=d.volatility||'--';"));
    server->sendContent(F("el=document.getElementById('ret1m');if(el)el.textContent=d.ret1m!=0?d.ret1m.toFixed(2)+'%':'--';"));
    server->sendContent(F("el=document.getElementById('ret30m');if(el)el.textContent=d.ret30m!=0?d.ret30m.toFixed(2)+'%':'--';"));
    server->sendContent(F("el=document.getElementById('anchor');if(el)el.textContent=d.anchor>0?d.anchor.toFixed(2)+' EUR':'--';"));
    server->sendContent(F("el=document.getElementById('anchorDelta');if(el)el.textContent=d.anchorDeltaPct!=0?d.anchorDeltaPct.toFixed(2)+'%':'--';"));
    server->sendContent(F("el=document.getElementById('apiState');if(el)el.textContent='';"));
    server->sendContent(F("}).catch(function(e){"));
    server->sendContent(F("var el=document.getElementById('apiState');if(el)el.textContent='NET?';"));
    server->sendContent(F("});"));
    server->sendContent(F("}"));
    server->sendContent(F("refreshStatus();"));
    server->sendContent(F("setInterval(refreshStatus,2000);"));
    
    server->sendContent(F("});"));
    server->sendContent(F("})();"));
    server->sendContent(F("</script>"));
    server->sendContent(F("</head><body>"));
    server->sendContent(F("<div class='container'>"));
    
    // Title - gebruik binanceSymbol in plaats van platformName, voeg versie toe
    char h1Buf[128];
    snprintf(h1Buf, sizeof(h1Buf), "<h1>%s %s %s v%s</h1>", 
             getText("Instellingen", "Settings"), binanceSymbol, ntfyTopic, VERSION_STRING);
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

void WebServerModule::sendDropdownRow(const char* label, const char* name, int value, const char* options[], int optionCount) {
    if (server == nullptr) return;
    server->sendContent("<tr><td>");
    server->sendContent(label);
    server->sendContent(":</td><td><select name=\"");
    server->sendContent(name);
    server->sendContent("\">");
    for (int i = 0; i < optionCount; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "<option value=\"%d\"%s>%s</option>",
                 i, (i == value) ? " selected" : "", options[i]);
        server->sendContent(buf);
    }
    server->sendContent("</select></td></tr>");
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

// Helper: Get trend text (geoptimaliseerd: elimineert switch duplicatie)
const char* WebServerModule::getTrendText(TrendState trend) {
    switch (trend) {
        case TREND_UP: return getText("OMHOOG", "UP");
        case TREND_DOWN: return getText("OMLAAG", "DOWN");
        case TREND_SIDEWAYS:
        default: return getText("VLAK", "SIDEWAYS");
    }
}

// Helper: Get volatility text (geoptimaliseerd: elimineert switch duplicatie)
const char* WebServerModule::getVolatilityText(VolatilityState vol) {
    switch (vol) {
        case VOLATILITY_LOW: return getText("Laag", "Low");
        case VOLATILITY_MEDIUM: return getText("Gemiddeld", "Medium");
        case VOLATILITY_HIGH: return getText("Hoog", "High");
        default: return getText("Onbekend", "Unknown");
    }
}

// Helper: Parse float argument (geoptimaliseerd: elimineert code duplicatie)
bool WebServerModule::parseFloatArg(const char* argName, float& result, float minVal, float maxVal) {
    if (!server || !server->hasArg(argName)) {
        return false;
    }
    // Fix: gebruik String object eerst om dangling pointer te voorkomen
    String argStr = server->arg(argName);
    float val;
    if (safeAtof(argStr.c_str(), val) && val >= minVal && val <= maxVal) {
        result = val;
        return true;
    }
    return false;
}

// Helper: Parse int argument (geoptimaliseerd: elimineert code duplicatie, gebruikt C-style i.p.v. String)
bool WebServerModule::parseIntArg(const char* argName, int& result, int minVal, int maxVal) {
    if (!server || !server->hasArg(argName)) {
        return false;
    }
    // Fix: gebruik String object eerst om dangling pointer te voorkomen
    String argStr = server->arg(argName);
    int val = atoi(argStr.c_str());
    if (val >= minVal && val <= maxVal) {
        result = val;
        return true;
    }
    return false;
}

// Helper: Parse string argument (geoptimaliseerd: elimineert code duplicatie, gebruikt C-style i.p.v. String)
bool WebServerModule::parseStringArg(const char* argName, char* dest, size_t destSize) {
    if (!server || !server->hasArg(argName)) {
        return false;
    }
    // Fix: gebruik String object eerst om dangling pointer te voorkomen
    String str = server->arg(argName);
    size_t strLen = str.length();
    
    if (strLen >= destSize) {
        strLen = destSize - 1;
    }
    
    // Copy to temp buffer for trimming
    char tempBuffer[128];  // Max size for most string args
    if (strLen >= sizeof(tempBuffer)) {
        strLen = sizeof(tempBuffer) - 1;
    }
    strncpy(tempBuffer, str.c_str(), strLen);
    tempBuffer[strLen] = '\0';
    
    // Trim leading/trailing whitespace
    char* start = tempBuffer;
    while (*start == ' ' || *start == '\t') start++;
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    strLen = strlen(start);
    
    if (strLen > 0 && strLen < destSize) {
        strncpy(dest, start, strLen);
        dest[strLen] = '\0';
        return true;
    }
    return false;
}
