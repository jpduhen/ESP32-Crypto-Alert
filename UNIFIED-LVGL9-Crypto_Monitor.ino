// Tutorial : https://youtu.be/JqQEG0eipic
// Unified Crypto Monitor - Supports TTGO T-Display and CYD 2.8
// Select platform in platform_config.h

#define LV_CONF_INCLUDE_SIMPLE // Use the lv_conf.h included in this project, to configure see https://docs.lvgl.io/master/get-started/platforms/arduino.html

// Platform config moet als eerste, definieert platform-specifieke instellingen
#include "platform_config.h"

#include <WiFi.h>                   // Included with Espressif ESP32 Dev Module
#include <HTTPClient.h>             // Included with Espressif ESP32 Dev Module
#include <WiFiManager.h>            // Install "WiFiManager" with the Library Manager
#include <WebServer.h>              // Included with Espressif ESP32 Dev Module
#include <Preferences.h>            // Included with Espressif ESP32 Dev Module
#include <PubSubClient.h>           // Install "PubSubClient3" from https://github.com/hmueller01/pubsubclient3
#include "atomic.h"                 // Included in this project
#include <lvgl.h>                   // Install "lvgl" with the Library Manager (last tested on v9.2.2)
#include "Arduino.h"

// Touchscreen functionaliteit volledig verwijderd - gebruik nu fysieke boot knop (GPIO 0)
#include <SPI.h>
#include <time.h>                   // For time functions
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

// ============================================================================
// Constants and Configuration
// ============================================================================

// --- Version and Build Configuration ---
#define VERSION_MAJOR 3
#define VERSION_MINOR 43
#define VERSION_STRING "3.43"

// --- Debug Configuration ---
#define DEBUG_BUTTON_ONLY 1  // Zet op 1 om alleen knop-acties te loggen, 0 voor alle logging

#if DEBUG_BUTTON_ONLY
    // Disable all Serial output except button actions
    #define Serial_printf(...) ((void)0)
    #define Serial_println(...) ((void)0)
    #define Serial_print(...) ((void)0)
#else
    // Normal Serial output
    #define Serial_printf Serial.printf
    #define Serial_println Serial.println
    #define Serial_print Serial.print
#endif

// --- Display Configuration ---
#define SCREEN_BRIGHTNESS 255  // Screen brightness (0-255)

// --- Symbol Configuration ---
#define SYMBOL_COUNT 3  // Number of symbols to track live on Binance

// --- API Configuration ---
#define BINANCE_API "https://api.binance.com/api/v3/ticker/price?symbol="  // Binance API endpoint
#define BINANCE_SYMBOL_DEFAULT "BTCEUR"  // Default Binance symbol
#define HTTP_TIMEOUT_MS 3000  // HTTP timeout (verhoogd voor betere stabiliteit bij langzame netwerken)
#define HTTP_CONNECT_TIMEOUT_MS 2000  // Connect timeout (sneller falen bij connect problemen)

// --- Chart Configuration ---
#define PRICE_RANGE 200         // The range of price for the chart, adjust as needed
#define POINTS_TO_CHART 60      // Number of points on the chart (60 = 1 minute of data)

// --- Timing Configuration ---
#define UPDATE_UI_INTERVAL 1100   // UI update in ms (iets verschoven van API voor minder conflicten)
#define UPDATE_API_INTERVAL 1500   // API update in ms (verhoogd naar 1500ms voor betere stabiliteit bij langzame netwerken)
#define UPDATE_WEB_INTERVAL 5000  // Web interface update in ms (elke 5 seconden)
#define RECONNECT_INTERVAL 60000  // WiFi reconnect interval (60 seconden tussen reconnect pogingen)
#define MQTT_RECONNECT_INTERVAL 5000  // MQTT reconnect interval (5 seconden)

// --- Anchor Price Configuration ---
#define ANCHOR_TAKE_PROFIT_DEFAULT 5.0f    // Take profit: +5% boven anchor price
#define ANCHOR_MAX_LOSS_DEFAULT -3.0f      // Max loss: -3% onder anchor price

// --- Trend Detection Configuration ---
#define TREND_THRESHOLD_DEFAULT 1.2f       // Trend threshold: ±1.2% voor 2h trend (geoptimaliseerd op basis van metingen)
#define TREND_CHANGE_COOLDOWN_MS 600000UL  // 10 minuten cooldown voor trend change notificaties

// --- Volatility Configuration ---
#define VOLATILITY_LOW_THRESHOLD_DEFAULT 0.05f   // Volatiliteit laag: < 0.05% (geoptimaliseerd voor rustige nachturen)
#define VOLATILITY_HIGH_THRESHOLD_DEFAULT 0.15f  // Volatiliteit hoog: >= 0.15% (geoptimaliseerd voor piekactiviteit)
#define VOLATILITY_LOOKBACK_MINUTES 60  // Bewaar laatste 60 minuten aan absolute 1m returns

// --- Notification Configuration ---
// Grenswaarden voor notificaties (in percentage per tijdseenheid)
#define THRESHOLD_1MIN_UP_DEFAULT 0.5f     // Notificatie bij stijgende trend > 0.5% per minuut
#define THRESHOLD_1MIN_DOWN_DEFAULT -0.5f // Notificatie bij dalende trend < -0.5% per minuut
#define THRESHOLD_30MIN_UP_DEFAULT 2.0f   // Notificatie bij stijgende trend > 2% per uur
#define THRESHOLD_30MIN_DOWN_DEFAULT -2.0f // Notificatie bij dalende trend < -2% per uur

// Spike/Move alert thresholds (geoptimaliseerd op basis van metingen)
#define SPIKE_1M_THRESHOLD_DEFAULT 0.28f   // 1m spike: |ret_1m| >= 0.28% (op basis van ruis-maxima)
#define SPIKE_5M_THRESHOLD_DEFAULT 0.65f   // 5m spike filter: |ret_5m| >= 0.65% (past bij actuele volatiliteit)
#define MOVE_30M_THRESHOLD_DEFAULT 1.3f    // 30m move: |ret_30m| >= 1.3% (0.8% was te gevoelig)
#define MOVE_5M_THRESHOLD_DEFAULT 0.40f    // 5m move filter: |ret_5m| >= 0.40% (gevoeliger op momentum-opbouw)
#define MOVE_5M_ALERT_THRESHOLD_DEFAULT 0.8f  // 5m move alert: |ret_5m| >= 0.8% (historisch vaak bij trend start)

// Cooldown tijden (in milliseconden) om spam te voorkomen (geoptimaliseerd op basis van metingen)
#define NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT 90000    // 90 seconden tussen 1-minuut spike notificaties (minder spam in snelle pumps)
#define NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT 900000  // 15 minuten tussen 30-minuten move notificaties (grote moves → langere rust)
#define NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT 420000   // 7 minuten tussen 5-minuten move notificaties (sneller tweede signaal bij doorbraak)

// Max alerts per uur
#define MAX_1M_ALERTS_PER_HOUR 3
#define MAX_30M_ALERTS_PER_HOUR 2
#define MAX_5M_ALERTS_PER_HOUR 3

// --- MQTT Configuration ---
#define MQTT_HOST_DEFAULT "192.168.1.100"  // Standaard MQTT broker IP (pas aan naar jouw MQTT broker)
#define MQTT_PORT_DEFAULT 1883             // Standaard MQTT poort
#define MQTT_USER_DEFAULT "mqtt_user"       // Standaard MQTT gebruiker (pas aan)
#define MQTT_PASS_DEFAULT "mqtt_password"  // Standaard MQTT wachtwoord (pas aan)

// --- Language Configuration ---
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // Standaard: Nederlands (0 = Nederlands, 1 = English)
#endif

// --- Array Size Configuration ---
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_5MINUTES 300
#define MINUTES_FOR_30MIN_CALC 120

// --- CPU Measurement Configuration ---
#define CPU_MEASUREMENT_SAMPLES 20  // Meet over 20 loops voor gemiddelde


// ============================================================================
// Global Variables
// ============================================================================

// Touchscreen functionaliteit volledig verwijderd - CYD's gebruiken nu fysieke boot knop (GPIO 0)

// LVGL Display global variables
lv_display_t *disp;

// Widgets LVGL global variables
static lv_obj_t *chart;
static lv_chart_series_t *dataSeries;     // Blauwe serie voor alle punten
static lv_obj_t *lblFooterLine1; // Footer regel 1 (alleen voor CYD: dBm links, RAM rechts)
static lv_obj_t *lblFooterLine2; // Footer regel 2 (alleen voor CYD: IP links, versie rechts)
static lv_obj_t *ramLabel; // RAM label rechts op regel 1 (alleen voor CYD)

// One card per symbol
static lv_obj_t *priceBox[SYMBOL_COUNT];
static lv_obj_t *priceTitle[SYMBOL_COUNT];
static lv_obj_t *priceLbl[SYMBOL_COUNT];
// resetBtn en resetPressed verwijderd - functionaliteit nu op hele BTCEUR blok

// FreeRTOS mutex voor data synchronisatie tussen cores
SemaphoreHandle_t dataMutex = NULL;
// Symbols array - eerste element wordt dynamisch ingesteld via binanceSymbol
static char symbolsArray[SYMBOL_COUNT][16] = {"BTCEUR", SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL};
static const char *symbols[SYMBOL_COUNT] = {symbolsArray[0], symbolsArray[1], symbolsArray[2]};
static float prices[SYMBOL_COUNT] = {0};
static float openPrices[SYMBOL_COUNT] = {0};
static float averagePrices[SYMBOL_COUNT] = {0}; // Gemiddelde prijzen voor 1 min en 30 min


// Anchor price (referentie prijs voor koop/verkoop tracking)
static float anchorPrice = 0.0f;
static float anchorMax = 0.0f;  // Hoogste prijs sinds anchor
static float anchorMin = 0.0f;  // Laagste prijs sinds anchor
static unsigned long anchorTime = 0;
static bool anchorActive = false;
// anchorSetPending verwijderd - niet meer nodig zonder touchscreen
static bool anchorNotificationPending = false;  // Flag voor pending anchor set notificatie
static float anchorTakeProfit = ANCHOR_TAKE_PROFIT_DEFAULT;  // Take profit threshold (%)
static float anchorMaxLoss = ANCHOR_MAX_LOSS_DEFAULT;        // Max loss threshold (%)
static bool anchorTakeProfitSent = false;  // Flag om te voorkomen dat take profit meerdere keren wordt verzonden
static bool anchorMaxLossSent = false;    // Flag om te voorkomen dat max loss meerdere keren wordt verzonden

// Trend detection
enum TrendState {
    TREND_UP,
    TREND_DOWN,
    TREND_SIDEWAYS
};
static float ret_2h = 0.0f;  // 2-hour return percentage
static TrendState trendState = TREND_SIDEWAYS;  // Current trend state
static TrendState previousTrendState = TREND_SIDEWAYS;  // Previous trend state (voor change detection)
static float trendThreshold = TREND_THRESHOLD_DEFAULT;  // Trend threshold (%)

// Volatiliteit detection
enum VolatilityState {
    VOLATILITY_LOW,      // Rustig: < 0.05%
    VOLATILITY_MEDIUM,  // Gemiddeld: 0.05% - 0.15%
    VOLATILITY_HIGH     // Volatiel: >= 0.15%
};
static float abs1mReturns[VOLATILITY_LOOKBACK_MINUTES];  // Array voor absolute 1m returns
static uint8_t volatilityIndex = 0;  // Index voor circulaire buffer
static bool volatilityArrayFilled = false;  // Flag om aan te geven of array gevuld is
static VolatilityState volatilityState = VOLATILITY_MEDIUM;  // Current volatility state
static float volatilityLowThreshold = VOLATILITY_LOW_THRESHOLD_DEFAULT;  // Low threshold (%)
static float volatilityHighThreshold = VOLATILITY_HIGH_THRESHOLD_DEFAULT;  // High threshold (%)
static unsigned long lastTrendChangeNotification = 0;  // Timestamp van laatste trend change notificatie

static uint8_t symbolIndexToChart = 0; // The symbol index to chart
static uint32_t maxRange;
static uint32_t minRange;
// chartMaxLabel verwijderd - niet meer nodig
static lv_obj_t *chartTitle;     // Label voor chart titel (symbool) - alleen voor CYD
static lv_obj_t *chartVersionLabel; // Label voor versienummer (rechts bovenste regel)
static lv_obj_t *chartDateLabel; // Label voor datum rechtsboven (vanaf pixel 180)
static lv_obj_t *chartTimeLabel; // Label voor tijd rechtsboven
static lv_obj_t *chartBeginLettersLabel; // Label voor beginletters (TTGO, links tweede regel)
static lv_obj_t *ipLabel; // IP-adres label (TTGO, onderin, gecentreerd)
static lv_obj_t *price1MinMaxLabel; // Label voor max waarde in 1 min buffer
static lv_obj_t *price1MinMinLabel; // Label voor min waarde in 1 min buffer
static lv_obj_t *price1MinDiffLabel; // Label voor verschil tussen max en min in 1 min buffer
static lv_obj_t *price30MinMaxLabel; // Label voor max waarde in 30 min buffer
static lv_obj_t *price30MinMinLabel; // Label voor min waarde in 30 min buffer
static lv_obj_t *price30MinDiffLabel; // Label voor verschil tussen max en min in 30 min buffer
// anchorButton en anchorButtonLabel verwijderd - niet meer nodig zonder touchscreen
static lv_obj_t *anchorLabel; // Label voor anchor price info (rechts midden, met percentage verschil)
static lv_obj_t *anchorMaxLabel; // Label voor "Pak winst" (rechts, groen, boven)
static lv_obj_t *anchorMinLabel; // Label voor "Stop loss" (rechts, rood, onder)
static lv_obj_t *anchorDeltaLabel; // Label voor anchor delta % (TTGO, rechts)
static lv_obj_t *trendLabel; // Label voor trend weergave
static lv_obj_t *volatilityLabel; // Label voor volatiliteit weergave

static uint32_t lastApiMs = 0; // Time of last api call

// CPU usage measurement (alleen voor web interface)
static float cpuUsagePercent = 0.0f;
static unsigned long loopTimeSum = 0;
static uint16_t loopCount = 0;
static const unsigned long LOOP_PERIOD_MS = UPDATE_UI_INTERVAL; // 1100ms

// Price history for calculating returns and moving averages
// Array van 60 posities voor laatste 60 seconden (1 minuut)
static float secondPrices[SECONDS_PER_MINUTE];
static uint8_t secondIndex = 0;
static bool secondArrayFilled = false;
static bool newPriceDataAvailable = false;  // Flag om aan te geven of er nieuwe prijsdata is voor grafiek update

// Array van 300 posities voor laatste 300 seconden (5 minuten) - voor ret_5m berekening
static float fiveMinutePrices[SECONDS_PER_5MINUTES];
static uint16_t fiveMinuteIndex = 0;
static bool fiveMinuteArrayFilled = false;

// Array van 120 posities voor laatste 120 minuten (2 uur)
// Elke minuut wordt het gemiddelde van de 60 seconden opgeslagen
// We hebben 60 posities nodig om het gemiddelde van laatste 30 minuten te vergelijken
// met het gemiddelde van de 30 minuten daarvoor (maar we houden 120 voor buffer)
static float minuteAverages[MINUTES_FOR_30MIN_CALC];
static uint8_t minuteIndex = 0;
static bool minuteArrayFilled = false;
static unsigned long lastMinuteUpdate = 0;
static float firstMinuteAverage = 0.0f; // Eerste minuut gemiddelde prijs als basis voor 30-min berekening

// Notification settings - NTFY.sh
// Note: NTFY topic wordt dynamisch gegenereerd met ESP32 device ID
// Format: [ESP32-ID]-alert (bijv. 9MK28H3Q-alert)
// ESP32-ID is 8 karakters (Crockford Base32 encoding) voor veilige, unieke identificatie
// Dit voorkomt conflicten tussen verschillende devices

// Language setting (0 = Nederlands, 1 = English)
// DEFAULT_LANGUAGE wordt gedefinieerd in platform_config.h (fallback als er nog geen waarde in Preferences staat)
static uint8_t language = DEFAULT_LANGUAGE;  // 0 = Nederlands, 1 = English

// Instelbare grenswaarden (worden geladen uit Preferences)
// Note: ntfyTopic wordt geïnitialiseerd in loadSettings() met unieke ESP32 ID
static char ntfyTopic[64] = "";  // NTFY topic (max 63 karakters)
static char binanceSymbol[16] = BINANCE_SYMBOL_DEFAULT;  // Binance symbool (max 15 karakters, bijv. BTCEUR, BTCUSDT)
static float threshold1MinUp = THRESHOLD_1MIN_UP_DEFAULT;
static float threshold1MinDown = THRESHOLD_1MIN_DOWN_DEFAULT;
static float threshold30MinUp = THRESHOLD_30MIN_UP_DEFAULT;
static float threshold30MinDown = THRESHOLD_30MIN_DOWN_DEFAULT;
static float spike1mThreshold = SPIKE_1M_THRESHOLD_DEFAULT;
static float spike5mThreshold = SPIKE_5M_THRESHOLD_DEFAULT;
static float move30mThreshold = MOVE_30M_THRESHOLD_DEFAULT;
static float move5mThreshold = MOVE_5M_THRESHOLD_DEFAULT;
static float move5mAlertThreshold = MOVE_5M_ALERT_THRESHOLD_DEFAULT;  // 5m move alert threshold
static unsigned long notificationCooldown1MinMs = NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT;
static unsigned long notificationCooldown30MinMs = NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT;
static unsigned long notificationCooldown5MinMs = NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT;

static unsigned long lastNotification1Min = 0;
static unsigned long lastNotification30Min = 0;
static unsigned long lastNotification5Min = 0;

// Max alerts per uur tracking
static uint8_t alerts1MinThisHour = 0;
static uint8_t alerts30MinThisHour = 0;
static uint8_t alerts5MinThisHour = 0;
static unsigned long hourStartTime = 0; // Starttijd van het huidige uur

// Web server voor instellingen
WebServer server(80);
Preferences preferences;

// MQTT configuratie (instelbaar via web interface)
static char mqttHost[64] = MQTT_HOST_DEFAULT;    // MQTT broker IP
static uint16_t mqttPort = MQTT_PORT_DEFAULT;    // MQTT poort
static char mqttUser[64] = MQTT_USER_DEFAULT;     // MQTT gebruiker
static char mqttPass[64] = MQTT_PASS_DEFAULT;     // MQTT wachtwoord
// MQTT_CLIENT_ID_PREFIX wordt nu gedefinieerd in platform_config.h als MQTT_TOPIC_PREFIX
#define MQTT_CLIENT_ID_PREFIX MQTT_TOPIC_PREFIX "_"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;

// WiFi reconnect controle
// Geoptimaliseerd: betere reconnect logica met retry counter
static bool wifiReconnectEnabled = false;
static unsigned long lastReconnectAttempt = 0;
static bool wifiInitialized = false;
static uint8_t reconnectAttemptCount = 0;
static const uint8_t MAX_RECONNECT_ATTEMPTS = 5; // Max aantal reconnect pogingen voordat we wachten


// ============================================================================
// HTTP and API Functions
// ============================================================================

// Simple HTTP GET – returns body as String or empty on fail
// Geoptimaliseerd: betere error handling, timeouts en resource cleanup
static String httpGET(const char *url, uint32_t timeoutMs = HTTP_TIMEOUT_MS)
{
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS); // Sneller falen bij connect problemen
    http.setReuse(false); // Voorkom connection reuse problemen
    
    unsigned long requestStart = millis();
    
    if (!http.begin(url))
    {
        http.end();
        Serial_printf("[HTTP] http.begin() gefaald voor: %s\n", url);
        return String();
    }
    
    int code = http.GET();
    unsigned long requestTime = millis() - requestStart;
    String payload;
    
    if (code == 200)
    {
        payload = http.getString();
        // Log alleen bij langzame calls (> 1500ms) voor debugging
        if (requestTime > 1500) {
            Serial_printf("[HTTP] Langzame response: %lu ms\n", requestTime);
        }
    }
    else
    {
        // Log alleen bij echte fouten (niet bij 200)
        if (code > 0)
        {
            Serial_printf("[HTTP] GET gefaald: code=%d, tijd=%lu ms\n", code, requestTime);
        }
        else if (code == HTTPC_ERROR_CONNECTION_REFUSED || code == HTTPC_ERROR_CONNECTION_LOST)
        {
            Serial_printf("[HTTP] Connectie probleem: code=%d, tijd=%lu ms\n", code, requestTime);
        }
        else if (code == HTTPC_ERROR_READ_TIMEOUT)
        {
            Serial_printf("[HTTP] Read timeout na %lu ms\n", requestTime);
        }
        else if (code < 0)
        {
            // Andere HTTPClient error codes (negatieve waarden zijn error codes)
            Serial_printf("[HTTP] Error code=%d, tijd=%lu ms\n", code, requestTime);
        }
    }
    
    http.end();
    return payload;
}

// Send notification via Ntfy.sh
// colorTag: "green_square" voor stijging, "red_square" voor daling, "blue_square" voor neutraal
// Geoptimaliseerd: betere error handling en resource cleanup
static bool sendNtfyNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    // Check WiFi verbinding eerst
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial_println("[Notify] WiFi niet verbonden, kan NTFY notificatie niet versturen");
        return false;
    }
    
    // Valideer inputs
    if (strlen(ntfyTopic) == 0)
    {
        Serial_println("[Notify] Ntfy topic niet geconfigureerd");
        return false;
    }
    
    if (title == nullptr || message == nullptr)
    {
        Serial_println("[Notify] Ongeldige title of message pointer");
        return false;
    }
    
    // Valideer lengte van inputs om buffer overflows te voorkomen
    if (strlen(title) > 64 || strlen(message) > 512)
    {
        Serial_println("[Notify] Title of message te lang");
        return false;
    }
    
    char url[128];
    int urlLen = snprintf(url, sizeof(url), "https://ntfy.sh/%s", ntfyTopic);
    if (urlLen < 0 || urlLen >= (int)sizeof(url))
    {
        Serial_println("[Notify] URL buffer overflow");
        return false;
    }
    
    Serial_printf("[Notify] Ntfy URL: %s\n", url);
    Serial_printf("[Notify] Ntfy Title: %s\n", title);
    Serial_printf("[Notify] Ntfy Message: %s\n", message);
    
    HTTPClient http;
    http.setTimeout(5000);
    http.setReuse(false); // Voorkom connection reuse problemen
    
    if (!http.begin(url))
    {
        Serial_println("[Notify] Ntfy HTTP begin gefaald");
        http.end();
        return false;
    }
    
    http.addHeader("Title", title);
    http.addHeader("Priority", "high");
    
    // Voeg kleur tag toe als opgegeven
    if (colorTag != nullptr && strlen(colorTag) > 0)
    {
        if (strlen(colorTag) <= 64) // Valideer lengte
        {
            http.addHeader("Tags", colorTag);
            Serial_printf("[Notify] Ntfy Tag: %s\n", colorTag);
        }
    }
    
    Serial_println("[Notify] Ntfy POST versturen...");
    int code = http.POST(message);
    
    // Haal response alleen op bij succes (bespaar geheugen)
    String response;
    if (code == 200 || code == 201)
    {
        response = http.getString();
        if (response.length() > 0)
            Serial_printf("[Notify] Ntfy response: %s\n", response.c_str());
    }
    
    http.end();
    
    bool result = (code == 200 || code == 201);
    if (result)
    {
        Serial_printf("[Notify] Ntfy bericht succesvol verstuurd! (code: %d)\n", code);
    }
    else
    {
        Serial_printf("[Notify] Ntfy fout bij versturen (code: %d)\n", code);
    }
    
    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

// Get formatted timestamp string (dd-mm-yyyy hh:mm:ss)
static void getFormattedTimestamp(char *buffer, size_t bufferSize) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(buffer, bufferSize, "%d-%m-%Y %H:%M:%S", &timeinfo);
    } else {
        // Fallback als tijd niet beschikbaar is
        snprintf(buffer, bufferSize, "?\\?-?\\?-???? ??:??:??");
    }
}

// Format IP address to string (geoptimaliseerd: gebruik char array i.p.v. String)
static void formatIPAddress(IPAddress ip, char *buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// ============================================================================
// Notification Functions
// ============================================================================

// Send notification via NTFY
static bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    return sendNtfyNotification(title, message, colorTag);
}

// Check trend change en stuur notificatie indien nodig
static void checkTrendChange(float ret_30m_value)
{
    unsigned long now = millis();
    
    // Check of trend state is veranderd
    if (trendState != previousTrendState)
    {
        // Check cooldown: max 1 trend-change notificatie per 10 minuten
        bool cooldownPassed = (lastTrendChangeNotification == 0 || 
                               (now - lastTrendChangeNotification >= TREND_CHANGE_COOLDOWN_MS));
        
        // Alleen notificeren als cooldown is verstreken en we hebben geldige data
        if (cooldownPassed && ret_2h != 0.0f && (minuteArrayFilled || minuteIndex >= 120))
        {
            const char* fromTrend = "";
            const char* toTrend = "";
            const char* colorTag = "";
            
            // Bepaal tekst voor vorige trend
            switch (previousTrendState)
            {
                case TREND_UP:
                    fromTrend = "UP";
                    break;
                case TREND_DOWN:
                    fromTrend = "DOWN";
                    break;
                case TREND_SIDEWAYS:
                default:
                    fromTrend = "SIDEWAYS";
                    break;
            }
            
            // Bepaal tekst voor nieuwe trend
            switch (trendState)
            {
                case TREND_UP:
                    toTrend = "UP";
                    colorTag = "green_square,📈";
                    break;
                case TREND_DOWN:
                    toTrend = "DOWN";
                    colorTag = "red_square,📉";
                    break;
                case TREND_SIDEWAYS:
                default:
                    toTrend = "SIDEWAYS";
                    colorTag = "grey_square,➡️";
                    break;
            }
            
            // Bepaal volatiliteit tekst
            const char* volText = "";
            switch (volatilityState)
            {
                case VOLATILITY_LOW:
                    volText = "Rustig";
                    break;
                case VOLATILITY_MEDIUM:
                    volText = "Gemiddeld";
                    break;
                case VOLATILITY_HIGH:
                    volText = "Volatiel";
                    break;
            }
            
            char title[64];
            char msg[256];
            snprintf(title, sizeof(title), "%s Trend Change", binanceSymbol);
            snprintf(msg, sizeof(msg), 
                     "Trend change: %s → %s\n2h: %+.2f%%\n30m: %+.2f%%\nVol: %s",
                     fromTrend, toTrend, ret_2h, ret_30m_value, volText);
            
            sendNotification(title, msg, colorTag);
            lastTrendChangeNotification = now;
            
            Serial_printf("[Trend] Trend change notificatie verzonden: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n", 
                         fromTrend, toTrend, ret_2h, ret_30m_value, volText);
        }
        
        // Update previous trend state
        previousTrendState = trendState;
    }
}

// ============================================================================
// Anchor Price Functions
// ============================================================================

// Publiceer anchor event naar MQTT
void publishMqttAnchorEvent(float anchor_price, const char* event_type) {
    if (!mqttConnected) return;
    
    // Haal lokale tijd op
    struct tm timeinfo;
    char timeStr[25] = "";
    if (getLocalTime(&timeinfo)) {
        // Format: ISO 8601 (2025-11-26T21:34:00)
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    } else {
        // Fallback: gebruik millis() als timestamp
        snprintf(timeStr, sizeof(timeStr), "%lu", millis());
    }
    
    // Maak JSON payload
    char payload[256];
    snprintf(payload, sizeof(payload), 
             "{\"time\":\"%s\",\"price\":%.2f,\"event\":\"%s\"}",
             timeStr, anchor_price, event_type);
    
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/anchor/event", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topic, payload, false);
    Serial_printf("[MQTT] Anchor event gepubliceerd: %s (prijs: %.2f, event: %s)\n", 
                 timeStr, anchor_price, event_type);
}

// Check anchor take profit / max loss alerts
static void checkAnchorAlerts()
{
    if (!anchorActive || anchorPrice <= 0.0f || prices[0] <= 0.0f) {
        return; // Geen actieve anchor of geen prijs data
    }
    
    // Bereken percentage verandering t.o.v. anchor
    float anchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
    
    // Check take profit
    if (!anchorTakeProfitSent && anchorPct >= anchorTakeProfit) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[256];
        snprintf(title, sizeof(title), "%s Take Profit", binanceSymbol);
        snprintf(msg, sizeof(msg), 
                 "Take profit bereikt: +%.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nWinst: +%.2f EUR",
                 anchorPct, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        sendNotification(title, msg, "green_square,💰");
        anchorTakeProfitSent = true;
        Serial_printf("[Anchor] Take profit notificatie verzonden: %.2f%% (anchor: %.2f, prijs: %.2f)\n", 
                     anchorPct, anchorPrice, prices[0]);
        
        // Publiceer take profit event naar MQTT
        publishMqttAnchorEvent(anchorPrice, "take_profit");
    }
    
    // Check max loss
    if (!anchorMaxLossSent && anchorPct <= anchorMaxLoss) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[256];
        snprintf(title, sizeof(title), "%s Max Loss", binanceSymbol);
        snprintf(msg, sizeof(msg), 
                 "Max loss bereikt: %.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nVerlies: %.2f EUR",
                 anchorPct, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        sendNotification(title, msg, "red_square,⚠️");
        anchorMaxLossSent = true;
        Serial_printf("[Anchor] Max loss notificatie verzonden: %.2f%% (anchor: %.2f, prijs: %.2f)\n", 
                     anchorPct, anchorPrice, prices[0]);
        
        // Publiceer max loss event naar MQTT
        publishMqttAnchorEvent(anchorPrice, "max_loss");
    }
}

// Check thresholds and send notifications if needed
// ret_1m: percentage verandering laatste 1 minuut
// ret_5m: percentage verandering laatste 5 minuten (voor filtering)
// ret_30m: percentage verandering laatste 30 minuten
static void checkAndNotify(float ret_1m, float ret_5m, float ret_30m)
{
    unsigned long now = millis();
    
    // Reset tellers elk uur
    if (hourStartTime == 0 || (now - hourStartTime >= 3600000UL)) { // 1 uur = 3600000 ms
        alerts1MinThisHour = 0;
        alerts30MinThisHour = 0;
        hourStartTime = now;
        Serial_printf("[Notify] Uur-tellers gereset\n");
    }
    
    // ===== 1-MINUUT SPIKE ALERT =====
    // Voorwaarde: |ret_1m| >= 0.30% EN |ret_5m| >= 0.60% in dezelfde richting
    if (ret_1m != 0.0f && ret_5m != 0.0f)
    {
        float absRet1m = fabsf(ret_1m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn (beide positief of beide negatief)
        bool sameDirection = ((ret_1m > 0 && ret_5m > 0) || (ret_1m < 0 && ret_5m < 0));
        
        // Threshold check: ret_1m >= spike1mThreshold EN ret_5m >= spike5mThreshold
        bool threshold1mMet = (absRet1m >= spike1mThreshold);
        bool threshold5mMet = (absRet5m >= spike5mThreshold);
        bool spikeDetected = threshold1mMet && threshold5mMet && sameDirection;
        
        // Cooldown: 10-15 minuten (we gebruiken 10 minuten = 600000 ms)
        bool cooldownPassed = (lastNotification1Min == 0 || (now - lastNotification1Min >= 600000UL));
        
        // Check max alerts per uur limiet
        bool hourlyLimitOk = (alerts1MinThisHour < MAX_1M_ALERTS_PER_HOUR);
        
        // Debug logging alleen bij spike detectie (niet elke keer)
        if (spikeDetected) {
            Serial_printf("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%, cooldown=%d, hourlyLimit=%d/%d\n",
                         ret_1m, ret_5m, cooldownPassed, alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
        }
        
        if (spikeDetected && cooldownPassed && hourlyLimitOk)
        {
            // Bereken min en max uit secondPrices buffer
            float minVal, maxVal;
            findMinMaxInSecondPrices(minVal, maxVal);
            
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char msg[256];
            snprintf(msg, sizeof(msg), 
                     "1m UP spike: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                     ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
            if (ret_1m < 0) {
                snprintf(msg, sizeof(msg), 
                         "1m DOWN spike: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
            }
            // Notificatie wordt verstuurd (geen extra logging)
            
            // Bepaal kleur op basis van sterkte
            const char *colorTag;
            if (ret_1m > 0) {
                // Stijging: blauw voor normale (🔼), paars voor 150% threshold (⏫️)
                // 150% van 0.5% = 0.75%
                if (absRet1m >= 0.75f) {
                    colorTag = "purple_square,⏫️";
                } else {
                    colorTag = "blue_square,🔼";
                }
            } else {
                // Daling: oranje voor normale (🔽), rood voor 150% threshold (⏬️)
                // 150% van 0.5% = 0.75%
                if (absRet1m <= -0.75f) {
                    colorTag = "red_square,⏬️";
                } else {
                    colorTag = "orange_square,🔽";
                }
            }
            char title[64];
            snprintf(title, sizeof(title), "%s 1m Spike Alert", binanceSymbol);
            sendNotification(title, msg, colorTag);
            lastNotification1Min = now;
            alerts1MinThisHour++;
            Serial_printf("[Notify] 1m spike notificatie verstuurd (%d/%d dit uur)\n", alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
        }
        else if (spikeDetected && !hourlyLimitOk)
        {
            Serial_printf("[Notify] 1m spike gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
        }
    }
    
    // ===== 30-MINUTEN TREND MOVE ALERT =====
    // Voorwaarde: |ret_30m| >= 2% EN |ret_5m| >= 0.5% in dezelfde richting
    if (ret_30m != 0.0f && ret_5m != 0.0f)
    {
        float absRet30m = fabsf(ret_30m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn
        bool sameDirection = ((ret_30m > 0 && ret_5m > 0) || (ret_30m < 0 && ret_5m < 0));
        
        // Threshold check: ret_30m >= move30mThreshold EN ret_5m >= move5mThreshold
        bool threshold30mMet = (absRet30m >= move30mThreshold);
        bool threshold5mMet = (absRet5m >= move5mThreshold);
        bool moveDetected = threshold30mMet && threshold5mMet && sameDirection;
        
        // Cooldown: gebruik bestaande cooldown (10 minuten)
        bool cooldownPassed = (lastNotification30Min == 0 || (now - lastNotification30Min >= notificationCooldown30MinMs));
        
        // Check max alerts per uur limiet
        bool hourlyLimitOk = (alerts30MinThisHour < MAX_30M_ALERTS_PER_HOUR);
        
        // Debug logging alleen bij move detectie (niet elke keer)
        if (moveDetected) {
            Serial_printf("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%, cooldown=%d, hourlyLimit=%d/%d\n",
                         ret_30m, ret_5m, cooldownPassed, alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
        }
        
        if (moveDetected && cooldownPassed && hourlyLimitOk)
        {
            // Bereken min en max uit laatste 30 minuten van minuteAverages buffer
            float minVal, maxVal;
            findMinMaxInLast30Minutes(minVal, maxVal);
            
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char msg[256];
            snprintf(msg, sizeof(msg), 
                     "30m UP move: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                     ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            if (ret_30m < 0) {
                snprintf(msg, sizeof(msg), 
                         "30m DOWN move: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            }
            // Notificatie wordt verstuurd (geen extra logging)
            
            // Bepaal kleur op basis van sterkte
            const char *colorTag;
            if (ret_30m > 0) {
                // Stijging: blauw voor normale (🔼), paars voor 150% threshold (⏫️)
                // 150% van 2.0% = 3.0%
                if (absRet30m >= 3.0f) {
                    colorTag = "purple_square,⏫️";
                } else {
                    colorTag = "blue_square,🔼";
                }
            } else {
                // Daling: oranje voor normale (🔽), rood voor 150% threshold (⏬️)
                // 150% van 2.0% = 3.0%
                if (absRet30m <= -3.0f) {
                    colorTag = "red_square,⏬️";
                } else {
                    colorTag = "orange_square,🔽";
                }
            }
            char title[64];
            snprintf(title, sizeof(title), "%s 30m Move Alert", binanceSymbol);
            sendNotification(title, msg, colorTag);
            lastNotification30Min = now;
            alerts30MinThisHour++;
            Serial_printf("[Notify] 30m move notificatie verstuurd (%d/%d dit uur)\n", alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
        }
        else if (moveDetected && !hourlyLimitOk)
        {
            Serial_printf("[Notify] 30m move gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
        }
    }
    
    // ===== 5-MINUTEN MOVE ALERT =====
    // Voorwaarde: |ret_5m| >= move5mAlertThreshold
    if (ret_5m != 0.0f)
    {
        float absRet5m = fabsf(ret_5m);
        
        // Threshold check: ret_5m >= move5mAlertThreshold
        bool move5mDetected = (absRet5m >= move5mAlertThreshold);
        
        // Cooldown: 10 minuten
        bool cooldownPassed = (lastNotification5Min == 0 || (now - lastNotification5Min >= notificationCooldown5MinMs));
        
        // Check max alerts per uur limiet
        bool hourlyLimitOk = (alerts5MinThisHour < MAX_5M_ALERTS_PER_HOUR);
        
        // Debug logging alleen bij move detectie (niet elke keer)
        if (move5mDetected) {
            Serial_printf("[Notify] 5m move: ret_5m=%.2f%%, cooldown=%d, hourlyLimit=%d/%d\n",
                         ret_5m, cooldownPassed, alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
        }
        
        if (move5mDetected && cooldownPassed && hourlyLimitOk)
        {
            // Bereken min en max uit fiveMinutePrices buffer
            float minVal = fiveMinutePrices[0];
            float maxVal = fiveMinutePrices[0];
            for (int i = 1; i < SECONDS_PER_5MINUTES; i++) {
                if (fiveMinutePrices[i] > 0.0f) {
                    if (fiveMinutePrices[i] < minVal || minVal <= 0.0f) minVal = fiveMinutePrices[i];
                    if (fiveMinutePrices[i] > maxVal || maxVal <= 0.0f) maxVal = fiveMinutePrices[i];
                }
            }
            
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char msg[256];
            snprintf(msg, sizeof(msg), 
                     "5m UP move: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                     ret_5m, timestamp, prices[0], maxVal, minVal);
            if (ret_5m < 0) {
                snprintf(msg, sizeof(msg), 
                         "5m DOWN move: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_5m, timestamp, prices[0], maxVal, minVal);
            }
            
            // Bepaal kleur op basis van sterkte
            const char *colorTag;
            if (ret_5m > 0) {
                // Stijging: blauw voor normale (🔼), paars voor 150% threshold (⏫️)
                // 150% van 1.0% = 1.5%
                if (absRet5m >= 1.5f) {
                    colorTag = "purple_square,⏫️";
                } else {
                    colorTag = "blue_square,🔼";
                }
            } else {
                // Daling: oranje voor normale (🔽), rood voor 150% threshold (⏬️)
                // 150% van 1.0% = 1.5%
                if (absRet5m <= -1.5f) {
                    colorTag = "red_square,⏬️";
                } else {
                    colorTag = "orange_square,🔽";
                }
            }
            char title[64];
            snprintf(title, sizeof(title), "%s 5m Move Alert", binanceSymbol);
            sendNotification(title, msg, colorTag);
            lastNotification5Min = now;
            alerts5MinThisHour++;
            Serial_printf("[Notify] 5m move notificatie verstuurd (%d/%d dit uur)\n", alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
        }
        else if (move5mDetected && !hourlyLimitOk)
        {
            Serial_printf("[Notify] 5m move gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
        }
    }
}

// Language translation function
// Returns the appropriate text based on the selected language
static const char* getText(const char* nlText, const char* enText) {
    return (language == 1) ? enText : nlText;
}

// Helper function for formatted trend text with "Wait Xm"
static void getTrendWaitText(char* buffer, size_t bufferSize, uint8_t minutes) {
    if (language == 1) {
        snprintf(buffer, bufferSize, "Wait %um", minutes);
    } else {
        snprintf(buffer, bufferSize, "Wacht %um", minutes);
    }
}

// Generate unique ESP32 device ID using Crockford Base32 encoding
// Uses safe character set without confusing characters (no 0/O, 1/I/L, U)
// Character set: 0123456789ABCDEFGHJKMNPQRSTVWXYZ (32 characters)
// Uses 8 characters = 40 bits, giving 2^40 = 1.1 trillion possible combinations
static const char* base32Alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// Geoptimaliseerd: gebruik char array i.p.v. String om geheugenfragmentatie te voorkomen
static void getESP32DeviceId(char* buffer, size_t bufferSize) {
    uint64_t chipid = ESP.getEfuseMac(); // 48-bit unique MAC address
    
    // Extract 40 bits (8 characters * 5 bits each) from the MAC address
    // We use the lower 40 bits of the 48-bit MAC address
    uint64_t value = chipid & 0xFFFFFFFFFF; // Mask to 40 bits
    
    // Valideer buffer size (minimaal 9 bytes nodig: 8 chars + null terminator)
    if (bufferSize < 9) {
        buffer[0] = '\0';
        return;
    }
    
    for (int i = 0; i < 8; i++) {
        uint8_t index = value & 0x1F;  // Get 5 bits (0-31)
        buffer[i] = base32Alphabet[index];
        value >>= 5;  // Shift right by 5 bits for next character
    }
    buffer[8] = '\0'; // Null terminator
}

// Generate default NTFY topic with ESP32 device ID
// Format: [ESP32-ID]-alert
// Example: 9MK28H3Q-alert (8 characters using Crockford Base32 encoding for safe, unique ID)
// Geoptimaliseerd: gebruik char array i.p.v. String
static void generateDefaultNtfyTopic(char* buffer, size_t bufferSize) {
    char deviceId[9];
    getESP32DeviceId(deviceId, sizeof(deviceId));
    snprintf(buffer, bufferSize, "%s-alert", deviceId);
}

// Extract ESP32 device ID from NTFY topic (everything before "-alert")
// If topic format is [ESP32-ID]-alert, returns the ESP32-ID
// Falls back to showing first part before any dash if format is different
static String getDeviceIdFromTopic(const char* topic) {
    // Look for "-alert" at the end
    const char* alertPos = strstr(topic, "-alert");
    if (alertPos != nullptr) {
        // Extract everything before "-alert"
        size_t len = alertPos - topic;
        if (len > 0 && len < 16) {
            char deviceId[16];
            strncpy(deviceId, topic, len);
            deviceId[len] = '\0';
            return String(deviceId);
        }
    }
    // Fallback: use first part before any dash (for backwards compatibility)
    const char* dashPos = strchr(topic, '-');
    if (dashPos != nullptr) {
        size_t len = dashPos - topic;
        if (len > 0 && len < 16) {
            char deviceId[16];
            strncpy(deviceId, topic, len);
            deviceId[len] = '\0';
            return String(deviceId);
        }
    }
    // Last resort: use whole topic (limited)
    return String(topic).substring(0, 15);
}

// Load settings from Preferences
// ============================================================================
// Settings Management Functions
// ============================================================================

static void loadSettings()
{
    preferences.begin("crypto", true); // read-only mode
    
    // Generate default NTFY topic with unique ESP32 device ID
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char defaultTopic[64];
    generateDefaultNtfyTopic(defaultTopic, sizeof(defaultTopic));
    
    // Load NTFY topic from Preferences, or use generated default
    String topic = preferences.getString("ntfyTopic", defaultTopic);
    
    // If the loaded topic is the old default (without device ID), replace it with new format
    // Also migrate old format with prefix (e.g. "crypt-xxxxxx-alert") to new format without prefix
    bool needsMigration = false;
    if (topic == "crypto-monitor-alerts") {
        // Old default format
        needsMigration = true;
    } else if (topic.endsWith("-alert")) {
        // Check if it has the old prefix format (e.g. "crypt-xxxxxx-alert")
        // New format should be just "xxxxxx-alert" (no prefix before first dash)
        int alertPos = topic.indexOf("-alert");
        if (alertPos > 0) {
            String beforeAlert = topic.substring(0, alertPos);
            // Check if there's a dash in the part before "-alert" (indicating old prefix format)
            int dashInBefore = beforeAlert.indexOf('-');
            if (dashInBefore >= 0) {
                // Has old format with prefix (e.g. "crypt-xxxxxx"), migrate to new format
                needsMigration = true;
            } else if (beforeAlert.length() != 8) {
                // Not exactly 8 chars (old format used 6 or 12), might be old format or custom, migrate to be safe
                needsMigration = true;
            }
        }
    }
    
    if (needsMigration) {
        // Migrate to new format without prefix
        topic = defaultTopic;
        // Save the new default topic
        preferences.end();
        preferences.begin("crypto", false);
        preferences.putString("ntfyTopic", topic);
        preferences.end();
        preferences.begin("crypto", true);
    }
    
    topic.toCharArray(ntfyTopic, sizeof(ntfyTopic));
    String symbol = preferences.getString("binanceSymbol", BINANCE_SYMBOL_DEFAULT);
    symbol.toCharArray(binanceSymbol, sizeof(binanceSymbol));
    // Update symbols array with the loaded binance symbol
    strncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]) - 1);
    symbolsArray[0][sizeof(symbolsArray[0]) - 1] = '\0';
    threshold1MinUp = preferences.getFloat("th1Up", THRESHOLD_1MIN_UP_DEFAULT);
    threshold1MinDown = preferences.getFloat("th1Down", THRESHOLD_1MIN_DOWN_DEFAULT);
    threshold30MinUp = preferences.getFloat("th30Up", THRESHOLD_30MIN_UP_DEFAULT);
    threshold30MinDown = preferences.getFloat("th30Down", THRESHOLD_30MIN_DOWN_DEFAULT);
    spike1mThreshold = preferences.getFloat("spike1m", SPIKE_1M_THRESHOLD_DEFAULT);
    spike5mThreshold = preferences.getFloat("spike5m", SPIKE_5M_THRESHOLD_DEFAULT);
    move30mThreshold = preferences.getFloat("move30m", MOVE_30M_THRESHOLD_DEFAULT);
    move5mThreshold = preferences.getFloat("move5m", MOVE_5M_THRESHOLD_DEFAULT);
    move5mAlertThreshold = preferences.getFloat("move5mAlert", MOVE_5M_ALERT_THRESHOLD_DEFAULT);
    notificationCooldown1MinMs = preferences.getULong("cd1Min", NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT);
    notificationCooldown30MinMs = preferences.getULong("cd30Min", NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT);
    notificationCooldown5MinMs = preferences.getULong("cd5Min", NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT);
    
    // Load MQTT settings
    String host = preferences.getString("mqttHost", MQTT_HOST_DEFAULT);
    host.toCharArray(mqttHost, sizeof(mqttHost));
    mqttPort = preferences.getUInt("mqttPort", MQTT_PORT_DEFAULT);
    String user = preferences.getString("mqttUser", MQTT_USER_DEFAULT);
    user.toCharArray(mqttUser, sizeof(mqttUser));
    String pass = preferences.getString("mqttPass", MQTT_PASS_DEFAULT);
    pass.toCharArray(mqttPass, sizeof(mqttPass));
    
    // Load anchor settings
    anchorTakeProfit = preferences.getFloat("anchorTP", ANCHOR_TAKE_PROFIT_DEFAULT);
    anchorMaxLoss = preferences.getFloat("anchorML", ANCHOR_MAX_LOSS_DEFAULT);
    
    // Load trend and volatility settings
    trendThreshold = preferences.getFloat("trendTh", TREND_THRESHOLD_DEFAULT);
    volatilityLowThreshold = preferences.getFloat("volLow", VOLATILITY_LOW_THRESHOLD_DEFAULT);
    volatilityHighThreshold = preferences.getFloat("volHigh", VOLATILITY_HIGH_THRESHOLD_DEFAULT);
    
    // Load language setting
    language = preferences.getUChar("language", DEFAULT_LANGUAGE);
    
    preferences.end();
    Serial_printf("[Settings] Loaded: topic=%s, symbol=%s, 1min trend=%.2f/%.2f%%/min, 30min trend=%.2f/%.2f%%/uur, cooldown=%lu/%lu ms\n",
                  ntfyTopic, binanceSymbol, threshold1MinUp, threshold1MinDown, threshold30MinUp, threshold30MinDown,
                  notificationCooldown1MinMs, notificationCooldown30MinMs);
}

// Save settings to Preferences
static void saveSettings()
{
    preferences.begin("crypto", false); // read-write mode
    preferences.putString("ntfyTopic", ntfyTopic);
    preferences.putString("binanceSymbol", binanceSymbol);
    preferences.putFloat("th1Up", threshold1MinUp);
    preferences.putFloat("th1Down", threshold1MinDown);
    preferences.putFloat("th30Up", threshold30MinUp);
    preferences.putFloat("th30Down", threshold30MinDown);
    preferences.putFloat("spike1m", spike1mThreshold);
    preferences.putFloat("spike5m", spike5mThreshold);
    preferences.putFloat("move30m", move30mThreshold);
    preferences.putFloat("move5m", move5mThreshold);
    preferences.putFloat("move5mAlert", move5mAlertThreshold);
    preferences.putULong("cd1Min", notificationCooldown1MinMs);
    preferences.putULong("cd30Min", notificationCooldown30MinMs);
    preferences.putULong("cd5Min", notificationCooldown5MinMs);
    
    // Save MQTT settings
    preferences.putString("mqttHost", mqttHost);
    preferences.putUInt("mqttPort", mqttPort);
    preferences.putString("mqttUser", mqttUser);
    preferences.putString("mqttPass", mqttPass);
    
    // Save anchor settings
    preferences.putFloat("anchorTP", anchorTakeProfit);
    preferences.putFloat("anchorML", anchorMaxLoss);
    
    // Save trend and volatility settings
    preferences.putFloat("trendTh", trendThreshold);
    preferences.putFloat("volLow", volatilityLowThreshold);
    preferences.putFloat("volHigh", volatilityHighThreshold);
    
    // Save language setting
    preferences.putUChar("language", language);
    
    preferences.end();
    Serial_println("[Settings] Saved");
}

// MQTT callback: verwerk instellingen van Home Assistant
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
    char topicBuffer[128];
    char msgBuffer[128];
    char prefixBuffer[64];
    
    // Kopieer topic naar buffer (max 127 chars)
    size_t topicLen = strlen(topic);
    if (topicLen >= sizeof(topicBuffer)) topicLen = sizeof(topicBuffer) - 1;
    strncpy(topicBuffer, topic, topicLen);
    topicBuffer[topicLen] = '\0';
    
    // Kopieer payload naar buffer
    size_t msgLen = (length < sizeof(msgBuffer) - 1) ? length : sizeof(msgBuffer) - 1;
    for (size_t i = 0; i < msgLen; i++) {
        msgBuffer[i] = (char)payload[i];
    }
    msgBuffer[msgLen] = '\0';
    
    // Trim whitespace van msg
    while (msgLen > 0 && (msgBuffer[msgLen-1] == ' ' || msgBuffer[msgLen-1] == '\t' || msgBuffer[msgLen-1] == '\n' || msgBuffer[msgLen-1] == '\r')) {
        msgLen--;
        msgBuffer[msgLen] = '\0';
    }
    
    Serial_printf("[MQTT] Message: %s => %s\n", topicBuffer, msgBuffer);
    
    // Helper: maak MQTT topic prefix
    snprintf(prefixBuffer, sizeof(prefixBuffer), "%s", MQTT_TOPIC_PREFIX);
    
    bool settingChanged = false;
    char topicBufferFull[192]; // Voor volledige topic strings
    char valueBuffer[32]; // Voor numerieke waarden
    
    // Helper functie voor topic vergelijking
    #define CHECK_TOPIC(suffix) (strcmp(topicBuffer, suffix) == 0)
    
    // Bouw volledige topic strings voor vergelijking
    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/spike1m/set", prefixBuffer);
    if (strcmp(topicBuffer, topicBufferFull) == 0) {
        spike1mThreshold = atof(msgBuffer);
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/spike1m", prefixBuffer);
        mqttClient.publish(topicBufferFull, msgBuffer, true);
        settingChanged = true;
    } else {
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/spike5m/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            spike5mThreshold = atof(msgBuffer);
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/spike5m", prefixBuffer);
            mqttClient.publish(topicBufferFull, msgBuffer, true);
            settingChanged = true;
        } else {
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move30m/set", prefixBuffer);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                move30mThreshold = atof(msgBuffer);
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move30m", prefixBuffer);
                mqttClient.publish(topicBufferFull, msgBuffer, true);
                settingChanged = true;
            } else {
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move5m/set", prefixBuffer);
                if (strcmp(topicBuffer, topicBufferFull) == 0) {
                    move5mThreshold = atof(msgBuffer);
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move5m", prefixBuffer);
                    mqttClient.publish(topicBufferFull, msgBuffer, true);
                    settingChanged = true;
                } else {
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move5mAlert/set", prefixBuffer);
                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                        move5mAlertThreshold = atof(msgBuffer);
                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/move5mAlert", prefixBuffer);
                        mqttClient.publish(topicBufferFull, msgBuffer, true);
                        settingChanged = true;
                    } else {
                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown1min/set", prefixBuffer);
                        if (strcmp(topicBuffer, topicBufferFull) == 0) {
                            notificationCooldown1MinMs = atoi(msgBuffer) * 1000UL;
                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown1min", prefixBuffer);
                            snprintf(valueBuffer, sizeof(valueBuffer), "%lu", notificationCooldown1MinMs / 1000);
                            mqttClient.publish(topicBufferFull, valueBuffer, true);
                            settingChanged = true;
                        } else {
                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown30min/set", prefixBuffer);
                            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                notificationCooldown30MinMs = atoi(msgBuffer) * 1000UL;
                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown30min", prefixBuffer);
                                snprintf(valueBuffer, sizeof(valueBuffer), "%lu", notificationCooldown30MinMs / 1000);
                                mqttClient.publish(topicBufferFull, valueBuffer, true);
                                settingChanged = true;
                            } else {
                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown5min/set", prefixBuffer);
                                if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                    notificationCooldown5MinMs = atoi(msgBuffer) * 1000UL;
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/cooldown5min", prefixBuffer);
                                    snprintf(valueBuffer, sizeof(valueBuffer), "%lu", notificationCooldown5MinMs / 1000);
                                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                                    settingChanged = true;
                                } else {
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/binanceSymbol/set", prefixBuffer);
                                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                        // Trim en uppercase symbol
                                        char symbol[16];
                                        size_t symLen = 0;
                                        while (symLen < msgLen && msgBuffer[symLen] != '\0' && symLen < sizeof(symbol) - 1) {
                                            char c = msgBuffer[symLen];
                                            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                                                symbol[symLen] = (c >= 'a' && c <= 'z') ? (c - 32) : c; // Uppercase
                                                symLen++;
                                            }
                                        }
                                        symbol[symLen] = '\0';
                                        if (symLen > 0 && symLen < sizeof(binanceSymbol)) {
                                            strncpy(binanceSymbol, symbol, sizeof(binanceSymbol) - 1);
                                            binanceSymbol[sizeof(binanceSymbol) - 1] = '\0';
                                            strncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]) - 1);
                                            symbolsArray[0][sizeof(symbolsArray[0]) - 1] = '\0';
                                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/binanceSymbol", prefixBuffer);
                                            mqttClient.publish(topicBufferFull, binanceSymbol, true);
                                            settingChanged = true;
                                        }
                                    } else {
                                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/ntfyTopic/set", prefixBuffer);
                                        if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                            // Trim topic
                                            size_t topicLenTrimmed = msgLen;
                                            while (topicLenTrimmed > 0 && (msgBuffer[topicLenTrimmed-1] == ' ' || msgBuffer[topicLenTrimmed-1] == '\t')) {
                                                topicLenTrimmed--;
                                            }
                                            if (topicLenTrimmed > 0 && topicLenTrimmed < sizeof(ntfyTopic)) {
                                                strncpy(ntfyTopic, msgBuffer, topicLenTrimmed);
                                                ntfyTopic[topicLenTrimmed] = '\0';
                                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/ntfyTopic", prefixBuffer);
                                                mqttClient.publish(topicBufferFull, ntfyTopic, true);
                                                settingChanged = true;
                                            }
                                        } else {
                                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorTakeProfit/set", prefixBuffer);
                                            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                float val = atof(msgBuffer);
                                                if (val >= 0.1f && val <= 100.0f) {
                                                    anchorTakeProfit = val;
                                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorTakeProfit", prefixBuffer);
                                                    mqttClient.publish(topicBufferFull, msgBuffer, true);
                                                    settingChanged = true;
                                                }
                                            } else {
                                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorMaxLoss/set", prefixBuffer);
                                                if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                    float val = atof(msgBuffer);
                                                    if (val >= -100.0f && val <= -0.1f) {
                                                        anchorMaxLoss = val;
                                                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorMaxLoss", prefixBuffer);
                                                        mqttClient.publish(topicBufferFull, msgBuffer, true);
                                                        settingChanged = true;
                                                    }
                                                } else {
                                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/trendThreshold/set", prefixBuffer);
                                                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                        float val = atof(msgBuffer);
                                                        if (val >= 0.1f && val <= 10.0f) {
                                                            trendThreshold = val;
                                                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/trendThreshold", prefixBuffer);
                                                            mqttClient.publish(topicBufferFull, msgBuffer, true);
                                                            settingChanged = true;
                                                        }
                                                    } else {
                                                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/volatilityLowThreshold/set", prefixBuffer);
                                                        if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                            float val = atof(msgBuffer);
                                                            if (val >= 0.01f && val <= 1.0f) {
                                                                volatilityLowThreshold = val;
                                                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/volatilityLowThreshold", prefixBuffer);
                                                                mqttClient.publish(topicBufferFull, msgBuffer, true);
                                                                settingChanged = true;
                                                            }
                                                        } else {
                                                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/volatilityHighThreshold/set", prefixBuffer);
                                                            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                                float val = atof(msgBuffer);
                                                                if (val >= 0.01f && val <= 1.0f && val > volatilityLowThreshold) {
                                                                    volatilityHighThreshold = val;
                                                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/volatilityHighThreshold", prefixBuffer);
                                                                    mqttClient.publish(topicBufferFull, msgBuffer, true);
                                                                    settingChanged = true;
                                                                }
                                                            } else {
                                                                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/button/reset/set", prefixBuffer);
                                                                if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                                                    // Reset button pressed via MQTT - gebruik als anchor
                                                                    if (strcmp(msgBuffer, "PRESS") == 0 || strcmp(msgBuffer, "press") == 0 || 
                                                                        strcmp(msgBuffer, "1") == 0 || strcmp(msgBuffer, "ON") == 0 || 
                                                                        strcmp(msgBuffer, "on") == 0) {
            Serial_println("[MQTT] Reset/Anchor button pressed via MQTT");
            // Execute reset/anchor (thread-safe)
            float currentPrice = 0.0f;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (prices[0] > 0.0f) {
                    currentPrice = prices[0];  // Sla prijs lokaal op
                    openPrices[0] = prices[0];
                    // Set anchor price
                    anchorPrice = prices[0];
                    anchorMax = prices[0];  // Initialiseer max/min met huidige prijs
                    anchorMin = prices[0];
                    anchorTime = millis();
                    anchorActive = true;
                    anchorTakeProfitSent = false;
                    anchorMaxLossSent = false;
                    Serial_printf("[MQTT] Anchor set: anchorPrice = %.2f\n", anchorPrice);
                }
                xSemaphoreGive(dataMutex);
                
                // Publiceer anchor event naar MQTT
                if (currentPrice > 0.0f) {
                publishMqttAnchorEvent(anchorPrice, "anchor_set");
                    
                    // Stuur NTFY notificatie
                    char timestamp[32];
                    getFormattedTimestamp(timestamp, sizeof(timestamp));
                    char title[64];
                    char msg[128];
                    snprintf(title, sizeof(title), "%s Anchor Set", binanceSymbol);
                    snprintf(msg, sizeof(msg), "%s: %.2f EUR", timestamp, currentPrice);
                    sendNotification(title, msg, "white_check_mark");
                }
                
                // Update UI (this will also take the mutex internally)
                updateUI();
            }
            // Publish state back (button entities don't need state, but we can acknowledge)
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/button/reset", prefixBuffer);
            mqttClient.publish(topicBufferFull, "PRESSED", false);
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
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (settingChanged) {
        saveSettings();
        Serial_println("[MQTT] Settings updated and saved");
    }
}

// Publiceer huidige instellingen naar MQTT
// Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
void publishMqttSettings() {
    if (!mqttConnected) return;
    
    char topicBuffer[128];
    char buffer[32];
    
    dtostrf(spike1mThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike1m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(spike5mThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike5m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(move30mThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move30m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(move5mThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(move5mAlertThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5mAlert", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    snprintf(buffer, sizeof(buffer), "%lu", notificationCooldown1MinMs / 1000);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown1min", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    snprintf(buffer, sizeof(buffer), "%lu", notificationCooldown30MinMs / 1000);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown30min", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    snprintf(buffer, sizeof(buffer), "%lu", notificationCooldown5MinMs / 1000);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown5min", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/binanceSymbol", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, binanceSymbol, true);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ntfyTopic", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, ntfyTopic, true);
    
    dtostrf(anchorTakeProfit, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorTakeProfit", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(anchorMaxLoss, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorMaxLoss", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(trendThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/trendThreshold", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(volatilityLowThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/volatilityLowThreshold", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
    
    dtostrf(volatilityHighThreshold, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/volatilityHighThreshold", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, true);
}

// Publiceer waarden naar MQTT (prijzen, percentages, etc.)
// Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
void publishMqttValues(float price, float ret_1m, float ret_5m, float ret_30m) {
    if (!mqttConnected) return;
    
    char topicBuffer[128];
    char buffer[32];
    
    dtostrf(price, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/price", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_1m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_1m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_5m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_5m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_30m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_30m", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, false);
    
    snprintf(buffer, sizeof(buffer), "%lu", millis());
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/timestamp", MQTT_TOPIC_PREFIX);
    mqttClient.publish(topicBuffer, buffer, false);
    
    // Publiceer IP-adres (alleen als WiFi verbonden is)
    if (WiFi.status() == WL_CONNECTED) {
        char ipBuffer[16];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", MQTT_TOPIC_PREFIX);
        mqttClient.publish(topicBuffer, ipBuffer, false);
    }
}

// Publiceer MQTT Discovery berichten voor Home Assistant
void publishMqttDiscovery() {
    if (!mqttConnected) return;
    
    String prefix = String(MQTT_TOPIC_PREFIX);
    String deviceId = prefix + "_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    String deviceName = DEVICE_NAME;
    String deviceManufacturer = "JanP";
    String deviceModel = DEVICE_MODEL;
    String deviceJson = "\"device\":{\"identifiers\":[\"" + deviceId + "\"],\"name\":\"" + deviceName + "\",\"manufacturer\":\"" + deviceManufacturer + "\",\"model\":\"" + deviceModel + "\"}";
    
    String discTopic1 = "homeassistant/number/" + deviceId + "_spike1m/config";
    String payload1 = "{\"name\":\"1m Spike Threshold\",\"unique_id\":\"" + deviceId + "_spike1m\",\"state_topic\":\"" + prefix + "/config/spike1m\",\"command_topic\":\"" + prefix + "/config/spike1m/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic1.c_str(), payload1.c_str(), true);
    delay(50);
    
    String discTopic2 = "homeassistant/number/" + deviceId + "_spike5m/config";
    String payload2 = "{\"name\":\"5m Spike Filter\",\"unique_id\":\"" + deviceId + "_spike5m\",\"state_topic\":\"" + prefix + "/config/spike5m\",\"command_topic\":\"" + prefix + "/config/spike5m/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:filter\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic2.c_str(), payload2.c_str(), true);
    delay(50);
    
    String discTopic3 = "homeassistant/number/" + deviceId + "_move30m/config";
    String payload3 = "{\"name\":\"30m Move Threshold\",\"unique_id\":\"" + deviceId + "_move30m\",\"state_topic\":\"" + prefix + "/config/move30m\",\"command_topic\":\"" + prefix + "/config/move30m/set\",\"min\":0.5,\"max\":20.0,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:trending-up\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic3.c_str(), payload3.c_str(), true);
    delay(50);
    
    String discTopic4 = "homeassistant/number/" + deviceId + "_move5m/config";
    String payload4 = "{\"name\":\"5m Move Filter\",\"unique_id\":\"" + deviceId + "_move5m\",\"state_topic\":\"" + prefix + "/config/move5m\",\"command_topic\":\"" + prefix + "/config/move5m/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:filter-variant\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic4.c_str(), payload4.c_str(), true);
    delay(50);
    
    String discTopic4a = "homeassistant/number/" + deviceId + "_move5mAlert/config";
    String payload4a = "{\"name\":\"5m Move Alert Threshold\",\"unique_id\":\"" + deviceId + "_move5mAlert\",\"state_topic\":\"" + prefix + "/config/move5mAlert\",\"command_topic\":\"" + prefix + "/config/move5mAlert/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:alert\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic4a.c_str(), payload4a.c_str(), true);
    delay(50);
    
    String discTopic5 = "homeassistant/number/" + deviceId + "_cooldown1min/config";
    String payload5 = "{\"name\":\"1m Cooldown\",\"unique_id\":\"" + deviceId + "_cooldown1min\",\"state_topic\":\"" + prefix + "/config/cooldown1min\",\"command_topic\":\"" + prefix + "/config/cooldown1min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic5.c_str(), payload5.c_str(), true);
    delay(50);
    
    String discTopic6 = "homeassistant/number/" + deviceId + "_cooldown30min/config";
    String payload6 = "{\"name\":\"30m Cooldown\",\"unique_id\":\"" + deviceId + "_cooldown30min\",\"state_topic\":\"" + prefix + "/config/cooldown30min\",\"command_topic\":\"" + prefix + "/config/cooldown30min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic6.c_str(), payload6.c_str(), true);
    delay(50);
    
    String discTopic6a = "homeassistant/number/" + deviceId + "_cooldown5min/config";
    String payload6a = "{\"name\":\"5m Cooldown\",\"unique_id\":\"" + deviceId + "_cooldown5min\",\"state_topic\":\"" + prefix + "/config/cooldown5min\",\"command_topic\":\"" + prefix + "/config/cooldown5min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-sand\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic6a.c_str(), payload6a.c_str(), true);
    delay(50);
    
    String discTopic7 = "homeassistant/text/" + deviceId + "_binanceSymbol/config";
    String payload7 = "{\"name\":\"Binance Symbol\",\"unique_id\":\"" + deviceId + "_binanceSymbol\",\"state_topic\":\"" + prefix + "/config/binanceSymbol\",\"command_topic\":\"" + prefix + "/config/binanceSymbol/set\",\"icon\":\"mdi:currency-btc\"," + deviceJson + "}";
    mqttClient.publish(discTopic7.c_str(), payload7.c_str(), true);
    delay(50);
    
    String discTopic8 = "homeassistant/text/" + deviceId + "_ntfyTopic/config";
    String payload8 = "{\"name\":\"NTFY Topic\",\"unique_id\":\"" + deviceId + "_ntfyTopic\",\"state_topic\":\"" + prefix + "/config/ntfyTopic\",\"command_topic\":\"" + prefix + "/config/ntfyTopic/set\",\"icon\":\"mdi:bell-ring\"," + deviceJson + "}";
    mqttClient.publish(discTopic8.c_str(), payload8.c_str(), true);
    delay(50);
    
    String discTopic9 = "homeassistant/sensor/" + deviceId + "_price/config";
    String payload9 = "{\"name\":\"Crypto Price\",\"unique_id\":\"" + deviceId + "_price\",\"state_topic\":\"" + prefix + "/values/price\",\"unit_of_measurement\":\"EUR\",\"icon\":\"mdi:currency-btc\",\"device_class\":\"monetary\"," + deviceJson + "}";
    mqttClient.publish(discTopic9.c_str(), payload9.c_str(), true);
    delay(50);
    
    String discTopic10 = "homeassistant/sensor/" + deviceId + "_return_1m/config";
    String payload10 = "{\"name\":\"1m Return\",\"unique_id\":\"" + deviceId + "_return_1m\",\"state_topic\":\"" + prefix + "/values/return_1m\",\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-line-variant\"," + deviceJson + "}";
    mqttClient.publish(discTopic10.c_str(), payload10.c_str(), true);
    delay(50);
    
    String discTopic11 = "homeassistant/sensor/" + deviceId + "_return_5m/config";
    String payload11 = "{\"name\":\"5m Return\",\"unique_id\":\"" + deviceId + "_return_5m\",\"state_topic\":\"" + prefix + "/values/return_5m\",\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-timeline-variant\"," + deviceJson + "}";
    mqttClient.publish(discTopic11.c_str(), payload11.c_str(), true);
    delay(50);
    
    String discTopic12 = "homeassistant/sensor/" + deviceId + "_return_30m/config";
    String payload12 = "{\"name\":\"30m Return\",\"unique_id\":\"" + deviceId + "_return_30m\",\"state_topic\":\"" + prefix + "/values/return_30m\",\"unit_of_measurement\":\"%\",\"icon\":\"mdi:trending-up\"," + deviceJson + "}";
    mqttClient.publish(discTopic12.c_str(), payload12.c_str(), true);
    delay(50);
    
    // Reset button
    String discTopic13 = "homeassistant/button/" + deviceId + "_reset/config";
    String payload13 = "{\"name\":\"Reset Open Price\",\"unique_id\":\"" + deviceId + "_reset\",\"command_topic\":\"" + prefix + "/button/reset/set\",\"icon\":\"mdi:restart\"," + deviceJson + "}";
    mqttClient.publish(discTopic13.c_str(), payload13.c_str(), true);
    delay(50);
    
    // Anchor take profit
    String discTopic14 = "homeassistant/number/" + deviceId + "_anchorTakeProfit/config";
    String payload14 = "{\"name\":\"Anchor Take Profit\",\"unique_id\":\"" + deviceId + "_anchorTakeProfit\",\"state_topic\":\"" + prefix + "/config/anchorTakeProfit\",\"command_topic\":\"" + prefix + "/config/anchorTakeProfit/set\",\"min\":0.1,\"max\":100.0,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:cash-plus\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic14.c_str(), payload14.c_str(), true);
    delay(50);
    
    // Anchor max loss
    String discTopic15 = "homeassistant/number/" + deviceId + "_anchorMaxLoss/config";
    String payload15 = "{\"name\":\"Anchor Max Loss\",\"unique_id\":\"" + deviceId + "_anchorMaxLoss\",\"state_topic\":\"" + prefix + "/config/anchorMaxLoss\",\"command_topic\":\"" + prefix + "/config/anchorMaxLoss/set\",\"min\":-100.0,\"max\":-0.1,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:cash-minus\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic15.c_str(), payload15.c_str(), true);
    delay(50);
    
    // Anchor event sensor
    String discTopic16 = "homeassistant/sensor/" + deviceId + "_anchor_event/config";
    String payload16 = "{\"name\":\"Anchor Event\",\"unique_id\":\"" + deviceId + "_anchor_event\",\"state_topic\":\"" + prefix + "/anchor/event\",\"json_attributes_topic\":\"" + prefix + "/anchor/event\",\"value_template\":\"{{ value_json.event }}\",\"icon\":\"mdi:anchor\"," + deviceJson + "}";
    mqttClient.publish(discTopic16.c_str(), payload16.c_str(), true);
    delay(50);
    
    // Trend threshold
    String discTopic17 = "homeassistant/number/" + deviceId + "_trendThreshold/config";
    String payload17 = "{\"name\":\"Trend Threshold\",\"unique_id\":\"" + deviceId + "_trendThreshold\",\"state_topic\":\"" + prefix + "/config/trendThreshold\",\"command_topic\":\"" + prefix + "/config/trendThreshold/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-line\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic17.c_str(), payload17.c_str(), true);
    delay(50);
    
    // Volatility low threshold
    String discTopic18 = "homeassistant/number/" + deviceId + "_volatilityLowThreshold/config";
    String payload18 = "{\"name\":\"Volatility Low Threshold\",\"unique_id\":\"" + deviceId + "_volatilityLowThreshold\",\"state_topic\":\"" + prefix + "/config/volatilityLowThreshold\",\"command_topic\":\"" + prefix + "/config/volatilityLowThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic18.c_str(), payload18.c_str(), true);
    delay(50);
    
    // Volatility high threshold
    String discTopic19 = "homeassistant/number/" + deviceId + "_volatilityHighThreshold/config";
    String payload19 = "{\"name\":\"Volatility High Threshold\",\"unique_id\":\"" + deviceId + "_volatilityHighThreshold\",\"state_topic\":\"" + prefix + "/config/volatilityHighThreshold\",\"command_topic\":\"" + prefix + "/config/volatilityHighThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%\",\"icon\":\"mdi:chart-timeline-variant-shimmer\",\"mode\":\"box\"," + deviceJson + "}";
    mqttClient.publish(discTopic19.c_str(), payload19.c_str(), true);
    delay(50);
    
    // IP Address sensor
    String discTopic20 = "homeassistant/sensor/" + deviceId + "_ip_address/config";
    String payload20 = "{\"name\":\"IP Address\",\"unique_id\":\"" + deviceId + "_ip_address\",\"state_topic\":\"" + prefix + "/values/ip_address\",\"icon\":\"mdi:ip-network\"," + deviceJson + "}";
    mqttClient.publish(discTopic20.c_str(), payload20.c_str(), true);
    delay(50);
    
    Serial_println("[MQTT] Discovery messages published");
}

// MQTT connect functie (niet-blokkerend)
void connectMQTT() {
    if (mqttConnected) return;
    
    mqttClient.setServer(mqttHost, mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    String clientId = String(MQTT_CLIENT_ID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial_printf("[MQTT] Connecting to %s:%d as %s...\n", mqttHost, mqttPort, clientId.c_str());
    
    if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPass)) {
        Serial_println("[MQTT] Connected!");
        mqttConnected = true;
        
        String prefix = String(MQTT_TOPIC_PREFIX);
        mqttClient.subscribe((prefix + "/config/spike1m/set").c_str());
        mqttClient.subscribe((prefix + "/config/spike5m/set").c_str());
        mqttClient.subscribe((prefix + "/config/move30m/set").c_str());
        mqttClient.subscribe((prefix + "/config/move5m/set").c_str());
        mqttClient.subscribe((prefix + "/config/cooldown1min/set").c_str());
        mqttClient.subscribe((prefix + "/config/cooldown30min/set").c_str());
        mqttClient.subscribe((prefix + "/config/binanceSymbol/set").c_str());
        mqttClient.subscribe((prefix + "/config/ntfyTopic/set").c_str());
        mqttClient.subscribe((prefix + "/button/reset/set").c_str());
        mqttClient.subscribe((prefix + "/config/anchorTakeProfit/set").c_str());
        mqttClient.subscribe((prefix + "/config/anchorMaxLoss/set").c_str());
        
        publishMqttSettings();
        publishMqttDiscovery();
        
    } else {
        Serial_printf("[MQTT] Connect failed, rc=%d\n", mqttClient.state());
        mqttConnected = false;
    }
}

// Web server HTML page
static String getSettingsHTML()
{
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>" + String(binanceSymbol) + " " + String(getText("Instellingen", "Settings")) + "</title>";
    html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff;}";
    html += "h1{color:#00BCD4;}form{max-width:500px;}";
    html += "label{display:block;margin:15px 0 5px;color:#ccc;}";
    html += "input[type=number],input[type=text],select{width:100%;padding:8px;border:1px solid #444;background:#2a2a2a;color:#fff;border-radius:4px;}";
    html += "button{background:#00BCD4;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;}";
    html += "button:hover{background:#00acc1;}.info{color:#888;font-size:12px;margin-top:5px;}</style></head><body>";
    html += "<h1>" + String(binanceSymbol) + " " + String(getText("Instellingen", "Settings")) + "</h1>";
    html += "<form method='POST' action='/save'>";
    html += "<label>" + String(getText("Taal / Language:", "Language:")) + "<select name='language'>";
    html += "<option value='0'" + String(language == 0 ? " selected" : "") + ">Nederlands</option>";
    html += "<option value='1'" + String(language == 1 ? " selected" : "") + ">English</option>";
    html += "</select></label>";
    html += "<div class='info'>" + String(getText("Selecteer de taal voor het display en de web interface", "Select language for display and web interface")) + "</div>";
    html += "<label>NTFY Topic:<input type='text' name='ntfytopic' value='" + String(ntfyTopic) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("Dit is de NTFY topic waarop je je moet abonneren in de NTFY app om notificaties te ontvangen op je mobiel. Standaard wordt automatisch een uniek topic gegenereerd met je ESP32 device ID (format: [ESP32-ID]-alert).", "This is the NTFY topic you need to subscribe to in the NTFY app to receive notifications on your mobile. By default, a unique topic is automatically generated with your ESP32 device ID (format: [ESP32-ID]-alert).")) + "</div>";
    html += "<label>" + String(getText("Binance Symbool:", "Binance Symbol:")) + "<input type='text' name='binancesymbol' value='" + String(binanceSymbol) + "' maxlength='15'></label>";
    html += "<div class='info'>" + String(getText("Binance trading pair (bijv. BTCEUR, BTCUSDT, ETHUSDT)", "Binance trading pair (e.g. BTCEUR, BTCUSDT, ETHUSDT)")) + "</div>";
    html += "<hr style='border:1px solid #444;margin:30px 0;'>";
    html += "<h2 style='color:#00BCD4;margin-top:30px;'>" + String(getText("Spike & Move Alerts", "Spike & Move Alerts")) + "</h2>";
    html += "<label>1m Spike - ret_1m threshold (%):<input type='number' step='0.01' name='spike1m' value='" + String(spike1mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("1m spike alert: |ret_1m| >= deze waarde (standaard: 0.30%)", "1m spike alert: |ret_1m| >= this value (default: 0.30%)")) + "</div>";
    html += "<label>1m Spike - ret_5m filter (%):<input type='number' step='0.01' name='spike5m' value='" + String(spike5mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("1m spike alert: |ret_5m| >= deze waarde in dezelfde richting (standaard: 0.60%)", "1m spike alert: |ret_5m| >= this value in same direction (default: 0.60%)")) + "</div>";
    html += "<label>30m Move - ret_30m threshold (%):<input type='number' step='0.01' name='move30m' value='" + String(move30mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("30m move alert: |ret_30m| >= deze waarde (standaard: 2.0%)", "30m move alert: |ret_30m| >= this value (default: 2.0%)")) + "</div>";
    html += "<label>30m Move - ret_5m filter (%):<input type='number' step='0.01' name='move5m' value='" + String(move5mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("30m move alert: |ret_5m| >= deze waarde in dezelfde richting (standaard: 0.5%)", "30m move alert: |ret_5m| >= this value in same direction (default: 0.5%)")) + "</div>";
    html += "<label>5m Move Alert - threshold (%):<input type='number' step='0.01' name='move5mAlert' value='" + String(move5mAlertThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("5m move alert: |ret_5m| >= deze waarde (standaard: 1.0%)", "5m move alert: |ret_5m| >= this value (default: 1.0%)")) + "</div>";
    html += "<hr style='border:1px solid #444;margin:30px 0;'>";
    html += "<h2 style='color:#00BCD4;margin-top:30px;'>" + String(getText("Cooldowns", "Cooldowns")) + "</h2>";
    html += "<label>" + String(getText("1-minuut spike cooldown (seconden):", "1-minute spike cooldown (seconds):")) + "<input type='number' name='cd1min' value='" + String(notificationCooldown1MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Tijd tussen 1-minuut spike notificaties", "Time between 1-minute spike notifications")) + "</div>";
    html += "<label>" + String(getText("30-minuten move cooldown (seconden):", "30-minute move cooldown (seconds):")) + "<input type='number' name='cd30min' value='" + String(notificationCooldown30MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Tijd tussen 30-minuten move notificaties", "Time between 30-minute move notifications")) + "</div>";
    html += "<label>" + String(getText("5-minuten move cooldown (seconden):", "5-minute move cooldown (seconds):")) + "<input type='number' name='cd5min' value='" + String(notificationCooldown5MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Tijd tussen 5-minuten move notificaties", "Time between 5-minute move notifications")) + "</div>";
    html += "<hr style='border:1px solid #444;margin:30px 0;'>";
    html += "<h2 style='color:#00BCD4;margin-top:30px;'>" + String(getText("MQTT Instellingen", "MQTT Settings")) + "</h2>";
    html += "<label>" + String(getText("MQTT Host (IP):", "MQTT Host (IP):")) + "<input type='text' name='mqtthost' value='" + String(mqttHost) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("IP-adres van de MQTT broker (bijv. 192.168.68.3)", "IP address of MQTT broker (e.g. 192.168.68.3)")) + "</div>";
    html += "<label>" + String(getText("MQTT Poort:", "MQTT Port:")) + "<input type='number' name='mqttport' value='" + String(mqttPort) + "' min='1' max='65535'></label>";
    html += "<div class='info'>" + String(getText("MQTT broker poort (standaard: 1883)", "MQTT broker port (default: 1883)")) + "</div>";
    html += "<label>" + String(getText("MQTT Gebruiker:", "MQTT User:")) + "<input type='text' name='mqttuser' value='" + String(mqttUser) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("MQTT broker gebruikersnaam", "MQTT broker username")) + "</div>";
    html += "<label>" + String(getText("MQTT Wachtwoord:", "MQTT Password:")) + "<input type='password' name='mqttpass' value='" + String(mqttPass) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("MQTT broker wachtwoord", "MQTT broker password")) + "</div>";
    html += "<hr style='border:1px solid #444;margin:30px 0;'>";
    html += "<h2 style='color:#00BCD4;margin-top:30px;'>" + String(getText("Trend & Volatiliteit Instellingen", "Trend & Volatility Settings")) + "</h2>";
    html += "<label>" + String(getText("Trend Threshold (%):", "Trend Threshold (%):")) + "<input type='number' step='0.1' name='trendTh' value='" + String(trendThreshold, 1) + "' min='0.1' max='10'></label>";
    html += "<div class='info'>" + String(getText("Trend detectie threshold voor 2h return (standaard: 1.0%)", "Trend detection threshold for 2h return (default: 1.0%)")) + "</div>";
    html += "<label>" + String(getText("Volatiliteit Low Threshold (%):", "Volatility Low Threshold (%):")) + "<input type='number' step='0.01' name='volLow' value='" + String(volatilityLowThreshold, 2) + "' min='0.01' max='1'></label>";
    html += "<div class='info'>" + String(getText("Onder deze waarde is volatiliteit LOW (standaard: 0.06%)", "Below this value volatility is LOW (default: 0.06%)")) + "</div>";
    html += "<label>" + String(getText("Volatiliteit High Threshold (%):", "Volatility High Threshold (%):")) + "<input type='number' step='0.01' name='volHigh' value='" + String(volatilityHighThreshold, 2) + "' min='0.01' max='1'></label>";
    html += "<div class='info'>" + String(getText("Boven deze waarde is volatiliteit HIGH (standaard: 0.12%)", "Above this value volatility is HIGH (default: 0.12%)")) + "</div>";
    html += "<hr style='border:1px solid #444;margin:30px 0;'>";
    html += "<h2 style='color:#00BCD4;margin-top:30px;'>" + String(getText("Anchor Instellingen", "Anchor Settings")) + "</h2>";
    html += "<label>" + String(getText("Anchor Take Profit (%):", "Anchor Take Profit (%):")) + "<input type='number' step='0.1' name='anchorTP' value='" + String(anchorTakeProfit, 1) + "' min='0.1' max='100'></label>";
    html += "<div class='info'>" + String(getText("Take profit threshold boven anchor price (standaard: 5.0%)", "Take profit threshold above anchor price (default: 5.0%)")) + "</div>";
    html += "<label>" + String(getText("Anchor Max Loss (%):", "Anchor Max Loss (%):")) + "<input type='number' step='0.1' name='anchorML' value='" + String(anchorMaxLoss, 1) + "' min='-100' max='-0.1'></label>";
    html += "<div class='info'>" + String(getText("Max loss threshold onder anchor price (standaard: -3.0%)", "Max loss threshold below anchor price (default: -3.0%)")) + "</div>";
    html += "<button type='submit'>" + String(getText("Opslaan", "Save")) + "</button></form>";
    html += "</body></html>";
    return html;
}

// ============================================================================
// Web Server Functions
// ============================================================================

// Web server handlers
static void handleRoot()
{
    server.send(200, "text/html", getSettingsHTML());
}

static void handleSave()
{
    // Handle language setting
    if (server.hasArg("language")) {
        uint8_t newLanguage = server.arg("language").toInt();
        if (newLanguage == 0 || newLanguage == 1) {
            language = newLanguage;
        }
    }
    
    if (server.hasArg("ntfytopic")) {
        String topic = server.arg("ntfytopic");
        topic.trim();
        if (topic.length() > 0 && topic.length() < sizeof(ntfyTopic)) {
            topic.toCharArray(ntfyTopic, sizeof(ntfyTopic));
        }
    }
    if (server.hasArg("binancesymbol")) {
        String symbol = server.arg("binancesymbol");
        symbol.trim();
        symbol.toUpperCase(); // Binance symbolen zijn altijd uppercase
        if (symbol.length() > 0 && symbol.length() < sizeof(binanceSymbol)) {
            symbol.toCharArray(binanceSymbol, sizeof(binanceSymbol));
            // Update symbols array
            strncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]) - 1);
            symbolsArray[0][sizeof(symbolsArray[0]) - 1] = '\0';
        }
    }
    if (server.hasArg("spike1m")) spike1mThreshold = server.arg("spike1m").toFloat();
    if (server.hasArg("spike5m")) spike5mThreshold = server.arg("spike5m").toFloat();
    if (server.hasArg("move30m")) move30mThreshold = server.arg("move30m").toFloat();
    if (server.hasArg("move5m")) move5mThreshold = server.arg("move5m").toFloat();
    if (server.hasArg("move5mAlert")) move5mAlertThreshold = server.arg("move5mAlert").toFloat();
    if (server.hasArg("cd1min")) notificationCooldown1MinMs = server.arg("cd1min").toInt() * 1000UL;
    if (server.hasArg("cd30min")) notificationCooldown30MinMs = server.arg("cd30min").toInt() * 1000UL;
    if (server.hasArg("cd5min")) notificationCooldown5MinMs = server.arg("cd5min").toInt() * 1000UL;
    
    // MQTT settings
    if (server.hasArg("mqtthost")) {
        String host = server.arg("mqtthost");
        host.trim();
        if (host.length() > 0 && host.length() < sizeof(mqttHost)) {
            host.toCharArray(mqttHost, sizeof(mqttHost));
        }
    }
    if (server.hasArg("mqttport")) {
        uint16_t port = server.arg("mqttport").toInt();
        if (port > 0 && port <= 65535) {
            mqttPort = port;
        }
    }
    if (server.hasArg("mqttuser")) {
        String user = server.arg("mqttuser");
        user.trim();
        if (user.length() > 0 && user.length() < sizeof(mqttUser)) {
            user.toCharArray(mqttUser, sizeof(mqttUser));
        }
    }
    if (server.hasArg("mqttpass")) {
        String pass = server.arg("mqttpass");
        pass.trim();
        if (pass.length() > 0 && pass.length() < sizeof(mqttPass)) {
            pass.toCharArray(mqttPass, sizeof(mqttPass));
        }
    }
    
    // Trend and volatility settings
    if (server.hasArg("trendTh")) {
        float val = server.arg("trendTh").toFloat();
        if (val >= 0.1f && val <= 10.0f) {
            trendThreshold = val;
        }
    }
    if (server.hasArg("volLow")) {
        float val = server.arg("volLow").toFloat();
        if (val >= 0.01f && val <= 1.0f) {
            volatilityLowThreshold = val;
        }
    }
    if (server.hasArg("volHigh")) {
        float val = server.arg("volHigh").toFloat();
        if (val >= 0.01f && val <= 1.0f && val > volatilityLowThreshold) {
            volatilityHighThreshold = val;
        }
    }
    
    // Anchor settings
    if (server.hasArg("anchorTP")) {
        float val = server.arg("anchorTP").toFloat();
        if (val >= 0.1f && val <= 100.0f) {
            anchorTakeProfit = val;
        }
    }
    if (server.hasArg("anchorML")) {
        float val = server.arg("anchorML").toFloat();
        if (val >= -100.0f && val <= -0.1f) {
            anchorMaxLoss = val;
        }
    }
    
    saveSettings();
    
    // Herconnect MQTT als instellingen zijn gewijzigd
    if (mqttConnected) {
        mqttClient.disconnect();
        mqttConnected = false;
        lastMqttReconnectAttempt = 0;
    }
    
    // Bouw HTML string op zonder vreemde tekens
    String html = "";
    html += "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta http-equiv='refresh' content='2;url=/'>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Opgeslagen</title>";
    html += "<style>";
    html += "body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff;text-align:center;}";
    html += "h1{color:#4CAF50;}";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<h1>" + String(getText("Instellingen opgeslagen!", "Settings saved!")) + "</h1>";
    html += "<p>" + String(getText("Terug naar instellingen...", "Returning to settings...")) + "</p>";
    html += "</body>";
    html += "</html>";
    server.send(200, "text/html", html);
}


// Status pagina handler verwijderd - niet meer gebruikt

// 404 handler voor onbekende routes
static void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
    Serial_println("[WebServer] 404: " + server.uri());
}

static void setupWebServer()
{
    Serial.println("[WebServer] Routes registreren...");
    server.on("/", handleRoot);
    Serial.println("[WebServer] Route '/' geregistreerd");
    server.on("/save", HTTP_POST, handleSave);
    Serial.println("[WebServer] Route '/save' geregistreerd");
    // Status pagina route verwijderd - niet meer gebruikt
    server.onNotFound(handleNotFound); // 404 handler
    Serial.println("[WebServer] 404 handler geregistreerd");
    server.begin();
    Serial.println("[WebServer] Server gestart");
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char ipBuffer[16];
    formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
    Serial.printf("[WebServer] Gestart op http://%s\n", ipBuffer);
}

// Parse Binance JSON – very small, avoid ArduinoJson for flash size
// Geoptimaliseerd: gebruik const char* i.p.v. String om geheugen te besparen
static bool parsePrice(const String &body, float &out)
{
    // Check of body niet leeg is
    if (body.length() == 0)
        return false;
    
    int idx = body.indexOf("\"price\":\"");
    if (idx < 0)
        return false;
    idx += 9; // skip to first digit
    int end = body.indexOf('"', idx);
    if (end < 0)
        return false;
    
    // Gebruik substring alleen als nodig, en valideer lengte
    if (end - idx > 20) // Max 20 karakters voor prijs (veiligheidscheck)
        return false;
    
    out = body.substring(idx, end).toFloat();
    
    // Valideer dat we een geldige float hebben gekregen
    if (out <= 0.0f || isnan(out) || isinf(out))
        return false;
    
    return true;
}

// Calculate average of array
static float calculateAverage(float *array, uint8_t size, bool filled)
{
    float sum = 0.0f;
    uint8_t count = filled ? size : 0;
    
    if (!filled)
    {
        // Count non-zero values
        for (uint8_t i = 0; i < size; i++)
        {
            if (array[i] != 0.0f)
            {
                sum += array[i];
                count++;
            }
        }
    }
    else
    {
        // Sum all values
        for (uint8_t i = 0; i < size; i++)
        {
            sum += array[i];
        }
    }
    
    if (count == 0)
        return 0.0f;
    
    return sum / count;
}

// ============================================================================
// Price History Management Functions
// ============================================================================

// Find min and max values in secondPrices array
static void findMinMaxInSecondPrices(float &minVal, float &maxVal)
{
    minVal = 0.0f;
    maxVal = 0.0f;
    
    if (!secondArrayFilled && secondPrices[0] == 0.0f)
        return;
    
    uint8_t count = secondArrayFilled ? SECONDS_PER_MINUTE : secondIndex;
    if (count == 0)
        return;
    
    minVal = secondPrices[0];
    maxVal = secondPrices[0];
    
    for (uint8_t i = 1; i < count; i++)
    {
        if (secondPrices[i] > 0.0f)
        {
            if (secondPrices[i] < minVal) minVal = secondPrices[i];
            if (secondPrices[i] > maxVal) maxVal = secondPrices[i];
        }
    }
}

// ============================================================================
// Price Calculation Functions
// ============================================================================

// NIEUWE METHODE: Bereken percentage verandering tussen tijdstippen
// ret_1m: prijs nu vs 60 seconden geleden
static float calculateReturn1Minute()
{
    // We hebben minimaal 60 seconden geschiedenis nodig (huidige + 60 seconden geleden)
    // Omdat de array een ringbuffer is van 60 posities, moeten we wachten tot de array gevuld is
    // (secondArrayFilled == true) voordat we kunnen berekenen
    if (!secondArrayFilled)
    {
        // Array nog niet rond, we moeten wachten tot de array gevuld is
        // Zodra secondIndex = 60 wordt, wordt het gereset naar 0 en wordt secondArrayFilled = true
        averagePrices[1] = 0.0f;
        // Debug: log wanneer we nog niet genoeg data hebben
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        if (now - lastLogTime > 10000) { // Log elke 10 seconden
            Serial_printf("[Ret1m] Wachten op data: secondIndex=%u (nodig: array gevuld, arrayFilled=%d)\n", secondIndex, secondArrayFilled);
            lastLogTime = now;
        }
        return 0.0f; // Nog niet genoeg data - wacht tot array gevuld is
    }
    
    // Array is gevuld (ringbuffer), we kunnen altijd berekenen
    // secondIndex wijst naar volgende schrijfpositie
    // Laatste geschreven waarde staat op (secondIndex - 1 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    // 60 seconden geleden: we gaan 60 posities terug in de ringbuffer
    // Voorbeeld: als secondIndex = 5, dan laatste op 4, en 60 sec geleden op (4 - 60 + 60) % 60 = 4
    // Maar we willen echt 60 seconden terug: (secondIndex - 1 - 60 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    // Vereenvoudigd: (secondIndex - 61 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    
    // Gebruik de prijs die net is toegevoegd aan de buffer (laatste geschreven waarde)
    // Dit is betrouwbaarder dan prices[0] omdat het exact de prijs is die in de buffer staat
    // Laatste geschreven positie: (secondIndex - 1 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    uint8_t lastWrittenIdx = (secondIndex == 0) ? (SECONDS_PER_MINUTE - 1) : (secondIndex - 1);
    float priceNow = secondPrices[lastWrittenIdx];
    
    // 60 seconden geleden: bereken index 60 posities terug in de ringbuffer
    // secondIndex wijst naar de volgende schrijfpositie
    // De laatste geschreven waarde staat op (secondIndex - 1 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    // Voor 60 seconden geleden: als secondIndex = 0, dan laatste op 59, en 60 sec geleden op (59 - 60 + 60) % 60 = 59
    // Maar dat is verkeerd - we moeten de waarde gebruiken die 60 posities terug staat
    // Als secondIndex = 0, dan hebben we net geschreven op 59, en 60 sec geleden staat op (0 - 60 + 60) % 60 = 0
    // Wacht, dat klopt ook niet...
    
    // Correcte berekening: als secondIndex = 0, dan laatste op 59
    // 60 seconden geleden: toen was secondIndex ook 0 (of 60, wat gereset werd naar 0)
    // Dus de waarde 60 seconden geleden staat op dezelfde positie als waar we net hebben geschreven
    // Maar dat betekent dat we de waarde moeten gebruiken die 60 seconden geleden op die positie stond
    
    // Eigenlijk: als secondIndex = 0, dan hebben we net geschreven op positie 59
    // 60 seconden geleden stond er een andere waarde op positie 59 (toen secondIndex ook 0 was, of 60)
    // Maar omdat de array een ringbuffer is, staat de waarde van 60 seconden geleden op positie (secondIndex - 60 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    
    // Correcte berekening voor ringbuffer:
    // secondIndex wijst naar de volgende schrijfpositie
    // Laatste geschreven positie is (secondIndex - 1 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    // 60 seconden geleden: we moeten 60 posities terug in de ringbuffer
    // Als secondIndex = 0, dan laatste op 59, en 60 sec geleden op (0 - 60 + 60) % 60 = 0
    // Maar dat is verkeerd! Als secondIndex = 0, dan hebben we net geschreven op 59
    // 60 seconden geleden stond er een andere waarde op positie 59 (toen secondIndex = 59, wat nu 0 is geworden)
    // Dus: 60 seconden geleden staat op (secondIndex - 60 + SECONDS_PER_MINUTE) % SECONDS_PER_MINUTE
    
    // Veilige modulo berekening
    int32_t idx1mAgo = ((int32_t)secondIndex - 60 + SECONDS_PER_MINUTE * 2) % SECONDS_PER_MINUTE;
    
    // Extra veiligheidscheck
    if (idx1mAgo < 0 || idx1mAgo >= SECONDS_PER_MINUTE) {
        Serial_printf("[Ret1m] FATAL: idx1mAgo=%ld buiten bereik [0,%u], secondIndex=%u\n", idx1mAgo, SECONDS_PER_MINUTE, secondIndex);
        return 0.0f;
    }
    
    uint8_t idx1mAgo_u = (uint8_t)idx1mAgo;
    float price1mAgo = secondPrices[idx1mAgo_u];
    
    if (price1mAgo <= 0.0f || priceNow <= 0.0f)
    {
        averagePrices[1] = 0.0f;
        Serial_printf("[Ret1m] ERROR: secondIndex=%u (filled), priceNow=%.2f, price1mAgo=%.2f (idx=%u) - invalid!\n",
                     secondIndex, priceNow, price1mAgo, idx1mAgo_u);
        return 0.0f;
    }
    
    // Bereken gemiddelde voor weergave
    float currentAvg = calculateAverage(secondPrices, SECONDS_PER_MINUTE, secondArrayFilled);
    averagePrices[1] = currentAvg;
    
    // Return als percentage: (nu - 1m geleden) / 1m geleden * 100
    float ret = ((priceNow - price1mAgo) / price1mAgo) * 100.0f;
    
    return ret;
}

// ret_5m: prijs nu vs 300 seconden geleden
static float calculateReturn5Minutes()
{
    // We hebben minimaal 300 seconden nodig (300 seconden geleden)
    if (!fiveMinuteArrayFilled && fiveMinuteIndex < 300)
    {
        // Debug: log wanneer we nog niet genoeg data hebben
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        if (now - lastLogTime > 30000) { // Log elke 30 seconden
            Serial_printf("[Ret5m] Wachten op data: fiveMinuteIndex=%u (nodig: 300, arrayFilled=%d)\n", fiveMinuteIndex, fiveMinuteArrayFilled);
            lastLogTime = now;
        }
        return 0.0f; // Nog niet genoeg data
    }
    
    // Gebruik de prijs die net is toegevoegd aan de buffer (laatste geschreven waarde)
    float priceNow;
    if (fiveMinuteArrayFilled) {
        uint16_t lastWrittenIdx = (fiveMinuteIndex == 0) ? (SECONDS_PER_5MINUTES - 1) : (fiveMinuteIndex - 1);
        priceNow = fiveMinutePrices[lastWrittenIdx];
    } else {
        // Array nog niet gevuld, gebruik laatste geschreven positie
        if (fiveMinuteIndex == 0) {
            priceNow = 0.0f; // Nog geen data
        } else {
            priceNow = fiveMinutePrices[fiveMinuteIndex - 1];
        }
    }
    float price5mAgo;
    
    if (fiveMinuteArrayFilled)
    {
        // Array is gevuld (ringbuffer), fiveMinuteIndex wijst naar volgende schrijfpositie
        // 300 seconden geleden: gebruik dezelfde logica als calculateReturn1Minute
        // Correcte berekening: (fiveMinuteIndex - 300 + SECONDS_PER_5MINUTES * 2) % SECONDS_PER_5MINUTES
        int32_t idx5mAgo = ((int32_t)fiveMinuteIndex - 300 + SECONDS_PER_5MINUTES * 2) % SECONDS_PER_5MINUTES;
        
        // Extra veiligheidscheck
        if (idx5mAgo < 0 || idx5mAgo >= SECONDS_PER_5MINUTES) {
            Serial_printf("[Ret5m] FATAL: idx5mAgo=%ld buiten bereik [0,%u], fiveMinuteIndex=%u\n", idx5mAgo, SECONDS_PER_5MINUTES, fiveMinuteIndex);
            return 0.0f;
        }
        
        price5mAgo = fiveMinutePrices[(uint16_t)idx5mAgo];
    }
    else
    {
        // Array nog niet gevuld, fiveMinuteIndex = aantal geschreven waarden
        // 300 seconden geleden staat op fiveMinuteIndex - 300
        if (fiveMinuteIndex < 300) return 0.0f;
        price5mAgo = fiveMinutePrices[fiveMinuteIndex - 300];
    }
    
    if (price5mAgo <= 0.0f || priceNow <= 0.0f)
    {
        Serial_printf("[Ret5m] ERROR: fiveMinuteIndex=%u (filled=%d), priceNow=%.2f, price5mAgo=%.2f - invalid!\n",
                     fiveMinuteIndex, fiveMinuteArrayFilled, priceNow, price5mAgo);
        return 0.0f;
    }
    
    // Return als percentage: (nu - 5m geleden) / 5m geleden * 100
    float ret = ((priceNow - price5mAgo) / price5mAgo) * 100.0f;
    
    return ret;
}

// ret_30m: prijs nu vs 30 minuten geleden (gebruik minuteAverages)
static float calculateReturn30Minutes()
{
    // We hebben minimaal 30 minuten nodig
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled)
    {
        availableMinutes = minuteIndex;
    }
    else
    {
        availableMinutes = MINUTES_FOR_30MIN_CALC;
    }
    
    if (availableMinutes < 30)
    {
        // Debug: log wanneer we nog niet genoeg data hebben
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        if (now - lastLogTime > 60000) { // Log elke minuut
            Serial_printf("[Ret30m] Wachten op data: minuteIndex=%u (nodig: 30, available=%u, arrayFilled=%d)\n", 
                         minuteIndex, availableMinutes, minuteArrayFilled);
            lastLogTime = now;
        }
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Gebruik het laatste minuut-gemiddelde als priceNow (consistent met price30mAgo die ook een minuut-gemiddelde is)
    // minuteIndex wijst naar de volgende schrijfpositie, dus laatste geschreven positie is minuteIndex - 1
    uint8_t lastMinuteIdx;
    if (!minuteArrayFilled)
    {
        if (minuteIndex == 0) return 0.0f; // Nog geen minuut-gemiddelden
        lastMinuteIdx = minuteIndex - 1;
    }
    else
    {
        // Array is rond, laatste geschreven positie is (minuteIndex - 1 + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC
        lastMinuteIdx = (minuteIndex == 0) ? (MINUTES_FOR_30MIN_CALC - 1) : (minuteIndex - 1);
    }
    float priceNow = minuteAverages[lastMinuteIdx];
    
    // Haal prijs van 30 minuten geleden op uit minuteAverages
    uint8_t idx30mAgo;
    if (!minuteArrayFilled)
    {
        if (minuteIndex < 30) return 0.0f;
        idx30mAgo = minuteIndex - 30;
    }
    else
    {
        // Array is rond, bereken index 30 minuten geleden
        // minuteIndex wijst naar volgende schrijfpositie
        // 30 minuten geleden: gebruik dezelfde logica als calculateReturn1Minute
        // Correcte berekening: (minuteIndex - 30 + MINUTES_FOR_30MIN_CALC * 2) % MINUTES_FOR_30MIN_CALC
        int32_t idx30mAgo_temp = ((int32_t)minuteIndex - 30 + MINUTES_FOR_30MIN_CALC * 2) % MINUTES_FOR_30MIN_CALC;
        
        // Extra veiligheidscheck
        if (idx30mAgo_temp < 0 || idx30mAgo_temp >= MINUTES_FOR_30MIN_CALC) {
            Serial_printf("[Ret30m] FATAL: idx30mAgo=%ld buiten bereik [0,%u], minuteIndex=%u\n", idx30mAgo_temp, MINUTES_FOR_30MIN_CALC, minuteIndex);
            return 0.0f;
        }
        
        idx30mAgo = (uint8_t)idx30mAgo_temp;
    }
    
    float price30mAgo = minuteAverages[idx30mAgo];
    
    if (price30mAgo <= 0.0f || priceNow <= 0.0f)
    {
        Serial_printf("[Ret30m] ERROR: minuteIndex=%u (filled=%d), priceNow=%.2f, price30mAgo=%.2f (idx=%u) - invalid!\n",
                     minuteIndex, minuteArrayFilled, priceNow, price30mAgo, idx30mAgo);
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Bereken gemiddelde van laatste 30 minuten voor weergave
    float last30Sum = 0.0f;
    uint8_t last30Count = 0;
    for (uint8_t i = 0; i < 30; i++)
    {
        uint8_t idx;
        if (!minuteArrayFilled)
        {
            if (i >= minuteIndex) break;
            idx = minuteIndex - 1 - i;
        }
        else
        {
            // Veilige modulo berekening
            int32_t idx_temp = ((int32_t)minuteIndex - 1 - i + MINUTES_FOR_30MIN_CALC * 2) % MINUTES_FOR_30MIN_CALC;
            if (idx_temp < 0 || idx_temp >= MINUTES_FOR_30MIN_CALC) {
                break; // Buiten bereik, stop loop
            }
            idx = (uint8_t)idx_temp;
        }
        if (minuteAverages[idx] > 0.0f)
        {
            last30Sum += minuteAverages[idx];
            last30Count++;
        }
    }
    if (last30Count > 0)
    {
        averagePrices[2] = last30Sum / last30Count;
    }
    else
    {
        averagePrices[2] = 0.0f;
    }
    
    // Return als percentage: (nu - 30m geleden) / 30m geleden * 100
    float ret = ((priceNow - price30mAgo) / price30mAgo) * 100.0f;
    
    return ret;
}

// OUDE METHODE - behouden voor referentie, maar niet meer gebruikt
// Bereken lineaire regressie (trend) over de laatste 60 meetpunten
// Retourneert de helling (slope) als percentage per minuut
// Positieve waarde = stijgende trend, negatieve waarde = dalende trend
static float calculateLinearTrend1Minute()
{
    // We hebben minimaal 2 punten nodig voor een trend
    uint8_t count = secondArrayFilled ? SECONDS_PER_MINUTE : secondIndex;
    if (count < 2)
    {
        averagePrices[1] = 0.0f;
        return 0.0f;
    }
    
    // Bereken gemiddelde prijs voor weergave
    float currentAvg = calculateAverage(secondPrices, SECONDS_PER_MINUTE, secondArrayFilled);
    averagePrices[1] = currentAvg;
    
    // Lineaire regressie: y = a + b*x
    // x = tijd (0 tot count-1), y = prijs
    // b (slope) = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint8_t validPoints = 0;
    
    // Loop door alle beschikbare punten
    for (uint8_t i = 0; i < count; i++)
    {
        float price = secondPrices[i];
        if (price > 0.0f)
        {
            float x = (float)i; // Tijd index (0 tot count-1)
            float y = price;
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            validPoints++;
        }
    }
    
    if (validPoints < 2)
    {
        return 0.0f;
    }
    
    // Bereken slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f) // Voorkom deling door nul
    {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is nu de prijsverandering per seconde
    // Omzetten naar percentage per minuut: (slope * 60) / gemiddelde_prijs * 100
    if (currentAvg > 0.0f)
    {
        float slopePerMinute = slope * 60.0f; // Prijsverandering per minuut
        float pctPerMinute = (slopePerMinute / currentAvg) * 100.0f;
        return pctPerMinute;
    }
    
    return 0.0f;
}

// Calculate 1 minute moving average percentage and update average price
// OUDE METHODE - behouden voor referentie, maar niet meer gebruikt
static float calculate1MinutePct()
{
    // We hebben minimaal 60 seconden nodig voor huidige gemiddelde
    if (!secondArrayFilled && secondPrices[0] == 0.0f)
    {
        averagePrices[1] = 0.0f; // Reset gemiddelde prijs
        return 0.0f;
    }
    
    // Bereken gemiddelde van laatste 60 seconden
    float currentAvg = calculateAverage(secondPrices, SECONDS_PER_MINUTE, secondArrayFilled);
    averagePrices[1] = currentAvg; // Sla gemiddelde prijs op
    
    // Bereken gemiddelde van 60 seconden daarvoor (1 minuut geleden)
    // Dit is het gemiddelde dat 1 minuut geleden werd opgeslagen in minuteAverages
    // We hebben minstens 1 minuut geschiedenis nodig (minuteIndex moet > 0 of array moet gevuld zijn)
    if (minuteIndex == 0 && !minuteArrayFilled)
    {
        return 0.0f; // Nog geen minuut gemiddelde opgeslagen
    }
    
    // Het gemiddelde van 1 minuut geleden staat op de vorige positie in minuteAverages
    uint8_t prevMinuteIndex = (minuteIndex == 0) ? (MINUTES_FOR_30MIN_CALC - 1) : (minuteIndex - 1);
    float prevMinuteAvg = minuteAverages[prevMinuteIndex];
    
    if (prevMinuteAvg == 0.0f)
        return 0.0f;
    
    float pct = ((currentAvg - prevMinuteAvg) / prevMinuteAvg) * 100.0f;
    return pct;
}

// Bereken lineaire regressie (trend) over de laatste 30 minuten
// Retourneert de helling (slope) als percentage per uur
// Positieve waarde = stijgende trend, negatieve waarde = dalende trend
static float calculateLinearTrend30Minutes()
{
    // Tel aantal beschikbare minuten
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled)
    {
        availableMinutes = minuteIndex;
    }
    else
    {
        availableMinutes = MINUTES_FOR_30MIN_CALC;
    }
    
    // We hebben minimaal 30 minuten nodig voor een betrouwbare trend
    if (availableMinutes < 30)
    {
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Gebruik laatste 30 minuten voor trend berekening
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint8_t validPoints = 0;
    float last30Sum = 0.0f;
    uint8_t last30Count = 0;
    
    // Loop door laatste 30 minuten
    for (uint8_t i = 0; i < 30; i++)
    {
        uint8_t idx;
        if (!minuteArrayFilled)
        {
            // Array nog niet rond, gebruik laatste 30 minuten vanaf minuteIndex
            if (i >= minuteIndex) break; // Niet genoeg data
            idx = minuteIndex - 1 - i; // Start bij laatste minuut en werk achteruit
        }
        else
        {
            // Array is rond, gebruik laatste 30 minuten
            idx = (minuteIndex - 1 - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
        }
        
        float price = minuteAverages[idx];
        if (price > 0.0f)
        {
            float x = (float)i; // Tijd index (0 tot 29, waarbij 0 = oudste, 29 = nieuwste)
            float y = price;
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            last30Sum += price;
            last30Count++;
            validPoints++;
        }
    }
    
    if (validPoints < 2)
    {
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Bereken gemiddelde prijs voor weergave
    float last30Avg = last30Sum / last30Count;
    averagePrices[2] = last30Avg;
    
    // Bereken slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f) // Voorkom deling door nul
    {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is nu de prijsverandering per minuut
    // Omzetten naar percentage per uur: (slope * 60) / gemiddelde_prijs * 100
    if (last30Avg > 0.0f)
    {
        float slopePerHour = slope * 60.0f; // Prijsverandering per uur
        float pctPerHour = (slopePerHour / last30Avg) * 100.0f;
        return pctPerHour;
    }
    
    return 0.0f;
}

// Calculate 30 minute moving average percentage
// OUDE METHODE - behouden voor referentie, maar niet meer gebruikt
static float calculate30MinutePct()
{
    // Tel aantal beschikbare minuten (alleen als array nog niet rond is gegaan)
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled)
    {
        // Array is nog niet rond gegaan, tel van 0 tot minuteIndex
        availableMinutes = minuteIndex;
    }
    else
    {
        // Array is gevuld, we hebben altijd 120 minuten beschikbaar
        availableMinutes = MINUTES_FOR_30MIN_CALC;
    }
    
    // Als we nog niet genoeg data hebben (minder dan 60 minuten), gebruik eerste minuut als basis
    if (availableMinutes < 60)
    {
        if (firstMinuteAverage == 0.0f || availableMinutes == 0)
        {
            averagePrices[2] = 0.0f;
        return 0.0f;
        }
        
        // Bereken gemiddelde van alle beschikbare minuten
        float currentSum = 0.0f;
        uint8_t currentCount = 0;
        if (!minuteArrayFilled)
        {
            // Array is nog niet rond gegaan, tel van 0 tot minuteIndex
            for (uint8_t i = 0; i < minuteIndex; i++)
            {
                if (minuteAverages[i] != 0.0f)
                {
                    currentSum += minuteAverages[i];
                    currentCount++;
                }
            }
        }
        else
        {
            // Array is gevuld, tel alle posities (maar dit zou niet moeten gebeuren als availableMinutes < 60)
            for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++)
            {
                if (minuteAverages[i] != 0.0f)
                {
                    currentSum += minuteAverages[i];
                    currentCount++;
                }
            }
        }
        
        if (currentCount == 0)
        {
            averagePrices[2] = 0.0f;
            return 0.0f;
        }
        
        float currentAvg = currentSum / currentCount;
        averagePrices[2] = currentAvg; // Sla gemiddelde prijs op
        
        // Vergelijk met eerste minuut gemiddelde
        float pct = ((currentAvg - firstMinuteAverage) / firstMinuteAverage) * 100.0f;
        return pct;
    }
    
    // Normale berekening: we hebben minimaal 60 minuten geschiedenis
    // (30 minuten voor huidige periode + 30 minuten voor vorige periode)
    
    // Bereken gemiddelde van laatste 30 minuten (nieuwste waarden)
    // minuteIndex wijst naar de volgende positie, dus nieuwste is op (minuteIndex - 1)
    float last30Sum = 0.0f;
    uint8_t last30Count = 0;
    for (uint8_t i = 1; i <= 30; i++)
    {
        // Start vanaf 1 positie terug (nieuwste) tot 30 posities terug
        uint8_t idx = (minuteIndex - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
        if (minuteAverages[idx] != 0.0f)
        {
            last30Sum += minuteAverages[idx];
            last30Count++;
        }
    }
    
    // Bereken gemiddelde van 30 minuten daarvoor (oude waarden)
    float prev30Sum = 0.0f;
    uint8_t prev30Count = 0;
    for (uint8_t i = 31; i <= 60; i++)
    {
        // Start vanaf 31 posities terug tot 60 posities terug
        uint8_t idx = (minuteIndex - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
        if (minuteAverages[idx] != 0.0f)
        {
            prev30Sum += minuteAverages[idx];
            prev30Count++;
        }
    }
    
    if (prev30Count == 0 || prev30Sum == 0.0f || last30Count == 0)
    {
        averagePrices[2] = 0.0f; // Reset gemiddelde prijs
        return 0.0f;
    }
    
    float last30Avg = last30Sum / last30Count;
    averagePrices[2] = last30Avg; // Sla gemiddelde prijs op voor 30 minuten
    float prev30Avg = prev30Sum / prev30Count;
    
    if (prev30Avg == 0.0f)
        return 0.0f;
    
    float pct = ((last30Avg - prev30Avg) / prev30Avg) * 100.0f;
    return pct;
}

// ret_2h: prijs nu vs 120 minuten (2 uur) geleden (gebruik minuteAverages)
static float calculateReturn2Hours()
{
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled)
    {
        availableMinutes = minuteIndex;
    }
    else
    {
        availableMinutes = MINUTES_FOR_30MIN_CALC;  // 120 minuten
    }
    
    if (availableMinutes < 120)
    {
        return 0.0f;
    }
    
    // Gebruik het laatste minuut-gemiddelde als priceNow
    uint8_t lastMinuteIdx;
    if (!minuteArrayFilled)
    {
        if (minuteIndex == 0) return 0.0f;
        lastMinuteIdx = minuteIndex - 1;
    }
    else
    {
        lastMinuteIdx = (minuteIndex == 0) ? (MINUTES_FOR_30MIN_CALC - 1) : (minuteIndex - 1);
    }
    float priceNow = minuteAverages[lastMinuteIdx];
    
    // Prijs 120 minuten geleden
    uint8_t idx120mAgo;
    if (!minuteArrayFilled)
    {
        if (minuteIndex < 120) return 0.0f;
        idx120mAgo = minuteIndex - 120;
    }
    else
    {
        int32_t idx120mAgo_temp = ((int32_t)minuteIndex - 120 + MINUTES_FOR_30MIN_CALC * 2) % MINUTES_FOR_30MIN_CALC;
        if (idx120mAgo_temp < 0 || idx120mAgo_temp >= MINUTES_FOR_30MIN_CALC) {
            return 0.0f;
        }
        idx120mAgo = (uint8_t)idx120mAgo_temp;
    }
    
    float price120mAgo = minuteAverages[idx120mAgo];
    
    if (price120mAgo <= 0.0f || priceNow <= 0.0f)
    {
        return 0.0f;
    }
    
    float ret = ((priceNow - price120mAgo) / price120mAgo) * 100.0f;
    return ret;
}

// ============================================================================
// Trend Detection Functions
// ============================================================================

// Bepaal trend state op basis van 2h return en optioneel 30m return
static TrendState determineTrendState(float ret_2h_value, float ret_30m_value)
{
    if (ret_2h_value > trendThreshold)
    {
        return TREND_UP;
    }
    else if (ret_2h_value < -trendThreshold)
    {
        return TREND_DOWN;
    }
    else
    {
        return TREND_SIDEWAYS;
    }
}

// Voeg absolute 1m return toe aan volatiliteit buffer (wordt elke minuut aangeroepen)
// Geoptimaliseerd: bounds checking en validatie toegevoegd
static void addAbs1mReturnToVolatilityBuffer(float abs_ret_1m)
{
    // Zorg dat het absoluut is
    if (abs_ret_1m < 0.0f) abs_ret_1m = -abs_ret_1m;
    
    // Valideer input
    if (isnan(abs_ret_1m) || isinf(abs_ret_1m))
    {
        Serial_printf("[Array] WARN: Ongeldige abs_ret_1m: %.2f\n", abs_ret_1m);
        return;
    }
    
    // Bounds check voor abs1mReturns array
    if (volatilityIndex >= VOLATILITY_LOOKBACK_MINUTES)
    {
        Serial_printf("[Array] ERROR: volatilityIndex buiten bereik: %u >= %u\n", volatilityIndex, VOLATILITY_LOOKBACK_MINUTES);
        volatilityIndex = 0; // Reset naar veilige waarde
    }
    
    abs1mReturns[volatilityIndex] = abs_ret_1m;
    volatilityIndex = (volatilityIndex + 1) % VOLATILITY_LOOKBACK_MINUTES;
    
    if (volatilityIndex == 0)
    {
        volatilityArrayFilled = true;
    }
}

// Bereken gemiddelde van absolute 1m returns over laatste 60 minuten
static float calculateAverageAbs1mReturn()
{
    uint8_t count = volatilityArrayFilled ? VOLATILITY_LOOKBACK_MINUTES : volatilityIndex;
    
    if (count == 0)
    {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; i++)
    {
        sum += abs1mReturns[i];
    }
    
    return sum / count;
}

// Bepaal volatiliteit state op basis van gemiddelde absolute 1m return
static VolatilityState determineVolatilityState(float avg_abs_1m)
{
    // Volatiliteit bepaling (geoptimaliseerd: LOW < 0.05%, HIGH >= 0.15%)
    if (avg_abs_1m < volatilityLowThreshold)
    {
        return VOLATILITY_LOW;  // Rustig: < 0.05%
    }
    else if (avg_abs_1m < volatilityHighThreshold)
    {
        return VOLATILITY_MEDIUM;  // Gemiddeld: 0.05% - 0.15%
    }
    else
    {
        return VOLATILITY_HIGH;  // Volatiel: >= 0.15%
    }
}

// Find min and max values in last 30 minutes of minuteAverages array
static void findMinMaxInLast30Minutes(float &minVal, float &maxVal)
{
    minVal = 0.0f;
    maxVal = 0.0f;
    
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled)
    {
        availableMinutes = minuteIndex;
    }
    else
    {
        availableMinutes = MINUTES_FOR_30MIN_CALC;
    }
    
    if (availableMinutes == 0)
        return;
    
    // Gebruik laatste 30 minuten (of minder als niet beschikbaar)
    uint8_t count = (availableMinutes < 30) ? availableMinutes : 30;
    bool firstValid = false;
    
    for (uint8_t i = 1; i <= count; i++)
    {
        uint8_t idx = (minuteIndex - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
        if (minuteAverages[idx] > 0.0f)
        {
            if (!firstValid)
            {
                minVal = minuteAverages[idx];
                maxVal = minuteAverages[idx];
                firstValid = true;
            }
            else
            {
                if (minuteAverages[idx] < minVal) minVal = minuteAverages[idx];
                if (minuteAverages[idx] > maxVal) maxVal = minuteAverages[idx];
            }
        }
    }
}

// Add price to second array (called every second)
// Geoptimaliseerd: bounds checking toegevoegd voor robuustheid
static void addPriceToSecondArray(float price)
{
    // Valideer input
    if (isnan(price) || isinf(price) || price <= 0.0f)
    {
        Serial_printf("[Array] WARN: Ongeldige prijs in addPriceToSecondArray: %.2f\n", price);
        return;
    }
    
    // Bounds check voor secondPrices array
    if (secondIndex >= SECONDS_PER_MINUTE)
    {
        Serial_printf("[Array] ERROR: secondIndex buiten bereik: %u >= %u\n", secondIndex, SECONDS_PER_MINUTE);
        secondIndex = 0; // Reset naar veilige waarde
    }
    
    secondPrices[secondIndex] = price;
    secondIndex = (secondIndex + 1) % SECONDS_PER_MINUTE;
    if (secondIndex == 0)
    {
        secondArrayFilled = true;
    }
    
    // Ook toevoegen aan 5-minuten buffer met bounds checking
    if (fiveMinuteIndex >= SECONDS_PER_5MINUTES)
    {
        Serial_printf("[Array] ERROR: fiveMinuteIndex buiten bereik: %u >= %u\n", fiveMinuteIndex, SECONDS_PER_5MINUTES);
        fiveMinuteIndex = 0; // Reset naar veilige waarde
    }
    
    fiveMinutePrices[fiveMinuteIndex] = price;
    fiveMinuteIndex = (fiveMinuteIndex + 1) % SECONDS_PER_5MINUTES;
    if (fiveMinuteIndex == 0)
    {
        fiveMinuteArrayFilled = true;
    }
}

// Update minute averages (called every minute)
// Geoptimaliseerd: bounds checking en validatie toegevoegd
static void updateMinuteAverage()
{
    // Bereken gemiddelde van de 60 seconden
    float minuteAvg = calculateAverage(secondPrices, SECONDS_PER_MINUTE, secondArrayFilled);
    
    // Valideer gemiddelde
    if (isnan(minuteAvg) || isinf(minuteAvg) || minuteAvg <= 0.0f)
    {
        Serial_printf("[Array] WARN: Ongeldig minuut gemiddelde: %.2f\n", minuteAvg);
        return; // Skip update bij ongeldige data
    }
    
    // Sla eerste minuut gemiddelde op als basis voor 30-min berekening
    if (firstMinuteAverage == 0.0f && minuteAvg > 0.0f)
    {
        firstMinuteAverage = minuteAvg;
    }
    
    // Bounds check voor minuteAverages array
    if (minuteIndex >= MINUTES_FOR_30MIN_CALC)
    {
        Serial_printf("[Array] ERROR: minuteIndex buiten bereik: %u >= %u\n", minuteIndex, MINUTES_FOR_30MIN_CALC);
        minuteIndex = 0; // Reset naar veilige waarde
    }
    
    // Sla op in minute array
    minuteAverages[minuteIndex] = minuteAvg;
    minuteIndex = (minuteIndex + 1) % MINUTES_FOR_30MIN_CALC;
    if (minuteIndex == 0)
        minuteArrayFilled = true;
}

// ============================================================================
// Price Fetching and Management Functions
// ============================================================================

// Fetch the symbols' current prices (thread-safe met mutex)
static void fetchPrice()
{
    // Controleer eerst of WiFi verbonden is
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi niet verbonden, skip fetch");
        return;
    }
    
    unsigned long fetchStart = millis();
    float fetched = prices[0]; // Start met huidige waarde als fallback
    bool ok = false;

    // Geoptimaliseerd: gebruik char array i.p.v. String concatenatie om geheugenfragmentatie te voorkomen
    char url[128];
    snprintf(url, sizeof(url), "%s%s", BINANCE_API, binanceSymbol);
    String body = httpGET(url, HTTP_TIMEOUT_MS);
    unsigned long fetchTime = millis() - fetchStart;
    
    if (body.isEmpty()) {
        // Leeg response - kan komen door timeout of netwerkproblemen
        Serial.printf("[API] WARN -> %s leeg response (tijd: %lu ms) - mogelijk timeout of netwerkprobleem\n", binanceSymbol, fetchTime);
        // Gebruik laatste bekende prijs als fallback (al ingesteld als fetched = prices[0])
    } else if (!parsePrice(body, fetched)) {
        Serial.printf("[API] ERR -> %s parse gefaald\n", binanceSymbol);
    } else {
        // Succesvol opgehaald (alleen loggen bij langzame calls > 1200ms)
        if (fetchTime > 1200) {
            Serial.printf("[API] OK -> %s %.2f (tijd: %lu ms) - langzaam\n", binanceSymbol, fetched, fetchTime);
        }
        
        // Neem mutex voor data updates (timeout aangepast per platform)
        // CYD heeft meer rendering overhead, dus iets kortere timeout om UI task meer kans te geven
        #ifdef PLATFORM_TTGO
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(300); // TTGO: 300ms
        #else
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(200); // CYD: 200ms voor snellere UI updates
        #endif
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        static uint32_t mutexTimeoutCount = 0;
        if (xSemaphoreTake(dataMutex, apiMutexTimeout) == pdTRUE)
        {
            // Reset timeout counter bij succes
            if (mutexTimeoutCount > 0) {
                mutexTimeoutCount = 0;
            }
            
            if (openPrices[0] == 0)
                openPrices[0] = fetched; // capture session open once
            lastApiMs = millis();
            
            prices[0] = fetched;
            
            // Update anchor min/max als anchor actief is
            if (anchorActive && anchorPrice > 0.0f) {
                if (fetched > anchorMax) {
                    anchorMax = fetched;
                }
                if (fetched < anchorMin) {
                    anchorMin = fetched;
                }
            }
            
            // Add price to second array (every second)
            addPriceToSecondArray(fetched);
            
            // Update minute average every minute
            unsigned long now = millis();
            bool minuteUpdate = (lastMinuteUpdate == 0 || (now - lastMinuteUpdate >= 60000UL)); // 60 seconden
            if (minuteUpdate)
            {
                updateMinuteAverage();
                lastMinuteUpdate = now;
            }
            
            // Calculate returns for 1 minute, 5 minutes, 30 minutes, and 2 hours
            float ret_1m = calculateReturn1Minute();   // Percentage verandering laatste 1 minuut
            float ret_5m = calculateReturn5Minutes();  // Percentage verandering laatste 5 minuten
            float ret_30m = calculateReturn30Minutes(); // Percentage verandering laatste 30 minuten
            ret_2h = calculateReturn2Hours();
            
            // Bepaal trend state op basis van 2h return
            trendState = determineTrendState(ret_2h, ret_30m);
            
            // Check trend change en stuur notificatie indien nodig
            checkTrendChange(ret_30m);
            
            // Update volatiliteit buffer elke minuut met absolute 1m return
            if (minuteUpdate && ret_1m != 0.0f)
            {
                float abs_ret_1m = fabsf(ret_1m);
                addAbs1mReturnToVolatilityBuffer(abs_ret_1m);
                
                // Bereken gemiddelde en bepaal volatiliteit state
                float avg_abs_1m = calculateAverageAbs1mReturn();
                if (avg_abs_1m > 0.0f)
                {
                    volatilityState = determineVolatilityState(avg_abs_1m);
                }
            }
            
            // Voor weergave op scherm gebruiken we ret_1m en ret_30m
            // Alleen zetten als er data is, anders blijven ze 0.0f (wat wordt geïnterpreteerd als "geen data")
            if (secondArrayFilled) {
                prices[1] = ret_1m;
            } else {
                prices[1] = 0.0f; // Reset naar 0 om aan te geven dat er nog geen data is
            }
            if (minuteArrayFilled || minuteIndex >= 30) {
                prices[2] = ret_30m;
            } else {
                prices[2] = 0.0f; // Reset naar 0 om aan te geven dat er nog geen data is
            }
            
            // Check thresholds and send notifications if needed (met ret_5m voor extra filtering)
            checkAndNotify(ret_1m, ret_5m, ret_30m);
            
            // Check anchor take profit / max loss alerts
            checkAnchorAlerts();
            
            // Touchscreen anchor set functionaliteit verwijderd - gebruik nu fysieke knop
            
            // Publiceer waarden naar MQTT
            publishMqttValues(fetched, ret_1m, ret_5m, ret_30m);
            
            // Zet flag voor nieuwe data (voor grafiek update)
            newPriceDataAvailable = true;
            
            xSemaphoreGive(dataMutex);
            ok = true;
        } else {
            // Geoptimaliseerd: log alleen bij meerdere opeenvolgende timeouts
            mutexTimeoutCount++;
            if (mutexTimeoutCount == 1 || mutexTimeoutCount % 10 == 0) {
                Serial.printf("[API] WARN -> %s mutex timeout (count: %lu)\n", binanceSymbol, mutexTimeoutCount);
            }
            // Fallback: update prijs zonder mutex als timeout te vaak voorkomt (alleen voor noodgeval)
            if (mutexTimeoutCount > 50) {
                Serial.printf("[API] CRIT -> %s mutex timeout te vaak, mogelijk deadlock!\n", binanceSymbol);
                mutexTimeoutCount = 0; // Reset counter
            }
        }
    }
}

// Update the UI (wordt aangeroepen vanuit uiTask met mutex)
// Update UI - Refactored to use helper functions
void updateUI()
{
    // Veiligheid: controleer of chart en dataSeries bestaan voordat we ze gebruiken
    if (chart == nullptr || dataSeries == nullptr) {
        Serial_println("[UI] WARN: Chart of dataSeries is null, skip update");
        return;
    }
    
    // Data wordt al beschermd door mutex in uiTask
    int32_t p = (int32_t)lroundf(prices[symbolIndexToChart] * 100.0f);
    
    // Voeg het nieuwe punt alleen toe als er nieuwe data is
    if (newPriceDataAvailable && prices[symbolIndexToChart] > 0.0f) {
        lv_chart_set_next_value(chart, dataSeries, p);
        newPriceDataAvailable = false;
    }

    // Update chart range
    updateChartRange(p);
    
    // Update chart title
    // Geoptimaliseerd: gebruik char array i.p.v. String om geheugen te besparen
    if (chartTitle != nullptr) {
        static char deviceIdBuffer[16] = {0}; // Static buffer om geheugenfragmentatie te voorkomen
        // Extract device ID direct uit topic zonder String gebruik
        const char* alertPos = strstr(ntfyTopic, "-alert");
        if (alertPos != nullptr) {
            size_t len = alertPos - ntfyTopic;
            if (len > 0 && len < sizeof(deviceIdBuffer)) {
                strncpy(deviceIdBuffer, ntfyTopic, len);
                deviceIdBuffer[len] = '\0';
            } else {
                strncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer) - 1);
                deviceIdBuffer[sizeof(deviceIdBuffer) - 1] = '\0';
            }
        } else {
            // Fallback: gebruik eerste deel van topic
            strncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer) - 1);
            deviceIdBuffer[sizeof(deviceIdBuffer) - 1] = '\0';
        }
        lv_label_set_text(chartTitle, deviceIdBuffer);
    }
    
    // Update datum/tijd labels
    updateDateTimeLabels();
    
    // Update trend en volatiliteit labels
    updateTrendLabel();
    updateVolatilityLabel();
    

    // Update price cards
    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i)
    {
        float pct = 0.0f;
        
        if (i == 0)
        {
            // BTCEUR card
            updateBTCEURCard();
            pct = 0.0f; // BTCEUR heeft geen percentage voor kleur
        }
        else
        {
            // 1min/30min cards
            pct = prices[i];
            updateAveragePriceCard(i);
        }

        // Update kleuren
        updatePriceCardColor(i, pct);
    }

    // Update footer
    updateFooter();
}

// ============================================================================
// UI Update Helper Functions - Refactored from updateUI() for better code organization
// ============================================================================

// Helper functie om chart range te berekenen en bij te werken
static void updateChartRange(int32_t currentPrice)
{
    int32_t chartMin = INT32_MAX;
    int32_t chartMax = INT32_MIN;
    int32_t sum = 0;
    uint16_t count = 0;
    
    int32_t *yArray = lv_chart_get_series_y_array(chart, dataSeries);
    
    for (uint16_t i = 0; i < POINTS_TO_CHART; i++)
    {
        int32_t val = yArray[i];
        if (val != LV_CHART_POINT_NONE)
        {
            if (val < chartMin) chartMin = val;
            if (val > chartMax) chartMax = val;
            sum += val;
            count++;
        }
    }
    
    int32_t chartAverage = 0;
    if (count > 0 && chartMin != INT32_MAX && chartMax != INT32_MIN)
    {
        chartAverage = sum / count;
        
        if (chartMin == INT32_MAX || chartMax == INT32_MIN || chartMin > chartMax)
        {
            chartMin = chartAverage - PRICE_RANGE;
            chartMax = chartAverage + PRICE_RANGE;
        }
        
        if (chartMin == chartMax)
        {
            int32_t minMargin = chartAverage / 100;
            if (minMargin < 10) minMargin = 10;
            chartMin = chartMin - minMargin;
            chartMax = chartMax + minMargin;
        }
        
        int32_t range = chartMax - chartMin;
        int32_t margin = range / 20;
        if (margin < 10) margin = 10;
        
        minRange = chartMin - margin;
        maxRange = chartMax + margin;
        
        if (currentPrice < minRange) minRange = currentPrice - margin;
        if (currentPrice > maxRange) maxRange = currentPrice + margin;
        
        if (minRange < 0) minRange = 0;
        if (maxRange < 0) maxRange = 0;
        if (minRange >= maxRange)
        {
            int32_t fallbackMargin = PRICE_RANGE / 20;
            if (fallbackMargin < 10) fallbackMargin = 10;
            minRange = chartAverage - PRICE_RANGE - fallbackMargin;
            maxRange = chartAverage + PRICE_RANGE + fallbackMargin;
            if (minRange < 0) minRange = 0;
        }
    }
    else
    {
        chartAverage = currentPrice;
        int32_t margin = PRICE_RANGE / 20;
        if (margin < 10) margin = 10;
        minRange = currentPrice - PRICE_RANGE - margin;
        maxRange = currentPrice + PRICE_RANGE + margin;
    }
    
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);
}

// Helper functie om datum/tijd labels bij te werken
static void updateDateTimeLabels()
{
    if (chartDateLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char dateStr[9];
            strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
            lv_label_set_text(chartDateLabel, dateStr);
        }
    }
    
    if (chartTimeLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char timeStr[9];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            lv_label_set_text(chartTimeLabel, timeStr);
        }
    }
}

// Helper functie om trend label bij te werken
static void updateTrendLabel()
{
    if (trendLabel == nullptr) return;
    
    if (ret_2h != 0.0f && (minuteArrayFilled || minuteIndex >= 120))
    {
        const char* trendText = "";
        lv_color_t trendColor = lv_palette_main(LV_PALETTE_GREY);
        
        switch (trendState) {
            case TREND_UP:
                trendText = getText("OMHOOG", "UP");
                trendColor = lv_palette_main(LV_PALETTE_GREEN);
                break;
            case TREND_DOWN:
                trendText = getText("OMLAAG", "DOWN");
                trendColor = lv_palette_main(LV_PALETTE_RED);
                break;
            case TREND_SIDEWAYS:
            default:
                trendText = getText("ZIJWAARTS", "SIDEWAYS");
                trendColor = lv_palette_main(LV_PALETTE_GREY);
                break;
        }
        
        lv_label_set_text(trendLabel, trendText);
        lv_obj_set_style_text_color(trendLabel, trendColor, 0);
    }
    else
    {
        uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
        uint8_t minutesNeeded = (availableMinutes < 120) ? (120 - availableMinutes) : 0;
        
        if (minutesNeeded > 0) {
            char waitText[16];
            getTrendWaitText(waitText, sizeof(waitText), minutesNeeded);
            lv_label_set_text(trendLabel, waitText);
        } else {
            lv_label_set_text(trendLabel, "--");
        }
        lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    }
}

// Helper functie om volatiliteit label bij te werken
static void updateVolatilityLabel()
{
    if (volatilityLabel == nullptr) return;
    
    const char* volText = "";
    lv_color_t volColor = lv_palette_main(LV_PALETTE_GREY);
    
    switch (volatilityState) {
        case VOLATILITY_LOW:
            volText = getText("RUSTIG", "CALM");
            volColor = lv_palette_main(LV_PALETTE_GREEN);
            break;
        case VOLATILITY_MEDIUM:
            volText = getText("GEMIDDELD", "MEDIUM");
            volColor = lv_palette_main(LV_PALETTE_ORANGE);
            break;
        case VOLATILITY_HIGH:
            volText = getText("VOLATIEL", "VOLATILE");
            volColor = lv_palette_main(LV_PALETTE_RED);
            break;
    }
    
    lv_label_set_text(volatilityLabel, volText);
    lv_obj_set_style_text_color(volatilityLabel, volColor, 0);
}

// ============================================================================
// RGB LED Functions (alleen voor CYD platforms)
// ============================================================================


// Helper functie om BTCEUR card bij te werken
static void updateBTCEURCard()
{
    if (priceTitle[0] != nullptr) {
        lv_label_set_text(priceTitle[0], "BTCEUR");
    }
    
    lv_label_set_text_fmt(priceLbl[0], "%.2f", prices[0]);
    
    #ifdef PLATFORM_TTGO
    if (anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            float takeProfitPrice = anchorPrice * (1.0f + anchorTakeProfit / 100.0f);
            lv_label_set_text_fmt(anchorMaxLabel, "%.2f", takeProfitPrice);
        } else {
            lv_label_set_text(anchorMaxLabel, "");
        }
    }
    
    if (anchorLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            lv_label_set_text_fmt(anchorLabel, "%.2f", anchorPrice);
        } else {
            lv_label_set_text(anchorLabel, "");
        }
    }
    
    if (anchorMinLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            float stopLossPrice = anchorPrice * (1.0f + anchorMaxLoss / 100.0f);
            lv_label_set_text_fmt(anchorMinLabel, "%.2f", stopLossPrice);
        } else {
            lv_label_set_text(anchorMinLabel, "");
        }
    }
    #else
    if (anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            float takeProfitPrice = anchorPrice * (1.0f + anchorTakeProfit / 100.0f);
            lv_label_set_text_fmt(anchorMaxLabel, "+%.2f%% %.2f", anchorTakeProfit, takeProfitPrice);
        } else {
            lv_label_set_text(anchorMaxLabel, "");
        }
    }
    
    if (anchorLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f && prices[0] > 0.0f) {
            float anchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
            lv_label_set_text_fmt(anchorLabel, "%c%.2f%% %.2f",
                                  anchorPct >= 0 ? '+' : '-', fabsf(anchorPct), anchorPrice);
        } else if (anchorActive && anchorPrice > 0.0f) {
            lv_label_set_text_fmt(anchorLabel, "%.2f", anchorPrice);
        } else {
            lv_label_set_text(anchorLabel, "");
        }
    }
    
    if (anchorMinLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            float stopLossPrice = anchorPrice * (1.0f + anchorMaxLoss / 100.0f);
            lv_label_set_text_fmt(anchorMinLabel, "%.2f%% %.2f", anchorMaxLoss, stopLossPrice);
        } else {
            lv_label_set_text(anchorMinLabel, "");
        }
    }
    #endif
}

// Helper functie om average price cards (1min/30min) bij te werken
static void updateAveragePriceCard(uint8_t index)
{
    float pct = prices[index];
    bool hasData1m = (index == 1) ? secondArrayFilled : true;
    bool hasData30m = (index == 2) ? (minuteArrayFilled || minuteIndex >= 30) : true;
    bool hasData = (index == 1) ? hasData1m : ((index == 2) ? hasData30m : true);
    
    if (!hasData) {
        pct = 0.0f;
    }
    
    if (priceTitle[index] != nullptr) {
        if (hasData && pct != 0.0f) {
            lv_label_set_text_fmt(priceTitle[index], "%s  %c%.2f%%", symbols[index], pct >= 0 ? '+' : '-', fabsf(pct));
        } else {
            lv_label_set_text(priceTitle[index], symbols[index]);
        }
    }
    
    if (index == 1 && price1MinMaxLabel != nullptr && price1MinMinLabel != nullptr && price1MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInSecondPrices(minVal, maxVal);
        
        if (minVal > 0.0f && maxVal > 0.0f)
        {
            float diff = maxVal - minVal;
            lv_label_set_text_fmt(price1MinMaxLabel, "%.2f", maxVal);
            lv_label_set_text_fmt(price1MinDiffLabel, "%.2f", diff);
            lv_label_set_text_fmt(price1MinMinLabel, "%.2f", minVal);
        }
        else
        {
            lv_label_set_text(price1MinMaxLabel, "--");
            lv_label_set_text(price1MinDiffLabel, "--");
            lv_label_set_text(price1MinMinLabel, "--");
        }
    }
    
    if (index == 2 && price30MinMaxLabel != nullptr && price30MinMinLabel != nullptr && price30MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInLast30Minutes(minVal, maxVal);
        
        if (minVal > 0.0f && maxVal > 0.0f)
        {
            float diff = maxVal - minVal;
            lv_label_set_text_fmt(price30MinMaxLabel, "%.2f", maxVal);
            lv_label_set_text_fmt(price30MinDiffLabel, "%.2f", diff);
            lv_label_set_text_fmt(price30MinMinLabel, "%.2f", minVal);
        }
        else
        {
            lv_label_set_text(price30MinMaxLabel, "--");
            lv_label_set_text(price30MinDiffLabel, "--");
            lv_label_set_text(price30MinMinLabel, "--");
        }
    }
    
    if (!hasData)
    {
        lv_label_set_text(priceLbl[index], "--");
    }
    else if (averagePrices[index] > 0.0f)
    {
        lv_label_set_text_fmt(priceLbl[index], "%.2f", averagePrices[index]);
    }
    else
    {
        lv_label_set_text(priceLbl[index], "--");
    }
}

// Helper functie om price card kleuren bij te werken
static void updatePriceCardColor(uint8_t index, float pct)
{
    bool hasDataForColor = (index == 0) ? true : ((index == 1) ? secondArrayFilled : (minuteArrayFilled || minuteIndex >= 30));
    
    if (hasDataForColor && pct != 0.0f)
    {
        lv_obj_set_style_text_color(priceLbl[index],
                                    pct >= 0 ? lv_palette_lighten(LV_PALETTE_GREEN, 4)
                                             : lv_palette_lighten(LV_PALETTE_RED, 3),
                                    0);
        
        lv_color_t bg = pct >= 0
                            ? lv_color_mix(lv_palette_main(LV_PALETTE_GREEN), lv_color_black(), 127)
                            : lv_color_mix(lv_palette_main(LV_PALETTE_RED), lv_color_black(), 127);
        lv_obj_set_style_bg_color(priceBox[index], bg, 0);
    }
    else
    {
        lv_obj_set_style_text_color(priceLbl[index], lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_bg_color(priceBox[index], lv_color_black(), 0);
    }
    
    lv_obj_set_height(priceBox[index], LV_SIZE_CONTENT);
}

// Helper functie om footer bij te werken
static void updateFooter()
{
    #ifdef PLATFORM_TTGO
    if (ipLabel != nullptr) {
        if (WiFi.status() == WL_CONNECTED) {
            // Geoptimaliseerd: gebruik char array i.p.v. String
            static char ipBuffer[16];
            formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
            lv_label_set_text(ipLabel, ipBuffer);
        } else {
            lv_label_set_text(ipLabel, "--");
        }
    }
    #else
    if (lblFooterLine1 != nullptr) {
        int rssi = 0;
        uint32_t freeRAM = 0;
        
        if (WiFi.status() == WL_CONNECTED) {
            rssi = WiFi.RSSI();
        }
        
        freeRAM = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
        
        lv_label_set_text_fmt(lblFooterLine1, "%ddBm", rssi);
        
        if (ramLabel != nullptr) {
            lv_label_set_text_fmt(ramLabel, "%ukB", freeRAM);
        }
    }
    
    if (lblFooterLine2 != nullptr) {
        // Geoptimaliseerd: gebruik char array i.p.v. String
        static char ipStr[16] = "--.--.--.--";
        
        if (WiFi.status() == WL_CONNECTED) {
            formatIPAddress(WiFi.localIP(), ipStr, sizeof(ipStr));
        } else {
            strncpy(ipStr, "--.--.--.--", sizeof(ipStr) - 1);
            ipStr[sizeof(ipStr) - 1] = '\0';
        }
        
        lv_label_set_text(lblFooterLine2, ipStr);
    }
    #endif
}

// ============================================================================
// UI Helper Functions - Refactored from buildUI() for better code organization
// ============================================================================

// Helper functie om scroll uit te schakelen voor een object
static void disableScroll(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(obj, 0, LV_PART_SCROLLBAR);
}

// Helper functie om chart en bijbehorende labels te creëren
static void createChart()
{
    // Chart - gebruik platform-specifieke afmetingen
    chart = lv_chart_create(lv_scr_act());
    lv_chart_set_point_count(chart, POINTS_TO_CHART);
    lv_obj_set_size(chart, CHART_WIDTH, CHART_HEIGHT);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, CHART_ALIGN_Y);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    disableScroll(chart);
    
    int32_t p = (int32_t)lroundf(openPrices[symbolIndexToChart] * 100.0f);
    maxRange = p + PRICE_RANGE;
    minRange = p - PRICE_RANGE;
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);

    // Maak één blauwe serie aan voor alle punten
    dataSeries = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    // Trend/volatiliteit labels in de chart, links uitgelijnd binnen de chart
    trendLabel = lv_label_create(chart);
    lv_obj_set_style_text_font(trendLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(trendLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(trendLabel, LV_ALIGN_TOP_LEFT, -4, -6);
    lv_label_set_text(trendLabel, "--");
    
    volatilityLabel = lv_label_create(chart);
    lv_obj_set_style_text_font(volatilityLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(volatilityLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(volatilityLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(volatilityLabel, LV_ALIGN_BOTTOM_LEFT, -4, 6);
    lv_label_set_text(volatilityLabel, "--");
    
    // Platform-specifieke layout voor chart title
    #ifndef PLATFORM_TTGO
    chartTitle = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTitle, &lv_font_montserrat_16, 0);
    String deviceId = getDeviceIdFromTopic(ntfyTopic);
    lv_label_set_text(chartTitle, deviceId.c_str());
    lv_obj_set_style_text_color(chartTitle, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align_to(chartTitle, chart, LV_ALIGN_OUT_TOP_LEFT, 0, -4);
    #endif
}

// Helper functie om header labels (datum/tijd/versie) te creëren
static void createHeaderLabels()
{
    #ifdef PLATFORM_TTGO
    // TTGO: Compacte layout met datum op regel 1, beginletters/versie/tijd op regel 2
    chartDateLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, CHART_WIDTH);
    lv_obj_set_pos(chartDateLabel, 0, 0);
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    String deviceId = getDeviceIdFromTopic(ntfyTopic);
    lv_label_set_text(chartBeginLettersLabel, deviceId.c_str());
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, CHART_WIDTH);
    lv_obj_set_pos(chartTimeLabel, 0, 10);
    #else
    // CYD: Ruimere layout met datum/tijd op verschillende posities
    chartDateLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, 180);
    lv_obj_set_pos(chartDateLabel, 0, 4);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, 240);
    lv_obj_set_pos(chartTimeLabel, 0, 4);
    #endif
}

// Helper functie om price boxes te creëren
static void createPriceBoxes()
{
    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i)
    {
        priceBox[i] = lv_obj_create(lv_scr_act());
        lv_obj_set_size(priceBox[i], LV_PCT(100), LV_SIZE_CONTENT);

        if (i == 0) {
            lv_obj_align(priceBox[i], LV_ALIGN_TOP_LEFT, 0, PRICE_BOX_Y_START);
        }
        else {
            lv_obj_align_to(priceBox[i], priceBox[i - 1], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        }

        lv_obj_set_style_radius(priceBox[i], 6, 0);
        lv_obj_set_style_pad_all(priceBox[i], 4, 0);
        disableScroll(priceBox[i]);

        // Symbol caption
        priceTitle[i] = lv_label_create(priceBox[i]);
        if (i == 0) {
            lv_obj_set_style_text_font(priceTitle[i], FONT_SIZE_TITLE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceTitle[i], FONT_SIZE_TITLE_OTHER, 0);
        }
        lv_obj_set_style_text_color(priceTitle[i], lv_color_white(), 0);
        lv_label_set_text(priceTitle[i], symbols[i]);
        lv_obj_align(priceTitle[i], LV_ALIGN_TOP_LEFT, 0, 0);

        // Live price - platform-specifieke layout
        priceLbl[i] = lv_label_create(priceBox[i]);
        if (i == 0) {
            lv_obj_set_style_text_font(priceLbl[i], FONT_SIZE_PRICE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceLbl[i], FONT_SIZE_PRICE_OTHER, 0);
        }
        
        #ifdef PLATFORM_TTGO
        if (i == 0) {
            lv_obj_set_style_text_align(priceLbl[i], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (i == 0) - TTGO layout
        if (i == 0) {
            anchorMaxLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_ORANGE), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
        }
        #else
        if (i == 0) {
            lv_obj_set_style_text_align(priceLbl[i], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (i == 0) - CYD layout
        if (i == 0) {
            anchorLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_ORANGE), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMaxLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
        }
        #endif
        
        lv_label_set_text(priceLbl[i], "--");
        
        // Min/Max/Diff labels voor 1 min blok
        if (i == 1)
        {
            price1MinMaxLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price1MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price1MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMaxLabel, "--");
            lv_obj_align(price1MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price1MinDiffLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price1MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price1MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinDiffLabel, "--");
            lv_obj_align(price1MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price1MinMinLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price1MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price1MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMinLabel, "--");
            lv_obj_align(price1MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        
        // Min/Max/Diff labels voor 30 min blok
        if (i == 2)
        {
            price30MinMaxLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price30MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price30MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMaxLabel, "--");
            lv_obj_align(price30MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price30MinDiffLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price30MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price30MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinDiffLabel, "--");
            lv_obj_align(price30MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price30MinMinLabel = lv_label_create(priceBox[i]);
            lv_obj_set_style_text_font(price30MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price30MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMinLabel, "--");
            lv_obj_align(price30MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
    }
}

// Helper functie om footer te creëren
static void createFooter()
{
    #ifdef PLATFORM_TTGO
    // TTGO: IP-adres links, versienummer rechts
    ipLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ipLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ipLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text_fmt(chartVersionLabel, "%s", VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    
    if (WiFi.status() == WL_CONNECTED) {
        // Geoptimaliseerd: gebruik char array i.p.v. String
        static char ipBuffer[16];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        lv_label_set_text(ipLabel, ipBuffer);
    } else {
        lv_label_set_text(ipLabel, "--");
    }
    #else
    // CYD: Footer met 2 regels
    lblFooterLine1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lblFooterLine1, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(lblFooterLine1, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(lblFooterLine1, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lblFooterLine1, LV_ALIGN_BOTTOM_LEFT, 0, -18);
    lv_label_set_text(lblFooterLine1, "--dBm");
    
    ramLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ramLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ramLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ramLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(ramLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -18);
    lv_label_set_text(ramLabel, "--kB");
    
    lblFooterLine2 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(lblFooterLine2, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(lblFooterLine2, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(lblFooterLine2, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lblFooterLine2, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    lv_label_set_text(lblFooterLine2, "--.--.--.--");
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text_fmt(chartVersionLabel, "%s", VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    #endif
}

// Build the UI - Refactored to use helper functions
static void buildUI()
{
    lv_obj_clean(lv_scr_act());
    disableScroll(lv_scr_act());
    
    createChart();
    createHeaderLabels();
    createPriceBoxes();
    createFooter();
}

// ============================================================================
// LVGL Callback Functions
// ============================================================================

// LVGL calls this function to print log information
void my_print(lv_log_level_t level, const char *buf)
{
    LV_UNUSED(level);
    Serial_println(buf);
    Serial.flush();
}

// LVGL callback function to retrieve elapsed time
uint32_t millis_cb(void)
{
    return millis();
}

// LVGL calls this function when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    lv_disp_flush_ready(disp);
}

// Physical button check function (voor TTGO en CYD platforms)
#if HAS_PHYSICAL_BUTTON
// Button debouncing - edge detection voor betere eerste-druk detectie
static unsigned long lastButtonPress = 0;
static int lastButtonState = HIGH; // Start met HIGH (niet ingedrukt)
static const unsigned long BUTTON_DEBOUNCE_MS = 500; // 500ms debounce

void checkButton() {
    unsigned long now = millis();
    
    // Read button state (LOW = pressed, HIGH = not pressed due to INPUT_PULLUP)
    int buttonState = digitalRead(BUTTON_PIN);
    
    // Edge detection: detect HIGH -> LOW transition (button pressed)
    // Dit zorgt ervoor dat we alleen triggeren bij het indrukken, niet tijdens het ingedrukt houden
    if (buttonState == LOW && lastButtonState == HIGH && (now - lastButtonPress >= BUTTON_DEBOUNCE_MS)) {
        lastButtonPress = now;
        lastButtonState = buttonState; // Update state
        Serial_println("[Button] Physical reset button pressed - setting anchor price");
        
        // Execute reset and set anchor (thread-safe, same as MQTT callback)
        float currentPrice = 0.0f;
        
        // Als prices[0] nog 0 is, probeer eerst een prijs op te halen (alleen als WiFi verbonden is)
        if (WiFi.status() == WL_CONNECTED) {
            // Check of we al een prijs hebben, zo niet, haal er een op
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (prices[0] <= 0.0f) {
                    Serial_println("[Button] Prijs nog niet beschikbaar, haal prijs op...");
                    xSemaphoreGive(dataMutex);
                    // Haal prijs op (buiten mutex om deadlock te voorkomen)
                    fetchPrice();
                    // Wacht even zodat de prijs kan worden opgeslagen
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else {
                    xSemaphoreGive(dataMutex);
                }
            }
        }
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (prices[0] > 0.0f) {
                currentPrice = prices[0];  // Sla prijs lokaal op
                openPrices[0] = prices[0];
                // Set anchor price
                anchorPrice = prices[0];
                anchorMax = prices[0];  // Initialiseer max/min met huidige prijs
                anchorMin = prices[0];
                anchorTime = millis();
                anchorActive = true;
                // Reset take profit / max loss flags bij nieuwe anchor
                anchorTakeProfitSent = false;
                anchorMaxLossSent = false;
                Serial_printf("[Button] Reset: openPrices[0] = %.2f, Anchor = %.2f\n", openPrices[0], anchorPrice);
            } else {
                Serial_println("[Button] WARN: Prijs nog steeds niet beschikbaar na fetch");
            }
            xSemaphoreGive(dataMutex);
                
            // Publiceer anchor event naar MQTT en stuur notificatie
            if (currentPrice > 0.0f) {
                publishMqttAnchorEvent(anchorPrice, "anchor_set");
                
                // Stuur NTFY notificatie
                char timestamp[32];
                getFormattedTimestamp(timestamp, sizeof(timestamp));
                char title[64];
                char msg[128];
                snprintf(title, sizeof(title), "%s Anchor Set", binanceSymbol);
                snprintf(msg, sizeof(msg), "%s: %.2f EUR", timestamp, currentPrice);
                sendNotification(title, msg, "white_check_mark");
            }
            
            // Update UI (this will also take the mutex internally)
            updateUI();
        }
        
        // Publish to MQTT if connected (optional, for logging)
        if (mqttConnected) {
            String topic = String(MQTT_TOPIC_PREFIX) + "/button/reset";
            mqttClient.publish(topic.c_str(), "PRESSED", false);
        }
    }
    
    // Update lastButtonState voor volgende iteratie (ook als button wordt losgelaten)
    if (buttonState == HIGH) {
        lastButtonState = HIGH;
    }
}
#endif

// touchscreen_read functie verwijderd - niet meer nodig zonder touchscreen

void setup()
{
    // Load settings from Preferences
    loadSettings();
    Serial.begin(115200);
    DEV_DEVICE_INIT();
    
    
    // Initialiseer fysieke reset button (voor TTGO en CYD platforms)
    #if HAS_PHYSICAL_BUTTON
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif
    delay(2000); // For debugging, give time for the board to reconnect to com port

    Serial_println("Arduino_GFX LVGL_Arduino_v9 example ");
    String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial_println(LVGL_Arduino);

    // Init Display
    if (!gfx->begin())
    {
        Serial_println("gfx->begin() failed!");
        while (true)
        {
            /* no need to continue */
        }
    }
    gfx->setRotation(0);
    #ifdef PLATFORM_TTGO
    gfx->invertDisplay(false); // TTGO T-Display heeft geen inversie nodig (ST7789)
    #else
    gfx->invertDisplay(true); // Invert colors (as defined in Setup902_CYD28R_2USB.h with TFT_INVERSION_ON)
    #endif
    gfx->fillScreen(RGB565_BLACK);
    setDisplayBrigthness();
    
    // Geef display tijd om te stabiliseren na initialisatie (vooral belangrijk voor CYD displays)
    #ifndef PLATFORM_TTGO
    delay(100);
    #endif

    // init LVGL
    lv_init();

    // Set a tick source so that LVGL will know how much time elapsed
    lv_tick_set_cb(millis_cb);

    // register print function for debugging
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    uint32_t screenWidth = gfx->width();
    uint32_t screenHeight = gfx->height();
    // Buffer grootte - platform-specifiek
    #ifdef PLATFORM_TTGO
    // TTGO: 30 regels voor RAM besparing
    uint32_t bufSize = screenWidth * 30;
    #else
    // CYD: 40 regels (zoals in originele CYD code)
    uint32_t bufSize = screenWidth * 40;
    #endif

    lv_color_t *disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
        // remove MALLOC_CAP_INTERNAL flag try again
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }
    if (!disp_draw_buf)
    {
        Serial_println("LVGL disp_draw_buf allocate failed!");
        while (true)
        {
            /* no need to continue */
        }
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Touchscreen initialisatie verwijderd - gebruik nu fysieke boot knop (GPIO 0)
    
    // Watchdog configuratie - platform-specifiek
    #ifdef PLATFORM_CYD24
    // Schakel task watchdog UIT voor Core 0 (UI task met LVGL)
    // LVGL rendering gebruikt veel CPU tijd en kan de IDLE task blokkeren
    // Door de watchdog uit te schakelen voorkomen we crashes tijdens rendering
    // Dit is nodig voor de 2.4 inch display omdat lv_task_handler() langer duurt
    // Work-around om zwarte scherm te voorkomen
    esp_err_t wdt_err = esp_task_wdt_deinit();
    if (wdt_err != ESP_OK && wdt_err != ESP_ERR_NOT_FOUND) {
        Serial.printf("[WDT] Deinit error: %d\n", wdt_err);
    } else {
        Serial.println("[WDT] Watchdog UITGESCHAKELD voor Core 0 (LVGL rendering) - CYD 2.4 work-around");
    }
    #else
    // Configureer task watchdog timeout (10 seconden) voor andere platforms
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    #endif
    
    // WiFi event handlers voor reconnect controle
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf("[WiFi] Event: %d\n", event);
        switch(event) {
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                if (wifiInitialized) {
                    Serial.println("[WiFi] Verbinding verbroken");
                    wifiReconnectEnabled = true;
                    lastReconnectAttempt = 0;
                    reconnectAttemptCount = 0; // Reset reconnect counter
                }
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.println("[WiFi] Verbonden met AP");
                break;
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                // Geoptimaliseerd: gebruik char array i.p.v. String
                {
                    char ipBuffer[16];
                    IPAddress ip(info.got_ip.ip_info.ip.addr);
                    formatIPAddress(ip, ipBuffer, sizeof(ipBuffer));
                    Serial.printf("[WiFi] IP verkregen: %s\n", ipBuffer);
                    
                    // Publiceer IP-adres naar MQTT (als MQTT verbonden is)
                    if (mqttConnected) {
                        char topicBuffer[128];
                        snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", MQTT_TOPIC_PREFIX);
                        mqttClient.publish(topicBuffer, ipBuffer, false);
                    }
                }
                wifiReconnectEnabled = false;
                wifiInitialized = true;
                reconnectAttemptCount = 0; // Reset reconnect counter bij succesvolle verbinding
                // Start MQTT connectie na WiFi verbinding
                if (!mqttConnected) {
                    connectMQTT();
                }
                break;
            default:
                break;
        }
    });
    
    // Touchscreen LVGL input device verwijderd - gebruik nu fysieke boot knop (GPIO 0)

    // Maak mutex VOOR we het gebruiken (moet eerst aangemaakt worden)
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("[Error] Kon mutex niet aanmaken!");
    } else {
        Serial.println("[FreeRTOS] Mutex aangemaakt");
    }

    wifiConnectionAndFetchPrice();

    Serial_println("Setup done");
    fetchPrice();
    buildUI();
    
    // Force LVGL to render immediately after UI creation (CYD 2.4 work-around)
    #ifdef PLATFORM_CYD24
    if (disp != NULL) {
        lv_refr_now(disp);
    }
    #else
    // Voor andere platforms, roep timer handler aan om scherm te renderen
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        delay(10);
    }
    #endif

    // FreeRTOS Tasks voor multi-core processing
    // Core 1: API calls (elke seconde)
    xTaskCreatePinnedToCore(
        apiTask,           // Task function
        "API_Task",        // Task name
        8192,              // Stack size
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        1                  // Core 1
    );

    // Core 2: UI updates (elke seconde)
    xTaskCreatePinnedToCore(
        uiTask,            // Task function
        "UI_Task",         // Task name
        8192,              // Stack size
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        0                  // Core 0 (Arduino loop core)
    );

    // Core 2: Web server (elke 5 seconden, maar server.handleClient() continu)
    xTaskCreatePinnedToCore(
        webTask,           // Task function
        "Web_Task",        // Task name
        4096,              // Stack size
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        0                  // Core 0 (Arduino loop core)
    );

    Serial.println("[FreeRTOS] Tasks gestart op Core 1 (API) en Core 0 (UI/Web)");
}

// Toon verbindingsinfo (SSID en IP-adres) en "Opening Binance Session" op het scherm
// ============================================================================
// WiFi Helper Functions - Refactored from wifiConnectionAndFetchPrice()
// ============================================================================

// Helper functie om eerste prijs op te halen met retry logica
static void fetchInitialPrice()
{
    // Haal prijzen op - dit bepaalt hoe lang het scherm wordt getoond
    // fetchPrice() roept zelf lv_timer_handler() aan tijdens het ophalen
    fetchPrice();
    
    // Wacht tot de prijs succesvol is opgehaald (max 5 seconden)
    int retries = 0;
    while (retries < 50 && prices[0] <= 0.0f) {
        vTaskDelay(pdMS_TO_TICKS(100));
        lv_timer_handler();
        retries++;
    }
    if (prices[0] > 0.0f) {
        Serial_printf("[WiFi] Eerste prijs succesvol opgehaald: %.2f\n", prices[0]);
    } else {
        Serial.println("[WiFi] WARN: Eerste prijs niet opgehaald na 5 seconden");
    }
}

// Helper functie om WiFi verbinding op te zetten
static bool setupWiFiConnection()
{
    static lv_obj_t *wifiSpinner;
    static lv_obj_t *wifiLabel;
    static lv_obj_t *apSSIDLabel;
    static lv_obj_t *apPasswordLabel;
    static lv_obj_t *instructionLabel;
    static lv_obj_t *viaAPLabel;
    static lv_obj_t *webInterfaceLabel;
    
    // Schakel scroll uit voor hoofdscherm
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(lv_scr_act(), 0, LV_PART_SCROLLBAR);
    
    wifiSpinner = lv_spinner_create(lv_scr_act());
    lv_spinner_set_anim_params(wifiSpinner, 8000, 200);
    lv_obj_set_size(wifiSpinner, 80, 80);
    lv_obj_align(wifiSpinner, LV_ALIGN_CENTER, 0, 0);

    instructionLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(instructionLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(instructionLabel, lv_color_hex3(0x0cf), 0);
    lv_obj_set_width(instructionLabel, 140);
    lv_label_set_long_mode(instructionLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(instructionLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(instructionLabel, getText("Verbinden met WiFi:", "Connecting to WiFi:"));
    lv_obj_align_to(instructionLabel, wifiSpinner, LV_ALIGN_OUT_TOP_MID, 0, -10);

    wifiLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(wifiLabel, lv_color_hex3(0x0cf), 0);
    lv_obj_set_width(wifiLabel, 140);
    lv_label_set_long_mode(wifiLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(wifiLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(wifiLabel, "Verbinden...");
    lv_obj_align_to(wifiLabel, wifiSpinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Initialize WiFiManager
    WiFiManager wm;
    wm.setConfigPortalTimeout(0);
    wm.setEnableConfigPortal(false);
    wm.setWiFiAutoReconnect(false);
    
    String apSSID = wm.getConfigPortalSSID();
    String apPassword = "";
    
    lv_label_set_text(wifiLabel, getText("Zoeken naar WiFi...", "Searching for WiFi..."));
    lv_timer_handler();
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.persistent(true);
    bool connected = false;
    
    // Probeer verbinding met opgeslagen credentials
    if (wm.getWiFiIsSaved()) {
        unsigned long connectStart = millis();
        unsigned long connectTimeout = 15000;
        
        lv_label_set_text(wifiLabel, getText("Verbinden...", "Connecting..."));
        lv_timer_handler();
        
        WiFi.disconnect(false);
        delay(500);
        
        WiFi.begin();
        
        while (WiFi.status() != WL_CONNECTED && (millis() - connectStart) < connectTimeout) {
            lv_timer_handler();
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
            tzset();
            connected = true;
            wifiReconnectEnabled = false;
            wifiInitialized = true;
            Serial.println("[WiFi] Succesvol verbonden");
        } else {
            Serial.println("[WiFi] Verbinding timeout");
        }
    }
    
    if (!connected) {
        // Start config portal en toon AP credentials op scherm
        lv_obj_del(wifiSpinner);
        lv_obj_del(wifiLabel);
        lv_obj_del(instructionLabel);
        
        instructionLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(instructionLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(instructionLabel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_width(instructionLabel, 200);
        lv_label_set_long_mode(instructionLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(instructionLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(instructionLabel, getText("Stel de WiFi in", "Configure WiFi"));
        lv_obj_align(instructionLabel, LV_ALIGN_TOP_MID, 0, 10);
        
        wifiLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(wifiLabel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_width(wifiLabel, 200);
        lv_label_set_long_mode(wifiLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(wifiLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(wifiLabel, getText("Maak contact", "Connect"));
        lv_obj_align_to(wifiLabel, instructionLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        
        viaAPLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(viaAPLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(viaAPLabel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_width(viaAPLabel, 200);
        lv_label_set_long_mode(viaAPLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(viaAPLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(viaAPLabel, getText("via AP:", "via AP:"));
        lv_obj_align_to(viaAPLabel, wifiLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        
        apSSIDLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(apSSIDLabel, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(apSSIDLabel, lv_color_white(), 0);
        lv_obj_set_width(apSSIDLabel, 200);
        lv_label_set_long_mode(apSSIDLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(apSSIDLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(apSSIDLabel, apSSID.c_str());
        lv_obj_align_to(apSSIDLabel, viaAPLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
        
        lv_timer_handler();
        
        Serial_printf("AP Mode gestart!\n");
        Serial_printf("SSID: %s\n", apSSID.c_str());
        if (apPassword.length() > 0) {
            Serial_printf("Wachtwoord: %s\n", apPassword.c_str());
        } else {
            Serial_printf("Wachtwoord: (Geen)\n");
        }
        
        wm.setConfigPortalTimeout(0);
        
        WiFi.mode(WIFI_AP);
        if (apPassword.length() > 0) {
            WiFi.softAP(apSSID.c_str(), apPassword.c_str());
        } else {
            WiFi.softAP(apSSID.c_str());
        }
        
        delay(500);
        
        // Geoptimaliseerd: gebruik char array i.p.v. String
        char apIP[16];
        formatIPAddress(WiFi.softAPIP(), apIP, sizeof(apIP));
        
        webInterfaceLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(webInterfaceLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(webInterfaceLabel, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_width(webInterfaceLabel, 200);
        lv_label_set_long_mode(webInterfaceLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(webInterfaceLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(webInterfaceLabel, getText("Webinterface:", "Web Interface:"));
        lv_obj_align_to(webInterfaceLabel, apSSIDLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
        
        apPasswordLabel = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(apPasswordLabel, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(apPasswordLabel, lv_color_white(), 0);
        lv_obj_set_width(apPasswordLabel, 200);
        lv_label_set_long_mode(apPasswordLabel, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(apPasswordLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(apPasswordLabel, apIP);
        lv_obj_align_to(apPasswordLabel, webInterfaceLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
        
        lv_timer_handler();
        
        Serial_printf("AP IP: %s\n", apIP.c_str());
        
        wm.setEnableConfigPortal(true);
        
        if (apPassword.length() > 0) {
            wm.startConfigPortal(apSSID.c_str(), apPassword.c_str());
        } else {
            wm.startConfigPortal(apSSID.c_str(), NULL);
        }
        
        wm.setEnableConfigPortal(false);
        
        lv_obj_del(instructionLabel);
        lv_obj_del(apSSIDLabel);
        lv_obj_del(apPasswordLabel);
        lv_obj_del(wifiLabel);
        lv_obj_del(viaAPLabel);
        lv_obj_del(webInterfaceLabel);
        
        connected = true;
    }
    
    return connected;
}

void showConnectionInfo()
{
    // Verwijder alle bestaande labels op het scherm
    lv_obj_clean(lv_scr_act());
    
    // Schakel scroll uit voor hoofdscherm om scroll indicators te voorkomen
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_MOMENTUM);
    // Verberg scroll indicators volledig
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(lv_scr_act(), 0, LV_PART_SCROLLBAR);
    
    // Maak spinner voor "Opening Binance Session" (8px naar beneden vanaf midden)
    lv_obj_t *spinner = lv_spinner_create(lv_scr_act());
    lv_spinner_set_anim_params(spinner, 8000, 200);
    lv_obj_set_size(spinner, 80, 80);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 8); // 8px naar beneden vanaf midden
    // Maak spinner groen (goede verbinding) - indicator is het bewegende deel
    lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_palette_darken(LV_PALETTE_GREEN, 3), LV_PART_MAIN);
    
    // SSID label (boven de spinner)
    lv_obj_t *ssidTitleLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ssidTitleLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ssidTitleLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(ssidTitleLabel, 150);
    lv_label_set_long_mode(ssidTitleLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ssidTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ssidTitleLabel, "SSID:");
    lv_obj_align_to(ssidTitleLabel, spinner, LV_ALIGN_OUT_TOP_MID, 0, -70); // Meer ruimte boven spinner voor IP-adres
    
    lv_obj_t *ssidLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ssidLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ssidLabel, lv_palette_lighten(LV_PALETTE_GREEN, 2), 0);
    lv_obj_set_width(ssidLabel, 150);
    lv_label_set_long_mode(ssidLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ssidLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ssidLabel, WiFi.SSID().c_str());
    lv_obj_align_to(ssidLabel, ssidTitleLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    // IP-adres label
    lv_obj_t *ipTitleLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ipTitleLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ipTitleLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(ipTitleLabel, 150);
    lv_label_set_long_mode(ipTitleLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ipTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ipTitleLabel, "IP-adres:");
    lv_obj_align_to(ipTitleLabel, ssidLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    lv_obj_t *ipLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ipLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_lighten(LV_PALETTE_GREEN, 2), 0);
    lv_obj_set_width(ipLabel, 150);
    lv_label_set_long_mode(ipLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_CENTER, 0);
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char ipBuffer[16];
    formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
    lv_label_set_text(ipLabel, ipBuffer);
    lv_obj_align_to(ipLabel, ipTitleLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    
    // "Opening Binance Session" label (onder de spinner)
    lv_obj_t *binanceLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(binanceLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(binanceLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(binanceLabel, 150);
    lv_label_set_long_mode(binanceLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(binanceLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(binanceLabel, "Opening Binance\nSession");
    lv_obj_align_to(binanceLabel, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 10); // Onder spinner
    
    // Update het scherm
    lv_timer_handler();
}

// Wi-Fi connection & first prices fetched with splash screen - Refactored
void wifiConnectionAndFetchPrice()
{
    // Setup WiFi verbinding
    bool connected = setupWiFiConnection();
    
    if (connected) {
        // Toon verbindingsinfo (SSID en IP) en "Opening Binance Session"
        showConnectionInfo();
        
        // Haal eerste prijs op
        fetchInitialPrice();
    }

    // Geoptimaliseerd: gebruik char array i.p.v. String
    char ipBuffer[16];
    formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
    Serial_printf("Verbonden! IP: %s\n", ipBuffer);
    
    wifiInitialized = true;
    wifiReconnectEnabled = false;
    
    // Start web server voor instellingen
    setupWebServer();
    
    // Scherm wordt leeggemaakt door buildUI() in setup()
}
// ============================================================================
// FreeRTOS Tasks
// ============================================================================

// FreeRTOS Task: API calls op Core 1 (elke 1.3 seconde)
void apiTask(void *parameter)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(UPDATE_API_INTERVAL);
    static unsigned long lastCallTime = 0;
    static uint32_t callCount = 0;
    static uint32_t missedCallCount = 0;
    
    Serial.println("[API Task] Gestart op Core 1");
    
    // Wacht tot mutex is aangemaakt
    while (dataMutex == NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Wacht tot WiFi verbonden is voordat we beginnen
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API Task] Wachten op WiFi verbinding...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wacht 1 seconde
    }
    
    Serial.println("[API Task] WiFi verbonden, start API calls");
    lastCallTime = millis();
    
    for (;;)
    {
        unsigned long callStart = millis();
        callCount++;
        
        // Detecteer gemiste calls (als er meer dan 1.3x interval tussen calls zit)
        if (lastCallTime > 0) {
            unsigned long timeSinceLastCall = callStart - lastCallTime;
            if (timeSinceLastCall > (UPDATE_API_INTERVAL * 130) / 100) {
                missedCallCount++;
                Serial.printf("[API Task] WARN: Gemiste call! Gap: %lu ms (interval: %d ms)\n", 
                             timeSinceLastCall, UPDATE_API_INTERVAL);
            }
        }
        lastCallTime = callStart;
        
        // Controleer WiFi status voordat we een request doet
        if (WiFi.status() == WL_CONNECTED) {
            fetchPrice();
            
            // Waarschuwing alleen bij echte problemen (call duurt langer dan 1.1x interval)
            unsigned long callDuration = millis() - callStart;
            if (callDuration > (UPDATE_API_INTERVAL * 110) / 100) {
                Serial.printf("[API Task] WARN: Call #%lu duurde %lu ms (langer dan 110%% van interval %d ms)\n", 
                             callCount, callDuration, UPDATE_API_INTERVAL);
            }
            
            // Log statistieken alleen bij problemen of elke 5 minuten
            if (missedCallCount > 0 && callCount % 60 == 0) {
                Serial.printf("[API Task] Stats: %lu calls, %lu gemist\n", callCount, missedCallCount);
            }
        } else {
            Serial.println("[API Task] WiFi verbinding verloren, wachten op reconnect...");
            // Wacht tot WiFi weer verbonden is
            while (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wacht 1 seconde
            }
            Serial.println("[API Task] WiFi weer verbonden");
        }
        
        // Gebruik vTaskDelayUntil om precieze timing te garanderen
        // Dit zorgt ervoor dat de volgende call precies na het interval start
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

// FreeRTOS Task: UI updates op Core 0 (elke seconde)
void uiTask(void *parameter)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(UPDATE_UI_INTERVAL);
    // LVGL handler frequentie - CYD heeft meer rendering overhead, dus iets vaker aanroepen
    #ifdef PLATFORM_TTGO
    const TickType_t lvglFrequency = pdMS_TO_TICKS(5); // TTGO: elke 5ms
    #else
    const TickType_t lvglFrequency = pdMS_TO_TICKS(3); // CYD: elke 3ms voor vloeiendere rendering
    #endif
    TickType_t lastLvglTime = xTaskGetTickCount();
    
    Serial.println("[UI Task] Gestart op Core 0");
    
    // Wacht tot mutex is aangemaakt
    while (dataMutex == NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    for (;;)
    {
        // Meet CPU usage: start tijd
        unsigned long taskStartTime = millis();
        
        // Roep LVGL task handler regelmatig aan (elke 5ms) om IDLE task tijd te geven
        // CYD 2.4 work-around: gebruik lv_refr_now() in plaats van lv_task_handler()
        // lv_task_handler() crasht op deze display, dus gebruiken we directe rendering
        TickType_t currentTime = xTaskGetTickCount();
        if ((currentTime - lastLvglTime) >= lvglFrequency) {
            #ifdef PLATFORM_CYD24
            if (disp != NULL) {
                lv_refr_now(disp);
            }
            #else
            lv_task_handler();
            #endif
            lastLvglTime = currentTime;
        }
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        // Neem mutex voor data lezen (timeout verhoogd voor CYD om haperingen te voorkomen)
        // CYD heeft grotere buffer en meer rendering overhead, dus iets langere timeout
        #ifdef PLATFORM_TTGO
        const TickType_t mutexTimeout = pdMS_TO_TICKS(50); // TTGO: korte timeout
        #else
        const TickType_t mutexTimeout = pdMS_TO_TICKS(100); // CYD: langere timeout voor betere grafiek updates
        #endif
        
        static uint32_t uiMutexTimeoutCount = 0;
        if (xSemaphoreTake(dataMutex, mutexTimeout) == pdTRUE)
        {
            // Reset timeout counter bij succes
            if (uiMutexTimeoutCount > 0) {
                uiMutexTimeoutCount = 0;
            }
            
            updateUI();
            xSemaphoreGive(dataMutex);
        }
        else
        {
            // Geoptimaliseerd: log alleen bij meerdere opeenvolgende timeouts
            uiMutexTimeoutCount++;
            if (uiMutexTimeoutCount == 1 || uiMutexTimeoutCount % 20 == 0) {
                Serial_printf("[UI Task] WARN: mutex timeout (count: %lu)\n", uiMutexTimeoutCount);
            }
            // Reset counter na lange tijd om te voorkomen dat deze blijft groeien
            if (uiMutexTimeoutCount > 100) {
                uiMutexTimeoutCount = 0;
            }
        }
        
        // Meet CPU usage: bereken tijd die deze task gebruikt
        unsigned long taskTime = millis() - taskStartTime;
        loopTimeSum += taskTime;
        loopCount++;
        
        // Bereken gemiddelde CPU usage elke N samples
        if (loopCount >= CPU_MEASUREMENT_SAMPLES) {
            float avgLoopTime = (float)loopTimeSum / (float)loopCount;
            cpuUsagePercent = (avgLoopTime / (float)LOOP_PERIOD_MS) * 100.0f;
            // Beperk tot 0-100%
            if (cpuUsagePercent > 100.0f) cpuUsagePercent = 100.0f;
            if (cpuUsagePercent < 0.0f) cpuUsagePercent = 0.0f;
            loopTimeSum = 0;
            loopCount = 0;
        }
        
        // Check physical button (alleen voor TTGO)
        #if HAS_PHYSICAL_BUTTON
        checkButton();
        #endif
        
        // Yield aan andere tasks om IDLE task tijd te geven
        vTaskDelay(1); // Geef 1 tick (10ms) aan andere tasks
        
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

// FreeRTOS Task: Web server op Core 0 (server.handleClient() continu, data update elke 5 seconden)
void webTask(void *parameter)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(100); // Server handle elke 100ms voor responsiviteit
    
    Serial.println("[Web Task] Gestart op Core 0");
    
    // Wacht tot WiFi verbonden is voordat we beginnen
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Web Task] Wachten op WiFi verbinding...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wacht 1 seconde
    }
    
    Serial.println("[Web Task] WiFi verbonden, start web server");
    
    for (;;)
    {
        // Handle web server requests alleen als WiFi verbonden is
        if (WiFi.status() == WL_CONNECTED) {
            server.handleClient();
        } else {
            // WiFi verbinding verloren, wacht op reconnect
            Serial.println("[Web Task] WiFi verbinding verloren, wachten op reconnect...");
            while (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wacht 1 seconde
            }
            Serial.println("[Web Task] WiFi weer verbonden");
        }
        
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

void loop()
{
    // Geef tijd aan andere tasks
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // MQTT loop (moet regelmatig worden aangeroepen)
    if (mqttConnected) {
        if (!mqttClient.loop()) {
            // Verbinding verloren, probeer reconnect
            mqttConnected = false;
            lastMqttReconnectAttempt = 0;
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        // Probeer MQTT reconnect als WiFi verbonden is
        unsigned long now = millis();
        if (lastMqttReconnectAttempt == 0 || (now - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL)) {
            lastMqttReconnectAttempt = now;
            connectMQTT();
        }
    }
    
    // Beheer WiFi reconnect indien nodig
    // Geoptimaliseerd: betere reconnect logica met retry counter en non-blocking timeout
    if (wifiInitialized && wifiReconnectEnabled && WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        
        // Check of we moeten reconnecten (interval verstreken of eerste poging)
        bool shouldReconnect = (lastReconnectAttempt == 0 || (now - lastReconnectAttempt >= RECONNECT_INTERVAL));
        
        // Als we te veel pogingen hebben gedaan, wacht langer tussen pogingen
        if (reconnectAttemptCount >= MAX_RECONNECT_ATTEMPTS) {
            // Verhoog interval na meerdere mislukte pogingen (exponentiële backoff)
            unsigned long extendedInterval = RECONNECT_INTERVAL * (1 + (reconnectAttemptCount - MAX_RECONNECT_ATTEMPTS));
            shouldReconnect = (now - lastReconnectAttempt >= extendedInterval);
        }
        
        if (shouldReconnect) {
            reconnectAttemptCount++;
            Serial.printf("[WiFi] Probeer reconnect (poging %u/%u)...\n", reconnectAttemptCount, MAX_RECONNECT_ATTEMPTS);
            
            // Non-blocking disconnect en reconnect
            WiFi.disconnect(false);
            delay(500); // Kortere delay voor snellere reconnect
            WiFi.begin();
            lastReconnectAttempt = now;
            
            // Non-blocking reconnect check (max 10 seconden)
            unsigned long reconnectStart = millis();
            bool reconnected = false;
            while (WiFi.status() != WL_CONNECTED && (millis() - reconnectStart) < 10000) {
                delay(100);
                lv_timer_handler(); // Geef LVGL tijd om te renderen tijdens reconnect
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Reconnect succesvol!");
                wifiReconnectEnabled = false;
                reconnectAttemptCount = 0; // Reset counter bij succes
                // Probeer MQTT reconnect na WiFi reconnect
                if (!mqttConnected) {
                    connectMQTT();
                }
            } else {
                Serial.printf("[WiFi] Reconnect timeout (poging %u)\n", reconnectAttemptCount);
                // Als we te veel pogingen hebben gedaan, log een waarschuwing
                if (reconnectAttemptCount >= MAX_RECONNECT_ATTEMPTS) {
                    Serial.printf("[WiFi] WARN: %u reconnect pogingen mislukt, wacht langer tussen pogingen\n", reconnectAttemptCount);
                }
            }
        }
    }
}

// Set the brightness of the display to GFX_BRIGHTNESS
void setDisplayBrigthness()
{
    ledcAttachChannel(GFX_BL, 1000, 8, 1);
    ledcWrite(GFX_BL, SCREEN_BRIGHTNESS);
}