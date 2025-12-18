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
#include <esp_heap_caps.h>

// SettingsStore module
#include "src/SettingsStore/SettingsStore.h"

// PriceData module (Fase 4.2.1: voor DataSource enum)
#include "src/PriceData/PriceData.h"

// ArduinoJson support (optioneel - als library niet beschikbaar is, gebruik handmatige parsing)
// Probeer ArduinoJson te includen - als het niet beschikbaar is, gebruik handmatige parsing
#define USE_ARDUINOJSON 0  // Standaard uit, wordt gezet naar 1 als ArduinoJson beschikbaar is
#ifdef __has_include
    #if __has_include(<ArduinoJson.h>)
        #include <ArduinoJson.h>
        #undef USE_ARDUINOJSON
        #define USE_ARDUINOJSON 1
    #endif
#else
    // Fallback voor compilers zonder __has_include
    #ifdef ARDUINOJSON_VERSION_MAJOR
        #include <ArduinoJson.h>
        #undef USE_ARDUINOJSON
        #define USE_ARDUINOJSON 1
    #endif
#endif

// ============================================================================
// Constants and Configuration
// ============================================================================

// --- Version and Build Configuration ---
#define VERSION_MAJOR 3
#define VERSION_MINOR 88
#define VERSION_STRING "3.88"

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
#define HTTP_TIMEOUT_MS 1200  // HTTP timeout (1200ms = 80% van API interval 1500ms, voorkomt timing opstapeling)
#define HTTP_CONNECT_TIMEOUT_MS 1000  // Connect timeout (1000ms voor snellere failure bij connect problemen)

// --- Chart Configuration ---
#define PRICE_RANGE 200         // The range of price for the chart, adjust as needed
#define POINTS_TO_CHART 60      // Number of points on the chart (60 points = ~1.5 minutes at 1500ms API interval)

// --- Timing Configuration ---
#define UPDATE_UI_INTERVAL 1000   // UI update in ms (elke seconde)
#define UPDATE_API_INTERVAL 1500   // API update in ms (verhoogd naar 1500ms voor betere stabiliteit bij langzame netwerken)
#define UPDATE_WEB_INTERVAL 5000  // Web interface update in ms (elke 5 seconden)
#define RECONNECT_INTERVAL 60000  // WiFi reconnect interval (60 seconden tussen reconnect pogingen)
#define MQTT_RECONNECT_INTERVAL 5000  // MQTT reconnect interval (5 seconden)

// --- Delay Constants (Magic Numbers Elimination) ---
#define DELAY_WIFI_CONNECT_LOOP_MS 100    // Delay in WiFi connect loops
#define DELAY_LVGL_RENDER_MS 10           // Delay for LVGL rendering loops
#define DELAY_RECONNECT_MS 500           // Delay for reconnection attempts
#define DELAY_DISPLAY_UPDATE_MS 50       // Delay for display updates
#define DELAY_DEBUG_RECONNECT_MS 2000     // Delay for debugging reconnection

// --- Anchor Price Configuration ---
#define ANCHOR_TAKE_PROFIT_DEFAULT 5.0f    // Take profit: +5% boven anchor price
#define ANCHOR_MAX_LOSS_DEFAULT -3.0f      // Max loss: -3% onder anchor price

// --- Trend-Adaptive Anchor Configuration ---
#define TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT false  // Default: uitgeschakeld
#define UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT 1.15f      // UP: maxLoss * 1.15
#define UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT 1.2f    // UP: takeProfit * 1.2
#define DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT 0.85f    // DOWN: maxLoss * 0.85
#define DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT 0.8f  // DOWN: takeProfit * 0.8

// --- Trend Detection Configuration ---
#define TREND_THRESHOLD_DEFAULT 1.30f      // Trend threshold: ±1.30% voor 2h trend
#define TREND_CHANGE_COOLDOWN_MS 600000UL  // 10 minuten cooldown voor trend change notificaties

// --- Smart Confluence Mode Configuration ---
#define SMART_CONFLUENCE_ENABLED_DEFAULT false  // Default: uitgeschakeld
#define CONFLUENCE_TIME_WINDOW_MS 300000UL     // 5 minuten tijdshorizon voor confluence (1m en 5m events moeten binnen ±5 minuten liggen)

// --- Warm-Start Configuration ---
#define WARM_START_ENABLED_DEFAULT true  // Default: warm-start met Binance historische data aan
#define WARM_START_1M_EXTRA_CANDLES_DEFAULT 15  // Extra 1m candles bovenop volatility window
#define WARM_START_5M_CANDLES_DEFAULT 12  // Aantal 5m candles (default: 12 = 1 uur)
#define WARM_START_30M_CANDLES_DEFAULT 8  // Aantal 30m candles (default: 8 = 4 uur)
#define WARM_START_2H_CANDLES_DEFAULT 6  // Aantal 2h candles (default: 6 = 12 uur)
#define BINANCE_KLINES_API "https://api.binance.com/api/v3/klines"  // Binance klines endpoint
#define WARM_START_TIMEOUT_MS 10000  // Timeout voor warm-start API calls (10 seconden)

// --- Auto-Volatility Mode Configuration ---
#define AUTO_VOLATILITY_ENABLED_DEFAULT false      // Default: uitgeschakeld
#define AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT 60  // Sliding window lengte in minuten
#define AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT 0.15f  // Baseline standaarddeviatie van 1m returns in procent
#define AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT 0.7f  // Minimum multiplier voor volFactor
#define AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT 1.6f  // Maximum multiplier voor volFactor
#define MAX_VOLATILITY_WINDOW_SIZE 120  // Maximum window size (voor array grootte)

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
#define SPIKE_1M_THRESHOLD_DEFAULT 0.31f   // 1m spike: |ret_1m| >= 0.31%
#define SPIKE_5M_THRESHOLD_DEFAULT 0.65f   // 5m spike filter: |ret_5m| >= 0.65% (past bij actuele volatiliteit)
#define MOVE_30M_THRESHOLD_DEFAULT 1.3f    // 30m move: |ret_30m| >= 1.3% (0.8% was te gevoelig)
#define MOVE_5M_THRESHOLD_DEFAULT 0.40f    // 5m move filter: |ret_5m| >= 0.40% (gevoeliger op momentum-opbouw)
#define MOVE_5M_ALERT_THRESHOLD_DEFAULT 0.8f  // 5m move alert: |ret_5m| >= 0.8% (historisch vaak bij trend start)

// Cooldown tijden (in milliseconden) om spam te voorkomen (geoptimaliseerd op basis van metingen)
#define NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT 120000   // 2 minuten tussen 1-minuut spike notificaties
#define NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT 900000  // 15 minuten tussen 30-minuten move notificaties (grote moves → langere rust)
#define NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT 420000   // 7 minuten tussen 5-minuten move notificaties (sneller tweede signaal bij doorbraak)

// Max alerts per uur
#define MAX_1M_ALERTS_PER_HOUR 3
#define MAX_30M_ALERTS_PER_HOUR 2
#define MAX_5M_ALERTS_PER_HOUR 3

// --- MQTT Configuration ---
#define MQTT_HOST_DEFAULT "192.168.68.3"  // Standaard MQTT broker IP (pas aan naar jouw MQTT broker)
#define MQTT_PORT_DEFAULT 1883             // Standaard MQTT poort
#define MQTT_USER_DEFAULT "mosquitto"       // Standaard MQTT gebruiker (pas aan)
#define MQTT_PASS_DEFAULT "mqtt_password"  // Standaard MQTT wachtwoord (pas aan)

// --- Language Configuration ---
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // Standaard: Nederlands (0 = Nederlands, 1 = English)
#endif

// --- Array Size Configuration ---
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_5MINUTES 300
#define MINUTES_FOR_30MIN_CALC 120

// --- Return Calculation Configuration ---
// Aantal waarden nodig voor return berekeningen gebaseerd op UPDATE_API_INTERVAL (1500ms)
// 1 minuut = 60000ms / 1500ms = 40 waarden
// 5 minuten = 300000ms / 1500ms = 200 waarden
#define VALUES_FOR_1MIN_RETURN ((60000UL) / (UPDATE_API_INTERVAL))
#define VALUES_FOR_5MIN_RETURN ((300000UL) / (UPDATE_API_INTERVAL))

// --- CPU Measurement Configuration ---
#define CPU_MEASUREMENT_SAMPLES 20  // Meet over 20 loops voor gemiddelde


// ============================================================================
// Global Variables
// ============================================================================

// Touchscreen functionaliteit volledig verwijderd - CYD's gebruiken nu fysieke boot knop (GPIO 0)

// LVGL Display global variables
lv_display_t *disp;
static lv_color_t *disp_draw_buf = nullptr;  // Draw buffer pointer (één keer gealloceerd bij init)
static size_t disp_draw_buf_size = 0;  // Buffer grootte in bytes (voor logging)

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
static bool anchorNotificationPending = false;  // Flag voor pending anchor set notificatie
static float anchorTakeProfit = ANCHOR_TAKE_PROFIT_DEFAULT;  // Take profit threshold (%)
static float anchorMaxLoss = ANCHOR_MAX_LOSS_DEFAULT;        // Max loss threshold (%)
static bool anchorTakeProfitSent = false;  // Flag om te voorkomen dat take profit meerdere keren wordt verzonden
static bool anchorMaxLossSent = false;    // Flag om te voorkomen dat max loss meerdere keren wordt verzonden

// Trend-adaptive anchor settings
static bool trendAdaptiveAnchorsEnabled = TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT;
static float uptrendMaxLossMultiplier = UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
static float uptrendTakeProfitMultiplier = UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;
static float downtrendMaxLossMultiplier = DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
static float downtrendTakeProfitMultiplier = DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;

// Warm-Start: Data source tracking
// Warm-Start: System status
enum WarmStartStatus {
    WARMING_UP,  // Buffers bevatten nog Binance data
    LIVE,        // Volledig op live data
    LIVE_COLD    // Live data maar warm-start gefaald (cold start)
};

// Warm-Start: Mode
enum WarmStartMode {
    WS_MODE_FULL,     // Alle timeframes succesvol geladen
    WS_MODE_PARTIAL,  // Gedeeltelijk geladen (sommige timeframes gefaald)
    WS_MODE_FAILED,   // Alle timeframes gefaald
    WS_MODE_DISABLED  // Warm-start uitgeschakeld
};

// Warm-Start: Statistics struct
struct WarmStartStats {
    uint16_t loaded1m;      // Aantal 1m candles geladen
    uint16_t loaded5m;      // Aantal 5m candles geladen
    uint16_t loaded30m;     // Aantal 30m candles geladen
    uint16_t loaded2h;      // Aantal 2h candles geladen
    bool warmStartOk1m;     // 1m warm-start succesvol
    bool warmStartOk5m;     // 5m warm-start succesvol
    bool warmStartOk30m;    // 30m warm-start succesvol
    bool warmStartOk2h;     // 2h warm-start succesvol
    WarmStartMode mode;     // Warm-start mode
    uint8_t warmUpProgress; // Warm-up progress percentage (0-100)
};

// Trend detection
enum TrendState {
    TREND_UP,
    TREND_DOWN,
    TREND_SIDEWAYS
};

// Dynamische anchor configuratie op basis van trend
struct AnchorConfigEffective {
    float maxLossPct;      // Effectieve max loss percentage (negatief)
    float takeProfitPct;   // Effectieve take profit percentage (positief)
};

// Smart Confluence Mode: State structs voor recente events
enum EventDirection {
    EVENT_UP,
    EVENT_DOWN,
    EVENT_NONE
};

struct LastOneMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_1m|
    bool usedInConfluence;  // Flag om te voorkomen dat dit event dubbel wordt gebruikt
};

struct LastFiveMinuteEvent {
    EventDirection direction;
    unsigned long timestamp;
    float magnitude;  // |ret_5m|
    bool usedInConfluence;  // Flag om te voorkomen dat dit event dubbel wordt gebruikt
};

// Auto-Volatility Mode: Struct voor effective thresholds
struct EffectiveThresholds {
    float spike1m;
    float move5m;
    float move30m;
    float volFactor;
    float stdDev;
};

static float ret_2h = 0.0f;  // 2-hour return percentage
static float ret_30m = 0.0f;  // 30-minute return percentage (calculated from minuteAverages or warm-start data)
static bool hasRet2hWarm = false;  // Flag: ret_2h beschikbaar vanuit warm-start (minimaal 2 candles)
static bool hasRet30mWarm = false;  // Flag: ret_30m beschikbaar vanuit warm-start (minimaal 2 candles)
static bool hasRet2hLive = false;  // Flag: ret_2h kan worden berekend uit live data (minuteIndex >= 120)
static bool hasRet30mLive = false;  // Flag: ret_30m kan worden berekend uit live data (minuteIndex >= 30)
// Combined flags: beschikbaar vanuit warm-start OF live data
static bool hasRet2h = false;  // hasRet2hWarm || hasRet2hLive
static bool hasRet30m = false;  // hasRet30mWarm || hasRet30mLive
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

// Smart Confluence Mode state
static bool smartConfluenceEnabled = SMART_CONFLUENCE_ENABLED_DEFAULT;
static LastOneMinuteEvent last1mEvent = {EVENT_NONE, 0, 0.0f, false};
static LastFiveMinuteEvent last5mEvent = {EVENT_NONE, 0, 0.0f, false};
static unsigned long lastConfluenceAlert = 0;  // Timestamp van laatste confluence alert (cooldown)

// Auto-Volatility Mode state
static bool autoVolatilityEnabled = AUTO_VOLATILITY_ENABLED_DEFAULT;
static uint8_t autoVolatilityWindowMinutes = AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT;
static float autoVolatilityBaseline1mStdPct = AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT;
static float autoVolatilityMinMultiplier = AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT;
static float autoVolatilityMaxMultiplier = AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT;
static float volatility1mReturns[MAX_VOLATILITY_WINDOW_SIZE];  // Sliding window voor 1m returns
static uint8_t volatility1mIndex = 0;  // Index voor circulaire buffer
static bool volatility1mArrayFilled = false;  // Flag om aan te geven of array gevuld is
static float currentVolFactor = 1.0f;  // Huidige volatility factor
static unsigned long lastVolatilityLog = 0;  // Timestamp van laatste volatility log (voor debug)
#define VOLATILITY_LOG_INTERVAL_MS 300000UL  // Log elke 5 minuten

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
static lv_obj_t *anchorLabel; // Label voor anchor price info (rechts midden, met percentage verschil)
static lv_obj_t *anchorMaxLabel; // Label voor "Pak winst" (rechts, groen, boven)
static lv_obj_t *anchorMinLabel; // Label voor "Stop loss" (rechts, rood, onder)
static lv_obj_t *anchorDeltaLabel; // Label voor anchor delta % (TTGO, rechts)
static lv_obj_t *trendLabel; // Label voor trend weergave
static lv_obj_t *warmStartStatusLabel; // Label voor warm-start status weergave (rechts bovenin chart)
static lv_obj_t *volatilityLabel; // Label voor volatiliteit weergave

static uint32_t lastApiMs = 0; // Time of last api call

// CPU usage measurement (alleen voor web interface)
static float cpuUsagePercent = 0.0f;
static unsigned long loopTimeSum = 0;
static uint16_t loopCount = 0;
static const unsigned long LOOP_PERIOD_MS = UPDATE_UI_INTERVAL; // 1000ms

// Heap Telemetry: Low watermark tracking
static uint32_t heapLowWatermark = UINT32_MAX;  // Minimum free heap sinds boot
static unsigned long lastHeapTelemetryLog = 0;   // Timestamp van laatste heap telemetry log
static const unsigned long HEAP_TELEMETRY_INTERVAL_MS = 60000UL; // Elke 60 seconden

// Static buffers voor hot paths (voorkomt String allocaties)
static char httpResponseBuffer[512];  // Buffer voor HTTP responses (httpGET, sendNtfyNotification)
static char notificationMsgBuffer[512];  // Buffer voor notification messages
static char notificationTitleBuffer[128];  // Buffer voor notification titles

// Streaming buffer voor Binance klines parsing (geen grote heap allocaties)
static char binanceStreamBuffer[1024];  // Fixed-size buffer voor chunked JSON parsing

// LVGL UI buffers en cache (voorkomt herhaalde allocaties en onnodige updates)
static char priceLblBuffer[32];  // Buffer voor price label (%.2f format)
static char anchorMaxLabelBuffer[32];  // Buffer voor anchor max label
static char anchorLabelBuffer[32];  // Buffer voor anchor label
static char anchorMinLabelBuffer[32];  // Buffer voor anchor min label
static char priceTitleBuffer[3][64];  // Buffers voor price titles (3 symbols)
static char price1MinMaxLabelBuffer[32];  // Buffer voor 1m max label
static char price1MinMinLabelBuffer[32];  // Buffer voor 1m min label
static char price1MinDiffLabelBuffer[32];  // Buffer voor 1m diff label
static char price30MinMaxLabelBuffer[32];  // Buffer voor 30m max label
static char price30MinMinLabelBuffer[32];  // Buffer voor 30m min label
static char price30MinDiffLabelBuffer[32];  // Buffer voor 30m diff label

// Cache laatste waarden (alleen updaten als veranderd)
static float lastPriceLblValue = -1.0f;  // Cache voor price label
static float lastAnchorMaxValue = -1.0f;  // Cache voor anchor max
static float lastAnchorValue = -1.0f;  // Cache voor anchor
static float lastAnchorMinValue = -1.0f;  // Cache voor anchor min
static float lastPrice1MinMaxValue = -1.0f;  // Cache voor 1m max
static float lastPrice1MinMinValue = -1.0f;  // Cache voor 1m min
static float lastPrice1MinDiffValue = -1.0f;  // Cache voor 1m diff
static float lastPrice30MinMaxValue = -1.0f;  // Cache voor 30m max
static float lastPrice30MinMinValue = -1.0f;  // Cache voor 30m min
static float lastPrice30MinDiffValue = -1.0f;  // Cache voor 30m diff
static char lastPriceTitleText[3][64] = {""};  // Cache voor price titles
static char priceLblBufferArray[3][32];  // Buffers voor average price labels (3 symbols)
static char footerRssiBuffer[16];  // Buffer voor footer RSSI
static char footerRamBuffer[16];  // Buffer voor footer RAM
static float lastPriceLblValueArray[3] = {-1.0f, -1.0f, -1.0f};  // Cache voor average price labels
static int32_t lastRssiValue = -999;  // Cache voor RSSI
static uint32_t lastRamValue = 0;  // Cache voor RAM
// lastDateText en lastTimeText zijn verplaatst naar direct voor updateDateTimeLabels() functie

// ArduinoJson: globaal hergebruikte StaticJsonDocument (geen herhaalde allocaties per tick)
// Conservatieve capaciteit: 256 bytes is voldoende voor Binance ticker/price responses (~100 bytes)
#if USE_ARDUINOJSON
static StaticJsonDocument<256> jsonDoc;  // Hergebruik voor alle JSON parsing
#endif

// Price history for calculating returns and moving averages
// Array van 60 posities voor laatste 60 seconden (1 minuut)
// Fase 4.2.3: static verwijderd tijdelijk voor parallelle implementatie (wordt later weer static)
float secondPrices[SECONDS_PER_MINUTE];
DataSource secondPricesSource[SECONDS_PER_MINUTE];  // Source tracking per sample
uint8_t secondIndex = 0;
bool secondArrayFilled = false;
static bool newPriceDataAvailable = false;  // Flag om aan te geven of er nieuwe prijsdata is voor grafiek update

// Array van 300 posities voor laatste 300 seconden (5 minuten) - voor ret_5m berekening
// Voor CYD zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
// Fase 4.2.3: static verwijderd tijdelijk voor parallelle implementatie
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
float *fiveMinutePrices = nullptr;  // Dynamisch gealloceerd voor CYD zonder PSRAM
DataSource *fiveMinutePricesSource = nullptr;  // Dynamisch gealloceerd
#else
float fiveMinutePrices[SECONDS_PER_5MINUTES];
DataSource fiveMinutePricesSource[SECONDS_PER_5MINUTES];  // Source tracking per sample
#endif
uint16_t fiveMinuteIndex = 0;
bool fiveMinuteArrayFilled = false;

// Array van 120 posities voor laatste 120 minuten (2 uur)
// Elke minuut wordt het gemiddelde van de 60 seconden opgeslagen
// We hebben 60 posities nodig om het gemiddelde van laatste 30 minuten te vergelijken
// met het gemiddelde van de 30 minuten daarvoor (maar we houden 120 voor buffer)
// Voor CYD zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
// Fase 4.2.9: static verwijderd zodat PriceData getters deze kunnen gebruiken
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
float *minuteAverages = nullptr;  // Dynamisch gealloceerd voor CYD zonder PSRAM
DataSource *minuteAveragesSource = nullptr;  // Dynamisch gealloceerd
#else
float minuteAverages[MINUTES_FOR_30MIN_CALC];
DataSource minuteAveragesSource[MINUTES_FOR_30MIN_CALC];  // Source tracking per sample
#endif
// Fase 4.2.9: static verwijderd zodat PriceData getters deze kunnen gebruiken
uint8_t minuteIndex = 0;
bool minuteArrayFilled = false;
static unsigned long lastMinuteUpdate = 0;
static float firstMinuteAverage = 0.0f; // Eerste minuut gemiddelde prijs als basis voor 30-min berekening

// Warm-Start state
static bool warmStartEnabled = WARM_START_ENABLED_DEFAULT;
static uint8_t warmStart1mExtraCandles = WARM_START_1M_EXTRA_CANDLES_DEFAULT;
static uint8_t warmStart5mCandles = WARM_START_5M_CANDLES_DEFAULT;
static uint8_t warmStart30mCandles = WARM_START_30M_CANDLES_DEFAULT;
static uint8_t warmStart2hCandles = WARM_START_2H_CANDLES_DEFAULT;
static WarmStartStatus warmStartStatus = LIVE;  // Default: LIVE (cold start als warm-start faalt)
static unsigned long warmStartCompleteTime = 0;  // Timestamp wanneer systeem volledig LIVE werd
static WarmStartStats warmStartStats = {0, 0, 0, 0, false, false, false, false, WS_MODE_DISABLED, 0};

// Notification settings - NTFY.sh
// Note: NTFY topic wordt dynamisch gegenereerd met ESP32 device ID
// Format: [ESP32-ID]-alert (bijv. 9MK28H3Q-alert)
// ESP32-ID is 8 karakters (Crockford Base32 encoding) voor veilige, unieke identificatie
// Dit voorkomt conflicten tussen verschillende devices

// Language setting (0 = Nederlands, 1 = English)
// DEFAULT_LANGUAGE wordt gedefinieerd in platform_config.h (fallback als er nog geen waarde in Preferences staat)
static uint8_t language = DEFAULT_LANGUAGE;  // 0 = Nederlands, 1 = English

// Settings structs voor betere organisatie
// NOTE: AlertThresholds en NotificationCooldowns zijn nu gedefinieerd in SettingsStore.h

// Instelbare grenswaarden (worden geladen uit Preferences)
// Note: ntfyTopic wordt geïnitialiseerd in loadSettings() met unieke ESP32 ID
static char ntfyTopic[64] = "";  // NTFY topic (max 63 karakters)
static char binanceSymbol[16] = BINANCE_SYMBOL_DEFAULT;  // Binance symbool (max 15 karakters, bijv. BTCEUR, BTCUSDT)

// Alert thresholds in struct voor betere organisatie
static AlertThresholds alertThresholds = {
    .spike1m = SPIKE_1M_THRESHOLD_DEFAULT,
    .spike5m = SPIKE_5M_THRESHOLD_DEFAULT,
    .move30m = MOVE_30M_THRESHOLD_DEFAULT,
    .move5m = MOVE_5M_THRESHOLD_DEFAULT,
    .move5mAlert = MOVE_5M_ALERT_THRESHOLD_DEFAULT,
    .threshold1MinUp = THRESHOLD_1MIN_UP_DEFAULT,
    .threshold1MinDown = THRESHOLD_1MIN_DOWN_DEFAULT,
    .threshold30MinUp = THRESHOLD_30MIN_UP_DEFAULT,
    .threshold30MinDown = THRESHOLD_30MIN_DOWN_DEFAULT
};

// Notification cooldowns in struct voor betere organisatie
static NotificationCooldowns notificationCooldowns = {
    .cooldown1MinMs = NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT,
    .cooldown30MinMs = NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT,
    .cooldown5MinMs = NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT
};

// Backward compatibility: legacy variabelen (verwijzen naar struct)
#define spike1mThreshold alertThresholds.spike1m
#define spike5mThreshold alertThresholds.spike5m
#define move30mThreshold alertThresholds.move30m
#define move5mThreshold alertThresholds.move5m
#define move5mAlertThreshold alertThresholds.move5mAlert
#define threshold1MinUp alertThresholds.threshold1MinUp
#define threshold1MinDown alertThresholds.threshold1MinDown
#define threshold30MinUp alertThresholds.threshold30MinUp
#define threshold30MinDown alertThresholds.threshold30MinDown
#define notificationCooldown1MinMs notificationCooldowns.cooldown1MinMs
#define notificationCooldown30MinMs notificationCooldowns.cooldown30MinMs
#define notificationCooldown5MinMs notificationCooldowns.cooldown5MinMs

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

// SettingsStore instance
SettingsStore settingsStore;

// ApiClient instance (Fase 4.1 voltooid)
#include "src/ApiClient/ApiClient.h"
ApiClient apiClient;

// PriceData instance (Fase 4.2.1: module structuur aangemaakt)
PriceData priceData;

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

// MQTT Message Queue - voorkomt message loss bij disconnect
#define MQTT_QUEUE_SIZE 10  // Max aantal berichten in queue
struct MqttMessage {
    char topic[128];
    char payload[128];
    bool retained;
    bool valid;
};

static MqttMessage mqttQueue[MQTT_QUEUE_SIZE];
static uint8_t mqttQueueHead = 0;

// Anchor setting queue - voorkomt crashes door web server thread
// Thread-safe: geschreven vanuit web server/MQTT, gelezen vanuit uiTask
struct AnchorSetting {
    volatile float value;      // volatile voor thread-safe access
    volatile bool pending;      // volatile voor thread-safe access
    volatile bool useCurrentPrice; // volatile voor thread-safe access
};
static AnchorSetting pendingAnchorSetting = {0.0f, false, false};
static volatile unsigned long lastAnchorSetTime = 0; // volatile voor thread-safe access
static const unsigned long ANCHOR_SET_COOLDOWN_MS = 2000; // Minimaal 2 seconden tussen anchor sets

// Helper functie om anchor setting in queue te zetten (thread-safe)
// Centraliseert de logica voor alle input methoden (web, MQTT)
// Returns: true als succesvol in queue gezet, false bij fout
static bool queueAnchorSetting(float value, bool useCurrentPrice) {
    // Valideer waarde (alleen als niet useCurrentPrice)
    if (!useCurrentPrice && (value <= 0.0f || !isValidPrice(value))) {
        Serial_printf(F("[Anchor Queue] WARN: Ongeldige waarde: %.2f\n"), value);
        return false;
    }
    
    // Thread-safe write: schrijf eerst value en useCurrentPrice, dan pending flag
    // Dit voorkomt dat uiTask een incomplete state leest
    pendingAnchorSetting.value = useCurrentPrice ? 0.0f : value;
    pendingAnchorSetting.useCurrentPrice = useCurrentPrice;
    // Memory barrier effect: pending flag als laatste (garandeert dat value en useCurrentPrice al geschreven zijn)
    pendingAnchorSetting.pending = true;
    
    return true;
}
static uint8_t mqttQueueTail = 0;
static uint8_t mqttQueueCount = 0;

// WiFi reconnect controle
// Geoptimaliseerd: betere reconnect logica met retry counter en exponential backoff
static bool wifiReconnectEnabled = false;
static unsigned long lastReconnectAttempt = 0;
static bool wifiInitialized = false;
static uint8_t reconnectAttemptCount = 0;
static const uint8_t MAX_RECONNECT_ATTEMPTS = 5; // Max aantal reconnect pogingen voordat we exponential backoff starten

// MQTT reconnect controle met exponential backoff
static uint8_t mqttReconnectAttemptCount = 0;
static const uint8_t MAX_MQTT_RECONNECT_ATTEMPTS = 3; // Max aantal reconnect pogingen voordat we exponential backoff starten


// ============================================================================
// HTTP and API Functions
// ============================================================================
// Oude httpGET() en parsePrice() functies zijn verwijderd - nu via ApiClient module

// ============================================================================
// Warm-Start: Binance Klines Functions
// ============================================================================

// Parse een enkele kline entry uit JSON array
// Format: [openTime, open, high, low, close, volume, ...]
// Returns: true als succesvol, false bij fout
static bool parseKlineEntry(const char* jsonStr, float* closePrice, unsigned long* openTime)
{
    if (jsonStr == nullptr || closePrice == nullptr || openTime == nullptr) {
        return false;
    }
    
    // Skip opening bracket
    const char* ptr = jsonStr;
    while (*ptr && (*ptr == '[' || *ptr == ' ')) ptr++;
    if (*ptr == '\0') return false;
    
    // Parse openTime (eerste veld)
    unsigned long time = 0;
    while (*ptr && *ptr != ',') {
        if (*ptr >= '0' && *ptr <= '9') {
            time = time * 10 + (*ptr - '0');
        }
        ptr++;
    }
    if (*ptr != ',') return false;
    *openTime = time;
    ptr++; // Skip comma
    
    // Skip open, high, low (velden 2-4)
    for (int i = 0; i < 3; i++) {
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr != ',') return false;
        ptr++; // Skip comma
    }
    
    // Parse close price (veld 5)
    char priceStr[32];
    int priceIdx = 0;
    while (*ptr && *ptr != ',' && priceIdx < (int)sizeof(priceStr) - 1) {
        if (*ptr == '"') {
            ptr++;
            continue;
        }
        priceStr[priceIdx++] = *ptr;
        ptr++;
    }
    priceStr[priceIdx] = '\0';
    
    float price;
    if (!safeAtof(priceStr, price) || !isValidPrice(price)) {
        return false;
    }
    *closePrice = price;
    return true;
}

// Haal Binance klines op voor een specifiek timeframe
// Memory efficient: streaming parsing, bewaar alleen laatste maxCount candles
// Returns: aantal candles opgehaald, of -1 bij fout
static int fetchBinanceKlines(const char* symbol, const char* interval, uint16_t limit, float* prices, unsigned long* timestamps, uint16_t maxCount)
{
    if (symbol == nullptr || interval == nullptr || prices == nullptr || maxCount == 0) {
        return -1;
    }
    
    // Build URL
    char url[256];
    int urlLen = snprintf(url, sizeof(url), "%s?symbol=%s&interval=%s&limit=%u", 
                         BINANCE_KLINES_API, symbol, interval, limit);
    if (urlLen < 0 || urlLen >= (int)sizeof(url)) {
        return -1;
    }
    
    // Streaming HTTP request: parse direct van stream
    HTTPClient http;
    http.setTimeout(WARM_START_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setReuse(false);
    
    if (!http.begin(url)) {
        http.end();
        return -1;
    }
    
    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    if (stream == nullptr) {
        http.end();
        return -1;
    }
    
    // Streaming JSON parser: gebruik fixed-size buffer voor chunked reading
    // Parse iteratief en sla alleen noodzakelijke values op (closes/returns)
    int writeIdx = 0;  // Schrijf index in circulaire buffer
    int totalParsed = 0;
    bool bufferFilled = false;  // True wanneer buffer vol is en we gaan wrappen
    
    // Parser state
    enum ParseState {
        PS_START,
        PS_OUTER_ARRAY,
        PS_ENTRY_START,
        PS_FIELD,
        PS_ENTRY_END
    };
    ParseState state = PS_START;
    int fieldIdx = 0;
    char fieldBuf[64];
    int fieldBufIdx = 0;
    unsigned long openTime = 0;
    float closePrice = 0.0f;
    
    // Buffer voor chunked reading (hergebruik fixed buffer)
    size_t bufferPos = 0;
    size_t bufferLen = 0;
    const size_t BUFFER_SIZE = sizeof(binanceStreamBuffer);
    
    // Feed watchdog tijdens parsing
    unsigned long lastWatchdogFeed = millis();
    const unsigned long WATCHDOG_FEED_INTERVAL = 1000; // Feed elke seconde
    
    // Timeout voor parsing
    unsigned long parseStartTime = millis();
    const unsigned long PARSE_TIMEOUT_MS = 8000;
    unsigned long lastDataTime = millis();
    const unsigned long DATA_TIMEOUT_MS = 2000;
    
    // Parse streaming JSON
    // Continue zolang stream connected/available OF er nog data in buffer is
    while (stream->connected() || stream->available() || (bufferPos < bufferLen)) {
        // Timeout check: stop als parsing te lang duurt
        if ((millis() - parseStartTime) > PARSE_TIMEOUT_MS) {
            break;
        }
        // Feed watchdog periodiek (alleen elke seconde)
        if ((millis() - lastWatchdogFeed) >= WATCHDOG_FEED_INTERVAL) {
            yield();
            delay(0);
            lastWatchdogFeed = millis();
        }
        
        // Read chunk into buffer als nodig
        if (bufferPos >= bufferLen) {
            if (stream->available()) {
                bufferLen = stream->readBytes((uint8_t*)binanceStreamBuffer, BUFFER_SIZE - 1);
                binanceStreamBuffer[bufferLen] = '\0';
                bufferPos = 0;
                lastDataTime = millis();
                
                if (bufferLen == 0) {
                    break;
                }
            } else {
                // Check data timeout
                if ((millis() - lastDataTime) > DATA_TIMEOUT_MS) {
                    break;
                }
                // Wait a bit for more data
                delay(10);
                continue;
            }
        }
        
        char c = binanceStreamBuffer[bufferPos++];
        
        // State machine voor JSON parsing
        switch (state) {
            case PS_START:
                if (c == '[') {
                    state = PS_OUTER_ARRAY;
                } else if (c == ']') {
                    goto parse_done;
                }
                break;
                
            case PS_OUTER_ARRAY:
                if (c == '[') {
                    state = PS_ENTRY_START;
                    fieldIdx = 0;
                    fieldBufIdx = 0;
                    openTime = 0;
                    closePrice = 0.0f;
                } else if (c == ']') {
                    // End of outer array
                    goto parse_done;
                }
                break;
                
            case PS_ENTRY_START:
            case PS_FIELD:
                if (c == ',' && fieldBufIdx > 0) {
                    // End of field
                    fieldBuf[fieldBufIdx] = '\0';
                    
                    if (fieldIdx == 0) {
                        // openTime
                        unsigned long time = 0;
                        for (int i = 0; fieldBuf[i] != '\0'; i++) {
                            if (fieldBuf[i] >= '0' && fieldBuf[i] <= '9') {
                                time = time * 10 + (fieldBuf[i] - '0');
                            }
                        }
                        openTime = time;
                    } else if (fieldIdx == 4) {
                        // close price (5th field, index 4)
                        float price;
                        if (safeAtof(fieldBuf, price) && isValidPrice(price)) {
                            closePrice = price;
                        }
                    }
                    
                    fieldIdx++;
                    fieldBufIdx = 0;
                    state = PS_FIELD;
                } else if (c == ']') {
                    // End of entry
                    if (fieldBufIdx > 0) {
                        // Process last field
                        fieldBuf[fieldBufIdx] = '\0';
                        if (fieldIdx == 4) {
                            float price;
                            if (safeAtof(fieldBuf, price) && isValidPrice(price)) {
                                closePrice = price;
                            }
                        }
                    }
                    state = PS_ENTRY_END;
                } else if (c != ' ' && c != '\n' && c != '\r' && c != '"') {
                    // Accumulate field character
                    if (fieldBufIdx < (int)sizeof(fieldBuf) - 1) {
                        fieldBuf[fieldBufIdx++] = c;
                    }
                    state = PS_FIELD;
                }
                break;
                
            case PS_ENTRY_END:
                // Store candle in circulaire buffer
                if (closePrice > 0.0f) {
                    prices[writeIdx] = closePrice;
                    if (timestamps != nullptr) {
                        timestamps[writeIdx] = openTime;
                    }
                    
                    writeIdx++;
                    if (writeIdx >= (int)maxCount) {
                        writeIdx = 0;  // Wrap around
                        bufferFilled = true;
                    }
                    totalParsed++;
                    
                    if (totalParsed >= (int)limit) {
                        goto parse_done;
                    }
                }
                
                // Move to next entry
                if (c == ',') {
                    state = PS_ENTRY_START;
                    fieldIdx = 0;
                    fieldBufIdx = 0;
                    openTime = 0;
                    closePrice = 0.0f;
                } else if (c == ']') {
                    // End of outer array
                    goto parse_done;
                } else if (c == '[') {
                    // Next entry
                    state = PS_ENTRY_START;
                    fieldIdx = 0;
                    fieldBufIdx = 0;
                    openTime = 0;
                    closePrice = 0.0f;
                }
                break;
        }
    }
    
parse_done:
    http.end();
    
    // Als buffer gewrapped is, herschik naar chronologische volgorde
    // Memory efficient: gebruik alleen kleine temp buffer of heap voor grote buffers
    int storedCount = bufferFilled ? (int)maxCount : writeIdx;
    if (bufferFilled && writeIdx > 0) {
        // Buffer is gewrapped: [writeIdx..maxCount-1, 0..writeIdx-1] -> [0..maxCount-1]
        // Voor kleine buffers: gebruik stack temp (max 60)
        if (writeIdx <= 60 && maxCount <= 120) {
            float tempReorder[60];  // Max 60 floats = 240 bytes (veilig voor stack)
            unsigned long tempReorderTimes[60];
            
            // Kopieer eerste deel (0..writeIdx-1) naar temp
            for (int i = 0; i < writeIdx; i++) {
                tempReorder[i] = prices[i];
                if (timestamps != nullptr) {
                    tempReorderTimes[i] = timestamps[i];
                }
            }
            // Verschuif tweede deel (writeIdx..maxCount-1) naar begin
            for (int i = 0; i < (int)maxCount - writeIdx; i++) {
                prices[i] = prices[writeIdx + i];
                if (timestamps != nullptr) {
                    timestamps[i] = timestamps[writeIdx + i];
                }
            }
            // Kopieer eerste deel naar einde
            for (int i = 0; i < writeIdx; i++) {
                prices[(int)maxCount - writeIdx + i] = tempReorder[i];
                if (timestamps != nullptr) {
                    timestamps[(int)maxCount - writeIdx + i] = tempReorderTimes[i];
                }
            }
        } else {
            // Voor grote buffers: gebruik heap allocatie
            float* tempFull = (float*)malloc(maxCount * sizeof(float));
            unsigned long* tempFullTimes = (timestamps != nullptr) ? (unsigned long*)malloc(maxCount * sizeof(unsigned long)) : nullptr;
            
            if (tempFull != nullptr) {
                // Kopieer hele buffer
                for (uint16_t i = 0; i < maxCount; i++) {
                    tempFull[i] = prices[i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        tempFullTimes[i] = timestamps[i];
                    }
                }
                // Herschik: [writeIdx..maxCount-1, 0..writeIdx-1] -> [0..maxCount-1]
                for (uint16_t i = 0; i < maxCount - writeIdx; i++) {
                    prices[i] = tempFull[writeIdx + i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        timestamps[i] = tempFullTimes[writeIdx + i];
                    }
                }
                for (uint16_t i = 0; i < writeIdx; i++) {
                    prices[(int)maxCount - writeIdx + i] = tempFull[i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        timestamps[(int)maxCount - writeIdx + i] = tempFullTimes[i];
                    }
                }
                free(tempFull);
                if (tempFullTimes != nullptr) free(tempFullTimes);
            }
            // Bij heap allocatie failure: buffer blijft in wrapped volgorde (geen probleem)
        }
    }
    
    return storedCount;
}

// Helper: Clamp waarde tussen min en max
static uint16_t clampUint16(uint16_t value, uint16_t minVal, uint16_t maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// Helper: Detecteer PSRAM beschikbaarheid (runtime check)
static bool hasPSRAM()
{
    #ifdef BOARD_HAS_PSRAM
        return psramFound();
    #else
        // Fallback: check PSRAM size (runtime)
        return (ESP.getPsramSize() > 0);
    #endif
}

// Helper: Bereken 1m candles nodig (volatility window + extra)
// PSRAM-aware: clamp afhankelijk van PSRAM beschikbaarheid
static uint16_t calculate1mCandles()
{
    uint16_t baseCandles = autoVolatilityWindowMinutes + warmStart1mExtraCandles;
    bool psramAvailable = hasPSRAM();
    uint16_t maxCandles = psramAvailable ? 150 : 80;  // Met PSRAM: 150, zonder: 80
    return clampUint16(baseCandles, 30, maxCandles);
}

// Forward declarations voor heap telemetry (nodig voor performWarmStart)
static void logHeapTelemetry(const char* context);

// Warm-start: Vul buffers met Binance historische data (returns-only, memory efficient)
// Returns: WarmStartMode (FULL/PARTIAL/FAILED/DISABLED)
static WarmStartMode performWarmStart()
{
    // Initialize stats
    warmStartStats = {0, 0, 0, 0, false, false, false, false, WS_MODE_DISABLED, 0};
    
    if (!warmStartEnabled) {
        Serial.println(F("[WarmStart] Warm-start uitgeschakeld, cold start"));
        warmStartStatus = LIVE;
        warmStartStats.mode = WS_MODE_DISABLED;
        return WS_MODE_DISABLED;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WarmStart] WiFi niet verbonden, cold start"));
        warmStartStatus = LIVE_COLD;
        warmStartStats.mode = WS_MODE_FAILED;
        hasRet2hWarm = false;
        hasRet30mWarm = false;
        hasRet2h = hasRet2hWarm || hasRet2hLive;
        hasRet30m = hasRet30mWarm || hasRet30mLive;
        return WS_MODE_FAILED;
    }
    
    // Fail-safe: check heap space vóór warm-start (minimaal 20KB nodig)
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 20000) {
        Serial_printf(F("[WarmStart] WARN: Onvoldoende heap (%u bytes), skip warm-start\n"), freeHeap);
        warmStartStatus = LIVE_COLD;
        warmStartStats.mode = WS_MODE_FAILED;
        hasRet2hWarm = false;
        hasRet30mWarm = false;
        hasRet2h = hasRet2hWarm || hasRet2hLive;
        hasRet30m = hasRet30mWarm || hasRet30mLive;
        return WS_MODE_FAILED;
    }
    
    warmStartStatus = WARMING_UP;
    
    // Bereken dynamische candle limits (PSRAM-aware clamping)
    bool psramAvailable = hasPSRAM();
    uint16_t req1mCandles = calculate1mCandles();  // PSRAM-aware (max 150 met PSRAM, 80 zonder)
    uint16_t max5m = psramAvailable ? 24 : 12;  // Met PSRAM: 24, zonder: 12
    uint16_t max30m = psramAvailable ? 12 : 6;  // Met PSRAM: 12, zonder: 6
    uint16_t max2h = psramAvailable ? 8 : 4;  // Met PSRAM: 8, zonder: 4
    uint16_t req5mCandles = clampUint16(warmStart5mCandles, 2, max5m);
    uint16_t req30mCandles = clampUint16(warmStart30mCandles, 2, max30m);
    uint16_t req2hCandles = clampUint16(warmStart2hCandles, 2, max2h);
    
    
    // 1. Vul 1m buffer voor volatiliteit (returns-only: alleen laatste closes nodig)
    // Memory efficient: alleen laatste SECONDS_PER_MINUTE closes bewaren
    float temp1mPrices[SECONDS_PER_MINUTE];  // Alleen laatste 60 nodig
    int count1m = fetchBinanceKlines(binanceSymbol, "1m", req1mCandles, temp1mPrices, nullptr, SECONDS_PER_MINUTE);
    if (count1m > 0) {
        // Vul secondPrices buffer (gebruik laatste count1m candles, max SECONDS_PER_MINUTE)
        int copyCount = (count1m < SECONDS_PER_MINUTE) ? count1m : SECONDS_PER_MINUTE;
        for (int i = 0; i < copyCount; i++) {
            int srcIdx = count1m - copyCount + i;
            if (srcIdx >= 0 && srcIdx < count1m) {
                secondPrices[i] = temp1mPrices[srcIdx];
                secondPricesSource[i] = SOURCE_BINANCE;
            }
        }
        secondIndex = copyCount;
        secondArrayFilled = (copyCount == SECONDS_PER_MINUTE);
        warmStartStats.loaded1m = count1m;
        warmStartStats.warmStartOk1m = true;
    } else {
        warmStartStats.warmStartOk1m = false;
    }
    
    // 2. Vul 5m buffer (returns-only: alleen laatste 2 closes nodig)
    float temp5mPrices[2];
    int count5m = fetchBinanceKlines(binanceSymbol, "5m", req5mCandles, temp5mPrices, nullptr, 2);
    if (count5m >= 2) {
        // Interpoleer laatste 2 candles naar fiveMinutePrices buffer
        // Elke 5m candle = 300 seconden, gebruik laatste candle voor hele buffer
        float lastPrice = temp5mPrices[count5m - 1];
        for (int s = 0; s < SECONDS_PER_5MINUTES; s++) {
            fiveMinutePrices[s] = lastPrice;
            fiveMinutePricesSource[s] = SOURCE_BINANCE;
        }
            fiveMinuteIndex = SECONDS_PER_5MINUTES;
            fiveMinuteArrayFilled = true;
            // Fase 4.2.5: Synchroniseer PriceData state na warm-start
            priceData.syncStateFromGlobals();
        warmStartStats.loaded5m = count5m;
        warmStartStats.warmStartOk5m = true;
    } else {
        warmStartStats.warmStartOk5m = false;
    }
    
    yield();
    delay(0);
    
    // 3. Vul 30m buffer via minuteAverages (returns-only: alleen laatste 2 closes nodig)
    // Retry-logica: probeer maximaal 3 keer als eerste poging faalt
    float temp30mPrices[2];
    int count30m = 0;
    const int maxRetries30m = 3;
    for (int retry = 0; retry < maxRetries30m; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 30m retry %d/%d...\n"), retry, maxRetries30m - 1);
            yield();
            delay(500);  // Korte delay tussen retries
        }
        count30m = fetchBinanceKlines(binanceSymbol, "30m", req30mCandles, temp30mPrices, nullptr, 2);
        if (count30m >= 2) {
            break;  // Succes, stop retries
        }
    }
    
    if (count30m >= 2) {
        // Bereken ret_30m uit eerste en laatste 30m candle (gesloten candles)
        float first30mPrice = temp30mPrices[0];
        float last30mPrice = temp30mPrices[count30m - 1];
        if (first30mPrice > 0.0f && last30mPrice > 0.0f) {
            ret_30m = ((last30mPrice - first30mPrice) / first30mPrice) * 100.0f;
            hasRet30mWarm = true;
        } else {
            hasRet30mWarm = false;
        }
        
        float lastPrice = temp30mPrices[count30m - 1];
        for (int m = 0; m < MINUTES_FOR_30MIN_CALC; m++) {
            minuteAverages[m] = lastPrice;
            minuteAveragesSource[m] = SOURCE_BINANCE;
        }
        minuteIndex = MINUTES_FOR_30MIN_CALC;
        minuteArrayFilled = true;
        firstMinuteAverage = minuteAverages[0];
        warmStartStats.loaded30m = count30m;
        warmStartStats.warmStartOk30m = true;
    } else {
        warmStartStats.warmStartOk30m = false;
        hasRet30mWarm = false;
        if (count30m < 0) {
            Serial_printf(F("[WarmStart] 30m fetch gefaald na %d pogingen (error: %d)\n"), maxRetries30m, count30m);
        } else if (count30m == 0) {
            Serial_printf(F("[WarmStart] 30m fetch: 0 candles na %d pogingen (mogelijk timeout of lege response)\n"), maxRetries30m);
        } else {
            Serial_printf(F("[WarmStart] 30m fetch: onvoldoende candles na %d pogingen (%d, minimaal 2 nodig)\n"), maxRetries30m, count30m);
        }
    }
    
    // Feed watchdog
    yield();
    delay(0);
    
    // 4. Initieer 2h trend berekening
    // Retry-logica: probeer maximaal 3 keer als eerste poging faalt
    float temp2hPrices[2];
    int count2h = 0;
    const int maxRetries2h = 3;
    for (int retry = 0; retry < maxRetries2h; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 2h retry %d/%d...\n"), retry, maxRetries2h - 1);
            yield();
            delay(500);  // Korte delay tussen retries
        }
        count2h = fetchBinanceKlines(binanceSymbol, "2h", req2hCandles, temp2hPrices, nullptr, 2);
        if (count2h >= 2) {
            break;  // Succes, stop retries
        }
    }
    
    if (count2h >= 2) {
        // Bereken ret_2h uit eerste en laatste candle (gesloten candles)
        float firstPrice = temp2hPrices[0];
        float lastPrice = temp2hPrices[count2h - 1];
        if (firstPrice > 0.0f && lastPrice > 0.0f) {
            ret_2h = ((lastPrice - firstPrice) / firstPrice) * 100.0f;
            hasRet2hWarm = true;
            warmStartStats.loaded2h = count2h;
            warmStartStats.warmStartOk2h = true;
        } else {
            warmStartStats.warmStartOk2h = false;
            hasRet2hWarm = false;
        }
    } else {
        warmStartStats.warmStartOk2h = false;
        hasRet2hWarm = false;
        if (count2h < 0) {
            Serial_printf(F("[WarmStart] 2h fetch gefaald na %d pogingen (error: %d)\n"), maxRetries2h, count2h);
        } else if (count2h == 0) {
            Serial_printf(F("[WarmStart] 2h fetch: 0 candles na %d pogingen (mogelijk timeout of lege response)\n"), maxRetries2h);
        } else {
            Serial_printf(F("[WarmStart] 2h fetch: onvoldoende candles na %d pogingen (%d, minimaal 2 nodig)\n"), maxRetries2h, count2h);
        }
    }
    
    // Update combined flags na warm-start
    hasRet2h = hasRet2hWarm || hasRet2hLive;
    hasRet30m = hasRet30mWarm || hasRet30mLive;
    
    // Bepaal trend state op basis van warm-start data
    if (hasRet2h && hasRet30m) {
        trendState = determineTrendState(ret_2h, ret_30m);
    }
    
    // Bepaal mode op basis van successen
    int successCount = (warmStartStats.warmStartOk1m ? 1 : 0) +
                       (warmStartStats.warmStartOk5m ? 1 : 0) +
                       (warmStartStats.warmStartOk30m ? 1 : 0) +
                       (warmStartStats.warmStartOk2h ? 1 : 0);
    
    if (successCount == 4) {
        warmStartStats.mode = WS_MODE_FULL;
        warmStartStatus = WARMING_UP;
    } else if (successCount > 0) {
        warmStartStats.mode = WS_MODE_PARTIAL;
        warmStartStatus = WARMING_UP;
    } else {
        warmStartStats.mode = WS_MODE_FAILED;
        warmStartStatus = LIVE_COLD;
    }
    
    // Compacte boot log regel
    const char* modeStr = (warmStartStats.mode == WS_MODE_FULL) ? "FULL" :
                          (warmStartStats.mode == WS_MODE_PARTIAL) ? "PARTIAL" :
                          (warmStartStats.mode == WS_MODE_FAILED) ? "FAILED" : "DISABLED";
    Serial.print(F("[WarmStart] 1m="));
    Serial.print(warmStartStats.loaded1m);
    Serial.print(F(" 5m="));
    Serial.print(warmStartStats.loaded5m);
    Serial.print(F(" 30m="));
    Serial.print(warmStartStats.loaded30m);
    Serial.print(F(" 2h="));
    Serial.print(warmStartStats.loaded2h);
    Serial.print(F(" (mode="));
    Serial.print(modeStr);
    Serial.print(F(", hasRet2h="));
    Serial.print(hasRet2hWarm ? 1 : 0);
    Serial.print(F("/"));
    Serial.print(hasRet2hLive ? 1 : 0);
    Serial.print(F(", hasRet30m="));
    Serial.print(hasRet30mWarm ? 1 : 0);
    Serial.print(F("/"));
    Serial.print(hasRet30mLive ? 1 : 0);
    Serial.println(F(")"));
    Serial.flush();
    
    // Fail-safe: als warm-start gefaald is, ga door als cold start
    if (warmStartStats.mode == WS_MODE_FAILED) {
        warmStartStatus = LIVE_COLD;
        hasRet2h = false;
        hasRet30m = false;
        Serial.println(F("[WarmStart] Warm-start gefaald, ga door als cold start (LIVE_COLD)"));
    }
    
    // Heap telemetry na warm-start
    logHeapTelemetry("warm-start");
    
    return warmStartStats.mode;
}

// Update warm-start status: check of systeem volledig LIVE is en bereken progress
// Fase 4.2.3: static verwijderd tijdelijk voor parallelle implementatie
void updateWarmStartStatus()
{
    if (warmStartStatus == LIVE || warmStartStatus == LIVE_COLD) {
        warmStartStats.warmUpProgress = 100;
        return; // Al LIVE, geen update nodig
    }
    
    // Bereken warm-up progress: percentage LIVE data in buffers
    uint8_t volatilityLivePct = 0;
    uint8_t trendLivePct = 0;
    
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Check volatiliteit: percentage LIVE in secondPrices buffer
    DataSource* sources = priceData.getSecondPricesSource();
    bool arrayFilled = priceData.getSecondArrayFilled();
    uint8_t index = priceData.getSecondIndex();
    
    if (arrayFilled) {
        uint8_t liveCount = 0;
        for (uint8_t i = 0; i < SECONDS_PER_MINUTE; i++) {
            if (sources[i] == SOURCE_LIVE) {
                liveCount++;
            }
        }
        volatilityLivePct = (liveCount * 100) / SECONDS_PER_MINUTE;
    } else if (index > 0) {
        uint8_t liveCount = 0;
        for (uint8_t i = 0; i < index; i++) {
            if (sources[i] == SOURCE_LIVE) {
                liveCount++;
            }
        }
        volatilityLivePct = (liveCount * 100) / index;
    }
    
    // Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Check trend: percentage LIVE in minuteAverages buffer
    DataSource* minuteSources = priceData.getMinuteAveragesSource();
    bool minuteArrayFilled = priceData.getMinuteArrayFilled();
    uint8_t minuteIndex = priceData.getMinuteIndex();
    
    if (minuteArrayFilled) {
        uint8_t liveCount = 0;
        for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
            if (minuteSources[i] == SOURCE_LIVE) {
                liveCount++;
            }
        }
        trendLivePct = (liveCount * 100) / MINUTES_FOR_30MIN_CALC;
    } else if (minuteIndex > 0) {
        uint8_t liveCount = 0;
        for (uint8_t i = 0; i < minuteIndex; i++) {
            if (minuteSources[i] == SOURCE_LIVE) {
                liveCount++;
            }
        }
        trendLivePct = (liveCount * 100) / minuteIndex;
    }
    
    // Warm-up progress = gemiddelde van volatiliteit en trend progress
    warmStartStats.warmUpProgress = (volatilityLivePct + trendLivePct) / 2;
    
    // Check of volledig LIVE (≥80% voor beide)
    bool volatilityLive = (volatilityLivePct >= 80);
    bool trendLive = (trendLivePct >= 80);
    
    if (volatilityLive && trendLive) {
        if (warmStartStatus == WARMING_UP) {
            warmStartStatus = LIVE;
            warmStartCompleteTime = millis();
            warmStartStats.warmUpProgress = 100;
            unsigned long bootTime = (warmStartCompleteTime / 1000); // seconden
            Serial_printf(F("[WarmStart] Status: LIVE (volledig op live data na %lu seconden)\n"), bootTime);
        }
    }
}

// Send notification via Ntfy.sh
// colorTag: "green_square" voor stijging, "red_square" voor daling, "blue_square" voor neutraal
// Geoptimaliseerd: betere error handling en resource cleanup
static bool sendNtfyNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    // Check WiFi verbinding eerst
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial_println(F("[Notify] WiFi niet verbonden, kan NTFY notificatie niet versturen"));
        return false;
    }
    
    // Valideer inputs
    if (strlen(ntfyTopic) == 0)
    {
        Serial_println(F("[Notify] Ntfy topic niet geconfigureerd"));
        return false;
    }
    
    if (title == nullptr || message == nullptr)
    {
        Serial_println(F("[Notify] Ongeldige title of message pointer"));
        return false;
    }
    
    // Valideer lengte van inputs om buffer overflows te voorkomen
    if (strlen(title) > 64 || strlen(message) > 512)
    {
        Serial_println(F("[Notify] Title of message te lang"));
        return false;
    }
    
    char url[128];
    int urlLen = snprintf(url, sizeof(url), "https://ntfy.sh/%s", ntfyTopic);
    if (urlLen < 0 || urlLen >= (int)sizeof(url))
    {
        Serial_println(F("[Notify] URL buffer overflow"));
        return false;
    }
    
    Serial_printf(F("[Notify] Ntfy URL: %s\n"), url);
    Serial_printf(F("[Notify] Ntfy Title: %s\n"), title);
    Serial_printf(F("[Notify] Ntfy Message: %s\n"), message);
    
    HTTPClient http;
    http.setTimeout(5000);
    http.setReuse(false); // Voorkom connection reuse problemen
    
    if (!http.begin(url))
    {
        Serial_println(F("[Notify] Ntfy HTTP begin gefaald"));
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
            http.addHeader(F("Tags"), colorTag);
            Serial_printf(F("[Notify] Ntfy Tag: %s\n"), colorTag);
        }
    }
    
    Serial_println(F("[Notify] Ntfy POST versturen..."));
    int code = http.POST(message);
    
    // Haal response alleen op bij succes (bespaar geheugen)
    // Gebruik static buffer i.p.v. String om fragmentatie te voorkomen
    if (code == 200 || code == 201)
    {
        WiFiClient* stream = http.getStreamPtr();
        if (stream != nullptr) {
            size_t totalLen = 0;
            while (stream->available() && totalLen < (sizeof(httpResponseBuffer) - 1)) {
                size_t bytesRead = stream->readBytes((uint8_t*)(httpResponseBuffer + totalLen), sizeof(httpResponseBuffer) - 1 - totalLen);
                totalLen += bytesRead;
            }
            httpResponseBuffer[totalLen] = '\0';
            if (totalLen > 0) {
                Serial_printf(F("[Notify] Ntfy response: %s\n"), httpResponseBuffer);
            }
        } else {
            // Fallback: gebruik getString() maar kopieer direct naar buffer
            String response = http.getString();
            if (response.length() > 0) {
                size_t len = response.length();
                if (len < sizeof(httpResponseBuffer)) {
                    strncpy(httpResponseBuffer, response.c_str(), sizeof(httpResponseBuffer) - 1);
                    httpResponseBuffer[sizeof(httpResponseBuffer) - 1] = '\0';
                    Serial_printf(F("[Notify] Ntfy response: %s\n"), httpResponseBuffer);
                }
            }
        }
    }
    
    http.end();
    
    bool result = (code == 200 || code == 201);
    if (result)
    {
        Serial_printf(F("[Notify] Ntfy bericht succesvol verstuurd! (code: %d)\n"), code);
    }
    else
    {
        Serial_printf(F("[Notify] Ntfy fout bij versturen (code: %d)\n"), code);
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
// ============================================================================
// Helper Functions
// ============================================================================

// Helper: Validate if price is valid (not NaN, Inf, or <= 0)
static bool isValidPrice(float price)
{
    return !isnan(price) && !isinf(price) && price > 0.0f;
}

// Helper: Validate if two prices are valid
// Fase 4.2.8: static verwijderd zodat PriceData.cpp deze functie kan aanroepen
bool areValidPrices(float price1, float price2)
{
    return isValidPrice(price1) && isValidPrice(price2);
}

// Helper: Safe atof() with NaN/Inf validation
// Returns true if conversion successful and value is valid, false otherwise
// Output parameter 'out' is only set if conversion is successful
static bool safeAtof(const char* str, float& out)
{
    if (str == nullptr || strlen(str) == 0) {
        return false;
    }
    
    float val = atof(str);
    
    // Check for NaN or Inf
    if (isnan(val) || isinf(val)) {
        Serial_printf(F("[Validation] Invalid float value (NaN/Inf): %s\n"), str);
        return false;
    }
    
    out = val;
    return true;
}

// Helper: Safe string copy with guaranteed null termination
static void safeStrncpy(char *dest, const char *src, size_t destSize)
{
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

// Deadlock detection: Track mutex hold times
static unsigned long mutexTakeTime = 0;
static const char* mutexHolderContext = nullptr;
static const unsigned long MAX_MUTEX_HOLD_TIME_MS = 2000; // Max 2 seconden hold time (deadlock threshold)

// Helper: Safe mutex take with deadlock detection
// Returns true on success, false on failure
static bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* context)
{
    if (mutex == nullptr) {
        Serial_printf(F("[Mutex] ERROR: Attempt to take nullptr mutex in %s\n"), context);
        return false;
    }
    
    // Check if mutex is already held for too long (potential deadlock)
    if (mutexHolderContext != nullptr && mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf(F("[Mutex] WARNING: Potential deadlock detected! Mutex held for %lu ms by %s\n"), 
                         holdTime, mutexHolderContext);
        }
    }
    
    BaseType_t result = xSemaphoreTake(mutex, timeout);
    if (result == pdTRUE) {
        mutexTakeTime = millis();
        mutexHolderContext = context;
        return true;
    }
    
    return false;
}

// Helper: Safe mutex give with error handling and deadlock detection
// Returns true on success, false on failure
static bool safeMutexGive(SemaphoreHandle_t mutex, const char* context)
{
    if (mutex == nullptr) {
        Serial_printf(F("[Mutex] ERROR: Attempt to give nullptr mutex in %s\n"), context);
        return false;
    }
    
    // Check if mutex was held for too long (potential deadlock)
    if (mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf(F("[Mutex] WARNING: Mutex held for %lu ms by %s (potential deadlock)\n"), 
                         holdTime, mutexHolderContext ? mutexHolderContext : "unknown");
        }
    }
    
    BaseType_t result = xSemaphoreGive(mutex);
    if (result != pdTRUE) {
        Serial_printf(F("[Mutex] ERROR: xSemaphoreGive failed in %s (result=%d)\n"), context, result);
        // Note: This could indicate a mutex leak or double-release
        return false;
    }
    
    // Reset tracking
    mutexTakeTime = 0;
    mutexHolderContext = nullptr;
    
    return true;
}

// Forward declarations
static void findMinMaxInSecondPrices(float &minVal, float &maxVal);
static void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
static void checkHeapTelemetry();

// ============================================================================
// Utility Functions
// ============================================================================

static void formatIPAddress(IPAddress ip, char *buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// ============================================================================
// Heap Telemetry Functions
// ============================================================================

// Log heap telemetry (compacte regel)
// context: optionele context string (bijv. "warm-start", "http", "lvgl")
static void logHeapTelemetry(const char* context = nullptr)
{
    uint32_t freeHeap = ESP.getFreeHeap();
    size_t largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t freeSize8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    
    // Update low watermark
    if (freeHeap < heapLowWatermark) {
        heapLowWatermark = freeHeap;
    }
    
    // PSRAM check
    bool hasPSRAM = (ESP.getPsramSize() > 0);
    size_t freeSizePSRAM = 0;
    if (hasPSRAM) {
        freeSizePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }
    
    // Compacte log regel
    if (context != nullptr) {
        if (hasPSRAM) {
            Serial_printf(F("[Heap] %s: free=%u largest=%u 8bit=%u PSRAM=%u low=%u\n"),
                         context, freeHeap, largestFreeBlock, freeSize8bit, freeSizePSRAM, heapLowWatermark);
        } else {
            Serial_printf(F("[Heap] %s: free=%u largest=%u 8bit=%u low=%u\n"),
                         context, freeHeap, largestFreeBlock, freeSize8bit, heapLowWatermark);
        }
    } else {
        if (hasPSRAM) {
            Serial_printf(F("[Heap] free=%u largest=%u 8bit=%u PSRAM=%u low=%u\n"),
                         freeHeap, largestFreeBlock, freeSize8bit, freeSizePSRAM, heapLowWatermark);
        } else {
            Serial_printf(F("[Heap] free=%u largest=%u 8bit=%u low=%u\n"),
                         freeHeap, largestFreeBlock, freeSize8bit, heapLowWatermark);
        }
    }
}

// Periodic heap telemetry (elke 60 seconden)
static void checkHeapTelemetry()
{
    unsigned long now = millis();
    if (now - lastHeapTelemetryLog >= HEAP_TELEMETRY_INTERVAL_MS) {
        logHeapTelemetry();
        lastHeapTelemetryLog = now;
    }
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
            
            Serial_printf(F("[Trend] Trend change notificatie verzonden: %s → %s (2h: %.2f%%, 30m: %.2f%%, Vol: %s)\n"), 
                         fromTrend, toTrend, ret_2h, ret_30m_value, volText);
        }
        
        // Update previous trend state
        previousTrendState = trendState;
    }
}

// ============================================================================
// Anchor Price Functions
// ============================================================================

// Bereken effectieve anchor-waarden op basis van trend
// Basiswaarden worden aangepast:
// - TREND_UP: meer ruimte voor verlies (1.25x), meer winst (1.10x)
// Bereken effectieve anchor thresholds op basis van trend (alleen als trend-adaptive enabled is)
// - TREND_UP: configureerbare multipliers (default: maxLoss * 1.15, takeProfit * 1.2)
// - TREND_DOWN: configureerbare multipliers (default: maxLoss * 0.85, takeProfit * 0.8)
// - TREND_SIDEWAYS: basiswaarden (geen aanpassing)
static AnchorConfigEffective calculateEffectiveAnchorThresholds(TrendState trend, float baseMaxLoss, float baseTakeProfit)
{
    AnchorConfigEffective eff;
    
    // Als trend-adaptive uit staat, gebruik basiswaarden
    if (!trendAdaptiveAnchorsEnabled) {
        eff.maxLossPct = baseMaxLoss;
        eff.takeProfitPct = baseTakeProfit;
        return eff;
    }
    
    // Pas multipliers toe op basis van trend
    switch (trend) {
        case TREND_UP:
            eff.maxLossPct = baseMaxLoss * uptrendMaxLossMultiplier;
            eff.takeProfitPct = baseTakeProfit * uptrendTakeProfitMultiplier;
            break;
            
        case TREND_DOWN:
            eff.maxLossPct = baseMaxLoss * downtrendMaxLossMultiplier;
            eff.takeProfitPct = baseTakeProfit * downtrendTakeProfitMultiplier;
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

// Legacy functie voor backward compatibility (gebruikt nu calculateEffectiveAnchorThresholds)
static AnchorConfigEffective calcEffectiveAnchor(float baseMaxLoss, float baseTakeProfit, TrendState trend)
{
    return calculateEffectiveAnchorThresholds(trend, baseMaxLoss, baseTakeProfit);
}

// Publiceer anchor event naar MQTT
// Helper functie om anchor in te stellen (thread-safe)
// anchorValue: de waarde om in te stellen (0.0 = gebruik huidige prijs)
// shouldUpdateUI: true = update UI direct (alleen vanuit main loop thread), false = skip UI update (voor web/MQTT threads)
// skipNotifications: true = skip NTFY en MQTT (voor web server thread om crashes te voorkomen), false = stuur notificaties
// returns: true als succesvol, false als mislukt
static bool setAnchorPrice(float anchorValue = 0.0f, bool shouldUpdateUI = true, bool skipNotifications = false) {
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
        anchorPrice = priceToSet;
        openPrices[0] = priceToSet;
        anchorMax = priceToSet;  // Initialiseer max/min met anchor prijs
        anchorMin = priceToSet;
        anchorTime = millis();
        anchorActive = true;
        anchorTakeProfitSent = false;
        anchorMaxLossSent = false;
        Serial_printf(F("[Anchor] Anchor set: anchorPrice = %.2f\n"), anchorPrice);
        
        safeMutexGive(dataMutex, "setAnchorPrice");
        
        // Publiceer anchor event naar MQTT en stuur notificatie alleen als niet overgeslagen
        // Doe dit BUITEN de mutex om blocking operaties te voorkomen
        if (!skipNotifications) {
            publishMqttAnchorEvent(anchorPrice, "anchor_set");
            
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
        if (shouldUpdateUI) {
            updateUI();
        }
        
        return true;
    }
    Serial_println("[Anchor] WARN: Mutex timeout bij setAnchorPrice");
    return false;
}

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
    
    // Try direct publish, queue if failed
    if (mqttConnected && mqttClient.publish(topic, payload, false)) {
        Serial_printf(F("[MQTT] Anchor event gepubliceerd: %s (prijs: %.2f, event: %s)\n"), 
                     timeStr, anchor_price, event_type);
    } else {
        // Queue message if not connected or publish failed
        enqueueMqttMessage(topic, payload, false);
        Serial_printf(F("[MQTT] Anchor event in queue: %s (prijs: %.2f, event: %s)\n"), 
                     timeStr, anchor_price, event_type);
    }
}

// ============================================================================
// Auto-Volatility Mode: VolatilityAdjuster Module
// ============================================================================

// Bereken standaarddeviatie van 1m returns in het sliding window
static float calculateStdDev1mReturns()
{
    if (!volatility1mArrayFilled && volatility1mIndex == 0) {
        return 0.0f;  // Geen data beschikbaar
    }
    
    // Gebruik de geconfigureerde window size, maar clamp naar array grootte
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    
    // Validatie: window size moet minimaal 1 zijn
    if (windowSize == 0) {
        return 0.0f;
    }
    
    uint8_t count = volatility1mArrayFilled ? windowSize : volatility1mIndex;
    
    if (count < 2) {
        return 0.0f;  // Minimaal 2 samples nodig voor std dev
    }
    
    // Bereken gemiddelde
    float sum = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        sum += volatility1mReturns[i];
    }
    float mean = sum / count;
    
    // Bereken variantie
    float variance = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float diff = volatility1mReturns[i] - mean;
        variance += diff * diff;
    }
    variance /= (count - 1);  // Sample variance (n-1)
    
    // Standaarddeviatie
    return sqrtf(variance);
}

// Update sliding window met nieuwe 1m return
static void updateVolatilityWindow(float ret_1m)
{
    if (!autoVolatilityEnabled) return;
    
    // Gebruik de geconfigureerde window size, maar clamp naar array grootte
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    
    // Voeg nieuwe return toe aan circulaire buffer
    volatility1mReturns[volatility1mIndex] = ret_1m;
    volatility1mIndex++;
    
    if (volatility1mIndex >= windowSize) {
        volatility1mIndex = 0;
        volatility1mArrayFilled = true;
    }
}

// Bereken volatility factor en effective thresholds
static EffectiveThresholds calculateEffectiveThresholds(float baseSpike1m, float baseMove5m, float baseMove30m)
{
    EffectiveThresholds eff;
    eff.volFactor = 1.0f;
    eff.stdDev = 0.0f;
    
    if (!autoVolatilityEnabled) {
        // Als disabled, gebruik basiswaarden
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    // Bereken standaarddeviatie
    eff.stdDev = calculateStdDev1mReturns();
    
    // Als er onvoldoende data is, gebruik volFactor = 1.0
    // Minimaal 10 samples nodig voor betrouwbare berekening
    uint8_t windowSize = (autoVolatilityWindowMinutes > MAX_VOLATILITY_WINDOW_SIZE) ? MAX_VOLATILITY_WINDOW_SIZE : autoVolatilityWindowMinutes;
    uint8_t minSamples = (windowSize < 10) ? windowSize : 10;
    
    if (eff.stdDev <= 0.0f || (!volatility1mArrayFilled && volatility1mIndex < minSamples)) {
        eff.volFactor = 1.0f;
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    // Bereken volatility factor
    // Validatie: voorkom deling door nul
    if (autoVolatilityBaseline1mStdPct <= 0.0f) {
        eff.volFactor = 1.0f;
        eff.spike1m = baseSpike1m;
        eff.move5m = baseMove5m;
        eff.move30m = baseMove30m;
        return eff;
    }
    
    float rawVolFactor = eff.stdDev / autoVolatilityBaseline1mStdPct;
    
    // Clamp tussen min en max (validatie)
    eff.volFactor = rawVolFactor;
    if (eff.volFactor < autoVolatilityMinMultiplier) {
        eff.volFactor = autoVolatilityMinMultiplier;
    }
    if (eff.volFactor > autoVolatilityMaxMultiplier) {
        eff.volFactor = autoVolatilityMaxMultiplier;
    }
    
    // Validatie: voorkom negatieve of nul thresholds
    if (eff.volFactor <= 0.0f) {
        eff.volFactor = 1.0f;
    }
    
    // Update globale volFactor voor logging
    currentVolFactor = eff.volFactor;
    
    // Bereken effective thresholds
    eff.spike1m = baseSpike1m * eff.volFactor;
    eff.move5m = baseMove5m * sqrtf(eff.volFactor);  // sqrt voor langere timeframes
    eff.move30m = baseMove30m * sqrtf(eff.volFactor);
    
    // Validatie: voorkom negatieve thresholds (safety check)
    if (eff.spike1m < 0.0f) eff.spike1m = baseSpike1m;
    if (eff.move5m < 0.0f) eff.move5m = baseMove5m;
    if (eff.move30m < 0.0f) eff.move30m = baseMove30m;
    
    return eff;
}

// Log volatility status (voor debug)
static void logVolatilityStatus(const EffectiveThresholds& eff)
{
    if (!autoVolatilityEnabled) return;
    
    unsigned long now = millis();
    if (lastVolatilityLog > 0 && (now - lastVolatilityLog) < VOLATILITY_LOG_INTERVAL_MS) {
        return;  // Nog niet tijd voor volgende log
    }
    
    Serial_printf(F("[Volatility] σ=%.4f%%, volFactor=%.3f, thresholds: 1m=%.3f%%, 5m=%.3f%%, 30m=%.3f%%\n"),
                  eff.stdDev, eff.volFactor, eff.spike1m, eff.move5m, eff.move30m);
    lastVolatilityLog = now;
}

// ============================================================================
// Smart Confluence Mode: ConfluenceDetector Module
// ============================================================================

// Helper: Check if two events are within confluence time window
static bool eventsWithinTimeWindow(unsigned long timestamp1, unsigned long timestamp2, unsigned long now)
{
    if (timestamp1 == 0 || timestamp2 == 0) return false;
    unsigned long timeDiff = (timestamp1 > timestamp2) ? (timestamp1 - timestamp2) : (timestamp2 - timestamp1);
    return (timeDiff <= CONFLUENCE_TIME_WINDOW_MS);
}

// Helper: Check if 30m trend supports the direction (UP/DOWN)
static bool trendSupportsDirection(EventDirection direction)
{
    if (direction == EVENT_UP) {
        // UP-confluence: 30m trend moet UP zijn of op zijn minst niet sterk DOWN
        return (trendState == TREND_UP || trendState == TREND_SIDEWAYS);
    } else if (direction == EVENT_DOWN) {
        // DOWN-confluence: 30m trend moet DOWN zijn of op zijn minst niet sterk UP
        return (trendState == TREND_DOWN || trendState == TREND_SIDEWAYS);
    }
    return false;
}

// Check for confluence and send combined alert if found
// Returns true if confluence was found and alert sent, false otherwise
static bool checkAndSendConfluenceAlert(unsigned long now, float ret_30m)
{
    if (!smartConfluenceEnabled) return false;
    
    // Check if we have valid 1m and 5m events
    if (last1mEvent.direction == EVENT_NONE || last5mEvent.direction == EVENT_NONE) {
        return false;
    }
    
    // Check if events are already used in confluence
    if (last1mEvent.usedInConfluence || last5mEvent.usedInConfluence) {
        return false;
    }
    
    // Check if events are within time window
    if (!eventsWithinTimeWindow(last1mEvent.timestamp, last5mEvent.timestamp, now)) {
        return false;
    }
    
    // Check if both events are in the same direction
    if (last1mEvent.direction != last5mEvent.direction) {
        return false;
    }
    
    // Check if 30m trend supports the direction
    if (!trendSupportsDirection(last1mEvent.direction)) {
        return false;
    }
    
    // Check cooldown (prevent spam)
    if (lastConfluenceAlert > 0 && (now - lastConfluenceAlert) < CONFLUENCE_TIME_WINDOW_MS) {
        return false;
    }
    
    // Confluence detected! Send combined alert
    EventDirection direction = last1mEvent.direction;
    const char* directionText = (direction == EVENT_UP) ? "UP" : "DOWN";
    const char* trendText = "";
    switch (trendState) {
        case TREND_UP: trendText = "UP"; break;
        case TREND_DOWN: trendText = "DOWN"; break;
        case TREND_SIDEWAYS: trendText = "SIDEWAYS"; break;
    }
    
    char timestamp[32];
    getFormattedTimestamp(timestamp, sizeof(timestamp));
    char title[80];
    snprintf(title, sizeof(title), "%s Confluence Alert (1m+5m+Trend)", binanceSymbol);
    
    char msg[320];
    if (direction == EVENT_UP) {
        snprintf(msg, sizeof(msg),
                 "Confluence %s gedetecteerd!\n\n"
                 "1m: +%.2f%%\n"
                 "5m: +%.2f%%\n"
                 "30m Trend: %s (%.2f%%)\n\n"
                 "Prijs %s: %.2f",
                 directionText,
                 last1mEvent.magnitude,
                 last5mEvent.magnitude,
                 trendText, ret_30m,
                 timestamp, prices[0]);
    } else {
        snprintf(msg, sizeof(msg),
                 "Confluence %s gedetecteerd!\n\n"
                 "1m: %.2f%%\n"
                 "5m: %.2f%%\n"
                 "30m Trend: %s (%.2f%%)\n\n"
                 "Prijs %s: %.2f",
                 directionText,
                 -last1mEvent.magnitude,
                 -last5mEvent.magnitude,
                 trendText, ret_30m,
                 timestamp, prices[0]);
    }
    
    const char* colorTag = (direction == EVENT_UP) ? "green_square,📈" : "red_square,📉";
    sendNotification(title, msg, colorTag);
    
    // Mark events as used
    last1mEvent.usedInConfluence = true;
    last5mEvent.usedInConfluence = true;
    lastConfluenceAlert = now;
    
    Serial_printf(F("[Confluence] Alert verzonden: 1m=%.2f%%, 5m=%.2f%%, trend=%s, ret_30m=%.2f%%\n"),
                  (direction == EVENT_UP ? last1mEvent.magnitude : -last1mEvent.magnitude),
                  (direction == EVENT_UP ? last5mEvent.magnitude : -last5mEvent.magnitude),
                  trendText, ret_30m);
    
    return true;
}

// Update 1m event state (gebruikt effective threshold voor consistentie)
static void update1mEvent(float ret_1m, unsigned long timestamp, float effectiveSpike1mThreshold)
{
    if (!smartConfluenceEnabled) return;
    
    float absRet1m = fabsf(ret_1m);
    if (absRet1m >= effectiveSpike1mThreshold) {
        last1mEvent.direction = (ret_1m > 0) ? EVENT_UP : EVENT_DOWN;
        last1mEvent.timestamp = timestamp;
        last1mEvent.magnitude = absRet1m;
        last1mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Update 5m event state (gebruikt effective threshold voor consistentie)
static void update5mEvent(float ret_5m, unsigned long timestamp, float effectiveMove5mThreshold)
{
    if (!smartConfluenceEnabled) return;
    
    float absRet5m = fabsf(ret_5m);
    // Check for 5m move alert threshold (effective threshold)
    if (absRet5m >= effectiveMove5mThreshold) {
        last5mEvent.direction = (ret_5m > 0) ? EVENT_UP : EVENT_DOWN;
        last5mEvent.timestamp = timestamp;
        last5mEvent.magnitude = absRet5m;
        last5mEvent.usedInConfluence = false;  // Reset flag when new event occurs
    }
}

// Check anchor take profit / max loss alerts
static void checkAnchorAlerts()
{
    if (!anchorActive || !isValidPrice(anchorPrice) || !isValidPrice(prices[0])) {
        return; // Geen actieve anchor of geen prijs data
    }
    
    // Bereken dynamische anchor-waarden op basis van trend
    AnchorConfigEffective effAnchor = calculateEffectiveAnchorThresholds(trendState, anchorMaxLoss, anchorTakeProfit);
    
    // Bereken percentage verandering t.o.v. anchor
    float anchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
    
    // Helper: get trend name
    const char* trendName = "";
    switch (trendState) {
        case TREND_UP: trendName = "UP"; break;
        case TREND_DOWN: trendName = "DOWN"; break;
        case TREND_SIDEWAYS: trendName = "SIDEWAYS"; break;
    }
    
    // Check take profit met dynamische waarde
    if (!anchorTakeProfitSent && anchorPct >= effAnchor.takeProfitPct) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[320];
        snprintf(title, sizeof(title), "%s Take Profit", binanceSymbol);
        
        // Toon trend en effective thresholds in notificatie
        if (trendAdaptiveAnchorsEnabled) {
            snprintf(msg, sizeof(msg), 
                     "Take profit bereikt: +%.2f%%\nTrend: %s, Threshold (eff.): +%.2f%% (basis: +%.2f%%)\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nWinst: +%.2f EUR",
                     anchorPct, trendName, effAnchor.takeProfitPct, anchorTakeProfit, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        } else {
            snprintf(msg, sizeof(msg), 
                     "Take profit bereikt: +%.2f%%\nThreshold: +%.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nWinst: +%.2f EUR",
                     anchorPct, effAnchor.takeProfitPct, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        }
        sendNotification(title, msg, "green_square,💰");
        anchorTakeProfitSent = true;
        Serial_printf(F("[Anchor] Take profit notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n"), 
                     anchorPct, effAnchor.takeProfitPct, anchorTakeProfit, trendName, anchorPrice, prices[0]);
        
        // Publiceer take profit event naar MQTT
        publishMqttAnchorEvent(anchorPrice, "take_profit");
    }
    
    // Check max loss met dynamische waarde
    if (!anchorMaxLossSent && anchorPct <= effAnchor.maxLossPct) {
        char timestamp[32];
        getFormattedTimestamp(timestamp, sizeof(timestamp));
        char title[64];
        char msg[320];
        snprintf(title, sizeof(title), "%s Max Loss", binanceSymbol);
        
        // Toon trend en effective thresholds in notificatie
        if (trendAdaptiveAnchorsEnabled) {
            snprintf(msg, sizeof(msg), 
                     "Max loss bereikt: %.2f%%\nTrend: %s, Threshold (eff.): %.2f%% (basis: %.2f%%)\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nVerlies: %.2f EUR",
                     anchorPct, trendName, effAnchor.maxLossPct, anchorMaxLoss, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        } else {
            snprintf(msg, sizeof(msg), 
                     "Max loss bereikt: %.2f%%\nThreshold: %.2f%%\nAnchor: %.2f EUR\nPrijs %s: %.2f EUR\nVerlies: %.2f EUR",
                     anchorPct, effAnchor.maxLossPct, anchorPrice, timestamp, prices[0], prices[0] - anchorPrice);
        }
        sendNotification(title, msg, "red_square,⚠️");
        anchorMaxLossSent = true;
        Serial_printf(F("[Anchor] Max loss notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n"), 
                     anchorPct, effAnchor.maxLossPct, anchorMaxLoss, trendName, anchorPrice, prices[0]);
        
        // Publiceer max loss event naar MQTT
        publishMqttAnchorEvent(anchorPrice, "max_loss");
    }
}

// Check thresholds and send notifications if needed
// ret_1m: percentage verandering laatste 1 minuut
// ret_5m: percentage verandering laatste 5 minuten (voor filtering)
// ret_30m: percentage verandering laatste 30 minuten
// Helper: Check if cooldown has passed and hourly limit is OK
static bool checkAlertConditions(unsigned long now, unsigned long& lastNotification, unsigned long cooldownMs, 
                                  uint8_t& alertsThisHour, uint8_t maxAlertsPerHour, const char* alertType)
{
    bool cooldownPassed = (lastNotification == 0 || (now - lastNotification >= cooldownMs));
    bool hourlyLimitOk = (alertsThisHour < maxAlertsPerHour);
    
    if (!hourlyLimitOk) {
        Serial_printf("[Notify] %s gedetecteerd maar max alerts per uur bereikt (%d/%d)\n", 
                     alertType, alertsThisHour, maxAlertsPerHour);
    }
    
    return cooldownPassed && hourlyLimitOk;
}

// Helper: Determine color tag based on return value and threshold
static const char* determineColorTag(float ret, float threshold, float strongThreshold)
{
    float absRet = fabsf(ret);
    if (ret > 0) {
        // Stijging: blauw voor normale (🔼), paars voor strong threshold (⏫️)
        return (absRet >= strongThreshold) ? "purple_square,⏫️" : "blue_square,🔼";
    } else {
        // Daling: oranje voor normale (🔽), rood voor strong threshold (⏬️)
        return (absRet >= strongThreshold) ? "red_square,⏬️" : "orange_square,🔽";
    }
}

// Helper: Format notification message with timestamp, price, and min/max
static void formatNotificationMessage(char* msg, size_t msgSize, float ret, const char* direction, 
                                       float minVal, float maxVal)
{
    char timestamp[32];
    getFormattedTimestamp(timestamp, sizeof(timestamp));
    
    if (ret >= 0) {
        snprintf(msg, msgSize, 
                "%s UP %s: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                direction, direction, ret, timestamp, prices[0], maxVal, minVal);
    } else {
        snprintf(msg, msgSize, 
                "%s DOWN %s: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                direction, direction, ret, timestamp, prices[0], maxVal, minVal);
    }
}

// Helper: Send alert notification with all checks
static bool sendAlertNotification(float ret, float threshold, float strongThreshold, 
                                   unsigned long now, unsigned long& lastNotification, 
                                   unsigned long cooldownMs, uint8_t& alertsThisHour, 
                                   uint8_t maxAlertsPerHour, const char* alertType, 
                                   const char* direction, float minVal, float maxVal)
{
    if (!checkAlertConditions(now, lastNotification, cooldownMs, alertsThisHour, maxAlertsPerHour, alertType)) {
        return false;
    }
    
    char msg[256];
    formatNotificationMessage(msg, sizeof(msg), ret, direction, minVal, maxVal);
    
    const char* colorTag = determineColorTag(ret, threshold, strongThreshold);
    
    char title[64];
    snprintf(title, sizeof(title), "%s %s Alert", binanceSymbol, alertType);
    
    sendNotification(title, msg, colorTag);
    lastNotification = now;
    alertsThisHour++;
        Serial_printf(F("[Notify] %s notificatie verstuurd (%d/%d dit uur)\n"), alertType, alertsThisHour, maxAlertsPerHour);
    
    return true;
}

static void checkAndNotify(float ret_1m, float ret_5m, float ret_30m)
{
    unsigned long now = millis();
    
    // Update volatility window met nieuwe 1m return (Auto-Volatility Mode)
    if (ret_1m != 0.0f) {
        updateVolatilityWindow(ret_1m);
    }
    
    // Bereken effective thresholds (Auto-Volatility Mode)
    EffectiveThresholds effThresh = calculateEffectiveThresholds(spike1mThreshold, move5mAlertThreshold, move30mThreshold);
    
    // Log volatility status (voor debug)
    logVolatilityStatus(effThresh);
    
    // Reset tellers elk uur
    if (hourStartTime == 0 || (now - hourStartTime >= 3600000UL)) { // 1 uur = 3600000 ms
        alerts1MinThisHour = 0;
        alerts30MinThisHour = 0;
        alerts5MinThisHour = 0;
        hourStartTime = now;
        Serial_printf(F("[Notify] Uur-tellers gereset\n"));
    }
    
    // ===== 1-MINUUT SPIKE ALERT =====
    // Voorwaarde: |ret_1m| >= effectiveSpike1mThreshold EN |ret_5m| >= spike5mThreshold in dezelfde richting
    if (ret_1m != 0.0f && ret_5m != 0.0f)
    {
        float absRet1m = fabsf(ret_1m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn (beide positief of beide negatief)
        bool sameDirection = ((ret_1m > 0 && ret_5m > 0) || (ret_1m < 0 && ret_5m < 0));
        
        // Threshold check: ret_1m >= effectiveSpike1mThreshold EN ret_5m >= spike5mThreshold
        bool spikeDetected = (absRet1m >= effThresh.spike1m) && (absRet5m >= spike5mThreshold) && sameDirection;
        
        // Update 1m event state voor Smart Confluence Mode
        if (spikeDetected) {
            update1mEvent(ret_1m, now, effThresh.spike1m);
        }
        
        // Debug logging alleen bij spike detectie
        if (spikeDetected) {
            Serial_printf(F("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%\n"), ret_1m, ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf(F("[Notify] 1m spike onderdrukt (gebruikt in confluence alert)\n"));
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                    Serial_printf(F("[Notify] 1m spike onderdrukt (al gebruikt in confluence)\n"));
                } else {
                    // Bereken min en max uit secondPrices buffer
                    float minVal, maxVal;
                    findMinMaxInSecondPrices(minVal, maxVal);
                    
                    // Format message with 5m info
                    char timestamp[32];
                    getFormattedTimestamp(timestamp, sizeof(timestamp));
                    char msg[256];
                    if (ret_1m >= 0) {
                        snprintf(msg, sizeof(msg), 
                                 "1m UP spike: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
                    } else {
                        snprintf(msg, sizeof(msg), 
                                 "1m DOWN spike: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_1m, ret_5m, timestamp, prices[0], maxVal, minVal);
                    }
                    
                    const char* colorTag = determineColorTag(ret_1m, effThresh.spike1m, effThresh.spike1m * 1.5f);
                    char title[64];
                    snprintf(title, sizeof(title), "%s 1m Spike Alert", binanceSymbol);
                    
                    if (checkAlertConditions(now, lastNotification1Min, notificationCooldown1MinMs, 
                                             alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR, "1m spike")) {
                        sendNotification(title, msg, colorTag);
                        lastNotification1Min = now;
                        alerts1MinThisHour++;
                        Serial_printf(F("[Notify] 1m spike notificatie verstuurd (%d/%d dit uur)\n"), alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
                    }
                }
            }
        }
    }
    
    // ===== 30-MINUTEN TREND MOVE ALERT =====
    // Voorwaarde: |ret_30m| >= effectiveMove30mThreshold EN |ret_5m| >= move5mThreshold in dezelfde richting
    if (ret_30m != 0.0f && ret_5m != 0.0f)
    {
        float absRet30m = fabsf(ret_30m);
        float absRet5m = fabsf(ret_5m);
        
        // Check of beide in dezelfde richting zijn
        bool sameDirection = ((ret_30m > 0 && ret_5m > 0) || (ret_30m < 0 && ret_5m < 0));
        
        // Threshold check: ret_30m >= effectiveMove30mThreshold EN ret_5m >= move5mThreshold
        // Note: move5mThreshold is de filter threshold, niet de alert threshold
        bool moveDetected = (absRet30m >= effThresh.move30m) && (absRet5m >= move5mThreshold) && sameDirection;
        
        // Debug logging alleen bij move detectie
        if (moveDetected) {
            Serial_printf(F("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%\n"), ret_30m, ret_5m);
            
            // Bereken min en max uit laatste 30 minuten van minuteAverages buffer
            float minVal, maxVal;
            findMinMaxInLast30Minutes(minVal, maxVal);
            
            // Format message with 5m info
            char timestamp[32];
            getFormattedTimestamp(timestamp, sizeof(timestamp));
            char msg[256];
            if (ret_30m >= 0) {
                snprintf(msg, sizeof(msg), 
                         "30m UP move: +%.2f%% (5m: +%.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            } else {
                snprintf(msg, sizeof(msg), 
                         "30m DOWN move: %.2f%% (5m: %.2f%%)\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                         ret_30m, ret_5m, timestamp, prices[0], maxVal, minVal);
            }
            
            const char* colorTag = determineColorTag(ret_30m, effThresh.move30m, effThresh.move30m * 1.5f);
            char title[64];
            snprintf(title, sizeof(title), "%s 30m Move Alert", binanceSymbol);
            
            if (checkAlertConditions(now, lastNotification30Min, notificationCooldown30MinMs, 
                                     alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR, "30m move")) {
                sendNotification(title, msg, colorTag);
                lastNotification30Min = now;
                alerts30MinThisHour++;
                Serial_printf(F("[Notify] 30m move notificatie verstuurd (%d/%d dit uur)\n"), alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
            }
        }
    }
    
    // ===== 5-MINUTEN MOVE ALERT =====
    // Voorwaarde: |ret_5m| >= effectiveMove5mThreshold
    if (ret_5m != 0.0f)
    {
        float absRet5m = fabsf(ret_5m);
        
        // Threshold check: ret_5m >= effectiveMove5mThreshold
        bool move5mDetected = (absRet5m >= effThresh.move5m);
        
        // Update 5m event state voor Smart Confluence Mode
        if (move5mDetected) {
            update5mEvent(ret_5m, now, effThresh.move5m);
        }
        
        // Debug logging alleen bij move detectie
        if (move5mDetected) {
            Serial_printf(F("[Notify] 5m move: ret_5m=%.2f%%\n"), ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf(F("[Notify] 5m move onderdrukt (gebruikt in confluence alert)\n"));
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last5mEvent.usedInConfluence) {
                    Serial_printf(F("[Notify] 5m move onderdrukt (al gebruikt in confluence)\n"));
                } else {
                    // Bereken min en max uit fiveMinutePrices buffer
                    float minVal = fiveMinutePrices[0];
                    float maxVal = fiveMinutePrices[0];
                    for (int i = 1; i < SECONDS_PER_5MINUTES; i++) {
                        if (fiveMinutePrices[i] > 0.0f) {
                            if (fiveMinutePrices[i] < minVal || minVal <= 0.0f) minVal = fiveMinutePrices[i];
                            if (fiveMinutePrices[i] > maxVal || maxVal <= 0.0f) maxVal = fiveMinutePrices[i];
                        }
                    }
                    
                    // Format message
                    char timestamp[32];
                    getFormattedTimestamp(timestamp, sizeof(timestamp));
                    char msg[256];
                    if (ret_5m >= 0) {
                        snprintf(msg, sizeof(msg), 
                                 "5m UP move: +%.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_5m, timestamp, prices[0], maxVal, minVal);
                    } else {
                        snprintf(msg, sizeof(msg), 
                                 "5m DOWN move: %.2f%%\nPrijs %s: %.2f\nTop: %.2f Dal: %.2f", 
                                 ret_5m, timestamp, prices[0], maxVal, minVal);
                    }
                    
                    const char* colorTag = determineColorTag(ret_5m, effThresh.move5m, effThresh.move5m * 1.5f);
                    char title[64];
                    snprintf(title, sizeof(title), "%s 5m Move Alert", binanceSymbol);
                    
                    if (checkAlertConditions(now, lastNotification5Min, notificationCooldown5MinMs, 
                                             alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR, "5m move")) {
                        sendNotification(title, msg, colorTag);
                        lastNotification5Min = now;
                        alerts5MinThisHour++;
                        Serial_printf(F("[Notify] 5m move notificatie verstuurd (%d/%d dit uur)\n"), alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
                    }
                }
            }
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

// Generate default NTFY topic with ESP32 device ID
// NOTE: getESP32DeviceId is nu verplaatst naar SettingsStore module
// Format: [ESP32-ID]-alert
// Example: 9MK28H3Q-alert (8 characters using Crockford Base32 encoding for safe, unique ID)
// Geoptimaliseerd: gebruik char array i.p.v. String
// NOTE: Deze functie is nu een wrapper voor SettingsStore::generateDefaultNtfyTopic
static void generateDefaultNtfyTopic(char* buffer, size_t bufferSize) {
    SettingsStore::generateDefaultNtfyTopic(buffer, bufferSize);
}

// Helper: Generate MQTT device ID with prefix (char array version - voorkomt String gebruik)
// Format: [PREFIX]_[ESP32-ID-HEX]
static void getMqttDeviceId(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) return;
    
    // Generate device ID from MAC address (lower 32 bits as HEX)
    uint32_t macLower = (uint32_t)ESP.getEfuseMac();
    snprintf(buffer, bufferSize, "%s_%08x", MQTT_TOPIC_PREFIX, macLower);
}

// Helper: Extract device ID from topic (char array version - voorkomt String gebruik)
// If topic format is [ESP32-ID]-alert, extracts the ESP32-ID
// Falls back to showing first part before any dash if format is different
static void getDeviceIdFromTopic(const char* topic, char* buffer, size_t bufferSize) {
    if (topic == nullptr || buffer == nullptr || bufferSize == 0) {
        if (buffer && bufferSize > 0) buffer[0] = '\0';
        return;
    }
    
    // Look for "-alert" at the end
    const char* alertPos = strstr(topic, "-alert");
    if (alertPos != nullptr) {
        // Extract everything before "-alert"
        size_t len = alertPos - topic;
        if (len > 0 && len < bufferSize) {
            strncpy(buffer, topic, len);
            buffer[len] = '\0';
            return;
        }
    }
    
    // Fallback: use first part before any dash (for backwards compatibility)
    const char* dashPos = strchr(topic, '-');
    if (dashPos != nullptr) {
        size_t len = dashPos - topic;
        if (len > 0 && len < bufferSize) {
            strncpy(buffer, topic, len);
            buffer[len] = '\0';
            return;
        }
    }
    
    // Last resort: use whole topic (limited to bufferSize-1)
    strncpy(buffer, topic, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}

// Load settings from Preferences
// ============================================================================
// Settings Management Functions
// ============================================================================

static void loadSettings()
{
    // Load settings using SettingsStore module
    CryptoMonitorSettings settings = settingsStore.load();
    
    // Copy settings to global variables (backward compatibility)
    safeStrncpy(ntfyTopic, settings.ntfyTopic, sizeof(ntfyTopic));
    safeStrncpy(binanceSymbol, settings.binanceSymbol, sizeof(binanceSymbol));
    // Update symbols array with the loaded binance symbol
    safeStrncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]));
    language = settings.language;
    
    // Copy alert thresholds
    alertThresholds = settings.alertThresholds;
    
    // Copy notification cooldowns
    notificationCooldowns = settings.notificationCooldowns;
    
    // Copy MQTT settings
    safeStrncpy(mqttHost, settings.mqttHost, sizeof(mqttHost));
    mqttPort = settings.mqttPort;
    safeStrncpy(mqttUser, settings.mqttUser, sizeof(mqttUser));
    safeStrncpy(mqttPass, settings.mqttPass, sizeof(mqttPass));
    
    // Copy anchor settings
    anchorTakeProfit = settings.anchorTakeProfit;
    anchorMaxLoss = settings.anchorMaxLoss;
    
    // Copy trend-adaptive anchor settings
    trendAdaptiveAnchorsEnabled = settings.trendAdaptiveAnchorsEnabled;
    uptrendMaxLossMultiplier = settings.uptrendMaxLossMultiplier;
    uptrendTakeProfitMultiplier = settings.uptrendTakeProfitMultiplier;
    downtrendMaxLossMultiplier = settings.downtrendMaxLossMultiplier;
    downtrendTakeProfitMultiplier = settings.downtrendTakeProfitMultiplier;
    
    // Copy Smart Confluence Mode settings
    smartConfluenceEnabled = settings.smartConfluenceEnabled;
    
    // Copy Warm-Start settings
    warmStartEnabled = settings.warmStartEnabled;
    warmStart1mExtraCandles = settings.warmStart1mExtraCandles;
    warmStart5mCandles = settings.warmStart5mCandles;
    warmStart30mCandles = settings.warmStart30mCandles;
    warmStart2hCandles = settings.warmStart2hCandles;
    
    // Copy Auto-Volatility Mode settings
    autoVolatilityEnabled = settings.autoVolatilityEnabled;
    autoVolatilityWindowMinutes = settings.autoVolatilityWindowMinutes;
    autoVolatilityBaseline1mStdPct = settings.autoVolatilityBaseline1mStdPct;
    autoVolatilityMinMultiplier = settings.autoVolatilityMinMultiplier;
    autoVolatilityMaxMultiplier = settings.autoVolatilityMaxMultiplier;
    
    // Copy trend and volatility settings
    trendThreshold = settings.trendThreshold;
    volatilityLowThreshold = settings.volatilityLowThreshold;
    volatilityHighThreshold = settings.volatilityHighThreshold;
    
    Serial_printf(F("[Settings] Loaded: topic=%s, symbol=%s, 1min trend=%.2f/%.2f%%/min, 30min trend=%.2f/%.2f%%/uur, cooldown=%lu/%lu ms\n"),
                  ntfyTopic, binanceSymbol, threshold1MinUp, threshold1MinDown, threshold30MinUp, threshold30MinDown,
                  notificationCooldown1MinMs, notificationCooldown30MinMs);
}

// Save settings to Preferences using SettingsStore
static void saveSettings()
{
    // Create settings struct from global variables
    CryptoMonitorSettings settings;
    
    // Copy basic settings
    safeStrncpy(settings.ntfyTopic, ntfyTopic, sizeof(settings.ntfyTopic));
    safeStrncpy(settings.binanceSymbol, binanceSymbol, sizeof(settings.binanceSymbol));
    settings.language = language;
    
    // Copy alert thresholds
    settings.alertThresholds = alertThresholds;
    
    // Copy notification cooldowns
    settings.notificationCooldowns = notificationCooldowns;
    
    // Copy MQTT settings
    safeStrncpy(settings.mqttHost, mqttHost, sizeof(settings.mqttHost));
    settings.mqttPort = mqttPort;
    safeStrncpy(settings.mqttUser, mqttUser, sizeof(settings.mqttUser));
    safeStrncpy(settings.mqttPass, mqttPass, sizeof(settings.mqttPass));
    
    // Copy anchor settings
    settings.anchorTakeProfit = anchorTakeProfit;
    settings.anchorMaxLoss = anchorMaxLoss;
    
    // Copy trend-adaptive anchor settings
    settings.trendAdaptiveAnchorsEnabled = trendAdaptiveAnchorsEnabled;
    settings.uptrendMaxLossMultiplier = uptrendMaxLossMultiplier;
    settings.uptrendTakeProfitMultiplier = uptrendTakeProfitMultiplier;
    settings.downtrendMaxLossMultiplier = downtrendMaxLossMultiplier;
    settings.downtrendTakeProfitMultiplier = downtrendTakeProfitMultiplier;
    
    // Copy Smart Confluence Mode settings
    settings.smartConfluenceEnabled = smartConfluenceEnabled;
    
    // Copy Warm-Start settings
    settings.warmStartEnabled = warmStartEnabled;
    settings.warmStart1mExtraCandles = warmStart1mExtraCandles;
    settings.warmStart5mCandles = warmStart5mCandles;
    settings.warmStart30mCandles = warmStart30mCandles;
    settings.warmStart2hCandles = warmStart2hCandles;
    
    // Copy Auto-Volatility Mode settings
    settings.autoVolatilityEnabled = autoVolatilityEnabled;
    settings.autoVolatilityWindowMinutes = autoVolatilityWindowMinutes;
    settings.autoVolatilityBaseline1mStdPct = autoVolatilityBaseline1mStdPct;
    settings.autoVolatilityMinMultiplier = autoVolatilityMinMultiplier;
    settings.autoVolatilityMaxMultiplier = autoVolatilityMaxMultiplier;
    
    // Copy trend and volatility settings
    settings.trendThreshold = trendThreshold;
    settings.volatilityLowThreshold = volatilityLowThreshold;
    settings.volatilityHighThreshold = volatilityHighThreshold;
    
    // Save using SettingsStore
    settingsStore.save(settings);
    Serial_println("[Settings] Saved");
}

// Handler functies voor verschillende setting types
static bool handleMqttFloatSetting(const char* value, float* target, float minVal, float maxVal, const char* stateTopic, const char* prefix) {
    float val;
    if (safeAtof(value, val) && val >= minVal && val <= maxVal) {
        *target = val;
        if (stateTopic != nullptr) {
            char topicBuffer[192];
            snprintf(topicBuffer, sizeof(topicBuffer), "%s%s", prefix, stateTopic);
            mqttClient.publish(topicBuffer, value, true);
        }
        return true;
    }
    return false;
}

// Helper: Convert seconds to milliseconds with overflow check
// Returns true on success, false on overflow or invalid input
static bool safeSecondsToMs(int seconds, uint32_t& resultMs)
{
    // Check range first (1-3600 seconds)
    if (seconds < 1 || seconds > 3600) {
        return false;
    }
    
    // Check for overflow: max uint32_t is 4,294,967,295 ms
    // Max safe value: 4,294,967 seconds (4294967 * 1000 = 4,294,967,000 ms)
    // Our max is 3600 seconds, so overflow is not possible, but check anyway for safety
    if (seconds > 4294967) {
        Serial_printf(F("[Overflow] Seconds value too large: %d (max: 4294967)\n"), seconds);
        return false;
    }
    
    // Safe multiplication: seconds * 1000UL
    resultMs = (uint32_t)seconds * 1000UL;
    
    // Verify result is reasonable (should be >= 1000 and <= 3,600,000 for our use case)
    if (resultMs < 1000UL || resultMs > 3600000UL) {
        Serial_printf(F("[Overflow] Invalid result: %lu ms (expected 1000-3600000)\n"), resultMs);
        return false;
    }
    
    return true;
}

static bool handleMqttIntSetting(const char* value, uint32_t* targetMs, int minVal, int maxVal, const char* stateTopic, const char* prefix) {
    int seconds = atoi(value);
    if (seconds >= minVal && seconds <= maxVal) {
        uint32_t resultMs;
        if (!safeSecondsToMs(seconds, resultMs)) {
            Serial_printf(F("[MQTT] Overflow check failed for cooldown: %d seconds\n"), seconds);
            return false;
        }
        *targetMs = resultMs;
        if (stateTopic != nullptr) {
            char topicBuffer[192];
            char valueBuffer[32];
            snprintf(topicBuffer, sizeof(topicBuffer), "%s%s", prefix, stateTopic);
            snprintf(valueBuffer, sizeof(valueBuffer), "%lu", *targetMs / 1000);
            mqttClient.publish(topicBuffer, valueBuffer, true);
        }
        return true;
    }
    return false;
}

static bool handleMqttStringSetting(const char* value, size_t valueLen, char* target, size_t targetSize, bool uppercase, const char* stateTopic, const char* prefix) {
    if (valueLen > 0 && valueLen < targetSize) {
        if (uppercase) {
            // Trim en uppercase
            char processed[64];
            size_t processedLen = 0;
            for (size_t i = 0; i < valueLen && i < sizeof(processed) - 1; i++) {
                char c = value[i];
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                    processed[processedLen++] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
                }
            }
            processed[processedLen] = '\0';
            if (processedLen > 0) {
                safeStrncpy(target, processed, targetSize);
            } else {
                return false;
            }
        } else {
            // Trim alleen
            size_t trimmedLen = valueLen;
            while (trimmedLen > 0 && (value[trimmedLen-1] == ' ' || value[trimmedLen-1] == '\t')) {
                trimmedLen--;
            }
            if (trimmedLen > 0) {
                safeStrncpy(target, value, trimmedLen + 1);
            } else {
                return false;
            }
        }
        
        if (stateTopic != nullptr) {
            char topicBuffer[192];
            snprintf(topicBuffer, sizeof(topicBuffer), "%s%s", prefix, stateTopic);
            mqttClient.publish(topicBuffer, target, true);
        }
        return true;
    }
    return false;
}

// MQTT callback: verwerk instellingen van Home Assistant
// Geoptimaliseerd: gebruik lookup table i.p.v. geneste if-else chain
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
    
    Serial_printf(F("[MQTT] Message: %s => %s\n"), topicBuffer, msgBuffer);
    
    // Helper: maak MQTT topic prefix
    snprintf(prefixBuffer, sizeof(prefixBuffer), "%s", MQTT_TOPIC_PREFIX);
    
    bool settingChanged = false;
    char topicBufferFull[192]; // Voor volledige topic strings
    char valueBuffer[32]; // Voor numerieke waarden
    
    // Lookup table voor MQTT settings - gebruik loop i.p.v. geneste if-else
    // Dit maakt de code veel leesbaarder en makkelijker uitbreidbaar
    struct MqttSetting {
        const char* suffix;
        bool isFloat;
        float minVal;
        float maxVal;
        void* targetVar;
        const char* stateSuffix;
    };
    
    static const MqttSetting floatSettings[] = {
        {"/config/spike1m/set", true, 0.01f, 10.0f, &alertThresholds.spike1m, "/config/spike1m"},
        {"/config/spike5m/set", true, 0.01f, 10.0f, &alertThresholds.spike5m, "/config/spike5m"},
        {"/config/move30m/set", true, 0.01f, 20.0f, &alertThresholds.move30m, "/config/move30m"},
        {"/config/move5m/set", true, 0.01f, 10.0f, &alertThresholds.move5m, "/config/move5m"},
        {"/config/move5mAlert/set", true, 0.01f, 10.0f, &alertThresholds.move5mAlert, "/config/move5mAlert"},
        {"/config/anchorTakeProfit/set", true, 0.1f, 100.0f, &anchorTakeProfit, "/config/anchorTakeProfit"},
        {"/config/anchorMaxLoss/set", true, -100.0f, -0.1f, &anchorMaxLoss, "/config/anchorMaxLoss"},
        {"/config/trendThreshold/set", true, 0.1f, 10.0f, &trendThreshold, "/config/trendThreshold"},
        {"/config/volatilityLowThreshold/set", true, 0.01f, 1.0f, &volatilityLowThreshold, "/config/volatilityLowThreshold"},
        {"/config/volatilityHighThreshold/set", true, 0.01f, 1.0f, &volatilityHighThreshold, "/config/volatilityHighThreshold"}
    };
    
    static const struct {
        const char* suffix;
        uint32_t* targetMs;
        const char* stateSuffix;
    } cooldownSettings[] = {
        {"/config/cooldown1min/set", &notificationCooldowns.cooldown1MinMs, "/config/cooldown1min"},
        {"/config/cooldown30min/set", &notificationCooldowns.cooldown30MinMs, "/config/cooldown30min"},
        {"/config/cooldown5min/set", &notificationCooldowns.cooldown5MinMs, "/config/cooldown5min"}
    };
    
    // Process float settings
    bool handled = false;
    for (size_t i = 0; i < sizeof(floatSettings) / sizeof(floatSettings[0]); i++) {
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, floatSettings[i].suffix);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            float val;
            bool valid = false;
            
            // Special check voor volatilityHighThreshold (moet > volatilityLowThreshold)
            if (strstr(floatSettings[i].suffix, "volatilityHighThreshold") != nullptr) {
                valid = safeAtof(msgBuffer, val) && val >= floatSettings[i].minVal && 
                        val <= floatSettings[i].maxVal && val > volatilityLowThreshold;
            } else {
                valid = safeAtof(msgBuffer, val) && val >= floatSettings[i].minVal && val <= floatSettings[i].maxVal;
            }
            
            if (valid) {
                *((float*)floatSettings[i].targetVar) = val;
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, floatSettings[i].stateSuffix);
                mqttClient.publish(topicBufferFull, msgBuffer, true);
                settingChanged = true;
                handled = true;
            } else {
                Serial_printf(F("[MQTT] Invalid value for %s: %s\n"), floatSettings[i].suffix, msgBuffer);
            }
            break;
        }
    }
    
    // Process cooldown settings (int -> ms conversion)
    if (!handled) {
        for (size_t i = 0; i < sizeof(cooldownSettings) / sizeof(cooldownSettings[0]); i++) {
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, cooldownSettings[i].suffix);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                int seconds = atoi(msgBuffer);
                                if (seconds >= 1 && seconds <= 3600) {
                                    uint32_t resultMs;
                                    if (!safeSecondsToMs(seconds, resultMs)) {
                                        Serial_printf(F("[MQTT] Overflow check failed for cooldown: %d seconds\n"), seconds);
                                        break;
                                    }
                                    *cooldownSettings[i].targetMs = resultMs;
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, cooldownSettings[i].stateSuffix);
                    snprintf(valueBuffer, sizeof(valueBuffer), "%lu", *cooldownSettings[i].targetMs / 1000);
                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                    settingChanged = true;
                    handled = true;
                } else {
                    Serial_printf(F("[MQTT] Invalid cooldown value (range: 1-3600 seconds): %s\n"), msgBuffer);
                }
                break;
            }
        }
    }
    
    // Special cases (niet in lookup table vanwege complexe logica)
    if (!handled) {
        // binanceSymbol - speciale logica (uppercase + symbolsArray update)
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/binanceSymbol/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            if (handleMqttStringSetting(msgBuffer, msgLen, binanceSymbol, sizeof(binanceSymbol), true, "/config/binanceSymbol", prefixBuffer)) {
                safeStrncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]));
                settingChanged = true;
            }
        } else {
            // ntfyTopic - speciale logica (trim)
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/ntfyTopic/set", prefixBuffer);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                if (handleMqttStringSetting(msgBuffer, msgLen, ntfyTopic, sizeof(ntfyTopic), false, "/config/ntfyTopic", prefixBuffer)) {
                    settingChanged = true;
                }
            } else {
                // language - speciale logica (saveSettings call)
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/language/set", prefixBuffer);
                if (strcmp(topicBuffer, topicBufferFull) == 0) {
                    uint8_t newLanguage = atoi(msgBuffer);
                    if (newLanguage == 0 || newLanguage == 1) {
                        language = newLanguage;
                        saveSettings(); // Save language to Preferences
                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/language", prefixBuffer);
                        snprintf(valueBuffer, sizeof(valueBuffer), "%u", language);
                        mqttClient.publish(topicBufferFull, valueBuffer, true);
                        settingChanged = true;
                    }
                } else {
                    // anchorValue/set - speciale logica (queue voor asynchrone verwerking)
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorValue/set", prefixBuffer);
                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                        // Verwerk anchor waarde via MQTT
                        float val = 0.0f;
                        bool useCurrentPrice = false;
                        bool valid = false;
                        
                        // Lege waarde of "current" = gebruik huidige prijs
                        if (strlen(msgBuffer) == 0 || strcmp(msgBuffer, "current") == 0 || 
                            strcmp(msgBuffer, "CURRENT") == 0 || strcmp(msgBuffer, "0") == 0) {
                            useCurrentPrice = true;
                            valid = queueAnchorSetting(0.0f, true);
                            if (valid) {
                                Serial_println("[MQTT] Anchor setting queued: gebruik huidige prijs");
                            }
                        } else if (safeAtof(msgBuffer, val) && val > 0.0f && isValidPrice(val)) {
                            // Valide waarde - zet in queue voor asynchrone verwerking
                            useCurrentPrice = false;
                            valid = queueAnchorSetting(val, false);
                            if (valid) {
                                Serial_printf(F("[MQTT] Anchor setting queued: %.2f\n"), val);
                            }
                        } else {
                            Serial_printf(F("[MQTT] WARN: Ongeldige anchor waarde opgegeven: %s\n"), msgBuffer);
                        }
                        
                        // Publiceer bevestiging terug
                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorValue", prefixBuffer);
                        if (valid) {
                            if (useCurrentPrice) {
                                mqttClient.publish(topicBufferFull, "QUEUED: current price", false);
                            } else {
                                snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", val);
                                mqttClient.publish(topicBufferFull, valueBuffer, false);
                            }
                        } else {
                            mqttClient.publish(topicBufferFull, "ERROR: Invalid value", false);
                        }
                        handled = true;
                    }
                    // button/reset - speciale logica (gebruik queue voor asynchrone verwerking)
                    if (!handled) {
                        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/button/reset/set", prefixBuffer);
                        if (strcmp(topicBuffer, topicBufferFull) == 0) {
                            // Reset button pressed via MQTT - gebruik als anchor (via queue)
                            if (strcmp(msgBuffer, "PRESS") == 0 || strcmp(msgBuffer, "press") == 0 || 
                                strcmp(msgBuffer, "1") == 0 || strcmp(msgBuffer, "ON") == 0 || 
                                strcmp(msgBuffer, "on") == 0) {
                                // Gebruik queue voor asynchrone verwerking (voorkomt crashes)
                                if (queueAnchorSetting(0.0f, true)) {
                                    Serial_println("[MQTT] Reset/Anchor button pressed via MQTT - queued");
                                    handled = true;
                                    // Publish state back (button entities don't need state, but we can acknowledge)
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/button/reset", prefixBuffer);
                                    mqttClient.publish(topicBufferFull, "PRESSED", false);
                                } else {
                                    Serial_println("[MQTT] WARN: Kon anchor setting niet in queue zetten");
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
// MQTT Message Queue functions
static bool enqueueMqttMessage(const char* topic, const char* payload, bool retained) {
    if (mqttQueueCount >= MQTT_QUEUE_SIZE) {
        Serial_printf("[MQTT Queue] Queue vol, bericht verloren: %s\n", topic);
        return false; // Queue vol
    }
    
    MqttMessage* msg = &mqttQueue[mqttQueueTail];
    strncpy(msg->topic, topic, sizeof(msg->topic) - 1);
    msg->topic[sizeof(msg->topic) - 1] = '\0';
    strncpy(msg->payload, payload, sizeof(msg->payload) - 1);
    msg->payload[sizeof(msg->payload) - 1] = '\0';
    msg->retained = retained;
    msg->valid = true;
    
    mqttQueueTail = (mqttQueueTail + 1) % MQTT_QUEUE_SIZE;
    mqttQueueCount++;
    
    return true;
}

static void processMqttQueue() {
    if (!mqttConnected || mqttQueueCount == 0) {
        return;
    }
    
    // Process max 3 messages per call om niet te lang te blokkeren
    uint8_t processed = 0;
    while (mqttQueueCount > 0 && processed < 3) {
        MqttMessage* msg = &mqttQueue[mqttQueueHead];
        if (!msg->valid) {
            mqttQueueHead = (mqttQueueHead + 1) % MQTT_QUEUE_SIZE;
            mqttQueueCount--;
            continue;
        }
        
        bool success = mqttClient.publish(msg->topic, msg->payload, msg->retained);
        if (success) {
            msg->valid = false;
            mqttQueueHead = (mqttQueueHead + 1) % MQTT_QUEUE_SIZE;
            mqttQueueCount--;
            processed++;
        } else {
            // Publish failed, stop processing (will retry next time)
            break;
        }
    }
}

// Helper functies voor MQTT publishing - reduceert code duplicatie
// Gebruikt queue om message loss te voorkomen
static void publishMqttFloat(const char* topicSuffix, float value) {
    char topicBuffer[128];
    char buffer[32];
    dtostrf(value, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", MQTT_TOPIC_PREFIX, topicSuffix);
    
    if (mqttConnected && mqttClient.publish(topicBuffer, buffer, true)) {
        // Direct publish succeeded
        return;
    }
    
    // Queue message if not connected or publish failed
    enqueueMqttMessage(topicBuffer, buffer, true);
}

static void publishMqttUint(const char* topicSuffix, unsigned long value) {
    char topicBuffer[128];
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", MQTT_TOPIC_PREFIX, topicSuffix);
    
    if (mqttConnected && mqttClient.publish(topicBuffer, buffer, true)) {
        // Direct publish succeeded
        return;
    }
    
    // Queue message if not connected or publish failed
    enqueueMqttMessage(topicBuffer, buffer, true);
}

static void publishMqttString(const char* topicSuffix, const char* value) {
    char topicBuffer[128];
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", MQTT_TOPIC_PREFIX, topicSuffix);
    
    if (mqttConnected && mqttClient.publish(topicBuffer, value, true)) {
        // Direct publish succeeded
        return;
    }
    
    // Queue message if not connected or publish failed
    enqueueMqttMessage(topicBuffer, value, true);
}

void publishMqttSettings() {
    // Queue messages even if not connected - they will be sent when connection is restored
    
    // Float settings
    publishMqttFloat("spike1m", spike1mThreshold);
    publishMqttFloat("spike5m", spike5mThreshold);
    publishMqttFloat("move30m", move30mThreshold);
    publishMqttFloat("move5m", move5mThreshold);
    publishMqttFloat("move5mAlert", move5mAlertThreshold);
    publishMqttFloat("anchorTakeProfit", anchorTakeProfit);
    publishMqttFloat("anchorMaxLoss", anchorMaxLoss);
    publishMqttFloat("trendThreshold", trendThreshold);
    publishMqttFloat("volatilityLowThreshold", volatilityLowThreshold);
    publishMqttFloat("volatilityHighThreshold", volatilityHighThreshold);
    
    // Unsigned int settings (cooldowns in seconds)
    publishMqttUint("cooldown1min", notificationCooldown1MinMs / 1000);
    publishMqttUint("cooldown30min", notificationCooldown30MinMs / 1000);
    publishMqttUint("cooldown5min", notificationCooldown5MinMs / 1000);
    publishMqttUint("language", language);
    
    // String settings
    publishMqttString("binanceSymbol", binanceSymbol);
    publishMqttString("ntfyTopic", ntfyTopic);
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
// Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
void publishMqttDiscovery() {
    if (!mqttConnected) return;
    
    // Generate device ID and device JSON string (char arrays)
    char deviceId[64];
    getMqttDeviceId(deviceId, sizeof(deviceId));
    
    char deviceJson[256];
    snprintf(deviceJson, sizeof(deviceJson), 
        "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"JanP\",\"model\":\"%s\"}",
        deviceId, DEVICE_NAME, DEVICE_MODEL);
    
    // Buffers voor topic en payload
    char topicBuffer[128];
    char payloadBuffer[512];
    
    // Discovery berichten met char arrays (geen String concatenatie)
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_spike1m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Spike Threshold\",\"unique_id\":\"%s_spike1m\",\"state_topic\":\"%s/config/spike1m\",\"command_topic\":\"%s/config/spike1m/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_spike5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Spike Filter\",\"unique_id\":\"%s_spike5m\",\"state_topic\":\"%s/config/spike5m\",\"command_topic\":\"%s/config/spike5m/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:filter\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Move Threshold\",\"unique_id\":\"%s_move30m\",\"state_topic\":\"%s/config/move30m\",\"command_topic\":\"%s/config/move30m/set\",\"min\":0.5,\"max\":20.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Move Filter\",\"unique_id\":\"%s_move5m\",\"state_topic\":\"%s/config/move5m\",\"command_topic\":\"%s/config/move5m/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:filter-variant\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move5mAlert/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Move Alert Threshold\",\"unique_id\":\"%s_move5mAlert\",\"state_topic\":\"%s/config/move5mAlert\",\"command_topic\":\"%s/config/move5mAlert/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:alert\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown1min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Cooldown\",\"unique_id\":\"%s_cooldown1min\",\"state_topic\":\"%s/config/cooldown1min\",\"command_topic\":\"%s/config/cooldown1min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown30min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Cooldown\",\"unique_id\":\"%s_cooldown30min\",\"state_topic\":\"%s/config/cooldown30min\",\"command_topic\":\"%s/config/cooldown30min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown5min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Cooldown\",\"unique_id\":\"%s_cooldown5min\",\"state_topic\":\"%s/config/cooldown5min\",\"command_topic\":\"%s/config/cooldown5min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-sand\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/text/%s_binanceSymbol/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Binance Symbol\",\"unique_id\":\"%s_binanceSymbol\",\"state_topic\":\"%s/config/binanceSymbol\",\"command_topic\":\"%s/config/binanceSymbol/set\",\"icon\":\"mdi:currency-btc\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/text/%s_ntfyTopic/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"NTFY Topic\",\"unique_id\":\"%s_ntfyTopic\",\"state_topic\":\"%s/config/ntfyTopic\",\"command_topic\":\"%s/config/ntfyTopic/set\",\"icon\":\"mdi:bell-ring\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_price/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Crypto Price\",\"unique_id\":\"%s_price\",\"state_topic\":\"%s/values/price\",\"unit_of_measurement\":\"EUR\",\"icon\":\"mdi:currency-btc\",\"device_class\":\"monetary\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_1m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Return\",\"unique_id\":\"%s_return_1m\",\"state_topic\":\"%s/values/return_1m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line-variant\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Return\",\"unique_id\":\"%s_return_5m\",\"state_topic\":\"%s/values/return_5m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Return\",\"unique_id\":\"%s_return_30m\",\"state_topic\":\"%s/values/return_30m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Reset button
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/button/%s_reset/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Reset Open Price\",\"unique_id\":\"%s_reset\",\"command_topic\":\"%s/button/reset/set\",\"icon\":\"mdi:restart\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor take profit
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorTakeProfit/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Take Profit\",\"unique_id\":\"%s_anchorTakeProfit\",\"state_topic\":\"%s/config/anchorTakeProfit\",\"command_topic\":\"%s/config/anchorTakeProfit/set\",\"min\":0.1,\"max\":100.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:cash-plus\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor max loss
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorMaxLoss/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Max Loss\",\"unique_id\":\"%s_anchorMaxLoss\",\"state_topic\":\"%s/config/anchorMaxLoss\",\"command_topic\":\"%s/config/anchorMaxLoss/set\",\"min\":-100.0,\"max\":-0.1,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:cash-minus\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor value (number entity for setting anchor price)
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorValue/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Value\",\"unique_id\":\"%s_anchorValue\",\"state_topic\":\"%s/config/anchorValue\",\"command_topic\":\"%s/config/anchorValue/set\",\"min\":0.01,\"max\":1000000.0,\"step\":0.01,\"unit_of_measurement\":\"EUR\",\"icon\":\"mdi:anchor\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor event sensor
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_anchor_event/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Event\",\"unique_id\":\"%s_anchor_event\",\"state_topic\":\"%s/anchor/event\",\"json_attributes_topic\":\"%s/anchor/event\",\"value_template\":\"{{ value_json.event }}\",\"icon\":\"mdi:anchor\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Trend threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_trendThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend Threshold\",\"unique_id\":\"%s_trendThreshold\",\"state_topic\":\"%s/config/trendThreshold\",\"command_topic\":\"%s/config/trendThreshold/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Volatility low threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_volatilityLowThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Volatility Low Threshold\",\"unique_id\":\"%s_volatilityLowThreshold\",\"state_topic\":\"%s/config/volatilityLowThreshold\",\"command_topic\":\"%s/config/volatilityLowThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Volatility high threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_volatilityHighThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Volatility High Threshold\",\"unique_id\":\"%s_volatilityHighThreshold\",\"state_topic\":\"%s/config/volatilityHighThreshold\",\"command_topic\":\"%s/config/volatilityHighThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant-shimmer\",\"mode\":\"box\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // IP Address sensor
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_ip_address/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"IP Address\",\"unique_id\":\"%s_ip_address\",\"state_topic\":\"%s/values/ip_address\",\"icon\":\"mdi:ip-network\",%s}", deviceId, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Language select
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/select/%s_language/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Language\",\"unique_id\":\"%s_language\",\"state_topic\":\"%s/config/language\",\"command_topic\":\"%s/config/language/set\",\"options\":[\"0\",\"1\"],\"icon\":\"mdi:translate\",%s}", deviceId, MQTT_TOPIC_PREFIX, MQTT_TOPIC_PREFIX, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    Serial_println("[MQTT] Discovery messages published");
}

// MQTT connect functie (niet-blokkerend)
void connectMQTT() {
    if (mqttConnected) return;
    
    mqttClient.setServer(mqttHost, mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char clientId[64];
    uint32_t macLower = (uint32_t)ESP.getEfuseMac();
    snprintf(clientId, sizeof(clientId), "%s%08x", MQTT_CLIENT_ID_PREFIX, macLower);
    Serial_printf(F("[MQTT] Connecting to %s:%d as %s...\n"), mqttHost, mqttPort, clientId);
    
    if (mqttClient.connect(clientId, mqttUser, mqttPass)) {
        Serial_println("[MQTT] Connected!");
        mqttConnected = true;
        mqttReconnectAttemptCount = 0; // Reset counter bij succesvolle verbinding
        
        // Geoptimaliseerd: gebruik char arrays i.p.v. String voor subscribe topics
        char topicBuffer[128];
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike1m/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike5m/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move30m/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5m/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown1min/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown30min/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/binanceSymbol/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ntfyTopic/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/button/reset/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorTakeProfit/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorMaxLoss/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorValue/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/language/set", MQTT_TOPIC_PREFIX);
        mqttClient.subscribe(topicBuffer);
        
        publishMqttSettings();
        publishMqttDiscovery();
        
        // Process queued messages after reconnection
        processMqttQueue();
        
    } else {
        Serial_printf(F("[MQTT] Connect failed, rc=%d (poging %u)\n"), mqttClient.state(), mqttReconnectAttemptCount);
        mqttConnected = false;
    }
}

// Web server HTML page
// Helper functies voor chunked HTML rendering
static void sendHtmlHeader(const char* platformName, const char* ntfyTopic)
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    
    // HTML doctype en head (lang='en' om punt als decimaal scheidingsteken te forceren)
    server.sendContent(F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    
    // Title
    char titleBuf[128];
    snprintf(titleBuf, sizeof(titleBuf), "<title>%s %s %s</title>", 
             getText("Instellingen", "Settings"), platformName, ntfyTopic);
    server.sendContent(titleBuf);
    
    // CSS
    server.sendContent(F("<style>"));
    server.sendContent(F("*{box-sizing:border-box;}"));
    server.sendContent(F("body{font-family:Arial;margin:0;padding:10px;background:#1a1a1a;color:#fff;}"));
    server.sendContent(F(".container{max-width:600px;margin:0 auto;padding:0 10px;}"));
    server.sendContent(F("h1{color:#00BCD4;margin:15px 0;font-size:24px;}"));
    server.sendContent(F("form{max-width:100%;}"));
    server.sendContent(F("label{display:block;margin:15px 0 5px;color:#ccc;}"));
    server.sendContent(F("input[type=number],input[type=text],select{width:100%;padding:8px;border:1px solid #444;background:#2a2a2a;color:#fff;border-radius:4px;box-sizing:border-box;}"));
    server.sendContent(F("button{background:#00BCD4;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;width:100%;}"));
    server.sendContent(F("button:hover{background:#00acc1;}"));
    server.sendContent(F(".info{color:#888;font-size:12px;margin-top:5px;}"));
    server.sendContent(F(".status-box{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:20px 0;max-width:100%;}"));
    server.sendContent(F(".status-row{display:flex;justify-content:space-between;margin:8px 0;padding:8px 0;border-bottom:1px solid #333;flex-wrap:wrap;}"));
    server.sendContent(F(".status-label{color:#888;flex:1;min-width:120px;}"));
    server.sendContent(F(".status-value{color:#fff;font-weight:bold;text-align:right;flex:1;min-width:100px;}"));
    server.sendContent(F(".section-header{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:12px;margin:15px 0 0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"));
    server.sendContent(F(".section-header:hover{background:#333;}"));
    server.sendContent(F(".section-header h3{margin:0;color:#00BCD4;font-size:16px;}"));
    server.sendContent(F(".section-content{display:none;padding:15px;background:#1a1a1a;border:1px solid #444;border-top:none;border-radius:0 0 4px 4px;}"));
    server.sendContent(F(".section-content.active{display:block;}"));
    server.sendContent(F(".section-desc{color:#888;font-size:12px;margin-top:5px;margin-bottom:15px;}"));
    server.sendContent(F(".toggle-icon{color:#00BCD4;font-size:18px;flex-shrink:0;margin-left:10px;}"));
    server.sendContent(F("@media (max-width:600px){"));
    server.sendContent(F("body{padding:5px;}"));
    server.sendContent(F(".container{padding:0 5px;}"));
    server.sendContent(F("h1{font-size:20px;margin:10px 0;}"));
    server.sendContent(F(".status-box{padding:10px;margin:15px 0;}"));
    server.sendContent(F(".status-row{flex-direction:column;padding:6px 0;}"));
    server.sendContent(F(".status-label{min-width:auto;margin-bottom:3px;}"));
    server.sendContent(F(".status-value{text-align:left;min-width:auto;}"));
    server.sendContent(F(".section-header{padding:10px;}"));
    server.sendContent(F(".section-header h3{font-size:14px;}"));
    server.sendContent(F(".section-content{padding:10px;}"));
    server.sendContent(F("button{padding:10px 20px;font-size:14px;}"));
    server.sendContent(F("label{font-size:14px;}"));
    server.sendContent(F("input[type=number],input[type=text],select{font-size:14px;padding:6px;}"));
    server.sendContent(F("}"));
    server.sendContent(F("</style>"));
    
    // JavaScript
    server.sendContent(F("<script type='text/javascript'>"));
    server.sendContent(F("(function(){"));
    server.sendContent(F("function toggleSection(id){"));
    server.sendContent(F("var content=document.getElementById('content-'+id);"));
    server.sendContent(F("var icon=document.getElementById('icon-'+id);"));
    server.sendContent(F("if(!content||!icon)return false;"));
    server.sendContent(F("if(content.classList.contains('active')){"));
    server.sendContent(F("content.classList.remove('active');"));
    server.sendContent(F("icon.innerHTML='&#9654;';"));
    server.sendContent(F("}else{"));
    server.sendContent(F("content.classList.add('active');"));
    server.sendContent(F("icon.innerHTML='&#9660;';"));
    server.sendContent(F("}"));
    server.sendContent(F("return false;"));
    server.sendContent(F("}"));
    server.sendContent(F("function setAnchorBtn(e){"));
    server.sendContent(F("if(e){e.preventDefault();e.stopPropagation();}"));
    server.sendContent(F("var input=document.getElementById('anchorValue');"));
    server.sendContent(F("if(!input){alert('Input not found');return false;}"));
    server.sendContent(F("var val=input.value||'';"));
    server.sendContent(F("var xhr=new XMLHttpRequest();"));
    server.sendContent(F("xhr.open('POST','/anchor/set',true);"));
    server.sendContent(F("xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"));
    server.sendContent(F("xhr.onreadystatechange=function(){"));
    server.sendContent(F("if(xhr.readyState==4){"));
    server.sendContent(F("if(xhr.status==200){"));
    
    char alertBuf[128];
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Anchor ingesteld!", "Anchor set!"));
    server.sendContent(alertBuf);
    
    server.sendContent(F("setTimeout(function(){location.reload();},500);"));
    server.sendContent(F("}else{"));
    
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Fout bij instellen anchor", "Error setting anchor"));
    server.sendContent(alertBuf);
    
    server.sendContent(F("}"));
    server.sendContent(F("}"));
    server.sendContent(F("};"));
    server.sendContent(F("xhr.send('value='+encodeURIComponent(val));"));
    server.sendContent(F("return false;"));
    server.sendContent(F("}"));
    server.sendContent(F("function resetNtfyBtn(e){"));
    server.sendContent(F("if(e){e.preventDefault();e.stopPropagation();}"));
    server.sendContent(F("var xhr=new XMLHttpRequest();"));
    server.sendContent(F("xhr.open('POST','/ntfy/reset',true);"));
    server.sendContent(F("xhr.onreadystatechange=function(){"));
    server.sendContent(F("if(xhr.readyState==4){"));
    server.sendContent(F("if(xhr.status==200){"));
    server.sendContent(F("setTimeout(function(){location.reload();},500);"));
    server.sendContent(F("}else{"));
    
    snprintf(alertBuf, sizeof(alertBuf), "alert('%s');", getText("Fout bij resetten NTFY topic", "Error resetting NTFY topic"));
    server.sendContent(alertBuf);
    
    server.sendContent(F("}"));
    server.sendContent(F("}"));
    server.sendContent(F("};"));
    server.sendContent(F("xhr.send();"));
    server.sendContent(F("return false;"));
    server.sendContent(F("}"));
    server.sendContent(F("window.addEventListener('DOMContentLoaded',function(){"));
    // Fix: converteer komma's naar punten in number inputs (locale fix)
    server.sendContent(F("var numberInputs=document.querySelectorAll('input[type=\"number\"]');"));
    server.sendContent(F("for(var i=0;i<numberInputs.length;i++){"));
    server.sendContent(F("numberInputs[i].addEventListener('input',function(e){"));
    server.sendContent(F("var val=this.value.replace(',','.');"));
    server.sendContent(F("if(val!==this.value){this.value=val;}});"));
    server.sendContent(F("numberInputs[i].addEventListener('blur',function(e){"));
    server.sendContent(F("var val=this.value.replace(',','.');"));
    server.sendContent(F("if(val!==this.value){this.value=val;}});"));
    server.sendContent(F("}"));
    server.sendContent(F("var headers=document.querySelectorAll('.section-header');"));
    server.sendContent(F("for(var i=0;i<headers.length;i++){"));
    server.sendContent(F("headers[i].addEventListener('click',function(e){"));
    server.sendContent(F("var id=this.getAttribute('data-section');"));
    server.sendContent(F("toggleSection(id);"));
    server.sendContent(F("e.preventDefault();"));
    server.sendContent(F("return false;"));
    server.sendContent(F("});"));
    server.sendContent(F("}"));
    server.sendContent(F("var basic=document.getElementById('icon-basic');"));
    server.sendContent(F("var anchor=document.getElementById('icon-anchor');"));
    server.sendContent(F("if(basic)basic.innerHTML='&#9660;';"));
    server.sendContent(F("if(anchor)anchor.innerHTML='&#9660;';"));
    server.sendContent(F("var anchorBtn=document.getElementById('anchorBtn');"));
    server.sendContent(F("if(anchorBtn){"));
    server.sendContent(F("anchorBtn.addEventListener('click',setAnchorBtn);"));
    server.sendContent(F("}"));
    server.sendContent(F("var ntfyResetBtn=document.getElementById('ntfyResetBtn');"));
    server.sendContent(F("if(ntfyResetBtn){"));
    server.sendContent(F("ntfyResetBtn.addEventListener('click',resetNtfyBtn);"));
    server.sendContent(F("}"));
    server.sendContent(F("});"));
    server.sendContent(F("})();"));
    server.sendContent(F("</script>"));
    server.sendContent(F("</head><body>"));
    server.sendContent(F("<div class='container'>"));
    
    // Title
    char h1Buf[128];
    snprintf(h1Buf, sizeof(h1Buf), "<h1>%s %s %s</h1>", 
             getText("Instellingen", "Settings"), platformName, ntfyTopic);
    server.sendContent(h1Buf);
}

static void sendHtmlFooter()
{
    server.sendContent(F("</div>"));
    server.sendContent(F("</body></html>"));
}

static void sendInputRow(const char* label, const char* name, const char* type, const char* value, 
                         const char* info, float minVal = 0, float maxVal = 0, float step = 0.01f)
{
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
    server.sendContent(buf);
    if (info && strlen(info) > 0) {
        snprintf(buf, sizeof(buf), "<div class='info'>%s</div>", info);
        server.sendContent(buf);
    }
}

static void sendCheckboxRow(const char* label, const char* name, bool checked)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "<label><input type='checkbox' name='%s' value='1'%s> %s</label>",
             name, checked ? " checked" : "", label);
    server.sendContent(buf);
}

static void sendStatusRow(const char* label, const char* value)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='status-row'><span class='status-label'>%s:</span><span class='status-value'>%s</span></div>",
             label, value);
    server.sendContent(buf);
}

static void sendSectionHeader(const char* title, const char* sectionId, bool expanded = false)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='section-header' data-section='%s'><h3>%s</h3><span class='toggle-icon' id='icon-%s'>%s</span></div>",
             sectionId, title, sectionId, expanded ? "&#9660;" : "&#9654;");
    server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='section-content%s' id='content-%s'>",
             expanded ? " active" : "", sectionId);
    server.sendContent(buf);
}

static void sendSectionFooter()
{
    server.sendContent(F("</div>"));
}

static void sendSectionDesc(const char* desc)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "<div class='section-desc'>%s</div>", desc);
    server.sendContent(buf);
}

// Refactored: chunked HTML rendering (geen grote String in heap)
static void renderSettingsHTML()
{
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
        currentTrend = trendState;
        currentVol = volatilityState;
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
    server.sendContent(F("<form method='POST' action='/save'>"));
    
    // Anchor instellen - helemaal bovenaan
    snprintf(valueBuf, sizeof(valueBuf), "%.2f", 
             (currentPrice > 0.0f) ? currentPrice : 
             ((currentAnchorActive && currentAnchorPrice > 0.0f) ? currentAnchorPrice : 0.0f));
    
    server.sendContent(F("<div style='background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:15px 0;'>"));
    snprintf(tmpBuf, sizeof(tmpBuf), "<label style='display:block;margin-top:0;margin-bottom:8px;color:#fff;font-weight:bold;'>%s (EUR):</label>", 
             getText("Referentieprijs (Anchor)", "Reference price (Anchor)"));
    server.sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<input type='number' step='0.01' id='anchorValue' value='%s' min='0.01' lang='en' style='width:100%%;padding:8px;margin-bottom:10px;border:1px solid #444;background:#1a1a1a;color:#fff;border-radius:4px;box-sizing:border-box;'>",
             valueBuf);
    server.sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<button type='button' id='anchorBtn' style='width:100%%;background:#4CAF50;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;font-weight:bold;'>%s</button>",
             getText("Stel Anchor in", "Set Anchor"));
    server.sendContent(tmpBuf);
    server.sendContent(F("</div>"));
    
    // Status box
    server.sendContent(F("<div class='status-box'>"));
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
    server.sendContent(F("</div>"));
    
    // Basis & Connectiviteit sectie
    sendSectionHeader(getText("Basis & Connectiviteit", "Basic & Connectivity"), "basic", true);
    sendSectionDesc(getText("Basisinstellingen voor symbol, notificaties en connectiviteit", "Basic settings for symbol, notifications and connectivity"));
    
    // NTFY Topic met reset knop (onder input veld, net als anchor)
    snprintf(tmpBuf, sizeof(tmpBuf), "<label>%s:", 
             getText("NTFY Topic", "NTFY Topic"));
    server.sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<input type='text' name='ntfytopic' value='%s' maxlength='63' style='width:100%%;padding:8px;margin-bottom:10px;border:1px solid #444;background:#1a1a1a;color:#fff;border-radius:4px;box-sizing:border-box;'>", ntfyTopic);
    server.sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<button type='button' id='ntfyResetBtn' style='width:100%%;background:#2196F3;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;font-weight:bold;'>%s</button>", 
             getText("Standaard uniek NTFY-topic", "Default unique NTFY topic"));
    server.sendContent(tmpBuf);
    server.sendContent(F("</label>"));
    snprintf(tmpBuf, sizeof(tmpBuf), "<div class='info'>%s</div>", 
             getText("NTFY.sh topic voor notificaties", "NTFY.sh topic for notifications"));
    server.sendContent(tmpBuf);
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
    server.sendContent(tmpBuf);
    
    server.sendContent(F("</form>"));
    
    // Footer
    sendHtmlFooter();
}

// ============================================================================
// Web Server Functions
// ============================================================================

// Web server handlers
static void handleRoot()
{
    renderSettingsHTML();
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
        // Allow empty topic (will use default) or valid length topic
        if (topic.length() == 0 || (topic.length() > 0 && topic.length() < sizeof(ntfyTopic))) {
            if (topic.length() == 0) {
                // Generate default topic if empty
                generateDefaultNtfyTopic(ntfyTopic, sizeof(ntfyTopic));
            } else {
                topic.toCharArray(ntfyTopic, sizeof(ntfyTopic));
            }
        }
    }
    if (server.hasArg("binancesymbol")) {
        String symbol = server.arg("binancesymbol");
        symbol.trim();
        symbol.toUpperCase(); // Binance symbolen zijn altijd uppercase
        if (symbol.length() > 0 && symbol.length() < sizeof(binanceSymbol)) {
            symbol.toCharArray(binanceSymbol, sizeof(binanceSymbol));
            // Update symbols array
            safeStrncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]));
        }
    }
    if (server.hasArg("spike1m")) {
        float val;
        if (safeAtof(server.arg("spike1m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            spike1mThreshold = val;
        }
    }
    if (server.hasArg("spike5m")) {
        float val;
        if (safeAtof(server.arg("spike5m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            spike5mThreshold = val;
        }
    }
    if (server.hasArg("move30m")) {
        float val;
        if (safeAtof(server.arg("move30m").c_str(), val) && val >= 0.01f && val <= 20.0f) {
            move30mThreshold = val;
        }
    }
    if (server.hasArg("move5m")) {
        float val;
        if (safeAtof(server.arg("move5m").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            move5mThreshold = val;
        }
    }
    if (server.hasArg("move5mAlert")) {
        float val;
        if (safeAtof(server.arg("move5mAlert").c_str(), val) && val >= 0.01f && val <= 10.0f) {
            move5mAlertThreshold = val;
        }
    }
    if (server.hasArg("cd1min")) {
        int seconds = server.arg("cd1min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown1MinMs = resultMs;
        }
    }
    if (server.hasArg("cd30min")) {
        int seconds = server.arg("cd30min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown30MinMs = resultMs;
        }
    }
    if (server.hasArg("cd5min")) {
        int seconds = server.arg("cd5min").toInt();
        uint32_t resultMs;
        if (safeSecondsToMs(seconds, resultMs)) {
            notificationCooldown5MinMs = resultMs;
        }
    }
    
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
        float val;
        if (safeAtof(server.arg("trendTh").c_str(), val) && val >= 0.1f && val <= 10.0f) {
            trendThreshold = val;
        }
    }
    if (server.hasArg("volLow")) {
        float val;
        if (safeAtof(server.arg("volLow").c_str(), val) && val >= 0.01f && val <= 1.0f) {
            volatilityLowThreshold = val;
        }
    }
    if (server.hasArg("volHigh")) {
        float val;
        if (safeAtof(server.arg("volHigh").c_str(), val) && val >= 0.01f && val <= 1.0f && val > volatilityLowThreshold) {
            volatilityHighThreshold = val;
        }
    }
    
    // Anchor settings - NIET vanuit web server thread verwerken om crashes te voorkomen
    // Anchor setting wordt verwerkt via een aparte route /anchor/set die sneller is
    if (server.hasArg("anchorTP")) {
        float val;
        if (safeAtof(server.arg("anchorTP").c_str(), val) && val >= 0.1f && val <= 100.0f) {
            anchorTakeProfit = val;
        }
    }
    if (server.hasArg("anchorML")) {
        float val;
        if (safeAtof(server.arg("anchorML").c_str(), val) && val >= -100.0f && val <= -0.1f) {
            anchorMaxLoss = val;
        }
    }
    
    // Trend-adaptive anchor settings
    trendAdaptiveAnchorsEnabled = server.hasArg("trendAdapt");
    if (server.hasArg("upMLMult")) {
        float val;
        if (safeAtof(server.arg("upMLMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            uptrendMaxLossMultiplier = val;
        }
    }
    if (server.hasArg("upTPMult")) {
        float val;
        if (safeAtof(server.arg("upTPMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            uptrendTakeProfitMultiplier = val;
        }
    }
    if (server.hasArg("downMLMult")) {
        float val;
        if (safeAtof(server.arg("downMLMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            downtrendMaxLossMultiplier = val;
        }
    }
    if (server.hasArg("downTPMult")) {
        float val;
        if (safeAtof(server.arg("downTPMult").c_str(), val) && val >= 0.5f && val <= 2.0f) {
            downtrendTakeProfitMultiplier = val;
        }
    }
    
    // Smart Confluence Mode settings
    smartConfluenceEnabled = server.hasArg("smartConf");
    
    // Warm-Start settings
    warmStartEnabled = server.hasArg("warmStart");
    if (server.hasArg("ws1mExtra")) {
        uint8_t val = server.arg("ws1mExtra").toInt();
        if (val >= 0 && val <= 100) {
            warmStart1mExtraCandles = val;
        }
    }
    if (server.hasArg("ws5m")) {
        uint8_t val = server.arg("ws5m").toInt();
        if (val >= 2 && val <= 200) {
            warmStart5mCandles = val;
        }
    }
    if (server.hasArg("ws30m")) {
        uint8_t val = server.arg("ws30m").toInt();
        if (val >= 2 && val <= 200) {
            warmStart30mCandles = val;
        }
    }
    if (server.hasArg("ws2h")) {
        uint8_t val = server.arg("ws2h").toInt();
        if (val >= 2 && val <= 200) {
            warmStart2hCandles = val;
        }
    }
    
    // Auto-Volatility Mode settings
    autoVolatilityEnabled = server.hasArg("autoVol");
    if (server.hasArg("autoVolWin")) {
        uint8_t val = server.arg("autoVolWin").toInt();
        if (val >= 10 && val <= 120) {
            autoVolatilityWindowMinutes = val;
        }
    }
    if (server.hasArg("autoVolBase")) {
        float val;
        if (safeAtof(server.arg("autoVolBase").c_str(), val) && val >= 0.01f && val <= 1.0f) {
            autoVolatilityBaseline1mStdPct = val;
        }
    }
    if (server.hasArg("autoVolMin")) {
        float val;
        if (safeAtof(server.arg("autoVolMin").c_str(), val) && val >= 0.1f && val <= 1.0f) {
            autoVolatilityMinMultiplier = val;
        }
    }
    if (server.hasArg("autoVolMax")) {
        float val;
        if (safeAtof(server.arg("autoVolMax").c_str(), val) && val >= 1.0f && val <= 3.0f) {
            autoVolatilityMaxMultiplier = val;
        }
    }
    
    saveSettings();
    
    // Update UI om wijzigingen direct te tonen (bijv. NTFY-topic op scherm)
    // Opmerking: updateUI() wordt periodiek aangeroepen vanuit uiTask, dus dit is optioneel
    // Voor anchor wijzigingen wordt de UI geüpdatet via de uiTask, niet vanuit de web server thread
    
    // Herconnect MQTT als instellingen zijn gewijzigd
    if (mqttConnected) {
        mqttClient.disconnect();
        mqttConnected = false;
        lastMqttReconnectAttempt = 0;
        mqttReconnectAttemptCount = 0; // Reset counter bij disconnect
    }
    
    // Chunked HTML output (geen String in heap)
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    
    server.sendContent(F("<!DOCTYPE html><html><head>"));
    server.sendContent(F("<meta http-equiv='refresh' content='2;url=/'><meta charset='UTF-8'>"));
    server.sendContent(F("<title>Opgeslagen</title>"));
    server.sendContent(F("<style>"));
    server.sendContent(F("body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff;text-align:center;}"));
    server.sendContent(F("h1{color:#4CAF50;}"));
    server.sendContent(F("</style></head><body>"));
    
    // Dynamische tekst via kleine buffer
    char tmpBuf[128];
    snprintf(tmpBuf, sizeof(tmpBuf), "<h1>%s</h1>", getText("Instellingen opgeslagen!", "Settings saved!"));
    server.sendContent(tmpBuf);
    snprintf(tmpBuf, sizeof(tmpBuf), "<p>%s</p>", getText("Terug naar instellingen...", "Returning to settings..."));
    server.sendContent(tmpBuf);
    
    server.sendContent(F("</body></html>"));
}



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
    Serial_printf(F("[WebServer] 404: %s\n"), server.uri().c_str());
}

// Handler voor anchor set (aparte route om crashes te voorkomen)
// Gebruikt een queue om asynchroon te verwerken vanuit main loop
// Thread-safe: schrijft naar volatile variabelen die worden gelezen vanuit uiTask
static void handleAnchorSet() {
    if (server.hasArg("value")) {
        String anchorValueStr = server.arg("value");
        anchorValueStr.trim();
        bool valid = false;
        
        if (anchorValueStr.length() > 0) {
            float val;
            if (safeAtof(anchorValueStr.c_str(), val) && val > 0.0f && isValidPrice(val)) {
                // Valide waarde - zet in queue voor asynchrone verwerking
                valid = queueAnchorSetting(val, false);
                if (valid) {
                    Serial_printf("[Web] Anchor setting queued: %.2f\n", val);
                }
            } else {
                Serial_printf(F("[Web] WARN: Ongeldige anchor waarde opgegeven: '%s'\n"), anchorValueStr.c_str());
            }
        } else {
            // Leeg veld = gebruik huidige prijs
            pendingAnchorSetting.value = 0.0f;
            pendingAnchorSetting.useCurrentPrice = true;
            // Zet pending flag als laatste (memory barrier effect)
            pendingAnchorSetting.pending = true;
            valid = true;
            Serial_println(F("[Web] Anchor setting queued: gebruik huidige prijs"));
        }
        
        if (valid) {
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "ERROR: Invalid anchor value");
        }
    } else {
        server.send(400, "text/plain", "ERROR: Missing 'value' parameter");
    }
}

static void handleNtfyReset() {
    // Genereer standaard topic en sla op
    generateDefaultNtfyTopic(ntfyTopic, sizeof(ntfyTopic));
    
    // Sla op via SettingsStore
    CryptoMonitorSettings settings = settingsStore.load();
    safeStrncpy(settings.ntfyTopic, ntfyTopic, sizeof(settings.ntfyTopic));
    settingsStore.save(settings);
    
    Serial_printf(F("[Web] NTFY topic gereset naar standaard: %s\n"), ntfyTopic);
    
    // Stuur succes response
    server.send(200, "text/plain", "OK");
}

static void setupWebServer()
{
    Serial.println(F("[WebServer] Routes registreren..."));
    server.on("/", handleRoot);
    Serial.println("[WebServer] Route '/' geregistreerd");
    server.on("/save", HTTP_POST, handleSave);
    Serial.println(F("[WebServer] Route '/save' geregistreerd"));
    server.on("/anchor/set", HTTP_POST, handleAnchorSet);
    Serial.println("[WebServer] Route '/anchor/set' geregistreerd");
    server.on("/ntfy/reset", HTTP_POST, handleNtfyReset);
    Serial.println(F("[WebServer] Route '/ntfy/reset' geregistreerd"));
    server.onNotFound(handleNotFound); // 404 handler
    Serial.println(F("[WebServer] 404 handler geregistreerd"));
    server.begin();
    Serial.println("[WebServer] Server gestart");
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char ipBuffer[16];
    formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
    Serial.printf("[WebServer] Gestart op http://%s\n", ipBuffer);
}

// Parse Binance JSON functies zijn verwijderd - nu via ApiClient::parseBinancePrice()

// Calculate average of array (optimized: single loop)
// Fase 4.2.8: static verwijderd zodat PriceData.cpp deze functie kan aanroepen
float calculateAverage(float *array, uint8_t size, bool filled)
{
    float sum = 0.0f;
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < size; i++)
    {
        if (filled || array[i] != 0.0f)
        {
            sum += array[i];
            count++;
        }
    }
    
    return (count == 0) ? 0.0f : (sum / count);
}

// ============================================================================
// Price History Management Functions
// ============================================================================

// Helper: Calculate ringbuffer index N positions ago from current write position
// Returns safe index in range [0, size) or -1 if invalid
// Fase 4.2.8: static verwijderd zodat PriceData.cpp deze functie kan aanroepen
int32_t getRingBufferIndexAgo(uint32_t currentIndex, uint32_t positionsAgo, uint32_t bufferSize)
{
    if (positionsAgo >= bufferSize) return -1;
    // Safe modulo calculation: (currentIndex - positionsAgo + bufferSize * 2) % bufferSize
    int32_t idx = ((int32_t)currentIndex - (int32_t)positionsAgo + (int32_t)bufferSize * 2) % (int32_t)bufferSize;
    if (idx < 0 || idx >= (int32_t)bufferSize) return -1;
    return idx;
}

// Helper: Get last written index in ringbuffer (currentIndex points to next write position)
// Fase 4.2.8: static verwijderd zodat PriceData.cpp deze functie kan aanroepen
uint32_t getLastWrittenIndex(uint32_t currentIndex, uint32_t bufferSize)
{
    return (currentIndex == 0) ? (bufferSize - 1) : (currentIndex - 1);
}

// Helper: Calculate percentage of SOURCE_LIVE entries in the last windowMinutes of minuteAverages
// Returns percentage (0-100) of entries that are SOURCE_LIVE
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
static uint8_t calcLivePctMinuteAverages(uint16_t windowMinutes)
{
    if (windowMinutes == 0 || windowMinutes > MINUTES_FOR_30MIN_CALC) {
        return 0;
    }
    
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    DataSource* sources = priceData.getMinuteAveragesSource();
    
    uint8_t availableMinutes = arrayFilled ? MINUTES_FOR_30MIN_CALC : index;
    if (availableMinutes < windowMinutes) {
        return 0;  // Niet genoeg data beschikbaar
    }
    
    // Tel hoeveel van de laatste windowMinutes entries SOURCE_LIVE zijn
    uint16_t liveCount = 0;
    for (uint16_t i = 1; i <= windowMinutes; i++) {
        // Bereken index N posities terug vanaf huidige write positie
        int32_t idx = getRingBufferIndexAgo(index, i, MINUTES_FOR_30MIN_CALC);
        if (idx >= 0 && idx < MINUTES_FOR_30MIN_CALC) {
            if (sources[idx] == SOURCE_LIVE) {
                liveCount++;
            }
        }
    }
    
    // Bereken percentage (0-100)
    return (liveCount * 100) / windowMinutes;
}

// Find min and max values in secondPrices array
static void findMinMaxInSecondPrices(float &minVal, float &maxVal)
{
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    minVal = 0.0f;
    maxVal = 0.0f;
    
    float* prices = priceData.getSecondPrices();
    bool arrayFilled = priceData.getSecondArrayFilled();
    uint8_t index = priceData.getSecondIndex();
    
    if (!arrayFilled && prices[0] == 0.0f)
        return;
    
    uint8_t count = arrayFilled ? SECONDS_PER_MINUTE : index;
    if (count == 0)
        return;
    
    minVal = prices[0];
    maxVal = prices[0];
    
    for (uint8_t i = 1; i < count; i++)
    {
        if (isValidPrice(prices[i]))
        {
            if (prices[i] < minVal) minVal = prices[i];
            if (prices[i] > maxVal) maxVal = prices[i];
        }
    }
}

// ============================================================================
// Price Calculation Functions
// ============================================================================

// Calculate 1-minute return: price now vs 60 seconds ago
// Generic return calculation function
// Calculates percentage return: (priceNow - priceXAgo) / priceXAgo * 100
// Supports different array types (uint8_t, uint16_t indices) and optional average calculation
static float calculateReturnGeneric(
    const float* priceArray,           // Price array
    uint16_t arraySize,                // Size of the array
    uint16_t currentIndex,             // Current index in the array
    bool arrayFilled,                  // Whether array is filled (ring buffer)
    uint16_t positionsAgo,             // How many positions ago to compare
    const char* logPrefix,             // Log prefix for debugging (e.g., "[Ret1m]")
    uint32_t logIntervalMs,            // Log interval in ms (0 = no logging)
    uint8_t averagePriceIndex = 255    // Index in averagePrices[] to update (255 = don't update)
)
{
    // Check if we have enough data
    if (!arrayFilled && currentIndex < positionsAgo)
    {
        if (averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = 0.0f;
        }
        if (logIntervalMs > 0) {
            static uint32_t lastLogTime = 0;
            uint32_t now = millis();
            if (now - lastLogTime > logIntervalMs) {
                Serial_printf("%s Wachten op data: index=%u (nodig: %u)\n", logPrefix, currentIndex, positionsAgo);
                lastLogTime = now;
            }
        }
        return 0.0f;
    }
    
    // Get current price
    float priceNow;
    if (arrayFilled) {
        uint16_t lastWrittenIdx = getLastWrittenIndex(currentIndex, arraySize);
        priceNow = priceArray[lastWrittenIdx];
    } else {
        if (currentIndex == 0) return 0.0f;
        priceNow = priceArray[currentIndex - 1];
    }
    
    // Get price X positions ago
    float priceXAgo;
    if (arrayFilled)
    {
        int32_t idxXAgo = getRingBufferIndexAgo(currentIndex, positionsAgo, arraySize);
        if (idxXAgo < 0) {
            Serial_printf("%s FATAL: idxXAgo invalid, currentIndex=%u\n", logPrefix, currentIndex);
            return 0.0f;
        }
        priceXAgo = priceArray[idxXAgo];
    }
    else
    {
        if (currentIndex < positionsAgo) return 0.0f;
        priceXAgo = priceArray[currentIndex - positionsAgo];
    }
    
    // Validate prices
    if (!areValidPrices(priceNow, priceXAgo))
    {
        if (averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = 0.0f;
        }
        Serial_printf("%s ERROR: priceNow=%.2f, priceXAgo=%.2f - invalid!\n", logPrefix, priceNow, priceXAgo);
        return 0.0f;
    }
    
    // Calculate average for display (if requested)
    if (averagePriceIndex < 3) {
        if (averagePriceIndex == 1) {
            // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
            // For 1m: use calculateAverage helper
            averagePrices[1] = calculateAverage(priceData.getSecondPrices(), SECONDS_PER_MINUTE, priceData.getSecondArrayFilled());
        } else if (averagePriceIndex == 2) {
            // For 30m: calculate average of last 30 minutes (handled separately in calculateReturn30Minutes)
            // This is a placeholder - actual calculation is done in the wrapper function
        }
    }
    
    // Return percentage: (now - X ago) / X ago * 100
    return ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
}

// Fase 4.2.8: calculateReturn1Minute() verplaatst naar PriceData
// Wrapper functie voor backward compatibility
static float calculateReturn1Minute()
{
    // Fase 4.2.8: Gebruik PriceData::calculateReturn1Minute()
    extern float averagePrices[];
    return priceData.calculateReturn1Minute(averagePrices);
}

// Calculate 5-minute return: price now vs 5 minutes ago
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
static float calculateReturn5Minutes()
{
    return calculateReturnGeneric(
        priceData.getFiveMinutePrices(),
        SECONDS_PER_5MINUTES,
        priceData.getFiveMinuteIndex(),
        priceData.getFiveMinuteArrayFilled(),
        VALUES_FOR_5MIN_RETURN,
        "[Ret5m]",
        30000,  // Log every 30 seconds
        255     // Don't update averagePrices
    );
}

// Calculate 30-minute return: price now vs 30 minutes ago (using minute averages)
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
static float calculateReturn30Minutes()
{
    // Need at least 30 minutes of history
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    uint8_t availableMinutes = arrayFilled ? MINUTES_FOR_30MIN_CALC : index;
    if (availableMinutes < 30)
    {
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        if (now - lastLogTime > 60000) {
            Serial_printf("[Ret30m] Wachten op data: minuteIndex=%u (nodig: 30, available=%u)\n", 
                         index, availableMinutes);
            lastLogTime = now;
        }
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Use generic function for return calculation
    float ret = calculateReturnGeneric(
        priceData.getMinuteAverages(),
        MINUTES_FOR_30MIN_CALC,
        index,
        arrayFilled,
        30,  // 30 minutes ago
        "[Ret30m]",
        60000,  // Log every 60 seconds
        255     // Don't update averagePrices here (we do it manually below)
    );
    
    // Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Calculate average of last 30 minutes for display (specific to 30m calculation)
    float* averages = priceData.getMinuteAverages();
    bool minuteArrayFilled = priceData.getMinuteArrayFilled();
    uint8_t minuteIndex = priceData.getMinuteIndex();
    
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
            int32_t idx_temp = getRingBufferIndexAgo(minuteIndex, i + 1, MINUTES_FOR_30MIN_CALC);
            if (idx_temp < 0) break;
            idx = (uint8_t)idx_temp;
        }
        if (isValidPrice(averages[idx]))
        {
            last30Sum += averages[idx];
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
    
    return ret;
}

// OUDE METHODE - behouden voor referentie, maar niet meer gebruikt
// Bereken lineaire regressie (trend) over de laatste 60 meetpunten
// Retourneert de helling (slope) als percentage per minuut
// Positieve waarde = stijgende trend, negatieve waarde = dalende trend
static float calculateLinearTrend1Minute()
{
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // We hebben minimaal 2 punten nodig voor een trend
    float* prices = priceData.getSecondPrices();
    bool arrayFilled = priceData.getSecondArrayFilled();
    uint8_t index = priceData.getSecondIndex();
    
    uint8_t count = arrayFilled ? SECONDS_PER_MINUTE : index;
    if (count < 2)
    {
        averagePrices[1] = 0.0f;
        return 0.0f;
    }
    
    // Bereken gemiddelde prijs voor weergave
    float currentAvg = calculateAverage(prices, SECONDS_PER_MINUTE, arrayFilled);
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
        float price = prices[i];
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
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Calculate 2-hour return: price now vs 120 minutes ago
static float calculateReturn2Hours()
{
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    float* averages = priceData.getMinuteAverages();
    
    uint8_t availableMinutes = arrayFilled ? MINUTES_FOR_30MIN_CALC : index;
    if (availableMinutes < 120)
    {
        return 0.0f;
    }
    
    // Get current price (last minute average)
    uint8_t lastMinuteIdx;
    if (!arrayFilled)
    {
        if (index == 0) return 0.0f;
        lastMinuteIdx = index - 1;
    }
    else
    {
        lastMinuteIdx = getLastWrittenIndex(index, MINUTES_FOR_30MIN_CALC);
    }
    float priceNow = averages[lastMinuteIdx];
    
    // Get price 120 minutes ago
    uint8_t idx120mAgo;
    if (!arrayFilled)
    {
        if (index < 120) return 0.0f;
        idx120mAgo = index - 120;
    }
    else
    {
        int32_t idx120mAgo_temp = getRingBufferIndexAgo(index, 120, MINUTES_FOR_30MIN_CALC);
        if (idx120mAgo_temp < 0) return 0.0f;
        idx120mAgo = (uint8_t)idx120mAgo_temp;
    }
    
    float price120mAgo = averages[idx120mAgo];
    
    // Validate prices
    if (price120mAgo <= 0.0f || priceNow <= 0.0f)
    {
        return 0.0f;
    }
    
    // Return percentage: (now - 120m ago) / 120m ago * 100
    return ((priceNow - price120mAgo) / price120mAgo) * 100.0f;
}

// ============================================================================
// Trend Detection Functions
// ============================================================================

// Bepaal trend state op basis van 2h return en optioneel 30m return
// Bepaal trend state op basis van 2h return en 30m return
// TREND_UP: 2h >= +trendThreshold EN 30m >= 0 (beide voorwaarden moeten waar zijn)
// TREND_DOWN: 2h <= -trendThreshold EN 30m <= 0 (beide voorwaarden moeten waar zijn)
// TREND_SIDEWAYS: anders
static TrendState determineTrendState(float ret_2h_value, float ret_30m_value)
{
    if (ret_2h_value >= trendThreshold && ret_30m_value >= 0.0f)
    {
        return TREND_UP;
    }
    else if (ret_2h_value <= -trendThreshold && ret_30m_value <= 0.0f)
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
        if (isValidPrice(minuteAverages[idx]))
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
// Fase 4.2.11: Oude addPriceToSecondArray() functie verwijderd
// Gebruik nu priceData.addPriceToSecondArray() in plaats daarvan

// Update minute averages (called every minute)
// Geoptimaliseerd: bounds checking en validatie toegevoegd
static void updateMinuteAverage()
{
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Bereken gemiddelde van de 60 seconden
    float minuteAvg = calculateAverage(priceData.getSecondPrices(), SECONDS_PER_MINUTE, priceData.getSecondArrayFilled());
    
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
    minuteAveragesSource[minuteIndex] = SOURCE_LIVE;  // Mark as live data
    minuteIndex = (minuteIndex + 1) % MINUTES_FOR_30MIN_CALC;
    if (minuteIndex == 0)
        minuteArrayFilled = true;
    
    // Update warm-start status na elke minuut update
    updateWarmStartStatus();
}

// ============================================================================
// Price Fetching and Management Functions
// ============================================================================

// Fetch the symbols' current prices (thread-safe met mutex)
static void fetchPrice()
{
    // Controleer eerst of WiFi verbonden is
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[API] WiFi niet verbonden, skip fetch"));
        return;
    }
    
    unsigned long fetchStart = millis();
    float fetched = prices[0]; // Start met huidige waarde als fallback
    bool ok = false;

    // Fase 4.1.7: Gebruik hoog-niveau fetchBinancePrice() method
    bool httpSuccess = apiClient.fetchBinancePrice(binanceSymbol, fetched);
    unsigned long fetchTime = millis() - fetchStart;
    
    if (!httpSuccess) {
        // Leeg response - kan komen door timeout of netwerkproblemen
        Serial.printf("[API] WARN -> %s leeg response (tijd: %lu ms) - mogelijk timeout of netwerkprobleem\n", binanceSymbol, fetchTime);
        // Gebruik laatste bekende prijs als fallback (al ingesteld als fetched = prices[0])
    } else {
        // Succesvol opgehaald (alleen loggen bij langzame calls > 1200ms)
        if (fetchTime > 1200) {
            Serial.printf(F("[API] OK -> %s %.2f (tijd: %lu ms) - langzaam\n"), binanceSymbol, fetched, fetchTime);
        }
        
        // Neem mutex voor data updates (timeout verhoogd om mutex conflicts te verminderen)
        // API task heeft prioriteit: verhoogde timeout om mutex te krijgen zelfs als UI bezig is
        #ifdef PLATFORM_TTGO
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(500); // TTGO: 500ms
        #else
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(400); // CYD/ESP32-S3: 400ms voor betere mutex acquisitie
        #endif
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        static uint32_t mutexTimeoutCount = 0;
        if (safeMutexTake(dataMutex, apiMutexTimeout, "apiTask fetchPrice"))
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
            // Fase 4.2.4: Gebruik PriceData::addPriceToSecondArray() (inline implementatie)
            priceData.addPriceToSecondArray(fetched);
            
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
            ret_30m = calculateReturn30Minutes(); // Percentage verandering laatste 30 minuten (update global)
            ret_2h = calculateReturn2Hours();
            
            // Update live availability flags: gebaseerd op data beschikbaarheid EN percentage live data
            // hasRet30mLive: true zodra er minimaal 30 minuten data is EN ≥80% daarvan SOURCE_LIVE is
            uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
            uint8_t livePct30 = calcLivePctMinuteAverages(30);
            hasRet30mLive = (availableMinutes >= 30 && livePct30 >= 80);
            
            // hasRet2hLive: true zodra er minimaal 120 minuten data is EN ≥80% daarvan SOURCE_LIVE is
            uint8_t livePct120 = calcLivePctMinuteAverages(120);
            hasRet2hLive = (availableMinutes >= 120 && livePct120 >= 80);
            
            // Update combined flags: beschikbaar vanuit warm-start OF live data
            hasRet2h = hasRet2hWarm || hasRet2hLive;
            hasRet30m = hasRet30mWarm || hasRet30mLive;
            
            // Bepaal trend state op basis van 2h return (alleen als beide flags true zijn)
            if (hasRet2h && hasRet30m) {
                trendState = determineTrendState(ret_2h, ret_30m);
            }
            
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
            
            safeMutexGive(dataMutex, "fetchPrice");
            ok = true;
        } else {
            // Geoptimaliseerd: log alleen bij meerdere opeenvolgende timeouts
            mutexTimeoutCount++;
            if (mutexTimeoutCount == 1 || mutexTimeoutCount % 10 == 0) {
                Serial.printf(F("[API] WARN -> %s mutex timeout (count: %lu)\n"), binanceSymbol, mutexTimeoutCount);
            }
            // Fallback: update prijs zonder mutex als timeout te vaak voorkomt (alleen voor noodgeval)
            if (mutexTimeoutCount > 50) {
                Serial.printf(F("[API] CRIT -> %s mutex timeout te vaak, mogelijk deadlock!\n"), binanceSymbol);
                mutexTimeoutCount = 0; // Reset counter
            }
        }
    }
}

// Update the UI (wordt aangeroepen vanuit uiTask met mutex)
// Update UI - Refactored to use helper functions
// UI Update Helper Functions - Split from updateUI() for better organization
static void updateChartSection(int32_t currentPrice, bool hasNewPriceData) {
    // Voeg een punt toe aan de grafiek als er geldige data is
    if (prices[symbolIndexToChart] > 0.0f) {
        // Track laatste chart waarde om conditional invalidate te doen
        static int32_t lastChartValue = 0;
        bool valueChanged = (currentPrice != lastChartValue);
        
        lv_chart_set_next_value(chart, dataSeries, currentPrice);
        
        // Conditional invalidate: alleen als waarde is veranderd of er nieuwe data is
        if (valueChanged || hasNewPriceData || newPriceDataAvailable) {
            lv_obj_invalidate(chart);
            lastChartValue = currentPrice;
        }
        
        // Reset flag na gebruik
        newPriceDataAvailable = false;
    }
    
    // Update chart range
    updateChartRange(currentPrice);
    
    // Update chart title (CYD displays)
    if (chartTitle != nullptr) {
        char deviceIdBuffer[16] = {0};
        const char* alertPos = strstr(ntfyTopic, "-alert");
        if (alertPos != nullptr) {
            size_t len = alertPos - ntfyTopic;
            if (len > 0 && len < sizeof(deviceIdBuffer)) {
                safeStrncpy(deviceIdBuffer, ntfyTopic, len + 1);
            } else {
                safeStrncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer));
            }
        } else {
            safeStrncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer));
        }
        lv_label_set_text(chartTitle, deviceIdBuffer);
    }
    
    // Update chart begin letters label (TTGO displays)
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI)
    if (chartBeginLettersLabel != nullptr) {
        char deviceIdBuffer[16];
        getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
        lv_label_set_text(chartBeginLettersLabel, deviceIdBuffer);
    }
    #endif
}

static void updateHeaderSection() {
    // Update datum/tijd labels
    updateDateTimeLabels();
    
    // Update trend en volatiliteit labels
    updateTrendLabel();
    updateVolatilityLabel();
}

static void updatePriceCardsSection(bool hasNewPriceData) {
    // Update price cards
    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i) {
        float pct = 0.0f;
        
        if (i == 0) {
            // BTCEUR card
            updateBTCEURCard(hasNewPriceData);
            pct = 0.0f; // BTCEUR heeft geen percentage voor kleur
        } else {
            // 1min/30min cards
            pct = prices[i];
            updateAveragePriceCard(i);
        }
        
        // Update kleuren
        updatePriceCardColor(i, pct);
    }
}

void updateUI()
{
    // Veiligheid: controleer of chart en dataSeries bestaan
    if (chart == nullptr || dataSeries == nullptr) {
        Serial_println(F("[UI] WARN: Chart of dataSeries is null, skip update"));
        return;
    }
    
    // Data wordt al beschermd door mutex in uiTask
    int32_t p = (int32_t)lroundf(prices[symbolIndexToChart] * 100.0f);
    
    // Bepaal of er nieuwe data is op basis van timestamp
    unsigned long currentTime = millis();
    bool hasNewPriceData = false;
    if (lastApiMs > 0) {
        unsigned long timeSinceLastApi = (currentTime >= lastApiMs) ? (currentTime - lastApiMs) : (ULONG_MAX - lastApiMs + currentTime);
        hasNewPriceData = (timeSinceLastApi < 2500);
    }
    
    // Update UI sections
    updateChartSection(p, hasNewPriceData);
    updateHeaderSection();
    updatePriceCardsSection(hasNewPriceData);
    updateFooter();
    
    // Heap telemetry na LVGL update (optioneel, alleen periodiek)
    // logHeapTelemetry("lvgl");  // Uitgecommentarieerd om spam te voorkomen
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

// Cache variabelen voor datum/tijd labels (lokaal voor deze functie)
static char lastDateText[11] = {0};  // Cache voor date label
static char lastTimeText[9] = {0};   // Cache voor time label

// Helper functie om datum/tijd labels bij te werken
static void updateDateTimeLabels()
{
    if (chartDateLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            #ifdef PLATFORM_TTGO
            // TTGO: compact formaat dd-mm-yy voor lagere resolutie
            char dateStr[9]; // dd-mm-yy + null terminator = 9 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
            #else
            // CYD/ESP32-S3: volledig formaat dd-mm-yyyy voor hogere resolutie
            char dateStr[11]; // dd-mm-yyyy + null terminator = 11 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
            #endif
            // Update alleen als datum veranderd is
            if (strcmp(lastDateText, dateStr) != 0) {
                strncpy(lastDateText, dateStr, sizeof(lastDateText) - 1);
                lastDateText[sizeof(lastDateText) - 1] = '\0';
                lv_label_set_text(chartDateLabel, dateStr);
            }
        }
    }
    
    if (chartTimeLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char timeStr[9];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            // Update alleen als tijd veranderd is
            if (strcmp(lastTimeText, timeStr) != 0) {
                strncpy(lastTimeText, timeStr, sizeof(lastTimeText) - 1);
                lastTimeText[sizeof(lastTimeText) - 1] = '\0';
                lv_label_set_text(chartTimeLabel, timeStr);
            }
        }
    }
}

// Helper functie om trend label bij te werken
static void updateTrendLabel()
{
    if (trendLabel == nullptr) return;
    
    // Toon trend alleen als beide availability flags true zijn
    if (hasRet2h && hasRet30m)
    {
        const char* trendText = "";
        lv_color_t trendColor = lv_palette_main(LV_PALETTE_GREY);
        
        // Bepaal of data uit warm-start of live komt
        bool isFromWarmStart = (hasRet2hWarm && hasRet30mWarm) && !(hasRet2hLive && hasRet30mLive);
        bool isFromLive = (hasRet2hLive && hasRet30mLive);
        
        switch (trendState) {
            case TREND_UP:
                trendText = getText("OMHOOG", "UP");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_GREEN); // Groen voor live UP
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
            case TREND_DOWN:
                trendText = getText("OMLAAG", "DOWN");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_RED); // Rood voor live DOWN
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
            case TREND_SIDEWAYS:
            default:
                trendText = getText("VLAK", "SIDEWAYS");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_BLUE); // Blauw voor live SIDEWAYS
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
        }
        
        // Geen "-warm" tekst meer - kleur geeft status aan
        lv_label_set_text(trendLabel, trendText);
        lv_obj_set_style_text_color(trendLabel, trendColor, 0);
    }
    else
    {
        // Toon specifiek wat ontbreekt: 30m of 2h
        uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
        char waitText[24];
        
        if (!hasRet30m) {
            // Warm-up 30m: toon status alleen als warm-start NIET succesvol was
            // Als warm-start succesvol was maar hasRet30m nog false, toon dan warm-start status
            if (hasRet30mWarm) {
                // Warm-start heeft 30m data, maar hasRet30m is nog false (mogelijk bug, toon "--")
                lv_label_set_text(trendLabel, "--");
                lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
            
            // Warm-start was niet succesvol: bereken minuten nodig voor 30m window met ≥80% live
            uint8_t livePct30 = calcLivePctMinuteAverages(30);
            
            if (availableMinutes < 30) {
                // Nog niet genoeg data: toon minuten tot 30
                uint8_t minutesNeeded = 30 - availableMinutes;
                if (language == 1) {
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %um", minutesNeeded);
                } else {
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %um", minutesNeeded);
                }
            } else if (livePct30 < 80) {
                // Genoeg data maar niet genoeg live: toon percentage live
                if (language == 1) {
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %u%%", livePct30);
                } else {
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %u%%", livePct30);
                }
            } else {
                // Zou niet moeten voorkomen (livePct30 >= 80 maar hasRet30m is false)
                lv_label_set_text(trendLabel, "--");
                lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
        } else if (!hasRet2h) {
            // Warm-up 2h: bereken minuten nodig voor 120m window met ≥80% live
            uint8_t livePct120 = calcLivePctMinuteAverages(120);
            
            if (availableMinutes < 120) {
                // Nog niet genoeg data: toon minuten tot 120
                uint8_t minutesNeeded = 120 - availableMinutes;
                if (language == 1) {
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %um", minutesNeeded);
                } else {
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %um", minutesNeeded);
                }
            } else if (livePct120 < 80) {
                // Genoeg data maar niet genoeg live: toon percentage live
                if (language == 1) {
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %u%%", livePct120);
                } else {
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %u%%", livePct120);
                }
            } else {
                // Zou niet moeten voorkomen (livePct120 >= 80 maar hasRet2h is false)
                lv_label_set_text(trendLabel, "--");
                lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
        } else {
            // Beide ontbreken (zou niet moeten voorkomen, maar fallback)
            lv_label_set_text(trendLabel, "--");
            lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            return;
        }
        
        lv_label_set_text(trendLabel, waitText);
        lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    
    // Update warm-start status label (rechts bovenin chart)
    if (warmStartStatusLabel != nullptr) {
        char warmStartText[16];
        if (warmStartStatus == WARMING_UP) {
            snprintf(warmStartText, sizeof(warmStartText), "DATA%u%%", warmStartStats.warmUpProgress);
        } else if (warmStartStatus == LIVE_COLD) {
            snprintf(warmStartText, sizeof(warmStartText), "COLD");
        } else {
            snprintf(warmStartText, sizeof(warmStartText), "LIVE");
        }
        lv_label_set_text(warmStartStatusLabel, warmStartText);
        lv_color_t statusColor = (warmStartStatus == WARMING_UP) ? lv_palette_main(LV_PALETTE_ORANGE) :
                                  (warmStartStatus == LIVE_COLD) ? lv_palette_main(LV_PALETTE_BLUE) :
                                  lv_palette_main(LV_PALETTE_BLUE);
        lv_obj_set_style_text_color(warmStartStatusLabel, statusColor, 0);
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
static void updateBTCEURCard(bool hasNewData)
{
    if (priceTitle[0] != nullptr) {
        lv_label_set_text(priceTitle[0], "BTCEUR");
    }
    
    // Update price label alleen als waarde veranderd is (cache check)
    if (priceLbl[0] != nullptr && (lastPriceLblValue != prices[0] || lastPriceLblValue < 0.0f)) {
        snprintf(priceLblBuffer, sizeof(priceLblBuffer), "%.2f", prices[0]);
        lv_label_set_text(priceLbl[0], priceLblBuffer);
        lastPriceLblValue = prices[0];
    }
    
    // Stel tekstkleur in op basis van nieuwe data: blauw bij nieuwe data, grijs bij oude data
    if (priceLbl[0] != nullptr) {
        if (hasNewData && prices[0] > 0.0f) {
            // Nieuwe data: blauw
            lv_obj_set_style_text_color(priceLbl[0], lv_palette_main(LV_PALETTE_BLUE), 0);
        } else {
            // Oude data: grijs
            lv_obj_set_style_text_color(priceLbl[0], lv_palette_main(LV_PALETTE_GREY), 0);
        }
    }
    
    // Bereken dynamische anchor-waarden op basis van trend voor UI weergave
    AnchorConfigEffective effAnchorUI;
    if (anchorActive && anchorPrice > 0.0f) {
        effAnchorUI = calcEffectiveAnchor(anchorMaxLoss, anchorTakeProfit, trendState);
    }
    
    #ifdef PLATFORM_TTGO
    if (anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Gebruik dynamische take profit waarde
            float takeProfitPrice = anchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                snprintf(anchorMaxLabelBuffer, sizeof(anchorMaxLabelBuffer), "%.2f", takeProfitPrice);
                lv_label_set_text(anchorMaxLabel, anchorMaxLabelBuffer);
                lastAnchorMaxValue = takeProfitPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMaxLabelBuffer) > 0) {
                anchorMaxLabelBuffer[0] = '\0';
                lv_label_set_text(anchorMaxLabel, "");
                lastAnchorMaxValue = -1.0f;
            }
        }
    }
    
    if (anchorLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Update alleen als waarde veranderd is
            if (lastAnchorValue != anchorPrice || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, sizeof(anchorLabelBuffer), "%.2f", anchorPrice);
                lv_label_set_text(anchorLabel, anchorLabelBuffer);
                lastAnchorValue = anchorPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorLabelBuffer) > 0) {
                anchorLabelBuffer[0] = '\0';
                lv_label_set_text(anchorLabel, "");
                lastAnchorValue = -1.0f;
            }
        }
    }
    
    if (anchorMinLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Gebruik dynamische max loss waarde
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                snprintf(anchorMinLabelBuffer, sizeof(anchorMinLabelBuffer), "%.2f", stopLossPrice);
                lv_label_set_text(anchorMinLabel, anchorMinLabelBuffer);
                lastAnchorMinValue = stopLossPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMinLabelBuffer) > 0) {
                anchorMinLabelBuffer[0] = '\0';
                lv_label_set_text(anchorMinLabel, "");
                lastAnchorMinValue = -1.0f;
            }
        }
    }
    #else
    if (anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Toon dynamische take profit waarde (effectief percentage)
            float takeProfitPrice = anchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                snprintf(anchorMaxLabelBuffer, sizeof(anchorMaxLabelBuffer), "+%.2f%% %.2f", effAnchorUI.takeProfitPct, takeProfitPrice);
                lv_label_set_text(anchorMaxLabel, anchorMaxLabelBuffer);
                lastAnchorMaxValue = takeProfitPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMaxLabelBuffer) > 0) {
                anchorMaxLabelBuffer[0] = '\0';
                lv_label_set_text(anchorMaxLabel, "");
                lastAnchorMaxValue = -1.0f;
            }
        }
    }
    
    if (anchorLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f && prices[0] > 0.0f) {
            float anchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
            // Update alleen als waarde veranderd is (check zowel anchorPrice als anchorPct)
            float currentValue = anchorPrice + anchorPct;  // Combinatie voor cache check
            if (lastAnchorValue != currentValue || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, sizeof(anchorLabelBuffer), "%c%.2f%% %.2f",
                         anchorPct >= 0 ? '+' : '-', fabsf(anchorPct), anchorPrice);
                lv_label_set_text(anchorLabel, anchorLabelBuffer);
                lastAnchorValue = currentValue;
            }
        } else if (anchorActive && anchorPrice > 0.0f) {
            // Update alleen als waarde veranderd is
            if (lastAnchorValue != anchorPrice || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, sizeof(anchorLabelBuffer), "%.2f", anchorPrice);
                lv_label_set_text(anchorLabel, anchorLabelBuffer);
                lastAnchorValue = anchorPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorLabelBuffer) > 0) {
                anchorLabelBuffer[0] = '\0';
                lv_label_set_text(anchorLabel, "");
                lastAnchorValue = -1.0f;
            }
        }
    }
    
    if (anchorMinLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Toon dynamische max loss waarde (effectief percentage)
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                snprintf(anchorMinLabelBuffer, sizeof(anchorMinLabelBuffer), "%.2f%% %.2f", effAnchorUI.maxLossPct, stopLossPrice);
                lv_label_set_text(anchorMinLabel, anchorMinLabelBuffer);
                lastAnchorMinValue = stopLossPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMinLabelBuffer) > 0) {
                anchorMinLabelBuffer[0] = '\0';
                lv_label_set_text(anchorMinLabel, "");
                lastAnchorMinValue = -1.0f;
            }
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
            // Format nieuwe tekst
            char newText[64];
            snprintf(newText, sizeof(newText), "%s  %c%.2f%%", symbols[index], pct >= 0 ? '+' : '-', fabsf(pct));
            // Update alleen als tekst veranderd is
            if (strcmp(lastPriceTitleText[index], newText) != 0) {
                strncpy(priceTitleBuffer[index], newText, sizeof(priceTitleBuffer[index]) - 1);
                priceTitleBuffer[index][sizeof(priceTitleBuffer[index]) - 1] = '\0';
                strncpy(lastPriceTitleText[index], newText, sizeof(lastPriceTitleText[index]) - 1);
                lastPriceTitleText[index][sizeof(lastPriceTitleText[index]) - 1] = '\0';
                lv_label_set_text(priceTitle[index], priceTitleBuffer[index]);
            }
        } else {
            // Update alleen als tekst veranderd is
            if (strcmp(lastPriceTitleText[index], symbols[index]) != 0) {
                strncpy(priceTitleBuffer[index], symbols[index], sizeof(priceTitleBuffer[index]) - 1);
                priceTitleBuffer[index][sizeof(priceTitleBuffer[index]) - 1] = '\0';
                strncpy(lastPriceTitleText[index], symbols[index], sizeof(lastPriceTitleText[index]) - 1);
                lastPriceTitleText[index][sizeof(lastPriceTitleText[index]) - 1] = '\0';
                lv_label_set_text(priceTitle[index], priceTitleBuffer[index]);
            }
        }
    }
    
    if (index == 1 && price1MinMaxLabel != nullptr && price1MinMinLabel != nullptr && price1MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInSecondPrices(minVal, maxVal);
        
        if (minVal > 0.0f && maxVal > 0.0f)
        {
            float diff = maxVal - minVal;
            // Update alleen als waarden veranderd zijn
            if (lastPrice1MinMaxValue != maxVal || lastPrice1MinMaxValue < 0.0f) {
                snprintf(price1MinMaxLabelBuffer, sizeof(price1MinMaxLabelBuffer), "%.2f", maxVal);
                lv_label_set_text(price1MinMaxLabel, price1MinMaxLabelBuffer);
                lastPrice1MinMaxValue = maxVal;
            }
            if (lastPrice1MinDiffValue != diff || lastPrice1MinDiffValue < 0.0f) {
                snprintf(price1MinDiffLabelBuffer, sizeof(price1MinDiffLabelBuffer), "%.2f", diff);
                lv_label_set_text(price1MinDiffLabel, price1MinDiffLabelBuffer);
                lastPrice1MinDiffValue = diff;
            }
            if (lastPrice1MinMinValue != minVal || lastPrice1MinMinValue < 0.0f) {
                snprintf(price1MinMinLabelBuffer, sizeof(price1MinMinLabelBuffer), "%.2f", minVal);
                lv_label_set_text(price1MinMinLabel, price1MinMinLabelBuffer);
                lastPrice1MinMinValue = minVal;
            }
        }
        else
        {
            // Update alleen als labels niet "--" zijn
            if (strcmp(price1MinMaxLabelBuffer, "--") != 0) {
                strcpy(price1MinMaxLabelBuffer, "--");
                lv_label_set_text(price1MinMaxLabel, "--");
                lastPrice1MinMaxValue = -1.0f;
            }
            if (strcmp(price1MinDiffLabelBuffer, "--") != 0) {
                strcpy(price1MinDiffLabelBuffer, "--");
                lv_label_set_text(price1MinDiffLabel, "--");
                lastPrice1MinDiffValue = -1.0f;
            }
            if (strcmp(price1MinMinLabelBuffer, "--") != 0) {
                strcpy(price1MinMinLabelBuffer, "--");
                lv_label_set_text(price1MinMinLabel, "--");
                lastPrice1MinMinValue = -1.0f;
            }
        }
    }
    
    if (index == 2 && price30MinMaxLabel != nullptr && price30MinMinLabel != nullptr && price30MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInLast30Minutes(minVal, maxVal);
        
        if (minVal > 0.0f && maxVal > 0.0f)
        {
            float diff = maxVal - minVal;
            // Update alleen als waarden veranderd zijn
            if (lastPrice30MinMaxValue != maxVal || lastPrice30MinMaxValue < 0.0f) {
                snprintf(price30MinMaxLabelBuffer, sizeof(price30MinMaxLabelBuffer), "%.2f", maxVal);
                lv_label_set_text(price30MinMaxLabel, price30MinMaxLabelBuffer);
                lastPrice30MinMaxValue = maxVal;
            }
            if (lastPrice30MinDiffValue != diff || lastPrice30MinDiffValue < 0.0f) {
                snprintf(price30MinDiffLabelBuffer, sizeof(price30MinDiffLabelBuffer), "%.2f", diff);
                lv_label_set_text(price30MinDiffLabel, price30MinDiffLabelBuffer);
                lastPrice30MinDiffValue = diff;
            }
            if (lastPrice30MinMinValue != minVal || lastPrice30MinMinValue < 0.0f) {
                snprintf(price30MinMinLabelBuffer, sizeof(price30MinMinLabelBuffer), "%.2f", minVal);
                lv_label_set_text(price30MinMinLabel, price30MinMinLabelBuffer);
                lastPrice30MinMinValue = minVal;
            }
        }
        else
        {
            // Update alleen als labels niet "--" zijn
            if (strcmp(price30MinMaxLabelBuffer, "--") != 0) {
                strcpy(price30MinMaxLabelBuffer, "--");
                lv_label_set_text(price30MinMaxLabel, "--");
                lastPrice30MinMaxValue = -1.0f;
            }
            if (strcmp(price30MinDiffLabelBuffer, "--") != 0) {
                strcpy(price30MinDiffLabelBuffer, "--");
                lv_label_set_text(price30MinDiffLabel, "--");
                lastPrice30MinDiffValue = -1.0f;
            }
            if (strcmp(price30MinMinLabelBuffer, "--") != 0) {
                strcpy(price30MinMinLabelBuffer, "--");
                lv_label_set_text(price30MinMinLabel, "--");
                lastPrice30MinMinValue = -1.0f;
            }
        }
    }
    
    if (!hasData)
    {
        lv_label_set_text(priceLbl[index], "--");
    }
    else if (averagePrices[index] > 0.0f)
    {
        // Update alleen als waarde veranderd is
        if (lastPriceLblValueArray[index] != averagePrices[index] || lastPriceLblValueArray[index] < 0.0f) {
            snprintf(priceLblBufferArray[index], sizeof(priceLblBufferArray[index]), "%.2f", averagePrices[index]);
            lv_label_set_text(priceLbl[index], priceLblBufferArray[index]);
            lastPriceLblValueArray[index] = averagePrices[index];
        }
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
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
    if (ipLabel != nullptr) {
        if (WiFi.status() == WL_CONNECTED) {
            // ESP32-S3: IP + dBm op één regel (5 spaties tussen IP en dBm)
            static char ipBuffer[32];
            formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
            int rssi = WiFi.RSSI();
            char *ipEnd = ipBuffer + strlen(ipBuffer);
            snprintf(ipEnd, sizeof(ipBuffer) - strlen(ipBuffer), "     %ddBm", rssi); // 5 spaties
            lv_label_set_text(ipLabel, ipBuffer);
        } else {
            lv_label_set_text(ipLabel, "--     --dBm");
        }
    }
    
    if (chartVersionLabel != nullptr) {
        // ESP32-S3: RAM + versie op één regel (5 spaties tussen kB en versie)
        uint32_t freeRAM = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
        static char versionBuffer[16];
        snprintf(versionBuffer, sizeof(versionBuffer), "%ukB     %s", freeRAM, VERSION_STRING); // 5 spaties
        lv_label_set_text(chartVersionLabel, versionBuffer);
    }
    #else
    if (lblFooterLine1 != nullptr) {
        int rssi = 0;
        uint32_t freeRAM = 0;
        
        if (WiFi.status() == WL_CONNECTED) {
            rssi = WiFi.RSSI();
        }
        
        freeRAM = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
        
        // Update alleen als RSSI veranderd is
        if (lastRssiValue != rssi || lastRssiValue == -999) {
            snprintf(footerRssiBuffer, sizeof(footerRssiBuffer), "%ddBm", rssi);
            lv_label_set_text(lblFooterLine1, footerRssiBuffer);
            lastRssiValue = rssi;
        }
        
        if (ramLabel != nullptr) {
            // Update alleen als RAM waarde veranderd is (afgerond op kB)
            if (lastRamValue != freeRAM || lastRamValue == 0) {
                snprintf(footerRamBuffer, sizeof(footerRamBuffer), "%ukB", freeRAM);
                lv_label_set_text(ramLabel, footerRamBuffer);
                lastRamValue = freeRAM;
            }
        }
    }
    
    if (lblFooterLine2 != nullptr) {
        // Geoptimaliseerd: gebruik char array i.p.v. String
        static char ipStr[16] = "--.--.--.--";
        
        if (WiFi.status() == WL_CONNECTED) {
            formatIPAddress(WiFi.localIP(), ipStr, sizeof(ipStr));
        } else {
            safeStrncpy(ipStr, "--.--.--.--", sizeof(ipStr));
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
    
    // Warm-start status label (rechts bovenin chart, zelfde hoogte als trend)
    warmStartStatusLabel = lv_label_create(chart);
    lv_obj_set_style_text_font(warmStartStatusLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(warmStartStatusLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(warmStartStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(warmStartStatusLabel, LV_ALIGN_TOP_RIGHT, 4, -6);
    lv_label_set_text(warmStartStatusLabel, "--");
    
    volatilityLabel = lv_label_create(chart);
    lv_obj_set_style_text_font(volatilityLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(volatilityLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(volatilityLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(volatilityLabel, LV_ALIGN_BOTTOM_LEFT, -4, 6);
    lv_label_set_text(volatilityLabel, "--");
    
    // Platform-specifieke layout voor chart title
    #if !defined(PLATFORM_TTGO) && !defined(PLATFORM_ESP32S3_SUPERMINI)
    chartTitle = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTitle, &lv_font_montserrat_16, 0);
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartTitle, deviceIdBuffer);
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
    lv_obj_set_pos(chartDateLabel, 0, 0); // TTGO: originele positie (geen aanpassing nodig)
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartBeginLettersLabel, deviceIdBuffer);
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, CHART_WIDTH);
    lv_obj_set_pos(chartTimeLabel, 0, 10);
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
    // ESP32-S3: Ruimere layout met datum/tijd zoals CYD, maar met device ID links
    chartDateLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, 180);
    lv_obj_set_pos(chartDateLabel, -2, 4); // 2 pixels naar links
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartBeginLettersLabel, deviceIdBuffer);
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, 240);
    lv_obj_set_pos(chartTimeLabel, 0, 4);
    #else
    // CYD: Ruimere layout met datum/tijd op verschillende posities
    chartDateLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, 180);
    lv_obj_set_pos(chartDateLabel, -2, 4); // 2 pixels naar links
    
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
        
        // Anchor labels alleen voor BTCEUR (i == 0) - CYD/ESP32-S3 layout (met percentages)
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
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    
    if (WiFi.status() == WL_CONNECTED) {
        // Geoptimaliseerd: gebruik char array i.p.v. String
        static char ipBuffer[16];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        lv_label_set_text(ipLabel, ipBuffer);
    } else {
        lv_label_set_text(ipLabel, "--");
    }
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
    // ESP32-S3 Super Mini: IP + dBm links, RAM + versie rechts (één regel, meer horizontale ruimte)
    ipLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(ipLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ipLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    
    if (WiFi.status() == WL_CONNECTED) {
        // ESP32-S3: IP + dBm op één regel (5 spaties tussen IP en dBm)
        static char ipBuffer[32];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        int rssi = WiFi.RSSI();
        char *ipEnd = ipBuffer + strlen(ipBuffer);
        snprintf(ipEnd, sizeof(ipBuffer) - strlen(ipBuffer), "     %ddBm", rssi); // 5 spaties
        lv_label_set_text(ipLabel, ipBuffer);
    } else {
        lv_label_set_text(ipLabel, "--     --dBm");
    }
    
    // Update versie label met RAM info (5 spaties tussen kB en versie)
    uint32_t freeRAM = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
    static char versionBuffer[16];
    snprintf(versionBuffer, sizeof(versionBuffer), "%ukB     %s", freeRAM, VERSION_STRING); // 5 spaties
    lv_label_set_text(chartVersionLabel, versionBuffer);
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
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
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
            if (safeMutexTake(dataMutex, pdMS_TO_TICKS(500), "checkButton price check")) {
                if (prices[0] <= 0.0f) {
                    Serial_println("[Button] Prijs nog niet beschikbaar, haal prijs op...");
                    safeMutexGive(dataMutex, "checkButton price check");
                    // Haal prijs op (buiten mutex om deadlock te voorkomen)
                    fetchPrice();
                    // Wacht even zodat de prijs kan worden opgeslagen
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else {
                    safeMutexGive(dataMutex, "checkButton price available");
                }
            }
        }
        
        // Gebruik helper functie om anchor in te stellen (gebruikt huidige prijs als default)
        if (setAnchorPrice(0.0f)) {
            // Update UI (this will also take the mutex internally)
            updateUI();
        } else {
            Serial_println("[Button] WARN: Kon anchor niet instellen");
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


// Setup helper functions - split setup() into logical sections
static void setupSerialAndDevice()
{
    // Load settings from Preferences
    // Initialize SettingsStore
    settingsStore.begin();
    
    // Fase 4.1: Initialize ApiClient
    apiClient.begin();
    
    // Fase 4.2.1: Initialize PriceData (module structuur)
    // Fase 4.2.5: State variabelen worden geïnitialiseerd in constructor en gesynchroniseerd in begin()
    priceData.begin();  // begin() synchroniseert state met globale variabelen
    
    // Load settings
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
}

static void setupDisplay()
{
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
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI)
    gfx->invertDisplay(false); // TTGO/ESP32-S3 T-Display heeft geen inversie nodig (ST7789)
    #else
    gfx->invertDisplay(true); // Invert colors (as defined in Setup902_CYD28R_2USB.h with TFT_INVERSION_ON)
    #endif
    gfx->fillScreen(RGB565_BLACK);
    setDisplayBrigthness();
    
    // Geef display tijd om te stabiliseren na initialisatie (vooral belangrijk voor CYD displays en ESP32-S3)
    #if !defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI)
    delay(200); // ESP32-S3 heeft extra tijd nodig voor SPI stabilisatie
    #endif
}

static void setupLVGL()
{
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
    
    // Detecteer PSRAM beschikbaarheid
    bool psramAvailable = hasPSRAM();
    
    // Bepaal useDoubleBuffer: board-aware
    bool useDoubleBuffer;
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // CYD zonder PSRAM: force single buffer (geen double buffering)
        useDoubleBuffer = false;  // Altijd false voor CYD zonder PSRAM
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_TTGO)
        // TTGO: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #else
        // Fallback: double buffer alleen met PSRAM
        useDoubleBuffer = psramAvailable;
    #endif
    
    // Bepaal buffer lines per board (compile-time instelbaar voor CYD)
    uint8_t bufLines;
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // CYD zonder PSRAM: compile-time instelbaar (default 4, kan 1/2/4 zijn voor testen)
        // Na geheugenoptimalisaties kunnen we meer buffer gebruiken voor betere performance
        #ifndef CYD_BUF_LINES_NO_PSRAM
        #define CYD_BUF_LINES_NO_PSRAM 4  // Default: 4 regels (was 1->2->4, verhoogd na geheugenoptimalisaties)
        #endif
        if (psramAvailable) {
            bufLines = 40;  // CYD met PSRAM: 40 regels
        } else {
            bufLines = CYD_BUF_LINES_NO_PSRAM;  // CYD zonder PSRAM: compile-time instelbaar
        }
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3 met PSRAM: 30 regels (of fallback kleiner als geen PSRAM)
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;  // ESP32-S3 zonder PSRAM: 2 regels
        }
    #elif defined(PLATFORM_TTGO)
        // TTGO: 30 regels met PSRAM, 2 zonder
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;
        }
    #else
        // Fallback
        bufLines = psramAvailable ? 30 : 2;
    #endif
    
    uint32_t bufSize = screenWidth * bufLines;
    uint8_t numBuffers = useDoubleBuffer ? 2 : 1;  // 1 of 2 buffers afhankelijk van useDoubleBuffer
    size_t bufSizeBytes = bufSize * sizeof(lv_color_t) * numBuffers;
    
    const char* bufferLocation;
    uint32_t freeHeapBefore = ESP.getFreeHeap();
    size_t largestFreeBlockBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Bepaal board naam voor logging
    const char* boardName;
    #if defined(PLATFORM_CYD24)
        boardName = "CYD24";
    #elif defined(PLATFORM_CYD28)
        boardName = "CYD28";
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        boardName = "ESP32-S3";
    #elif defined(PLATFORM_TTGO)
        boardName = "TTGO";
    #else
        boardName = "Unknown";
    #endif
    
    // Alloceer buffer één keer bij init (niet herhaald)
    if (disp_draw_buf == nullptr) {
        if (psramAvailable) {
            // Met PSRAM: probeer eerst SPIRAM allocatie
            disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (disp_draw_buf) {
                bufferLocation = "SPIRAM";
            } else {
                // Fallback naar INTERNAL+DMA als SPIRAM alloc faalt
                Serial.println("[LVGL] SPIRAM allocatie gefaald, valt terug op INTERNAL+DMA");
                bufferLocation = "INTERNAL+DMA (fallback)";
                disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            }
        } else {
            // Zonder PSRAM: gebruik INTERNAL+DMA geheugen (geen DEFAULT)
            bufferLocation = "INTERNAL+DMA";
            disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        
        if (!disp_draw_buf) {
            Serial.printf("[LVGL] FATAL: Draw buffer allocatie gefaald! Vereist: %u bytes\n", bufSizeBytes);
            Serial.printf("[LVGL] Free heap: %u bytes, Largest free block: %u bytes\n", 
                         ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            while (true) {
                /* no need to continue */
            }
        }
        
        disp_draw_buf_size = bufSizeBytes;
        
        // Uitgebreide logging bij boot
        uint32_t freeHeapAfter = ESP.getFreeHeap();
        size_t largestFreeBlockAfter = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        Serial.printf("[LVGL] Board: %s, Display: %ux%u pixels\n", boardName, screenWidth, screenHeight);
        Serial.printf("[LVGL] PSRAM: %s, useDoubleBuffer: %s\n", 
                     psramAvailable ? "yes" : "no", useDoubleBuffer ? "true" : "false");
        Serial.printf("[LVGL] Draw buffer: %u lines, %u pixels, %u bytes (%u buffer%s)\n", 
                     bufLines, bufSize, bufSizeBytes, numBuffers, numBuffers == 1 ? "" : "s");
        Serial.printf("[LVGL] Buffer locatie: %s\n", bufferLocation);
        Serial.printf("[LVGL] Heap: %u -> %u bytes free, Largest block: %u -> %u bytes\n",
                     freeHeapBefore, freeHeapAfter, largestFreeBlockBefore, largestFreeBlockAfter);
    } else {
        Serial.println(F("[LVGL] WARNING: Draw buffer al gealloceerd! (herhaalde allocatie voorkomen)"));
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    // LVGL buffer setup: single of double buffering
    // LVGL 9.0+ verwacht buffer size in BYTES, niet pixels
    size_t bufSizePixels = bufSize;  // Aantal pixels in buffer
    size_t bufSizeBytesPerBuffer = bufSizePixels * sizeof(lv_color_t);  // Bytes per buffer
    
    if (useDoubleBuffer) {
        // Double buffering: beide buffers in dezelfde allocatie
        // bufSizeBytes is al berekend als bufSize * sizeof(lv_color_t) * 2
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSizeBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        // Single buffering: alleen eerste buffer gebruiken (size in bytes)
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSizeBytesPerBuffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }
}

static void setupWatchdog()
{
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
}

static void setupWiFiEventHandlers()
{
    // WiFi event handlers voor reconnect controle
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf("[WiFi] Event: %d\n", event);
        switch(event) {
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                if (wifiInitialized) {
                    Serial.println(F("[WiFi] Verbinding verbroken"));
                    wifiReconnectEnabled = true;
                    lastReconnectAttempt = 0;
                    reconnectAttemptCount = 0; // Reset reconnect counter
                }
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.println(F("[WiFi] Verbonden met AP"));
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
}

static void setupMutex()
{
    // Maak mutex VOOR we het gebruiken (moet eerst aangemaakt worden)
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println(F("[Error] Kon mutex niet aanmaken!"));
    } else {
        Serial.println("[FreeRTOS] Mutex aangemaakt");
    }
}

// Alloceer grote arrays dynamisch voor CYD zonder PSRAM om DRAM overflow te voorkomen
static void allocateDynamicArrays()
{
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    // Voor CYD zonder PSRAM: alloceer arrays dynamisch
    if (!hasPSRAM()) {
        // Alloceer fiveMinutePrices arrays
        fiveMinutePrices = (float *)heap_caps_malloc(SECONDS_PER_5MINUTES * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        fiveMinutePricesSource = (DataSource *)heap_caps_malloc(SECONDS_PER_5MINUTES * sizeof(DataSource), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        
        // Alloceer minuteAverages arrays
        minuteAverages = (float *)heap_caps_malloc(MINUTES_FOR_30MIN_CALC * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        minuteAveragesSource = (DataSource *)heap_caps_malloc(MINUTES_FOR_30MIN_CALC * sizeof(DataSource), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        
        if (!fiveMinutePrices || !fiveMinutePricesSource || !minuteAverages || !minuteAveragesSource) {
            Serial.println(F("[Memory] FATAL: Dynamische array allocatie gefaald!"));
            Serial.printf("[Memory] Free heap: %u bytes\n", ESP.getFreeHeap());
            while (true) {
                /* no need to continue */
            }
        }
        
        // Initialiseer arrays naar 0
        for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
            fiveMinutePrices[i] = 0.0f;
            fiveMinutePricesSource[i] = SOURCE_LIVE;
        }
        for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
            minuteAverages[i] = 0.0f;
            minuteAveragesSource[i] = SOURCE_LIVE;
        }
        
        Serial.printf("[Memory] Dynamische arrays gealloceerd: fiveMinutePrices=%u bytes, minuteAverages=%u bytes\n",
                     SECONDS_PER_5MINUTES * sizeof(float) + SECONDS_PER_5MINUTES * sizeof(DataSource),
                     MINUTES_FOR_30MIN_CALC * sizeof(float) + MINUTES_FOR_30MIN_CALC * sizeof(DataSource));
    }
    #endif
}

static void startFreeRTOSTasks()
{
    // FreeRTOS Tasks voor multi-core processing
    // ESP32-S3 heeft mogelijk meer stack ruimte nodig
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
    const uint32_t apiTaskStack = 10240;  // ESP32-S3: meer stack voor API task
    const uint32_t uiTaskStack = 10240;   // ESP32-S3: meer stack voor UI task
    const uint32_t webTaskStack = 6144;   // ESP32-S3: meer stack voor web task
    #else
    const uint32_t apiTaskStack = 8192;   // ESP32: standaard stack
    const uint32_t uiTaskStack = 8192;    // ESP32: standaard stack
    const uint32_t webTaskStack = 4096;   // ESP32: standaard stack
    #endif
    
    // Core 1: API calls (elke seconde)
    xTaskCreatePinnedToCore(
        apiTask,           // Task function
        "API_Task",        // Task name
        apiTaskStack,      // Stack size (platform-specifiek)
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        1                  // Core 1
    );

    // Core 2: UI updates (elke seconde)
    xTaskCreatePinnedToCore(
        uiTask,            // Task function
        "UI_Task",         // Task name
        uiTaskStack,       // Stack size (platform-specifiek)
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        0                  // Core 0 (Arduino loop core)
    );

    // Core 2: Web server (elke 5 seconden, maar server.handleClient() continu)
    xTaskCreatePinnedToCore(
        webTask,           // Task function
        "Web_Task",        // Task name
        webTaskStack,      // Stack size (platform-specifiek)
        NULL,              // Parameters
        1,                 // Priority
        NULL,              // Task handle
        0                  // Core 0 (Arduino loop core)
    );

    Serial.println("[FreeRTOS] Tasks gestart op Core 1 (API) en Core 0 (UI/Web)");
}

void setup()
{
    // Setup in logical sections for better readability and maintainability
    setupSerialAndDevice();
    setupDisplay();
    setupLVGL();
    setupWatchdog();
    setupWiFiEventHandlers();
    setupMutex();  // Mutex moet vroeg aangemaakt worden, maar tasks starten later
    
    // Alloceer dynamische arrays voor CYD zonder PSRAM (moet voor initialisatie)
    allocateDynamicArrays();
    
    // Initialize source tracking arrays (default: all LIVE, wordt overschreven door warm-start)
    for (uint8_t i = 0; i < SECONDS_PER_MINUTE; i++) {
        secondPricesSource[i] = SOURCE_LIVE;
    }
    for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
        fiveMinutePricesSource[i] = SOURCE_LIVE;
    }
    for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
        minuteAveragesSource[i] = SOURCE_LIVE;
    }
    
    // WiFi connection and initial data fetch (maakt tijdelijk UI aan)
    wifiConnectionAndFetchPrice();
    
    // Fase 4.2.5: Synchroniseer PriceData state na warm-start (als warm-start is uitgevoerd)
    priceData.syncStateFromGlobals();
    
    // Warm-start: Vul buffers met Binance historische data (als WiFi verbonden is)
    if (WiFi.status() == WL_CONNECTED && warmStartEnabled) {
        performWarmStart();
    }
    
    Serial_println("Setup done");
    fetchPrice();
    
    // Build main UI (verwijdert WiFi UI en bouwt hoofd UI)
    buildUI();
    
    // Force LVGL to render immediately after UI creation
    // CYD 2.4 en CYD 2.8 (zonder PSRAM, single buffering): gebruik lv_refr_now() voor directe rendering
    // CYD boards hebben geen PSRAM, dus altijd single buffering
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    if (disp != NULL) {
        lv_refr_now(disp);
        Serial.println(F("[LVGL] Forced immediate refresh (lv_refr_now) voor CYD zonder PSRAM"));
    }
    #else
    // Voor andere platforms, roep timer handler aan om scherm te renderen
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        delay(DELAY_LVGL_RENDER_MS);
    }
    #endif
    
    // Start FreeRTOS tasks NA buildUI() zodat UI elementen bestaan
    startFreeRTOSTasks();
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
        delay(DELAY_RECONNECT_MS);
        
        WiFi.begin();
        
        while (WiFi.status() != WL_CONNECTED && (millis() - connectStart) < connectTimeout) {
            lv_timer_handler();
            delay(DELAY_WIFI_CONNECT_LOOP_MS);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
            tzset();
            connected = true;
            wifiReconnectEnabled = false;
            wifiInitialized = true;
            Serial.println(F("[WiFi] Succesvol verbonden"));
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
        
        delay(DELAY_RECONNECT_MS);
        
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
    
    Serial.println(F("[API Task] Gestart op Core 1"));
    
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
        
        // Timing fix: update lastWakeTime NA de call om drift te voorkomen
        // Als call langer duurt dan interval, reset timing en skip volgende call indien nodig
        unsigned long callEnd = millis();
        unsigned long callDuration = callEnd - callStart;
        
        // Als call langer duurt dan interval, reset timing om opstapeling te voorkomen
        if (callDuration >= UPDATE_API_INTERVAL) {
            // Call duurde langer dan interval - reset timing
            // Bereken hoeveel tijd over is tot volgende interval
            unsigned long timeUntilNextInterval = UPDATE_API_INTERVAL - (callDuration % UPDATE_API_INTERVAL);
            if (timeUntilNextInterval < 50) {
                // Te weinig tijd over - skip deze cycle en wacht tot volgende interval
                lastWakeTime = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(UPDATE_API_INTERVAL - 50)); // Wacht bijna volledige interval
            } else {
                // Genoeg tijd over - wacht resterende tijd
                lastWakeTime = xTaskGetTickCount();
                vTaskDelay(pdMS_TO_TICKS(timeUntilNextInterval));
            }
        } else {
            // Normale timing: gebruik vTaskDelayUntil voor precieze interval timing
            vTaskDelayUntil(&lastWakeTime, frequency);
        }
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
    const TickType_t lvglFrequency = pdMS_TO_TICKS(3); // CYD/ESP32-S3: elke 3ms voor vloeiendere rendering
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
        
        // Geoptimaliseerd: betere mutex timeout handling
        // UI task heeft lagere prioriteit: kortere timeout zodat API task voorrang krijgt
        // Als mutex niet beschikbaar is, skip deze update (UI kan volgende keer opnieuw proberen)
        #ifdef PLATFORM_TTGO
        const TickType_t mutexTimeout = pdMS_TO_TICKS(30); // TTGO: korte timeout
        #else
        const TickType_t mutexTimeout = pdMS_TO_TICKS(50); // CYD/ESP32-S3: korte timeout zodat API task voorrang krijgt
        #endif
        
        static uint32_t uiMutexTimeoutCount = 0;
        if (safeMutexTake(dataMutex, mutexTimeout, "uiTask updateUI"))
        {
            // Reset timeout counter bij succes
            if (uiMutexTimeoutCount > 0) {
                uiMutexTimeoutCount = 0;
            }
            
            updateUI();
            safeMutexGive(dataMutex, "uiTask updateUI");
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
        
        // Verwerk pending anchor setting (asynchroon vanuit web server/MQTT)
        // Thread-safe: kopieer waarde lokaal om race conditions te voorkomen
        bool hasPendingAnchor = false;
        float anchorValueToSet = 0.0f;
        bool useCurrentPrice = false;
        
        // Thread-safe check: volatile variabelen worden atomisch gelezen op ESP32
        // Voorkom te snelle opeenvolgende anchor sets met cooldown (handelt millis() wrap correct af)
        unsigned long now = millis();
        unsigned long lastSet = lastAnchorSetTime; // Lokale kopie voor thread-safety
        
        // Check cooldown: (now - lastSet) werkt correct zelfs bij millis() wrap
        // Als lastSet > now, dan is er een wrap geweest en moeten we doorlaten
        bool cooldownExpired = (lastSet == 0) || 
                               (now >= lastSet && (now - lastSet) >= ANCHOR_SET_COOLDOWN_MS) ||
                               (now < lastSet); // millis() wrap case
        
        if (pendingAnchorSetting.pending) {
            if (cooldownExpired) {
                // Kopieer waarden atomisch (ESP32 garandeert atomic read van 32-bit floats)
                // Lees eerst value en useCurrentPrice, dan reset pending flag
                // Dit voorkomt dat we een incomplete state lezen
                anchorValueToSet = pendingAnchorSetting.value;
                useCurrentPrice = pendingAnchorSetting.useCurrentPrice;
                // Reset flag direct om dubbele verwerking te voorkomen
                pendingAnchorSetting.pending = false;
                hasPendingAnchor = true;
            } else {
                // Te snel na vorige set - wacht nog even (behoud pending flag)
                unsigned long remaining = (lastSet == 0) ? ANCHOR_SET_COOLDOWN_MS :
                    (now >= lastSet) ? (ANCHOR_SET_COOLDOWN_MS - (now - lastSet)) : ANCHOR_SET_COOLDOWN_MS;
                static unsigned long lastLogTime = 0;
                // Log alleen elke 5 seconden om spam te voorkomen
                if (now - lastLogTime > 5000) {
                    Serial_printf("[UI Task] Anchor set cooldown actief: %lu ms (pending request wordt bewaard)\n", remaining);
                    lastLogTime = now;
                }
            }
        }
        
        if (hasPendingAnchor) {
            float valueToSet = useCurrentPrice ? 0.0f : anchorValueToSet;
            // Verwerk vanuit uiTask - dit is veilig omdat we in de main loop thread zijn
            // Gebruik skipNotifications=true om blocking operaties te voorkomen
            if (setAnchorPrice(valueToSet, true, true)) {
                lastAnchorSetTime = now; // Thread-safe write (uiTask is single-threaded voor deze variabele)
                Serial_printf("[UI Task] Anchor set asynchroon: %.2f\n", 
                    useCurrentPrice ? 0.0f : anchorValueToSet);
            } else {
                // Fout bij instellen - log maar probeer niet opnieuw (voorkomt infinite retries)
                // De cooldown voorkomt dat we te snel opnieuw proberen
                Serial_println("[UI Task] WARN: Kon anchor niet instellen (asynchroon) - mutex timeout of geen prijs beschikbaar");
                // Note: pending flag is al gereset, dus dit wordt niet opnieuw geprobeerd tot nieuwe request
            }
        }
        
        // Check physical button (alleen voor TTGO)
        #if HAS_PHYSICAL_BUTTON
        checkButton();
        #endif
        
        // Periodic heap telemetry check (elke 60 seconden)
        checkHeapTelemetry();
        
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
            // mqttReconnectAttemptCount wordt NIET gereset, zodat exponential backoff blijft werken
        } else {
            // Process queued messages when connected
            processMqttQueue();
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        // Probeer MQTT reconnect als WiFi verbonden is (met exponential backoff)
        unsigned long now = millis();
        
        // Bereken reconnect interval met exponential backoff
        unsigned long reconnectInterval = MQTT_RECONNECT_INTERVAL;
        if (mqttReconnectAttemptCount >= MAX_MQTT_RECONNECT_ATTEMPTS) {
            // Exponential backoff: interval verdubbelt bij elke mislukte poging
            // Max backoff: 8x het basis interval (3 extra pogingen = 2^3 = 8x)
            uint8_t backoffMultiplier = 1 << min((mqttReconnectAttemptCount - MAX_MQTT_RECONNECT_ATTEMPTS), 3);
            reconnectInterval = MQTT_RECONNECT_INTERVAL * backoffMultiplier;
        }
        
        if (lastMqttReconnectAttempt == 0 || (now - lastMqttReconnectAttempt >= reconnectInterval)) {
            lastMqttReconnectAttempt = now;
            mqttReconnectAttemptCount++;
            connectMQTT();
        }
    }
    
    // Beheer WiFi reconnect indien nodig
    // Geoptimaliseerd: betere reconnect logica met retry counter en non-blocking timeout
    if (wifiInitialized && wifiReconnectEnabled && WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        
        // Check of we moeten reconnecten (interval verstreken of eerste poging)
        bool shouldReconnect = (lastReconnectAttempt == 0 || (now - lastReconnectAttempt >= RECONNECT_INTERVAL));
        
        // Als we te veel pogingen hebben gedaan, gebruik exponential backoff
        if (reconnectAttemptCount >= MAX_RECONNECT_ATTEMPTS) {
            // Echte exponential backoff: interval verdubbelt bij elke mislukte poging
            // Max backoff: 16x het basis interval (4 extra pogingen = 2^4 = 16x)
            uint8_t backoffMultiplier = 1 << min((reconnectAttemptCount - MAX_RECONNECT_ATTEMPTS), 4);
            unsigned long extendedInterval = RECONNECT_INTERVAL * backoffMultiplier;
            shouldReconnect = (now - lastReconnectAttempt >= extendedInterval);
        }
        
        if (shouldReconnect) {
            reconnectAttemptCount++;
            Serial.printf("[WiFi] Probeer reconnect (poging %u/%u)...\n", reconnectAttemptCount, MAX_RECONNECT_ATTEMPTS);
            
            // Non-blocking disconnect en reconnect
            WiFi.disconnect(false);
            delay(DELAY_RECONNECT_MS); // Kortere delay voor snellere reconnect
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