// Tutorial : https://youtu.be/JqQEG0eipic
// Unified Crypto Monitor - Supports TTGO T-Display and CYD 2.8
// Select platform in platform_config.h

#define LV_CONF_INCLUDE_SIMPLE // Use the lv_conf.h included in this project, to configure see https://docs.lvgl.io/master/get-started/platforms/arduino.html

// Platform config moet als eerste, definieert platform-specifieke instellingen
// MODULE_INCLUDE is NIET gedefinieerd, zodat PINS files worden geïncludeerd
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

// TrendDetector module (Fase 5.1: voor TrendState enum en trend detection)
#include "src/TrendDetector/TrendDetector.h"

// VolatilityTracker module (Fase 5.2: voor VolatilityState enum en volatiliteit berekeningen)
#include "src/VolatilityTracker/VolatilityTracker.h"

// AlertEngine module (Fase 6.1: voor alert detection en notificaties)
#include "src/AlertEngine/AlertEngine.h"

// Memory module (M1: heap telemetry voor geheugenfragmentatie audit)
#include "src/Memory/HeapMon.h"

// Net module (M2: streaming HTTP fetch zonder String allocaties)
#include "src/Net/HttpFetch.h"

// ApiClient module (Fase 6.2: voor geconsolideerde error logging helpers)
#include "src/ApiClient/ApiClient.h"

// UIController module (Fase 8: UI Module refactoring)
#include "src/UIController/UIController.h"

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
// VERSION_STRING wordt gedefinieerd in platform_config.h (beschikbaar voor alle modules)
// Geen fallback nodig omdat platform_config.h altijd wordt geïncludeerd

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
// SYMBOL_COUNT wordt nu gedefinieerd in platform_config.h (per platform)
// Hier alleen een fallback als het nog niet gedefinieerd is
#ifndef SYMBOL_COUNT
#define SYMBOL_COUNT 3  // Fallback default
#endif

// --- API Configuration ---
#define BINANCE_API "https://api.binance.com/api/v3/ticker/price?symbol="  // Binance API endpoint
#define BINANCE_SYMBOL_DEFAULT "BTCEUR"  // Default Binance symbol
// T1: Verhoogde connect/read timeouts voor betere stabiliteit
#define HTTP_CONNECT_TIMEOUT_MS 4000  // Connect timeout (4000ms)
#define HTTP_READ_TIMEOUT_MS 4000     // Read timeout (4000ms)
#define HTTP_TIMEOUT_MS HTTP_READ_TIMEOUT_MS  // Backward compatibility: totale timeout = read timeout

// --- Chart Configuration ---
#define PRICE_RANGE 200         // The range of price for the chart, adjust as needed
#define POINTS_TO_CHART 60      // Number of points on the chart (60 points = 2 minutes at 2000ms API interval)

// --- Timing Configuration ---
#define UPDATE_UI_INTERVAL 1000   // UI update in ms (elke seconde)
#define UPDATE_API_INTERVAL 2000   // API update in ms (verhoogd naar 2000ms voor betere stabiliteit bij retries en langzame netwerken)
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
#define MINUTES_PER_HOUR 60
#define HOURS_FOR_7D 168

// --- Return Calculation Configuration ---
// Aantal waarden nodig voor return berekeningen gebaseerd op UPDATE_API_INTERVAL (2000ms)
// 1 minuut = 60000ms / 2000ms = 30 waarden
// 5 minuten = 300000ms / 2000ms = 150 waarden
#define VALUES_FOR_1MIN_RETURN ((60000UL) / (UPDATE_API_INTERVAL))
#define VALUES_FOR_5MIN_RETURN ((300000UL) / (UPDATE_API_INTERVAL))

// --- CPU Measurement Configuration ---
#define CPU_MEASUREMENT_SAMPLES 20  // Meet over 20 loops voor gemiddelde


// ============================================================================
// Global Variables
// ============================================================================

// Touchscreen functionaliteit volledig verwijderd - CYD's gebruiken nu fysieke boot knop (GPIO 0)

// LVGL Display global variables
// Fase 8: Display state - gebruikt door UIController module
lv_display_t *disp;
lv_color_t *disp_draw_buf = nullptr;  // Draw buffer pointer (één keer gealloceerd bij init)
size_t disp_draw_buf_size = 0;  // Buffer grootte in bytes (voor logging)

// Widgets LVGL global variables
// Fase 8: UI object pointers - gebruikt door UIController module (zie src/UIController/UIController.h)
lv_obj_t *chart;
lv_chart_series_t *dataSeries;     // Blauwe serie voor alle punten
lv_obj_t *lblFooterLine1; // Footer regel 1 (alleen voor CYD: dBm links, RAM rechts)
lv_obj_t *lblFooterLine2; // Footer regel 2 (alleen voor CYD: IP links, versie rechts)
lv_obj_t *ramLabel; // RAM label rechts op regel 1 (alleen voor CYD)

// One card per symbol
// Fase 8: UI object pointers - gebruikt door UIController module
lv_obj_t *priceBox[SYMBOL_COUNT];
lv_obj_t *priceTitle[SYMBOL_COUNT];
lv_obj_t *priceLbl[SYMBOL_COUNT];
lv_obj_t *volumeConfirmLabel;

// FreeRTOS mutex voor data synchronisatie tussen cores
SemaphoreHandle_t dataMutex = NULL;

// S0: FreeRTOS mutex voor netwerk/HTTP operaties (voorkomt gelijktijdige HTTPClient allocaties)
SemaphoreHandle_t gNetMutex = NULL;

// Symbols array - eerste element wordt dynamisch ingesteld via binanceSymbol
// Fase 8: UI data - gebruikt door UIController module
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
char symbolsArray[SYMBOL_COUNT][16] = {"BTCEUR", SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL, SYMBOL_2H_LABEL};
const char *symbols[SYMBOL_COUNT] = {symbolsArray[0], symbolsArray[1], symbolsArray[2], symbolsArray[3]};
#else
char symbolsArray[SYMBOL_COUNT][16] = {"BTCEUR", SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL};
const char *symbols[SYMBOL_COUNT] = {symbolsArray[0], symbolsArray[1], symbolsArray[2]};
#endif
// Fase 6.1: AlertEngine module gebruikt deze variabele (extern declaration in AlertEngine.cpp)
float prices[SYMBOL_COUNT] = {0};
// Fase 6.2: AnchorSystem module gebruikt deze variabele (extern declaration in AnchorSystem.cpp)
float openPrices[SYMBOL_COUNT] = {0};
// Fase 8: UI data - gebruikt door UIController module
float averagePrices[SYMBOL_COUNT] = {0}; // Gemiddelde prijzen voor 1 min en 30 min


// Anchor price (referentie prijs voor koop/verkoop tracking)
// Fase 6.2: AnchorSystem module gebruikt deze variabelen (extern declarations in AnchorSystem.cpp)
float anchorPrice = 0.0f;
float anchorMax = 0.0f;  // Hoogste prijs sinds anchor
float anchorMin = 0.0f;  // Laagste prijs sinds anchor
unsigned long anchorTime = 0;
bool anchorActive = false;
static bool anchorNotificationPending = false;  // Flag voor pending anchor set notificatie (alleen UI)
float anchorTakeProfit = ANCHOR_TAKE_PROFIT_DEFAULT;  // Take profit threshold (%)
float anchorMaxLoss = ANCHOR_MAX_LOSS_DEFAULT;        // Max loss threshold (%)
uint8_t anchorStrategy = 0;  // 0 = handmatig, 1 = conservatief (TP +1.8%, SL -1.2%), 2 = actief (TP +1.2%, SL -0.9%)
bool anchorTakeProfitSent = false;  // Flag om te voorkomen dat take profit meerdere keren wordt verzonden
bool anchorMaxLossSent = false;    // Flag om te voorkomen dat max loss meerdere keren wordt verzonden

// Trend-adaptive anchor settings
// Fase 6.2: AnchorSystem module gebruikt deze variabelen (extern declarations in AnchorSystem.cpp)
bool trendAdaptiveAnchorsEnabled = TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT;
float uptrendMaxLossMultiplier = UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
float uptrendTakeProfitMultiplier = UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;
float downtrendMaxLossMultiplier = DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
float downtrendTakeProfitMultiplier = DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;

// Warm-Start: Data source tracking
// Warm-Start: Enums en structs zijn nu gedefinieerd in WarmStart.h
// WarmStart module (Fase 7.2: wrapper voor status/logging/settings)
#include "src/WarmStart/WarmStart.h"

// Trend detection
// Fase 5.1: TrendState enum verplaatst naar TrendDetector.h
// Forward declaration voor backward compatibility
#include "src/TrendDetector/TrendDetector.h"

// Fase 6.2: AnchorConfigEffective struct verplaatst naar AnchorSystem.h
// Deze struct is nu gedefinieerd in src/AnchorSystem/AnchorSystem.h

// Smart Confluence Mode: State structs voor recente events
// Fase 6.1.1: EventDirection enum en event structs verplaatst naar AlertEngine.h
// Deze worden nu geïncludeerd via AlertEngine.h

// Fase 5.2: EffectiveThresholds struct verplaatst naar VolatilityTracker.h (al geïncludeerd boven)

// WEB-PERF-3: static verwijderd zodat WebServerModule deze variabelen extern kan gebruiken
float ret_2h = 0.0f;  // 2-hour return percentage
float ret_30m = 0.0f;  // 30-minute return percentage (calculated from minuteAverages or warm-start data)
float ret_4h = 0.0f;  // 4-hour return percentage (calculated from API during warm-start)
float ret_1d = 0.0f;  // 1-day return percentage (calculated from API during warm-start)
float ret_7d = 0.0f;  // 7-day return percentage (calculated from API during warm-start or hourly buffer)
// Fase 8: UI state - gebruikt door UIController module
bool hasRet2hWarm = false;  // Flag: ret_2h beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet30mWarm = false;  // Flag: ret_30m beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet4hWarm = false;  // Flag: ret_4h beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet1dWarm = false;  // Flag: ret_1d beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet7dWarm = false;  // Flag: ret_7d beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet2hLive = false;  // Flag: ret_2h kan worden berekend uit live data (minuteIndex >= 120)
bool hasRet30mLive = false;  // Flag: ret_30m kan worden berekend uit live data (minuteIndex >= 30)
bool hasRet4hLive = false;  // Flag: ret_4h kan worden berekend uit live data (hourly buffer >= 4)
// Combined flags: beschikbaar vanuit warm-start OF live data
bool hasRet2h = false;  // hasRet2hWarm || hasRet2hLive
bool hasRet30m = false;  // hasRet30mWarm || hasRet30mLive
bool hasRet4h = false;  // hasRet4hWarm || hasRet4hLive
bool hasRet1d = false;  // hasRet1dWarm (1d alleen via warm-start, geen live berekening)
bool hasRet7d = false;  // hasRet7dWarm (warm-start) of live hourly buffer
// Fase 5.3.17: Globale variabelen voor backward compatibility - modules zijn source of truth
// Deze variabelen worden gesynchroniseerd met TrendDetector module na elke update
// TODO: In toekomstige fase kunnen deze verwijderd worden zodra alle code volledig gemigreerd is
TrendState trendState = TREND_SIDEWAYS;  // Current trend state (backward compatibility)
TrendState previousTrendState = TREND_SIDEWAYS;  // Previous trend state (backward compatibility)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze variabele kan gebruiken
float trendThreshold = TREND_THRESHOLD_DEFAULT;  // Trend threshold (%)

// Fase 5.2: VolatilityState enum verplaatst naar VolatilityTracker.h (al geïncludeerd boven)
// Fase 5.3.17: Globale variabelen voor backward compatibility - modules zijn source of truth
// Deze variabelen worden gesynchroniseerd met VolatilityTracker module na elke update
// TODO: In toekomstige fase kunnen deze verwijderd worden zodra alle code volledig gemigreerd is
float abs1mReturns[VOLATILITY_LOOKBACK_MINUTES];  // Array voor absolute 1m returns
uint8_t volatilityIndex = 0;  // Index voor circulaire buffer
bool volatilityArrayFilled = false;  // Flag om aan te geven of array gevuld is
VolatilityState volatilityState = VOLATILITY_MEDIUM;  // Current volatility state (backward compatibility)
float volatilityLowThreshold = VOLATILITY_LOW_THRESHOLD_DEFAULT;  // Low threshold (%)
float volatilityHighThreshold = VOLATILITY_HIGH_THRESHOLD_DEFAULT;  // High threshold (%)
unsigned long lastTrendChangeNotification = 0;  // Timestamp van laatste trend change notificatie (backward compatibility)

// Smart Confluence Mode state
// Fase 6.1: AlertEngine module gebruikt deze variabele (extern declaration in AlertEngine.cpp)
bool smartConfluenceEnabled = SMART_CONFLUENCE_ENABLED_DEFAULT;
// Fase 6.1: AlertEngine module gebruikt deze variabelen (extern declarations in AlertEngine.cpp)
LastOneMinuteEvent last1mEvent = {EVENT_NONE, 0, 0.0f, false};
LastFiveMinuteEvent last5mEvent = {EVENT_NONE, 0, 0.0f, false};
unsigned long lastConfluenceAlert = 0;  // Timestamp van laatste confluence alert (cooldown)

// Fase 5.2: static verwijderd zodat VolatilityTracker module deze variabelen kan gebruiken
// Auto-Volatility Mode state
bool autoVolatilityEnabled = AUTO_VOLATILITY_ENABLED_DEFAULT;
uint8_t autoVolatilityWindowMinutes = AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT;
float autoVolatilityBaseline1mStdPct = AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT;
float autoVolatilityMinMultiplier = AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT;
float autoVolatilityMaxMultiplier = AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT;
// Fase 5.2: static verwijderd zodat VolatilityTracker module deze variabelen kan gebruiken
float volatility1mReturns[MAX_VOLATILITY_WINDOW_SIZE];  // Sliding window voor 1m returns
uint8_t volatility1mIndex = 0;  // Index voor circulaire buffer
bool volatility1mArrayFilled = false;  // Flag om aan te geven of array gevuld is
// Fase 5.2: static verwijderd zodat VolatilityTracker module deze variabele kan gebruiken
float currentVolFactor = 1.0f;  // Huidige volatility factor
static unsigned long lastVolatilityLog = 0;  // Timestamp van laatste volatility log (voor debug)
#define VOLATILITY_LOG_INTERVAL_MS 300000UL  // Log elke 5 minuten

// Fase 8: UI state - gebruikt door UIController module
uint8_t symbolIndexToChart = 0; // The symbol index to chart
uint32_t maxRange;
uint32_t minRange;
// chartMaxLabel verwijderd - niet meer nodig

// Fase 8: UI object pointers - gebruikt door UIController module (zie src/UIController/UIController.h)
lv_obj_t *chartTitle;     // Label voor chart titel (symbool) - alleen voor CYD
lv_obj_t *chartVersionLabel; // Label voor versienummer (rechts bovenste regel)
lv_obj_t *chartDateLabel; // Label voor datum rechtsboven (vanaf pixel 180)
lv_obj_t *chartTimeLabel; // Label voor tijd rechtsboven
lv_obj_t *chartBeginLettersLabel; // Label voor beginletters (TTGO, links tweede regel)
lv_obj_t *ipLabel; // IP-adres label (TTGO, onderin, gecentreerd)
lv_obj_t *price1MinMaxLabel; // Label voor max waarde in 1 min buffer
lv_obj_t *price1MinMinLabel; // Label voor min waarde in 1 min buffer
lv_obj_t *price1MinDiffLabel; // Label voor verschil tussen max en min in 1 min buffer
lv_obj_t *price30MinMaxLabel; // Label voor max waarde in 30 min buffer
lv_obj_t *price30MinMinLabel; // Label voor min waarde in 30 min buffer
lv_obj_t *price30MinDiffLabel; // Label voor verschil tussen max en min in 30 min buffer
lv_obj_t *price2HMaxLabel = nullptr; // Label voor max waarde in 2h buffer (alleen CYD, nullptr voor andere platforms)
lv_obj_t *price2HMinLabel = nullptr; // Label voor min waarde in 2h buffer (alleen CYD, nullptr voor andere platforms)
lv_obj_t *price2HDiffLabel = nullptr; // Label voor verschil tussen max en min in 2h buffer (alleen CYD, nullptr voor andere platforms)
lv_obj_t *anchorLabel; // Label voor anchor price info (rechts midden, met percentage verschil)
lv_obj_t *anchorMaxLabel; // Label voor "Pak winst" (rechts, groen, boven)
lv_obj_t *anchorMinLabel; // Label voor "Stop loss" (rechts, rood, onder)
static lv_obj_t *anchorDeltaLabel; // Label voor anchor delta % (TTGO, rechts)
lv_obj_t *trendLabel; // Label voor trend weergave
lv_obj_t *warmStartStatusLabel; // Label voor warm-start status weergave (rechts bovenin chart)
lv_obj_t *volatilityLabel; // Label voor volatiliteit weergave
lv_obj_t *mediumTrendLabel; // Label voor medium trend weergave (4h + 1d)
lv_obj_t *longTermTrendLabel; // Label voor lange termijn trend weergave (7d)

// Fase 8: UI state - gebruikt door UIController module
uint32_t lastApiMs = 0; // Time of last api call

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
static char httpResponseBuffer[248];  // Buffer voor HTTP responses (NTFY, etc.) - verkleind van 264 naar 248 bytes (bespaart 16 bytes DRAM)

// M2: Globale herbruikbare buffer voor HTTP responses (voorkomt String allocaties)
// Note: Niet static zodat ApiClient.cpp er toegang toe heeft via extern declaratie in ApiClient.h
// Verkleind van 2048 naar 512 bytes (genoeg voor price responses, ~100 bytes)
char gApiResp[304];  // Verkleind van 320 naar 304 bytes (bespaart 16 bytes DRAM)     // Buffer voor API price responses (M2: streaming)
// gKlinesResp verwijderd: fetchBinanceKlines gebruikt streaming parsing met binanceStreamBuffer

// Streaming buffer voor Binance klines parsing (geen grote heap allocaties)
static char binanceStreamBuffer[560];  // Fixed-size buffer voor chunked JSON parsing - verkleind van 576 naar 560 bytes (bespaart 16 bytes DRAM)

// LVGL UI buffers en cache (voorkomt herhaalde allocaties en onnodige updates)
// Fase 8.6.1: static verwijderd zodat UIController module deze kan gebruiken
char priceLblBuffer[24];  // Buffer voor price label (%.2f format, max: "12345.67" = ~8 chars)
char anchorMaxLabelBuffer[24];  // Buffer voor anchor max label (max: "12345.67" = ~8 chars)
char anchorLabelBuffer[24];  // Buffer voor anchor label (max: "12345.67" = ~8 chars)
char anchorMinLabelBuffer[24];  // Buffer voor anchor min label (max: "12345.67" = ~8 chars)
// Fase 8.6.2: static verwijderd zodat UIController module deze kan gebruiken
char priceTitleBuffer[SYMBOL_COUNT][40];  // Buffers voor price titles (verkleind van 48 naar 40 bytes, bespaart 24 bytes voor CYD)
char price1MinMaxLabelBuffer[20];  // Buffer voor 1m max label (max: "12345.67" = ~8 chars)
char price1MinMinLabelBuffer[20];  // Buffer voor 1m min label (max: "12345.67" = ~8 chars)
char price1MinDiffLabelBuffer[20];  // Buffer voor 1m diff label (max: "12345.67" = ~8 chars)
char price30MinMaxLabelBuffer[20];  // Buffer voor 30m max label (max: "12345.67" = ~8 chars)
char price30MinMinLabelBuffer[20];  // Buffer voor 30m min label (max: "12345.67" = ~8 chars)
char price30MinDiffLabelBuffer[20];  // Buffer voor 30m diff label (max: "12345.67" = ~8 chars)
char price2HMaxLabelBuffer[20];  // Buffer voor 2h max label (max: "12345.67" = ~8 chars, altijd gedefinieerd)
char price2HMinLabelBuffer[20];  // Buffer voor 2h min label (max: "12345.67" = ~8 chars, altijd gedefinieerd)
char price2HDiffLabelBuffer[20];  // Buffer voor 2h diff label (max: "12345.67" = ~8 chars, altijd gedefinieerd)

// Cache laatste waarden (alleen updaten als veranderd)
// Fase 8.6.1: static verwijderd zodat UIController module deze kan gebruiken
float lastPriceLblValue = -1.0f;  // Cache voor price label
float lastAnchorMaxValue = -1.0f;  // Cache voor anchor max
float lastAnchorValue = -1.0f;  // Cache voor anchor
float lastAnchorMinValue = -1.0f;  // Cache voor anchor min
// Fase 8.6.2: static verwijderd zodat UIController module deze kan gebruiken
float lastPrice1MinMaxValue = -1.0f;  // Cache voor 1m max
float lastPrice1MinMinValue = -1.0f;  // Cache voor 1m min
float lastPrice1MinDiffValue = -1.0f;  // Cache voor 1m diff
float lastPrice30MinMaxValue = -1.0f;  // Cache voor 30m max
float lastPrice30MinMinValue = -1.0f;  // Cache voor 30m min
float lastPrice30MinDiffValue = -1.0f;  // Cache voor 30m diff
float lastPrice2HMaxValue = -1.0f;  // Cache voor 2h max (alleen gebruikt voor CYD platforms)
float lastPrice2HMinValue = -1.0f;  // Cache voor 2h min (alleen gebruikt voor CYD platforms)
float lastPrice2HDiffValue = -1.0f;  // Cache voor 2h diff (alleen gebruikt voor CYD platforms)
char lastPriceTitleText[SYMBOL_COUNT][32] = {""};  // Cache voor price titles (max: "30 min  +12.34%" = ~20 chars, verkleind van 48 naar 32 bytes)
char priceLblBufferArray[SYMBOL_COUNT][24];  // Buffers voor average price labels (max: "12345.67" = ~8 chars)
static char footerRssiBuffer[16];  // Buffer voor footer RSSI
static char footerRamBuffer[16];  // Buffer voor footer RAM
// Fase 8.6.2: static verwijderd zodat UIController module deze kan gebruiken
float lastPriceLblValueArray[SYMBOL_COUNT];  // Cache voor average price labels (geïnitialiseerd in setup)
static int32_t lastRssiValue = -999;  // Cache voor RSSI
static uint32_t lastRamValue = 0;  // Cache voor RAM (0 = niet geïnitialiseerd, force update)
char lastVersionText[32] = "";  // Cache voor versie tekst (lege string = force update) - niet static zodat WebServer.cpp er toegang toe heeft
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
// Fase 8.7.1: static verwijderd zodat UIController module deze kan gebruiken
bool newPriceDataAvailable = false;  // Flag om aan te geven of er nieuwe prijsdata is voor grafiek update

// Array van 300 posities voor laatste 300 seconden (5 minuten) - voor ret_5m berekening
// Voor CYD en TTGO zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
// Fase 4.2.3: static verwijderd tijdelijk voor parallelle implementatie
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
float *fiveMinutePrices = nullptr;  // Dynamisch gealloceerd voor CYD/TTGO zonder PSRAM
DataSource *fiveMinutePricesSource = nullptr;  // Dynamisch gealloceerd
#else
float fiveMinutePrices[SECONDS_PER_5MINUTES];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
DataSource fiveMinutePricesSource[SECONDS_PER_5MINUTES];  // Source tracking per sample
#endif
uint16_t fiveMinuteIndex = 0;
bool fiveMinuteArrayFilled = false;

// Array van 120 posities voor laatste 120 minuten (2 uur)
// Elke minuut wordt het gemiddelde van de 60 seconden opgeslagen
// We hebben 60 posities nodig om het gemiddelde van laatste 30 minuten te vergelijken
// met het gemiddelde van de 30 minuten daarvoor (maar we houden 120 voor buffer)
// Voor CYD en TTGO zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
// Fase 4.2.9: static verwijderd zodat PriceData getters deze kunnen gebruiken
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
float *minuteAverages = nullptr;  // Dynamisch gealloceerd voor CYD/TTGO zonder PSRAM
DataSource *minuteAveragesSource = nullptr;  // Dynamisch gealloceerd
#else
float minuteAverages[MINUTES_FOR_30MIN_CALC];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
DataSource minuteAveragesSource[MINUTES_FOR_30MIN_CALC];  // Source tracking per sample
#endif
// Fase 4.2.9: static verwijderd zodat PriceData getters deze kunnen gebruiken
uint8_t minuteIndex = 0;
bool minuteArrayFilled = false;
static unsigned long lastMinuteUpdate = 0;
static float firstMinuteAverage = 0.0f; // Eerste minuut gemiddelde prijs als basis voor 30-min berekening
// Uur-aggregatie buffer voor lange perioden (max 7 dagen)
float *hourlyAverages = nullptr;
uint16_t hourIndex = 0;
bool hourArrayFilled = false;
uint8_t minutesSinceHourUpdate = 0;

// Laatste kline snapshots voor volume/range confirmatie
KlineMetrics lastKline1m;
KlineMetrics lastKline5m;
VolumeRangeStatus lastVolumeRange1m;
VolumeRangeStatus lastVolumeRange5m;

// Warm-Start state
// Fase 9.1.4: static verwijderd zodat WebServerModule deze variabelen kan gebruiken
bool warmStartEnabled = WARM_START_ENABLED_DEFAULT;
uint8_t warmStart1mExtraCandles = WARM_START_1M_EXTRA_CANDLES_DEFAULT;
uint8_t warmStart5mCandles = WARM_START_5M_CANDLES_DEFAULT;
uint8_t warmStart30mCandles = WARM_START_30M_CANDLES_DEFAULT;
uint8_t warmStart2hCandles = WARM_START_2H_CANDLES_DEFAULT;
// Fase 8.5.4: static verwijderd zodat UIController module deze kan gebruiken
WarmStartStatus warmStartStatus = LIVE;  // Default: LIVE (cold start als warm-start faalt)
static unsigned long warmStartCompleteTime = 0;  // Timestamp wanneer systeem volledig LIVE werd
// Fase 8.5.4: static verwijderd zodat UIController module deze kan gebruiken
WarmStartStats warmStartStats = {0, 0, 0, 0, false, false, false, false, WS_MODE_DISABLED, 0};

// Notification settings - NTFY.sh
// Note: NTFY topic wordt dynamisch gegenereerd met ESP32 device ID
// Format: [ESP32-ID]-alert (bijv. 9MK28H3Q-alert)
// ESP32-ID is 8 karakters (Crockford Base32 encoding) voor veilige, unieke identificatie
// Dit voorkomt conflicten tussen verschillende devices

// Language setting (0 = Nederlands, 1 = English)
// DEFAULT_LANGUAGE wordt gedefinieerd in platform_config.h (fallback als er nog geen waarde in Preferences staat)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze variabele kan gebruiken
uint8_t language = DEFAULT_LANGUAGE;  // 0 = Nederlands, 1 = English
uint8_t displayRotation = 0;  // Display rotatie: 0 = normaal, 2 = 180 graden gedraaid

// Forward declarations (moet vroeg in het bestand staan)
static bool enqueueMqttMessage(const char* topic, const char* payload, bool retained);
void publishMqttAnchorEvent(float anchor_price, const char* event_type);
void apiTask(void *parameter);
void uiTask(void *parameter);
void webTask(void *parameter);
void wifiConnectionAndFetchPrice();
void setDisplayBrigthness();

// Settings structs voor betere organisatie
// NOTE: AlertThresholds en NotificationCooldowns zijn nu gedefinieerd in SettingsStore.h

// Instelbare grenswaarden (worden geladen uit Preferences)
// Note: ntfyTopic wordt geïnitialiseerd in loadSettings() met unieke ESP32 ID
// Fase 8.7.1: static verwijderd zodat UIController module deze kan gebruiken
char ntfyTopic[64] = "";  // NTFY topic (max 63 karakters)
// Fase 5.1: static verwijderd zodat TrendDetector module deze variabele kan gebruiken
char binanceSymbol[16] = BINANCE_SYMBOL_DEFAULT;  // Binance symbool (max 15 karakters, bijv. BTCEUR, BTCUSDT)

// Alert thresholds in struct voor betere organisatie
// Fase 6.1: AlertEngine module gebruikt deze struct (extern declaration in AlertEngine.cpp)
AlertThresholds alertThresholds = {
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
// Fase 6.1: AlertEngine module gebruikt deze struct (extern declaration in AlertEngine.cpp)
NotificationCooldowns notificationCooldowns = {
    .cooldown1MinMs = NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT,
    .cooldown30MinMs = NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT,
    .cooldown5MinMs = NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT
};

// 2-hour alert thresholds in struct voor betere organisatie
// Wordt gebruikt door AlertEngine voor 2h notificaties
Alert2HThresholds alert2HThresholds = {
    .breakMarginPct = 0.15f,
    .breakResetMarginPct = 0.10f,
    .breakCooldownMs = 30UL * 60UL * 1000UL, // 30 min
    .meanMinDistancePct = 0.60f,
    .meanTouchBandPct = 0.10f,
    .meanCooldownMs = 60UL * 60UL * 1000UL, // 60 min
    .compressThresholdPct = 0.80f,
    .compressResetPct = 1.10f,
    .compressCooldownMs = 2UL * 60UL * 60UL * 1000UL, // 2 uur
    .anchorOutsideMarginPct = 0.25f,
    .anchorCooldownMs = 3UL * 60UL * 60UL * 1000UL // 3 uur
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

// Fase 6.1: AlertEngine module gebruikt deze variabelen (extern declarations in AlertEngine.cpp)
unsigned long lastNotification1Min = 0;
unsigned long lastNotification30Min = 0;
unsigned long lastNotification5Min = 0;

// Max alerts per uur tracking
uint8_t alerts1MinThisHour = 0;
uint8_t alerts30MinThisHour = 0;
uint8_t alerts5MinThisHour = 0;
unsigned long hourStartTime = 0; // Starttijd van het huidige uur

// Web server voor instellingen
WebServer server(80);

// SettingsStore instance
SettingsStore settingsStore;

// WarmStartWrapper instance (Fase 7.2: wrapper voor status/logging/settings)
static WarmStartWrapper warmWrap;

// ApiClient instance (Fase 4.1 voltooid)
#include "src/ApiClient/ApiClient.h"
ApiClient apiClient;

// PriceData instance (Fase 4.2.1: module structuur aangemaakt)
PriceData priceData;

// TrendDetector instance (Fase 5.1: trend detection module)
TrendDetector trendDetector;

// VolatilityTracker instance (Fase 5.2: volatiliteit tracking module)
VolatilityTracker volatilityTracker;

// AlertEngine instance (Fase 6.1.1: alert detection module - basis structuur)
AlertEngine alertEngine;

// AnchorSystem instance (Fase 6.2.1: anchor price tracking module - basis structuur)
#include "src/AnchorSystem/AnchorSystem.h"
AnchorSystem anchorSystem;

// UIController instance (Fase 8.1.1: UI Module refactoring)
UIController uiController;

// WebServerModule (Fase 9: Web Interface Module refactoring)
// Include moet na WebServer.h library include (regel 13)
#include "src/WebServer/WebServer.h"

// WebServerModule instance (Fase 9.1.1: Web Interface Module refactoring)
WebServerModule webServerModule;

// MQTT configuratie (instelbaar via web interface)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze variabelen kan gebruiken
char mqttHost[64] = MQTT_HOST_DEFAULT;    // MQTT broker IP
uint16_t mqttPort = MQTT_PORT_DEFAULT;    // MQTT poort
char mqttUser[64] = MQTT_USER_DEFAULT;     // MQTT gebruiker
char mqttPass[64] = MQTT_PASS_DEFAULT;     // MQTT wachtwoord
// MQTT_CLIENT_ID_PREFIX wordt nu dynamisch gegenereerd op basis van NTFY topic
// (niet meer nodig als macro, wordt nu direct in connectMQTT() gegenereerd)

WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;

// MQTT Message Queue - voorkomt message loss bij disconnect
#define MQTT_QUEUE_SIZE 8  // Max aantal berichten in queue
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
bool queueAnchorSetting(float value, bool useCurrentPrice) {
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze variabele kan gebruiken
uint8_t mqttReconnectAttemptCount = 0;
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
int fetchBinanceKlines(const char* symbol, const char* interval, uint16_t limit, float* prices, unsigned long* timestamps, uint16_t maxCount, float* highs = nullptr, float* lows = nullptr, float* volumes = nullptr)
{
    if (symbol == nullptr || interval == nullptr || prices == nullptr || maxCount == 0) {
        return -1;
    }
    
    // M1: Heap telemetry vóór URL build
    logHeap("KLINES_URL_BUILD");
    
    // Build URL
    char url[256];
    int urlLen = snprintf(url, sizeof(url), "%s?symbol=%s&interval=%s&limit=%u", 
                         BINANCE_KLINES_API, symbol, interval, limit);
    if (urlLen < 0 || urlLen >= (int)sizeof(url)) {
        return -1;
    }
    
    // M1: Heap telemetry vóór HTTP GET
    logHeap("KLINES_GET_PRE");
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("fetchBinanceKlines");
    
    int result = -1;
    HTTPClient http;
    
    // S2: do-while(0) patroon voor consistente cleanup
    do {
        // N1: Expliciete connect/read timeout settings
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
        http.setTimeout(WARM_START_TIMEOUT_MS > HTTP_READ_TIMEOUT_MS ? WARM_START_TIMEOUT_MS : HTTP_READ_TIMEOUT_MS);
    http.setReuse(false);
        
        unsigned long requestStart = millis();
    
        // N2: Voeg User-Agent header toe VOOR http.begin() om Cloudflare blocking te voorkomen
        // Headers moeten worden toegevoegd voordat de verbinding wordt geopend
        http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
        http.addHeader(F("Accept"), F("application/json"));
    
    if (!http.begin(url)) {
            Serial.println(F("[Klines] http.begin() gefaald"));
            break;
    }
    
    int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
        
        // M1: Heap telemetry na HTTP GET
        logHeap("KLINES_GET_POST");
        
    if (code != 200) {
            // Fase 6.2: Geconsolideerde error logging - gebruik ApiClient helpers
            const char* phase = ApiClient::detectHttpErrorPhase(code);
            ApiClient::logHttpError(code, phase, requestTime, 0, 1, "[Klines]");
            break;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    if (stream == nullptr) {
            Serial.println(F("[Klines] Stream pointer is null"));
            break;
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
    float highPrice = 0.0f;
    float lowPrice = 0.0f;
    float closePrice = 0.0f;
    float volume = 0.0f;
    KlineMetrics lastParsedKline = {};
    float volumeValue = 0.0f;
    
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
    
    // M1: Heap telemetry vóór JSON parse
    logHeap("KLINES_PARSE_PRE");
    
    // Parse streaming JSON
    // Continue zolang stream connected/available OF er nog data in buffer is
    while (stream->connected() || stream->available() || (bufferPos < bufferLen)) {
        // Timeout check: stop als parsing te lang duurt
        if ((millis() - parseStartTime) > PARSE_TIMEOUT_MS) {
            break;
        }
        // Feed watchdog periodiek (alleen elke seconde) en update LVGL spinner
        if ((millis() - lastWatchdogFeed) >= WATCHDOG_FEED_INTERVAL) {
            yield();
            delay(0);
            lv_timer_handler();  // Update spinner animatie tijdens warm-start
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
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    closePrice = 0.0f;
                    volume = 0.0f;
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    volumeValue = 0.0f;
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
                    } else if (fieldIdx == 2 || fieldIdx == 3 || fieldIdx == 4 || fieldIdx == 5) {
                        float value;
                        if (safeAtof(fieldBuf, value)) {
                            if (fieldIdx == 2 && isValidPrice(value)) {
                                highPrice = value;
                            } else if (fieldIdx == 3 && isValidPrice(value)) {
                                lowPrice = value;
                            } else if (fieldIdx == 4 && isValidPrice(value)) {
                                closePrice = value;
                            } else if (fieldIdx == 5 && value >= 0.0f) {
                                volume = value;
                            }
                        }
                    } else if (fieldIdx == 5) {
                        // volume (6th field, index 5)
                        float volume;
                        if (safeAtof(fieldBuf, volume) && volume >= 0.0f) {
                            volumeValue = volume;
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
                        if (fieldIdx == 2 || fieldIdx == 3 || fieldIdx == 4 || fieldIdx == 5) {
                            float value;
                            if (safeAtof(fieldBuf, value)) {
                                if (fieldIdx == 2 && isValidPrice(value)) {
                                    highPrice = value;
                                } else if (fieldIdx == 3 && isValidPrice(value)) {
                                    lowPrice = value;
                                } else if (fieldIdx == 4 && isValidPrice(value)) {
                                    closePrice = value;
                                } else if (fieldIdx == 5 && value >= 0.0f) {
                                    volume = value;
                                }
                            }
                        } else if (fieldIdx == 5) {
                            float volume;
                            if (safeAtof(fieldBuf, volume) && volume >= 0.0f) {
                                volumeValue = volume;
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
                    if (highs != nullptr) {
                        highs[writeIdx] = highPrice;
                    }
                    if (lows != nullptr) {
                        lows[writeIdx] = lowPrice;
                    }
                    if (volumes != nullptr) {
                        volumes[writeIdx] = volumeValue;
                    }
                    
                    writeIdx++;
                    if (writeIdx >= (int)maxCount) {
                        writeIdx = 0;  // Wrap around
                        bufferFilled = true;
                    }
                    totalParsed++;
                    
                    if (highPrice > 0.0f && lowPrice > 0.0f && highPrice >= lowPrice) {
                        lastParsedKline.high = highPrice;
                        lastParsedKline.low = lowPrice;
                        lastParsedKline.close = closePrice;
                        lastParsedKline.volume = volume;
                        lastParsedKline.openTime = openTime;
                        lastParsedKline.valid = true;
                    }
                    
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
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    closePrice = 0.0f;
                    volume = 0.0f;
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    volumeValue = 0.0f;
                } else if (c == ']') {
                    // End of outer array
                    goto parse_done;
                } else if (c == '[') {
                    // Next entry
                    state = PS_ENTRY_START;
                    fieldIdx = 0;
                    fieldBufIdx = 0;
                    openTime = 0;
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    closePrice = 0.0f;
                    volume = 0.0f;
                    highPrice = 0.0f;
                    lowPrice = 0.0f;
                    volumeValue = 0.0f;
                }
                break;
        }
    }
    
parse_done:
    // M1: Heap telemetry na JSON parse
    logHeap("KLINES_PARSE_POST");
    
    if (lastParsedKline.valid && interval != nullptr) {
        if (strcmp(interval, "1m") == 0) {
            lastKline1m = lastParsedKline;
        } else if (strcmp(interval, "5m") == 0) {
            lastKline5m = lastParsedKline;
        }
    }
    
    // Bereken resultaat
    int storedCount = bufferFilled ? (int)maxCount : writeIdx;
    result = storedCount;  // S2: Zet result voordat do-while eindigt
    
    if (bufferFilled && writeIdx > 0) {
        // Buffer is gewrapped: [writeIdx..maxCount-1, 0..writeIdx-1] -> [0..maxCount-1]
        // Voor kleine buffers: gebruik stack temp (max 60)
        if (writeIdx <= 60 && maxCount <= 120) {
            float tempReorder[60];  // Max 60 floats = 240 bytes (veilig voor stack)
            float tempReorderHighs[60];
            float tempReorderLows[60];
            float tempReorderVolumes[60];
            unsigned long tempReorderTimes[60];
            
            // Kopieer eerste deel (0..writeIdx-1) naar temp
            for (int i = 0; i < writeIdx; i++) {
                tempReorder[i] = prices[i];
                if (timestamps != nullptr) {
                    tempReorderTimes[i] = timestamps[i];
                }
                if (highs != nullptr) {
                    tempReorderHighs[i] = highs[i];
                }
                if (lows != nullptr) {
                    tempReorderLows[i] = lows[i];
                }
                if (volumes != nullptr) {
                    tempReorderVolumes[i] = volumes[i];
                }
            }
            // Verschuif tweede deel (writeIdx..maxCount-1) naar begin
            for (int i = 0; i < (int)maxCount - writeIdx; i++) {
                prices[i] = prices[writeIdx + i];
                if (timestamps != nullptr) {
                    timestamps[i] = timestamps[writeIdx + i];
                }
                if (highs != nullptr) {
                    highs[i] = highs[writeIdx + i];
                }
                if (lows != nullptr) {
                    lows[i] = lows[writeIdx + i];
                }
                if (volumes != nullptr) {
                    volumes[i] = volumes[writeIdx + i];
                }
            }
            // Kopieer eerste deel naar einde
            for (int i = 0; i < writeIdx; i++) {
                prices[(int)maxCount - writeIdx + i] = tempReorder[i];
                if (timestamps != nullptr) {
                    timestamps[(int)maxCount - writeIdx + i] = tempReorderTimes[i];
                }
                if (highs != nullptr) {
                    highs[(int)maxCount - writeIdx + i] = tempReorderHighs[i];
                }
                if (lows != nullptr) {
                    lows[(int)maxCount - writeIdx + i] = tempReorderLows[i];
                }
                if (volumes != nullptr) {
                    volumes[(int)maxCount - writeIdx + i] = tempReorderVolumes[i];
                }
            }
        } else {
            // Voor grote buffers: gebruik heap allocatie
            float* tempFull = (float*)malloc(maxCount * sizeof(float));
            float* tempFullHighs = (highs != nullptr) ? (float*)malloc(maxCount * sizeof(float)) : nullptr;
            float* tempFullLows = (lows != nullptr) ? (float*)malloc(maxCount * sizeof(float)) : nullptr;
            float* tempFullVolumes = (volumes != nullptr) ? (float*)malloc(maxCount * sizeof(float)) : nullptr;
            unsigned long* tempFullTimes = (timestamps != nullptr) ? (unsigned long*)malloc(maxCount * sizeof(unsigned long)) : nullptr;
            
            if (tempFull != nullptr) {
                // Kopieer hele buffer
                for (uint16_t i = 0; i < maxCount; i++) {
                    tempFull[i] = prices[i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        tempFullTimes[i] = timestamps[i];
                    }
                    if (tempFullHighs != nullptr && highs != nullptr) {
                        tempFullHighs[i] = highs[i];
                    }
                    if (tempFullLows != nullptr && lows != nullptr) {
                        tempFullLows[i] = lows[i];
                    }
                    if (tempFullVolumes != nullptr && volumes != nullptr) {
                        tempFullVolumes[i] = volumes[i];
                    }
                }
                // Herschik: [writeIdx..maxCount-1, 0..writeIdx-1] -> [0..maxCount-1]
                for (uint16_t i = 0; i < maxCount - writeIdx; i++) {
                    prices[i] = tempFull[writeIdx + i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        timestamps[i] = tempFullTimes[writeIdx + i];
                    }
                    if (tempFullHighs != nullptr && highs != nullptr) {
                        highs[i] = tempFullHighs[writeIdx + i];
                    }
                    if (tempFullLows != nullptr && lows != nullptr) {
                        lows[i] = tempFullLows[writeIdx + i];
                    }
                    if (tempFullVolumes != nullptr && volumes != nullptr) {
                        volumes[i] = tempFullVolumes[writeIdx + i];
                    }
                }
                for (uint16_t i = 0; i < writeIdx; i++) {
                    prices[(int)maxCount - writeIdx + i] = tempFull[i];
                    if (tempFullTimes != nullptr && timestamps != nullptr) {
                        timestamps[(int)maxCount - writeIdx + i] = tempFullTimes[i];
                    }
                    if (tempFullHighs != nullptr && highs != nullptr) {
                        highs[(int)maxCount - writeIdx + i] = tempFullHighs[i];
                    }
                    if (tempFullLows != nullptr && lows != nullptr) {
                        lows[(int)maxCount - writeIdx + i] = tempFullLows[i];
                    }
                    if (tempFullVolumes != nullptr && volumes != nullptr) {
                        volumes[(int)maxCount - writeIdx + i] = tempFullVolumes[i];
                    }
                }
                free(tempFull);
                if (tempFullTimes != nullptr) free(tempFullTimes);
                if (tempFullHighs != nullptr) free(tempFullHighs);
                if (tempFullLows != nullptr) free(tempFullLows);
                if (tempFullVolumes != nullptr) free(tempFullVolumes);
            }
            // Bij heap allocatie failure: buffer blijft in wrapped volgorde (geen probleem)
        }
    }
    
    } while(0);
    
    // C2: ALTIJD cleanup (ook bij code<0, code!=200, parse error)
    // Hard close: http.end() + client.stop() voor volledige cleanup
    http.end();
    WiFiClient* stream = http.getStreamPtr();
    if (stream != nullptr) {
        stream->stop();
    }
    
    // C2: Geef netwerk mutex vrij (met debug logging)
    netMutexUnlock("fetchBinanceKlines");
    
    return result;
}

// Helper: Clamp waarde tussen min en max
static uint16_t clampUint16(uint16_t value, uint16_t minVal, uint16_t maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// Helper: Detecteer PSRAM beschikbaarheid (runtime check)
// Fase 8: Helper functie - gebruikt door UIController module
bool hasPSRAM()
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
    // M1: Heap telemetry vóór warm-start
    logHeap("WARMSTART_PRE");
    
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
    lv_timer_handler();  // Update spinner animatie vóór fetch
    int count1m = fetchBinanceKlines(binanceSymbol, "1m", req1mCandles, temp1mPrices, nullptr, SECONDS_PER_MINUTE);
    lv_timer_handler();  // Update spinner animatie na fetch
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
    lv_timer_handler();  // Update spinner animatie vóór fetch
    int count5m = fetchBinanceKlines(binanceSymbol, "5m", req5mCandles, temp5mPrices, nullptr, 2);
    lv_timer_handler();  // Update spinner animatie na fetch
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
            lv_timer_handler();  // Update spinner animatie
        }
        lv_timer_handler();  // Update spinner animatie vóór fetch
        count30m = fetchBinanceKlines(binanceSymbol, "30m", req30mCandles, temp30mPrices, nullptr, 2);
        lv_timer_handler();  // Update spinner animatie na fetch
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
    
    // Feed watchdog en update LVGL (spinner animatie)
    yield();
    delay(0);
    lv_timer_handler();
    
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
            lv_timer_handler();  // Update spinner animatie
        }
        lv_timer_handler();  // Update spinner animatie vóór fetch
        count2h = fetchBinanceKlines(binanceSymbol, "2h", req2hCandles, temp2hPrices, nullptr, 2);
        lv_timer_handler();  // Update spinner animatie na fetch
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
    
    // 5. Haal 4h candles op voor lange termijn trend
    float temp4hPrices[2];
    int count4h = 0;
    const int maxRetries4h = 3;
    for (int retry = 0; retry < maxRetries4h; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 4h retry %d/%d...\n"), retry, maxRetries4h - 1);
            yield();
            delay(500);
            lv_timer_handler();
        }
        lv_timer_handler();
        count4h = fetchBinanceKlines(binanceSymbol, "4h", 2, temp4hPrices, nullptr, 2);
        lv_timer_handler();
        if (count4h >= 2) {
            break;
        }
    }
    
    if (count4h >= 2) {
        float firstPrice = temp4hPrices[0];
        float lastPrice = temp4hPrices[count4h - 1];
        if (firstPrice > 0.0f && lastPrice > 0.0f) {
            ret_4h = ((lastPrice - firstPrice) / firstPrice) * 100.0f;
            hasRet4hWarm = true;
        } else {
            hasRet4hWarm = false;
        }
    } else {
        hasRet4hWarm = false;
    }
    
    // 6. Haal 1d candles op voor lange termijn trend
    float temp1dPrices[2];
    int count1d = 0;
    const int maxRetries1d = 3;
    for (int retry = 0; retry < maxRetries1d; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 1d retry %d/%d...\n"), retry, maxRetries1d - 1);
            yield();
            delay(500);
            lv_timer_handler();
        }
        lv_timer_handler();
        count1d = fetchBinanceKlines(binanceSymbol, "1d", 2, temp1dPrices, nullptr, 2);
        lv_timer_handler();
        if (count1d >= 2) {
            break;
        }
    }
    
    if (count1d >= 2) {
        float firstPrice = temp1dPrices[0];
        float lastPrice = temp1dPrices[count1d - 1];
        if (firstPrice > 0.0f && lastPrice > 0.0f) {
            ret_1d = ((lastPrice - firstPrice) / firstPrice) * 100.0f;
            hasRet1dWarm = true;
        } else {
            hasRet1dWarm = false;
        }
    } else {
        hasRet1dWarm = false;
    }

    // 7. Haal 1w candles op voor lange termijn trend
    float temp1wPrices[2];
    int count1w = 0;
    const int maxRetries1w = 3;
    for (int retry = 0; retry < maxRetries1w; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 1w retry %d/%d...\n"), retry, maxRetries1w - 1);
            yield();
            delay(500);
            lv_timer_handler();
        }
        lv_timer_handler();
        count1w = fetchBinanceKlines(binanceSymbol, "1w", 2, temp1wPrices, nullptr, 2);
        lv_timer_handler();
        if (count1w >= 2) {
            break;
        }
    }

    if (count1w >= 2) {
        float firstPrice = temp1wPrices[0];
        float lastPrice = temp1wPrices[count1w - 1];
        if (firstPrice > 0.0f && lastPrice > 0.0f) {
            ret_7d = ((lastPrice - firstPrice) / firstPrice) * 100.0f;
            hasRet7dWarm = true;
        } else {
            hasRet7dWarm = false;
        }
    } else {
        hasRet7dWarm = false;
    }
    
    // Update combined flags na warm-start
    hasRet2h = hasRet2hWarm || hasRet2hLive;
    hasRet30m = hasRet30mWarm || hasRet30mLive;
    hasRet4h = hasRet4hWarm;  // 4h alleen via warm-start
    hasRet1d = hasRet1dWarm;  // 1d alleen via warm-start
    hasRet7d = hasRet7dWarm;  // 7d via warm-start of live hourly buffer
    
    // Fase 5.1: Bepaal trend state op basis van warm-start data (gebruik TrendDetector module)
    if (hasRet2h && hasRet30m) {
        extern float trendThreshold;
        // Fase 5.3.15: Update module eerst, synchroniseer dan globale variabele
        TrendState newTrendState = trendDetector.determineTrendState(ret_2h, ret_30m, trendThreshold);
        trendDetector.setTrendState(newTrendState);  // Update TrendDetector state
        trendState = newTrendState;  // Synchroniseer globale variabele
    }
    
    // Bepaal mode op basis van score: ok1m, ok5m, ok30m, ok2h
    // FULL: alle true (alle timeframes succesvol geladen)
    // PARTIAL: ok1m true maar >=1 van de others false (1m OK maar 5m/30m/2h niet volledig)
    // FAILED: ok1m false (1m gefaald, others ook false)
    bool ok1m = warmStartStats.warmStartOk1m;
    bool ok5m = warmStartStats.warmStartOk5m;
    bool ok30m = warmStartStats.warmStartOk30m;
    bool ok2h = warmStartStats.warmStartOk2h;
    
    if (ok1m && ok5m && ok30m && ok2h) {
        // Alle timeframes succesvol geladen
        warmStartStats.mode = WS_MODE_FULL;
        warmStartStatus = WARMING_UP;
    } else if (ok1m) {
        // 1m OK maar minstens één van 5m/30m/2h gefaald
        warmStartStats.mode = WS_MODE_PARTIAL;
        warmStartStatus = WARMING_UP;
    } else {
        // 1m gefaald (en others ook false)
        warmStartStats.mode = WS_MODE_FAILED;
        warmStartStatus = LIVE_COLD;
    }
    
    // Log score voor debugging
    Serial_printf(F("[WarmStart] Score: 1m=%d 5m=%d 30m=%d 2h=%d -> mode=%s\n"),
                  ok1m ? 1 : 0, ok5m ? 1 : 0, ok30m ? 1 : 0, ok2h ? 1 : 0,
                  (warmStartStats.mode == WS_MODE_FULL) ? "FULL" :
                  (warmStartStats.mode == WS_MODE_PARTIAL) ? "PARTIAL" :
                  (warmStartStats.mode == WS_MODE_FAILED) ? "FAILED" : "DISABLED");
    
    // Compacte boot log regel (gedetailleerde logging gebeurt in WarmStartWrapper)
    // Deze regel blijft voor backward compatibility en snelle boot overview
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
    Serial.print(hasRet2h ? 1 : 0);  // Combined flag (warm || live)
    Serial.print(F(", hasRet30m="));
    Serial.print(hasRet30m ? 1 : 0);  // Combined flag (warm || live)
    Serial.print(F(", ret2h="));
    Serial.print(ret_2h, 3);
    Serial.print(F(", ret30m="));
    Serial.print(ret_30m, 3);
    Serial.println(F(")"));
    Serial.flush();
    
    // Fail-safe: als warm-start gefaald is, ga door als cold start
    if (warmStartStats.mode == WS_MODE_FAILED) {
        warmStartStatus = LIVE_COLD;
        hasRet2h = false;
        hasRet30m = false;
        Serial.println(F("[WarmStart] Warm-start gefaald, ga door als cold start (LIVE_COLD)"));
    }
    
    // Auto Anchor update wordt UITGESTELD tot na FreeRTOS tasks zijn gestart
    // Dit voorkomt race conditions en mutex priority inheritance problemen
    Serial.printf("[WarmStart] Auto anchor update uitgesteld tot na tasks start (mode=%d)\n", alert2HThresholds.anchorSourceMode);
    
    // M1: Heap telemetry na warm-start (gebruik nieuwe logHeap i.p.v. oude logHeapTelemetry)
    // logHeapTelemetry("warm-start");  // Vervangen door logHeap("WARMSTART_POST") bovenaan functie
    
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
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("sendNtfyNotification");
    
    bool ok = false;
    HTTPClient http;
    
    // S2: do-while(0) patroon voor consistente cleanup
    do {
        // S2: Expliciete timeout settings
    http.setTimeout(5000);
        http.setReuse(false);
    
        if (!http.begin(url)) {
        Serial_println(F("[Notify] Ntfy HTTP begin gefaald"));
            break;
    }
    
    http.addHeader("Title", title);
    http.addHeader("Priority", "high");
    
    // Voeg kleur tag toe als opgegeven
        if (colorTag != nullptr && strlen(colorTag) > 0) {
            if (strlen(colorTag) <= 64) { // Valideer lengte
            http.addHeader(F("Tags"), colorTag);
            Serial_printf(F("[Notify] Ntfy Tag: %s\n"), colorTag);
        }
    }
    
    Serial_println(F("[Notify] Ntfy POST versturen..."));
    int code = http.POST(message);
    
    // Haal response alleen op bij succes (bespaar geheugen)
    // Gebruik static buffer i.p.v. String om fragmentatie te voorkomen
        if (code == 200 || code == 201) {
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
                // M2: Fallback: stream niet beschikbaar, lees response body direct
                // Voor POST responses kunnen we niet httpGetToBuffer() gebruiken
                // In plaats daarvan lezen we de response body in chunks
                size_t totalLen = 0;
                const size_t CHUNK_SIZE = 256;
                while (http.connected() && totalLen < (sizeof(httpResponseBuffer) - 1)) {
                    size_t remaining = sizeof(httpResponseBuffer) - 1 - totalLen;
                    size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
                    
                    // Probeer response body te lezen (POST response)
                    WiFiClient* client = http.getStreamPtr();
                    if (client == nullptr) {
                        break;
                    }
                    
                    size_t bytesRead = client->readBytes((uint8_t*)(httpResponseBuffer + totalLen), chunkSize);
                    if (bytesRead == 0) {
                        if (!client->available()) {
                            break;
                        }
                        delay(10);
                        continue;
                    }
                    totalLen += bytesRead;
                }
                httpResponseBuffer[totalLen] = '\0';
                if (totalLen > 0) {
                    Serial_printf(F("[Notify] Ntfy response: %s\n"), httpResponseBuffer);
                }
            }
            
        Serial_printf(F("[Notify] Ntfy bericht succesvol verstuurd! (code: %d)\n"), code);
            ok = true;
        } else {
            // S2: Log zonder String concatenatie
        Serial_printf(F("[Notify] Ntfy fout bij versturen (code: %d)\n"), code);
    }
    } while(0);
    
    // C2: ALTIJD cleanup (ook bij code<0, code!=200, parse error)
    // Hard close: http.end() + client.stop() voor volledige cleanup
    http.end();
    WiFiClient* stream = http.getStreamPtr();
    if (stream != nullptr) {
        stream->stop();
    }
    
    // C2: Geef netwerk mutex vrij (met debug logging)
    netMutexUnlock("sendNtfyNotification");
    
    return ok;
}

// ============================================================================
// Utility Functions
// ============================================================================

// Get formatted timestamp string (dd-mm-yyyy hh:mm:ss)
// Fase 6.1: AlertEngine module gebruikt deze functie (extern declaration in AlertEngine.cpp)
void getFormattedTimestamp(char *buffer, size_t bufferSize) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(buffer, bufferSize, "%d-%m-%Y %H:%M:%S", &timeinfo);
    } else {
        // Fallback als tijd niet beschikbaar is
        snprintf(buffer, bufferSize, "?\\?-?\\?-???? ??:??:??");
    }
}

// Get formatted timestamp string with slash (dd-mm-yyyy/hh:mm:ss) voor notificaties
// Nieuwe functie voor consistente notificatie-indeling
void getFormattedTimestampForNotification(char *buffer, size_t bufferSize) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        strftime(buffer, bufferSize, "%d-%m-%Y/%H:%M:%S", &timeinfo);
    } else {
        // Fallback als tijd niet beschikbaar is (geen trigraphs gebruiken)
        snprintf(buffer, bufferSize, "--/--/----/--:--:--");
    }
}

// Format IP address to string (geoptimaliseerd: gebruik char array i.p.v. String)
// ============================================================================
// Helper Functions
// ============================================================================

// Helper: Validate if price is valid (not NaN, Inf, or <= 0)
// Fase 6.2: AnchorSystem module gebruikt deze functie (extern declaration in AnchorSystem.h)
bool isValidPrice(float price)
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
bool safeAtof(const char* str, float& out)
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
// Fase 8.7.1: static verwijderd zodat UIController module deze kan gebruiken
void safeStrncpy(char *dest, const char *src, size_t destSize)
{
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

// Deadlock detection: Track mutex hold times
static unsigned long mutexTakeTime = 0;
static const char* mutexHolderContext = nullptr;
static const unsigned long MAX_MUTEX_HOLD_TIME_MS = 2000; // Max 2 seconden hold time (deadlock threshold)

// Fase 4.1: Geconsolideerde mutex timeout handling
// Helper: Handle mutex timeout with rate-limited logging
// Geoptimaliseerd: elimineert code duplicatie voor mutex timeout handling
static void handleMutexTimeout(uint32_t& timeoutCount, const char* context, const char* symbol = nullptr, uint32_t logInterval = 10, uint32_t resetThreshold = 50)
{
    timeoutCount++;
    // Log alleen bij eerste timeout of elke N-de timeout (rate limiting)
    if (timeoutCount == 1 || timeoutCount % logInterval == 0) {
        if (symbol) {
            Serial_printf(F("[%s] WARN -> %s mutex timeout (count: %lu)\n"), context, symbol, timeoutCount);
        } else {
            Serial_printf(F("[%s] WARN: mutex timeout (count: %lu)\n"), context, timeoutCount);
        }
    }
    // Reset counter na te veel timeouts (mogelijk deadlock)
    if (timeoutCount > resetThreshold) {
        if (symbol) {
            Serial_printf(F("[%s] CRIT -> %s mutex timeout te vaak, mogelijk deadlock!\n"), context, symbol);
        } else {
            Serial_printf(F("[%s] CRIT: mutex timeout te vaak, mogelijk deadlock!\n"), context);
        }
        timeoutCount = 0; // Reset counter
    }
}

// Fase 4.2: Geconsolideerde mutex pattern helper
// Helper: Reset mutex timeout counter bij succes (elimineert code duplicatie)
// Geoptimaliseerd: elimineert herhaalde "if (timeoutCount > 0) timeoutCount = 0;" pattern
static void resetMutexTimeoutCounter(uint32_t& timeoutCount)
{
    if (timeoutCount > 0) {
        timeoutCount = 0;
    }
}

// Helper: Safe mutex take with deadlock detection
// Returns true on success, false on failure
// Fase 6.2: AnchorSystem module gebruikt deze functie (extern declaration in AnchorSystem.h)
bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* context)
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
// Fase 6.2: AnchorSystem module gebruikt deze functie (extern declaration in AnchorSystem.h)
void safeMutexGive(SemaphoreHandle_t mutex, const char* context)
{
    if (mutex == nullptr) {
        Serial_printf(F("[Mutex] ERROR: Attempt to give nullptr mutex in %s\n"), context);
        return;
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
        return;
    }
    
    // Reset tracking
    mutexTakeTime = 0;
    mutexHolderContext = nullptr;
}

// C2: Helper functies voor netwerk mutex met debug logging (task name/core id)
// Niet static zodat andere modules (ApiClient, HttpFetch) ze kunnen gebruiken
void netMutexLock(const char* taskName)
{
    if (gNetMutex == NULL) {
        #if !DEBUG_BUTTON_ONLY
        Serial.printf(F("[NetMutex] WARN: gNetMutex is NULL, HTTP operatie zonder mutex (by %s)\n"), taskName);
        #endif
        return;
    }
    
    // C2: Debug logging met task name en core id
    #if !DEBUG_BUTTON_ONLY
    BaseType_t coreId = xPortGetCoreID();
    Serial.printf(F("[NetMutex] lock by %s (core %d)\n"), taskName, coreId);
    #endif
    
    xSemaphoreTake(gNetMutex, portMAX_DELAY);
}

void netMutexUnlock(const char* taskName)
{
    if (gNetMutex == NULL) {
        return;
    }
    
    // C2: Debug logging met task name en core id
    #if !DEBUG_BUTTON_ONLY
    BaseType_t coreId = xPortGetCoreID();
    Serial.printf(F("[NetMutex] unlock by %s (core %d)\n"), taskName, coreId);
    #endif
    
    xSemaphoreGive(gNetMutex);
}

// Forward declarations
// Fase 6.1: AlertEngine module gebruikt deze functies (extern declarations in AlertEngine.cpp)
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
void findMinMaxInLast2Hours(float &minVal, float &maxVal);  // Alleen voor CYD platforms
#endif
TwoHMetrics computeTwoHMetrics();  // Compute 2-hour metrics uniformly from existing state
static void checkHeapTelemetry();

// ============================================================================
// Utility Functions
// ============================================================================

// Fase 9.1.2: static verwijderd zodat WebServerModule deze kan gebruiken
void formatIPAddress(IPAddress ip, char *buffer, size_t bufferSize) {
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
// Fase 5.1: static verwijderd zodat TrendDetector module deze functie kan aanroepen (later verplaatst naar AlertEngine)
bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    return sendNtfyNotification(title, message, colorTag);
}

// Fase 5.3.4: checkTrendChange() wrapper functie verwijderd - alle calls gebruiken nu directe module calls

// ============================================================================
// Anchor Price Functions
// ============================================================================

// ============================================================================
// Anchor Price Functions
// ============================================================================
// Fase 6.2: Anchor functionaliteit verplaatst naar AnchorSystem module
// - calculateEffectiveAnchorThresholds(), calcEffectiveAnchor() → AnchorSystem
// - setAnchorPrice() → AnchorSystem
// - checkAnchorAlerts() → AnchorSystem

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
    // Gebruik dynamische MQTT prefix (gebaseerd op NTFY topic)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    snprintf(topic, sizeof(topic), "%s/anchor/event", mqttPrefix);
    
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
// Fase 5.3.11: Alle wrapper functies verwijderd - alle calls gebruiken nu directe module calls

// Log volatility status (voor debug)
// Fase 6.1: AlertEngine module gebruikt deze functie (extern declaration in AlertEngine.cpp)
void logVolatilityStatus(const EffectiveThresholds& eff)
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

// ============================================================================
// Alert & Anchor Functions
// ============================================================================
// Fase 6.1: Alert functionaliteit verplaatst naar AlertEngine module
// - checkAndNotify(), checkAlertConditions(), determineColorTag() → AlertEngine
// - formatNotificationMessage(), sendAlertNotification() → AlertEngine
// - update1mEvent(), update5mEvent(), checkAndSendConfluenceAlert() → AlertEngine
// Fase 6.2: Anchor functionaliteit verplaatst naar AnchorSystem module
// - checkAnchorAlerts() → AnchorSystem

// Language translation function
// Returns the appropriate text based on the selected language
// Fase 8.5.2: static verwijderd zodat UIController module deze kan gebruiken
const char* getText(const char* nlText, const char* enText) {
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
void generateDefaultNtfyTopic(char* buffer, size_t bufferSize) {
    SettingsStore::generateDefaultNtfyTopic(buffer, bufferSize);
}

// Helper: Get MQTT topic prefix from NTFY topic (uniek per device)
// Format: NTFY topic zonder "-alert" suffix
// Bijvoorbeeld: "9MK28H3Q-alert" -> "9MK28H3Q"
// Dit zorgt ervoor dat elke device een unieke MQTT prefix heeft, zelfs als er meerdere devices van hetzelfde type zijn
// Fase: static verwijderd zodat WebServerModule deze functie kan gebruiken
void getMqttTopicPrefix(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) return;
    
    // Gebruik NTFY topic als basis (uniek per device)
    // Verwijder "-alert" suffix als die aanwezig is
    size_t topicLen = strlen(ntfyTopic);
    if (topicLen == 0) {
        // Fallback: gebruik default NTFY topic generatie
        generateDefaultNtfyTopic(buffer, bufferSize);
        // Verwijder "-alert" suffix
        size_t len = strlen(buffer);
        if (len > 6 && strcmp(buffer + len - 6, "-alert") == 0) {
            buffer[len - 6] = '\0';
        }
        return;
    }
    
    // Kopieer NTFY topic
    size_t copyLen = (topicLen < bufferSize) ? topicLen : bufferSize - 1;
    strncpy(buffer, ntfyTopic, copyLen);
    buffer[copyLen] = '\0';
    
    // Verwijder "-alert" suffix als die aanwezig is
    size_t len = strlen(buffer);
    if (len > 6 && strcmp(buffer + len - 6, "-alert") == 0) {
        buffer[len - 6] = '\0';
    }
}

// Helper: Generate MQTT device ID with prefix (char array version - voorkomt String gebruik)
// Format: [MQTT-PREFIX]_[ESP32-ID-HEX]
// Gebruikt nu NTFY topic als basis voor unieke identificatie
static void getMqttDeviceId(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) return;
    
    // Haal MQTT prefix op (gebaseerd op NTFY topic)
    char prefix[64];
    getMqttTopicPrefix(prefix, sizeof(prefix));
    
    // Generate device ID from MAC address (lower 32 bits as HEX)
    uint32_t macLower = (uint32_t)ESP.getEfuseMac();
    snprintf(buffer, bufferSize, "%s_%08x", prefix, macLower);
}

// Helper: Extract device ID from topic (char array version - voorkomt String gebruik)
// If topic format is [ESP32-ID]-alert, extracts the ESP32-ID
// Falls back to showing first part before any dash if format is different
// Fase 8.11.1: static verwijderd zodat UIController module deze kan gebruiken
void getDeviceIdFromTopic(const char* topic, char* buffer, size_t bufferSize) {
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
    displayRotation = settings.displayRotation;
    
    // Copy alert thresholds
    alertThresholds = settings.alertThresholds;
    
    // Copy 2-hour alert thresholds
    alert2HThresholds = settings.alert2HThresholds;
    
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
    anchorStrategy = settings.anchorStrategy;
    
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
void saveSettings()
{
    // Create settings struct from global variables
    CryptoMonitorSettings settings;
    
    // Copy basic settings
    safeStrncpy(settings.ntfyTopic, ntfyTopic, sizeof(settings.ntfyTopic));
    safeStrncpy(settings.binanceSymbol, binanceSymbol, sizeof(settings.binanceSymbol));
    settings.language = language;
    
    // Copy alert thresholds
    settings.alertThresholds = alertThresholds;
    
    // Copy 2-hour alert thresholds
    settings.alert2HThresholds = alert2HThresholds;
    
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
    settings.anchorStrategy = anchorStrategy;
    
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
bool safeSecondsToMs(int seconds, uint32_t& resultMs)
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
    char topicBuffer[96];  // Verkleind van 128 naar 96 bytes (bespaart 32 bytes DRAM)
    char msgBuffer[96];     // Verkleind van 128 naar 96 bytes (bespaart 32 bytes DRAM)
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
    
    // Helper: maak MQTT topic prefix (gebaseerd op NTFY topic voor unieke identificatie)
    getMqttTopicPrefix(prefixBuffer, sizeof(prefixBuffer));
    
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
        {"/config/volatilityHighThreshold/set", true, 0.01f, 1.0f, &volatilityHighThreshold, "/config/volatilityHighThreshold"},
        // 2-hour alert thresholds
        {"/config/2hBreakMargin/set", true, 0.01f, 5.0f, &alert2HThresholds.breakMarginPct, "/config/2hBreakMargin"},
        {"/config/2hBreakReset/set", true, 0.01f, 5.0f, &alert2HThresholds.breakResetMarginPct, "/config/2hBreakReset"},
        {"/config/2hMeanMinDist/set", true, 0.01f, 10.0f, &alert2HThresholds.meanMinDistancePct, "/config/2hMeanMinDist"},
        {"/config/2hMeanTouch/set", true, 0.01f, 2.0f, &alert2HThresholds.meanTouchBandPct, "/config/2hMeanTouch"},
        {"/config/2hCompressTh/set", true, 0.01f, 5.0f, &alert2HThresholds.compressThresholdPct, "/config/2hCompressTh"},
        {"/config/2hCompressReset/set", true, 0.01f, 10.0f, &alert2HThresholds.compressResetPct, "/config/2hCompressReset"},
        {"/config/2hAnchorMargin/set", true, 0.01f, 5.0f, &alert2HThresholds.anchorOutsideMarginPct, "/config/2hAnchorMargin"},
        // FASE X.4: Trend hysteresis en throttling instellingen
        {"/config/2hTrendHyst/set", true, 0.1f, 1.0f, &alert2HThresholds.trendHysteresisFactor, "/config/2hTrendHyst"},
        // Trend-adaptive anchor multipliers
        {"/config/upMLMult/set", true, 0.5f, 2.0f, &uptrendMaxLossMultiplier, "/config/upMLMult"},
        {"/config/upTPMult/set", true, 0.5f, 2.0f, &uptrendTakeProfitMultiplier, "/config/upTPMult"},
        {"/config/downMLMult/set", true, 0.5f, 2.0f, &downtrendMaxLossMultiplier, "/config/downMLMult"},
        {"/config/downTPMult/set", true, 0.5f, 2.0f, &downtrendTakeProfitMultiplier, "/config/downTPMult"},
        // Auto-Volatility settings
        {"/config/autoVolBase/set", true, 0.01f, 1.0f, &autoVolatilityBaseline1mStdPct, "/config/autoVolBase"},
        {"/config/autoVolMin/set", true, 0.1f, 1.0f, &autoVolatilityMinMultiplier, "/config/autoVolMin"},
        {"/config/autoVolMax/set", true, 1.0f, 3.0f, &autoVolatilityMaxMultiplier, "/config/autoVolMax"}
    };
    
    static const struct {
        const char* suffix;
        uint32_t* targetMs;
        const char* stateSuffix;
    } cooldownSettings[] = {
        {"/config/cooldown1min/set", &notificationCooldowns.cooldown1MinMs, "/config/cooldown1min"},
        {"/config/cooldown30min/set", &notificationCooldowns.cooldown30MinMs, "/config/cooldown30min"},
        {"/config/cooldown5min/set", &notificationCooldowns.cooldown5MinMs, "/config/cooldown5min"},
        // 2-hour alert cooldowns
        {"/config/2hBreakCD/set", &alert2HThresholds.breakCooldownMs, "/config/2hBreakCD"},
        {"/config/2hMeanCD/set", &alert2HThresholds.meanCooldownMs, "/config/2hMeanCD"},
        {"/config/2hCompressCD/set", &alert2HThresholds.compressCooldownMs, "/config/2hCompressCD"},
        {"/config/2hAnchorCD/set", &alert2HThresholds.anchorCooldownMs, "/config/2hAnchorCD"},
        // FASE X.4: Throttling tijden
        {"/config/2hThrottleTC/set", &alert2HThresholds.throttlingTrendChangeMs, "/config/2hThrottleTC"},
        {"/config/2hThrottleTM/set", &alert2HThresholds.throttlingTrendToMeanMs, "/config/2hThrottleTM"},
        {"/config/2hThrottleMT/set", &alert2HThresholds.throttlingMeanTouchMs, "/config/2hThrottleMT"},
        {"/config/2hThrottleComp/set", &alert2HThresholds.throttlingCompressMs, "/config/2hThrottleComp"}
    };
    
    // Integer settings (uint8_t) - voor warm-start en auto-volatility window
    static const struct {
        const char* suffix;
        uint8_t* targetVar;
        int minVal;
        int maxVal;
        const char* stateSuffix;
    } intSettings[] = {
        {"/config/autoVolWin/set", &autoVolatilityWindowMinutes, 10, 120, "/config/autoVolWin"},
        {"/config/ws1mExtra/set", &warmStart1mExtraCandles, 0, 100, "/config/ws1mExtra"},
        {"/config/ws5m/set", &warmStart5mCandles, 2, 200, "/config/ws5m"},
        {"/config/ws30m/set", &warmStart30mCandles, 2, 200, "/config/ws30m"},
        {"/config/ws2h/set", &warmStart2hCandles, 2, 200, "/config/ws2h"}
    };
    
    // Boolean settings (switch entities)
    static const struct {
        const char* suffix;
        bool* targetVar;
        const char* stateSuffix;
    } boolSettings[] = {
        {"/config/trendAdapt/set", &trendAdaptiveAnchorsEnabled, "/config/trendAdapt"},
        {"/config/smartConf/set", &smartConfluenceEnabled, "/config/smartConf"},
        {"/config/autoVol/set", &autoVolatilityEnabled, "/config/autoVol"},
        {"/config/warmStart/set", &warmStartEnabled, "/config/warmStart"}
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
    // Note: 2h cooldowns en throttling zijn in seconden (niet minuten zoals in web interface)
    if (!handled) {
        for (size_t i = 0; i < sizeof(cooldownSettings) / sizeof(cooldownSettings[0]); i++) {
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, cooldownSettings[i].suffix);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                int seconds = atoi(msgBuffer);
                // 2h cooldowns en throttling kunnen groter zijn (tot 10 uur = 36000 seconden)
                int maxSeconds = (strstr(cooldownSettings[i].suffix, "2h") != nullptr) ? 36000 : 3600;
                if (seconds >= 1 && seconds <= maxSeconds) {
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
                    Serial_printf(F("[MQTT] Invalid cooldown value (range: 1-%d seconds): %s\n"), maxSeconds, msgBuffer);
                }
                break;
            }
        }
    }
    
    // FASE X.5: Secondary global cooldown en coalescing settings (in seconden, niet milliseconden)
    // Deze worden apart verwerkt omdat ze al in seconden zijn opgeslagen
    if (!handled) {
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/2hSecGlobalCD/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            int seconds = atoi(msgBuffer);
            if (seconds >= 60 && seconds <= 86400) {  // 1 min tot 24 uur
                alert2HThresholds.twoHSecondaryGlobalCooldownSec = seconds;
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/2hSecGlobalCD", prefixBuffer);
                snprintf(valueBuffer, sizeof(valueBuffer), "%lu", alert2HThresholds.twoHSecondaryGlobalCooldownSec);
                mqttClient.publish(topicBufferFull, valueBuffer, true);
                settingChanged = true;
                handled = true;
            } else {
                Serial_printf(F("[MQTT] Invalid 2hSecGlobalCD value (range: 60-86400 seconds): %s\n"), msgBuffer);
            }
        }
        
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/2hSecCoalesce/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            int seconds = atoi(msgBuffer);
            if (seconds >= 10 && seconds <= 600) {  // 10 sec tot 10 min
                alert2HThresholds.twoHSecondaryCoalesceWindowSec = seconds;
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/2hSecCoalesce", prefixBuffer);
                snprintf(valueBuffer, sizeof(valueBuffer), "%lu", alert2HThresholds.twoHSecondaryCoalesceWindowSec);
                mqttClient.publish(topicBufferFull, valueBuffer, true);
                settingChanged = true;
                handled = true;
            } else {
                Serial_printf(F("[MQTT] Invalid 2hSecCoalesce value (range: 10-600 seconds): %s\n"), msgBuffer);
            }
        }
    }
    
    // Process integer settings (uint8_t)
    if (!handled) {
        for (size_t i = 0; i < sizeof(intSettings) / sizeof(intSettings[0]); i++) {
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, intSettings[i].suffix);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                int val = atoi(msgBuffer);
                if (val >= intSettings[i].minVal && val <= intSettings[i].maxVal) {
                    *intSettings[i].targetVar = static_cast<uint8_t>(val);
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, intSettings[i].stateSuffix);
                    snprintf(valueBuffer, sizeof(valueBuffer), "%u", *intSettings[i].targetVar);
                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                    settingChanged = true;
                    handled = true;
                } else {
                    Serial_printf(F("[MQTT] Invalid value for %s (range: %d-%d): %s\n"), 
                                 intSettings[i].suffix, intSettings[i].minVal, intSettings[i].maxVal, msgBuffer);
                }
                break;
            }
        }
    }
    
    // Process boolean settings (switch entities)
    if (!handled) {
        for (size_t i = 0; i < sizeof(boolSettings) / sizeof(boolSettings[0]); i++) {
            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, boolSettings[i].suffix);
            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                // Home Assistant switch: "ON" = true, "OFF" = false
                bool newValue = (strcmp(msgBuffer, "ON") == 0 || strcmp(msgBuffer, "on") == 0 || 
                                strcmp(msgBuffer, "1") == 0 || strcmp(msgBuffer, "true") == 0);
                *boolSettings[i].targetVar = newValue;
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, boolSettings[i].stateSuffix);
                mqttClient.publish(topicBufferFull, newValue ? "ON" : "OFF", true);
                settingChanged = true;
                handled = true;
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
                    // displayRotation - speciale logica (saveSettings call + direct toepassen)
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/displayRotation/set", prefixBuffer);
                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                        uint8_t newRotation = atoi(msgBuffer);
                        if (newRotation == 0 || newRotation == 2) {
                            displayRotation = newRotation;
                            // Wis scherm eerst om residu te voorkomen
                            gfx->fillScreen(RGB565_BLACK);
                            // Pas rotatie direct toe
                            gfx->setRotation(newRotation);
                            // Wis scherm opnieuw na rotatie
                            gfx->fillScreen(RGB565_BLACK);
                            saveSettings(); // Save displayRotation to Preferences
                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/displayRotation", prefixBuffer);
                            snprintf(valueBuffer, sizeof(valueBuffer), "%u", displayRotation);
                            mqttClient.publish(topicBufferFull, valueBuffer, true);
                            settingChanged = true;
                        }
                        handled = true;
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
                                    // Publiceer huidige prijs als state (default waarde)
                                    extern float prices[];
                                    if (safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "mqttCallback anchorValue")) {
                                        float currentPrice = prices[0];
                                        safeMutexGive(dataMutex, "mqttCallback anchorValue");
                                        snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", currentPrice);
                                        mqttClient.publish(topicBufferFull, valueBuffer, true);
                                    } else {
                                        // Fallback: gebruik 0 als placeholder (wordt later geupdate)
                                        mqttClient.publish(topicBufferFull, "0.00", true);
                                    }
                            } else {
                                snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", val);
                                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                            }
                        } else {
                            mqttClient.publish(topicBufferFull, "ERROR: Invalid value", false);
                        }
                        handled = true;
                                } else {
                            // anchorStrategy/set - speciale logica (pas TP/SL aan)
                            snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorStrategy/set", prefixBuffer);
                            if (strcmp(topicBuffer, topicBufferFull) == 0) {
                                uint8_t newStrategy = atoi(msgBuffer);
                                if (newStrategy >= 0 && newStrategy <= 2) {
                                    anchorStrategy = newStrategy;
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
                                    saveSettings(); // Save anchorStrategy en TP/SL to Preferences
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorStrategy", prefixBuffer);
                                    snprintf(valueBuffer, sizeof(valueBuffer), "%u", anchorStrategy);
                                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                                    // Publiceer ook TP/SL updates
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorTP", prefixBuffer);
                                    snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", anchorTakeProfit);
                                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/anchorML", prefixBuffer);
                                    snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", anchorMaxLoss);
                                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                                    settingChanged = true;
                                }
                                handled = true;
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
    char topicBuffer[96];  // Verkleind van 128 naar 96 bytes (bespaart 32 bytes DRAM)
    char buffer[32];
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    dtostrf(value, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", mqttPrefix, topicSuffix);
    
    if (mqttConnected && mqttClient.publish(topicBuffer, buffer, true)) {
        // Direct publish succeeded
        return;
    }
    
    // Queue message if not connected or publish failed
    enqueueMqttMessage(topicBuffer, buffer, true);
}

static void publishMqttUint(const char* topicSuffix, unsigned long value) {
    char topicBuffer[96];  // Verkleind van 128 naar 96 bytes (bespaart 32 bytes DRAM)
    char buffer[32];
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    snprintf(buffer, sizeof(buffer), "%lu", value);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", mqttPrefix, topicSuffix);
    
    if (mqttConnected && mqttClient.publish(topicBuffer, buffer, true)) {
        // Direct publish succeeded
        return;
    }
    
    // Queue message if not connected or publish failed
    enqueueMqttMessage(topicBuffer, buffer, true);
}

static void publishMqttString(const char* topicSuffix, const char* value) {
    char topicBuffer[96];  // Verkleind van 128 naar 96 bytes (bespaart 32 bytes DRAM)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/%s", mqttPrefix, topicSuffix);
    
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
    publishMqttUint("anchorStrategy", anchorStrategy);
    publishMqttFloat("trendThreshold", trendThreshold);
    publishMqttFloat("volatilityLowThreshold", volatilityLowThreshold);
    publishMqttFloat("volatilityHighThreshold", volatilityHighThreshold);
    
    // 2-hour alert thresholds
    publishMqttFloat("2hBreakMargin", alert2HThresholds.breakMarginPct);
    publishMqttFloat("2hBreakReset", alert2HThresholds.breakResetMarginPct);
    publishMqttFloat("2hMeanMinDist", alert2HThresholds.meanMinDistancePct);
    publishMqttFloat("2hMeanTouch", alert2HThresholds.meanTouchBandPct);
    publishMqttFloat("2hCompressTh", alert2HThresholds.compressThresholdPct);
    publishMqttFloat("2hCompressReset", alert2HThresholds.compressResetPct);
    publishMqttFloat("2hAnchorMargin", alert2HThresholds.anchorOutsideMarginPct);
    publishMqttFloat("2hTrendHyst", alert2HThresholds.trendHysteresisFactor);
    
    // Trend-adaptive anchor multipliers
    publishMqttFloat("upMLMult", uptrendMaxLossMultiplier);
    publishMqttFloat("upTPMult", uptrendTakeProfitMultiplier);
    publishMqttFloat("downMLMult", downtrendMaxLossMultiplier);
    publishMqttFloat("downTPMult", downtrendTakeProfitMultiplier);
    
    // Auto-Volatility settings
    publishMqttFloat("autoVolBase", autoVolatilityBaseline1mStdPct);
    publishMqttFloat("autoVolMin", autoVolatilityMinMultiplier);
    publishMqttFloat("autoVolMax", autoVolatilityMaxMultiplier);
    
    // Unsigned int settings (cooldowns in seconds)
    publishMqttUint("cooldown1min", notificationCooldown1MinMs / 1000);
    publishMqttUint("cooldown30min", notificationCooldown30MinMs / 1000);
    publishMqttUint("cooldown5min", notificationCooldown5MinMs / 1000);
    publishMqttUint("language", language);
    publishMqttUint("displayRotation", displayRotation);
    
    // 2-hour alert cooldowns (in seconds)
    publishMqttUint("2hBreakCD", alert2HThresholds.breakCooldownMs / 1000);
    publishMqttUint("2hMeanCD", alert2HThresholds.meanCooldownMs / 1000);
    publishMqttUint("2hCompressCD", alert2HThresholds.compressCooldownMs / 1000);
    publishMqttUint("2hAnchorCD", alert2HThresholds.anchorCooldownMs / 1000);
    publishMqttUint("2hThrottleTC", alert2HThresholds.throttlingTrendChangeMs / 1000);
    publishMqttUint("2hThrottleTM", alert2HThresholds.throttlingTrendToMeanMs / 1000);
    publishMqttUint("2hThrottleMT", alert2HThresholds.throttlingMeanTouchMs / 1000);
    publishMqttUint("2hThrottleComp", alert2HThresholds.throttlingCompressMs / 1000);
    // FASE X.5: Secondary global cooldown en coalescing (in seconden)
    publishMqttUint("2hSecGlobalCD", alert2HThresholds.twoHSecondaryGlobalCooldownSec);
    publishMqttUint("2hSecCoalesce", alert2HThresholds.twoHSecondaryCoalesceWindowSec);
    
    // Integer settings (uint8_t)
    publishMqttUint("autoVolWin", autoVolatilityWindowMinutes);
    publishMqttUint("ws1mExtra", warmStart1mExtraCandles);
    publishMqttUint("ws5m", warmStart5mCandles);
    publishMqttUint("ws30m", warmStart30mCandles);
    publishMqttUint("ws2h", warmStart2hCandles);
    
    // Boolean settings (switch entities)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    char topicBuffer[128];
    char valueBuffer[8];
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/trendAdapt", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", trendAdaptiveAnchorsEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/smartConf", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", smartConfluenceEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVol", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", autoVolatilityEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/warmStart", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", warmStartEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    
    // String settings
    publishMqttString("binanceSymbol", binanceSymbol);
    publishMqttString("ntfyTopic", ntfyTopic);
    
    // Anchor value - publish current price as default (or current anchor if set)
    char mqttPrefixAnchor[64];
    getMqttTopicPrefix(mqttPrefixAnchor, sizeof(mqttPrefixAnchor));
    char topicBufferAnchor[128];
    char valueBufferAnchor[32];
    snprintf(topicBufferAnchor, sizeof(topicBufferAnchor), "%s/config/anchorValue", mqttPrefixAnchor);
    
    // Try to get current anchor price, otherwise use current price
    extern float anchorPrice;
    extern float prices[];
    float anchorValueToPublish = 0.0f;
    
    if (safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "publishMqttSettings anchorValue")) {
        // Check if anchor is active, otherwise use current price
        extern bool anchorActive;
        if (anchorActive && anchorPrice > 0.0f && isValidPrice(anchorPrice)) {
            anchorValueToPublish = anchorPrice;
        } else if (isValidPrice(prices[0])) {
            anchorValueToPublish = prices[0]; // Use current price as default
        }
        safeMutexGive(dataMutex, "publishMqttSettings anchorValue");
    } else {
        // Fallback: use current price if mutex unavailable
        if (isValidPrice(prices[0])) {
            anchorValueToPublish = prices[0];
        }
    }
    
    if (anchorValueToPublish > 0.0f) {
        snprintf(valueBufferAnchor, sizeof(valueBufferAnchor), "%.2f", anchorValueToPublish);
        enqueueMqttMessage(topicBufferAnchor, valueBufferAnchor, true);
    }
}

// Publiceer waarden naar MQTT (prijzen, percentages, etc.)
// Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
static void formatTrendLabel(char* buffer, size_t bufferSize, const char* prefix, TrendState trend) {
    const char* suffix = "=";
    switch (trend) {
        case TREND_UP:
            suffix = "//";
            break;
        case TREND_DOWN:
            suffix = "\\\\";
            break;
        case TREND_SIDEWAYS:
        default:
            suffix = "=";
            break;
    }
    snprintf(buffer, bufferSize, "%s%s", prefix, suffix);
}

void publishMqttValues(float price, float ret_1m, float ret_5m, float ret_30m) {
    if (!mqttConnected) return;
    
    char topicBuffer[128];
    char buffer[32];
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    
    dtostrf(price, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/price", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_1m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_1m", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_5m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_5m", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);
    
    dtostrf(ret_30m, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_30m", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);

    dtostrf(ret_2h, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_2h", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);

    dtostrf(ret_1d, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_1d", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);

    dtostrf(ret_7d, 0, 2, buffer);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/return_7d", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);

    char trend2h[8];
    char trend1d[8];
    char trend7d[8];
    formatTrendLabel(trend2h, sizeof(trend2h), "2h", trendDetector.getTrendState());
    formatTrendLabel(trend1d, sizeof(trend1d), "1d", trendDetector.getMediumTrendState());
    formatTrendLabel(trend7d, sizeof(trend7d), "7d", trendDetector.getLongTermTrendState());
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/trend_2h", mqttPrefix);
    mqttClient.publish(topicBuffer, trend2h, false);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/trend_1d", mqttPrefix);
    mqttClient.publish(topicBuffer, trend1d, false);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/trend_7d", mqttPrefix);
    mqttClient.publish(topicBuffer, trend7d, false);
    
    snprintf(buffer, sizeof(buffer), "%lu", millis());
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/timestamp", mqttPrefix);
    mqttClient.publish(topicBuffer, buffer, false);
    
    // Publiceer IP-adres (alleen als WiFi verbonden is)
    if (WiFi.status() == WL_CONNECTED) {
        char ipBuffer[16];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", mqttPrefix);
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
    
    // Haal MQTT prefix op (gebaseerd op NTFY topic voor unieke identificatie)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    
    char deviceJson[256];
    snprintf(deviceJson, sizeof(deviceJson), 
        "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"JanP\",\"model\":\"%s\"}",
        deviceId, DEVICE_NAME, DEVICE_MODEL);
    
    // Buffers voor topic en payload
    char topicBuffer[128];
    char payloadBuffer[400];  // Verkleind van 512 naar 400 bytes (bespaart 112 bytes DRAM)
    
    // Discovery berichten met char arrays (geen String concatenatie)
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_spike1m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Spike Threshold\",\"unique_id\":\"%s_spike1m\",\"state_topic\":\"%s/config/spike1m\",\"command_topic\":\"%s/config/spike1m/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_spike5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Spike Filter\",\"unique_id\":\"%s_spike5m\",\"state_topic\":\"%s/config/spike5m\",\"command_topic\":\"%s/config/spike5m/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:filter\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Move Threshold\",\"unique_id\":\"%s_move30m\",\"state_topic\":\"%s/config/move30m\",\"command_topic\":\"%s/config/move30m/set\",\"min\":0.5,\"max\":20.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Move Filter\",\"unique_id\":\"%s_move5m\",\"state_topic\":\"%s/config/move5m\",\"command_topic\":\"%s/config/move5m/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:filter-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_move5mAlert/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Move Alert Threshold\",\"unique_id\":\"%s_move5mAlert\",\"state_topic\":\"%s/config/move5mAlert\",\"command_topic\":\"%s/config/move5mAlert/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:alert\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown1min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Cooldown\",\"unique_id\":\"%s_cooldown1min\",\"state_topic\":\"%s/config/cooldown1min\",\"command_topic\":\"%s/config/cooldown1min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown30min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Cooldown\",\"unique_id\":\"%s_cooldown30min\",\"state_topic\":\"%s/config/cooldown30min\",\"command_topic\":\"%s/config/cooldown30min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_cooldown5min/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Cooldown\",\"unique_id\":\"%s_cooldown5min\",\"state_topic\":\"%s/config/cooldown5min\",\"command_topic\":\"%s/config/cooldown5min/set\",\"min\":10,\"max\":3600,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-sand\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/text/%s_binanceSymbol/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Binance Symbol\",\"unique_id\":\"%s_binanceSymbol\",\"state_topic\":\"%s/config/binanceSymbol\",\"command_topic\":\"%s/config/binanceSymbol/set\",\"icon\":\"mdi:currency-btc\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/text/%s_ntfyTopic/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"NTFY Topic\",\"unique_id\":\"%s_ntfyTopic\",\"state_topic\":\"%s/config/ntfyTopic\",\"command_topic\":\"%s/config/ntfyTopic/set\",\"icon\":\"mdi:bell-ring\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_price/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Crypto Price\",\"unique_id\":\"%s_price\",\"state_topic\":\"%s/values/price\",\"unit_of_measurement\":\"EUR\",\"icon\":\"mdi:currency-btc\",\"device_class\":\"monetary\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_1m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1m Return\",\"unique_id\":\"%s_return_1m\",\"state_topic\":\"%s/values/return_1m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line-variant\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"5m Return\",\"unique_id\":\"%s_return_5m\",\"state_topic\":\"%s/values/return_5m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"30m Return\",\"unique_id\":\"%s_return_30m\",\"state_topic\":\"%s/values/return_30m\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_2h/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Return\",\"unique_id\":\"%s_return_2h\",\"state_topic\":\"%s/values/return_2h\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_1d/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"1d Return\",\"unique_id\":\"%s_return_1d\",\"state_topic\":\"%s/values/return_1d\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:calendar-today\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_return_7d/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"7d Return\",\"unique_id\":\"%s_return_7d\",\"state_topic\":\"%s/values/return_7d\",\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:calendar-week\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_trend_2h/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend 2h\",\"unique_id\":\"%s_trend_2h\",\"state_topic\":\"%s/values/trend_2h\",\"icon\":\"mdi:chart-line\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_trend_1d/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend 1d\",\"unique_id\":\"%s_trend_1d\",\"state_topic\":\"%s/values/trend_1d\",\"icon\":\"mdi:chart-line\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_trend_7d/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend 7d\",\"unique_id\":\"%s_trend_7d\",\"state_topic\":\"%s/values/trend_7d\",\"icon\":\"mdi:chart-line\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor take profit
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorTakeProfit/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Take Profit\",\"unique_id\":\"%s_anchorTakeProfit\",\"state_topic\":\"%s/config/anchorTakeProfit\",\"command_topic\":\"%s/config/anchorTakeProfit/set\",\"min\":0.1,\"max\":100.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:cash-plus\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor max loss
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorMaxLoss/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Max Loss\",\"unique_id\":\"%s_anchorMaxLoss\",\"state_topic\":\"%s/config/anchorMaxLoss\",\"command_topic\":\"%s/config/anchorMaxLoss/set\",\"min\":-100.0,\"max\":-0.1,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:cash-minus\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor strategy (select entity)
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/select/%s_anchorStrategy/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"TP/SL Strategie\",\"unique_id\":\"%s_anchorStrategy\",\"state_topic\":\"%s/config/anchorStrategy\",\"command_topic\":\"%s/config/anchorStrategy/set\",\"options\":[\"0\",\"1\",\"2\"],\"icon\":\"mdi:strategy\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor value (number entity for setting anchor price) - renamed to "Reset Anchor Price"
    // Default value will be set to current price when publishing state
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_anchorValue/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Reset Anchor Price\",\"unique_id\":\"%s_anchorValue\",\"state_topic\":\"%s/config/anchorValue\",\"command_topic\":\"%s/config/anchorValue/set\",\"min\":0.01,\"max\":1000000.0,\"step\":0.01,\"unit_of_measurement\":\"EUR\",\"icon\":\"mdi:anchor\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Anchor event sensor
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_anchor_event/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Anchor Event\",\"unique_id\":\"%s_anchor_event\",\"state_topic\":\"%s/anchor/event\",\"json_attributes_topic\":\"%s/anchor/event\",\"value_template\":\"{{ value_json.event }}\",\"icon\":\"mdi:anchor\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Trend threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_trendThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend Threshold\",\"unique_id\":\"%s_trendThreshold\",\"state_topic\":\"%s/config/trendThreshold\",\"command_topic\":\"%s/config/trendThreshold/set\",\"min\":0.1,\"max\":10.0,\"step\":0.1,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-line\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Volatility low threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_volatilityLowThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Volatility Low Threshold\",\"unique_id\":\"%s_volatilityLowThreshold\",\"state_topic\":\"%s/config/volatilityLowThreshold\",\"command_topic\":\"%s/config/volatilityLowThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Volatility high threshold
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_volatilityHighThreshold/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Volatility High Threshold\",\"unique_id\":\"%s_volatilityHighThreshold\",\"state_topic\":\"%s/config/volatilityHighThreshold\",\"command_topic\":\"%s/config/volatilityHighThreshold/set\",\"min\":0.01,\"max\":1.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant-shimmer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Display Rotation discovery
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_displayRotation/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Display Rotation\",\"unique_id\":\"%s_displayRotation\",\"state_topic\":\"%s/config/displayRotation\",\"command_topic\":\"%s/config/displayRotation/set\",\"min\":0,\"max\":2,\"step\":2,\"icon\":\"mdi:rotate-3d-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // IP Address sensor
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/sensor/%s_ip_address/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"IP Address\",\"unique_id\":\"%s_ip_address\",\"state_topic\":\"%s/values/ip_address\",\"icon\":\"mdi:ip-network\",%s}", deviceId, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Language select
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/select/%s_language/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Language\",\"unique_id\":\"%s_language\",\"state_topic\":\"%s/config/language\",\"command_topic\":\"%s/config/language/set\",\"options\":[\"0\",\"1\"],\"icon\":\"mdi:translate\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Display Rotation discovery
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_displayRotation/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Display Rotation\",\"unique_id\":\"%s_displayRotation\",\"state_topic\":\"%s/config/displayRotation\",\"command_topic\":\"%s/config/displayRotation/set\",\"min\":0,\"max\":2,\"step\":2,\"icon\":\"mdi:rotate-3d-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // 2-hour Alert Thresholds
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hBreakMargin/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Breakout Margin\",\"unique_id\":\"%s_2hBreakMargin\",\"state_topic\":\"%s/config/2hBreakMargin\",\"command_topic\":\"%s/config/2hBreakMargin/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hBreakReset/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Breakout Reset\",\"unique_id\":\"%s_2hBreakReset\",\"state_topic\":\"%s/config/2hBreakReset\",\"command_topic\":\"%s/config/2hBreakReset/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hBreakCD/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Breakout Cooldown\",\"unique_id\":\"%s_2hBreakCD\",\"state_topic\":\"%s/config/2hBreakCD\",\"command_topic\":\"%s/config/2hBreakCD/set\",\"min\":1,\"max\":10800,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hMeanMinDist/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Mean Min Distance\",\"unique_id\":\"%s_2hMeanMinDist\",\"state_topic\":\"%s/config/2hMeanMinDist\",\"command_topic\":\"%s/config/2hMeanMinDist/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hMeanTouch/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Mean Touch Band\",\"unique_id\":\"%s_2hMeanTouch\",\"state_topic\":\"%s/config/2hMeanTouch\",\"command_topic\":\"%s/config/2hMeanTouch/set\",\"min\":0.01,\"max\":2.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hMeanCD/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Mean Cooldown\",\"unique_id\":\"%s_2hMeanCD\",\"state_topic\":\"%s/config/2hMeanCD\",\"command_topic\":\"%s/config/2hMeanCD/set\",\"min\":1,\"max\":10800,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hCompressTh/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Compress Threshold\",\"unique_id\":\"%s_2hCompressTh\",\"state_topic\":\"%s/config/2hCompressTh\",\"command_topic\":\"%s/config/2hCompressTh/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hCompressReset/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Compress Reset\",\"unique_id\":\"%s_2hCompressReset\",\"state_topic\":\"%s/config/2hCompressReset\",\"command_topic\":\"%s/config/2hCompressReset/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hCompressCD/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Compress Cooldown\",\"unique_id\":\"%s_2hCompressCD\",\"state_topic\":\"%s/config/2hCompressCD\",\"command_topic\":\"%s/config/2hCompressCD/set\",\"min\":1,\"max\":18000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hAnchorMargin/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Anchor Margin\",\"unique_id\":\"%s_2hAnchorMargin\",\"state_topic\":\"%s/config/2hAnchorMargin\",\"command_topic\":\"%s/config/2hAnchorMargin/set\",\"min\":0.01,\"max\":5.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:anchor\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hAnchorCD/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Anchor Cooldown\",\"unique_id\":\"%s_2hAnchorCD\",\"state_topic\":\"%s/config/2hAnchorCD\",\"command_topic\":\"%s/config/2hAnchorCD/set\",\"min\":1,\"max\":18000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Trend Hysteresis and Throttling
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hTrendHyst/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Trend Hysteresis\",\"unique_id\":\"%s_2hTrendHyst\",\"state_topic\":\"%s/config/2hTrendHyst\",\"command_topic\":\"%s/config/2hTrendHyst/set\",\"min\":0.1,\"max\":1.0,\"step\":0.01,\"icon\":\"mdi:chart-line\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hThrottleTC/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Throttle Trend Change\",\"unique_id\":\"%s_2hThrottleTC\",\"state_topic\":\"%s/config/2hThrottleTC\",\"command_topic\":\"%s/config/2hThrottleTC/set\",\"min\":1,\"max\":36000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hThrottleTM/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Throttle Trend→Mean\",\"unique_id\":\"%s_2hThrottleTM\",\"state_topic\":\"%s/config/2hThrottleTM\",\"command_topic\":\"%s/config/2hThrottleTM/set\",\"min\":1,\"max\":18000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hThrottleMT/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Throttle Mean Touch\",\"unique_id\":\"%s_2hThrottleMT\",\"state_topic\":\"%s/config/2hThrottleMT\",\"command_topic\":\"%s/config/2hThrottleMT/set\",\"min\":1,\"max\":18000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hThrottleComp/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Throttle Compress\",\"unique_id\":\"%s_2hThrottleComp\",\"state_topic\":\"%s/config/2hThrottleComp\",\"command_topic\":\"%s/config/2hThrottleComp/set\",\"min\":1,\"max\":36000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // FASE X.5: Secondary global cooldown en coalescing discovery
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hSecGlobalCD/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Secondary Global Cooldown\",\"unique_id\":\"%s_2hSecGlobalCD\",\"state_topic\":\"%s/config/2hSecGlobalCD\",\"command_topic\":\"%s/config/2hSecGlobalCD/set\",\"min\":60,\"max\":86400,\"step\":60,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-outline\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_2hSecCoalesce/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Secondary Coalesce Window\",\"unique_id\":\"%s_2hSecCoalesce\",\"state_topic\":\"%s/config/2hSecCoalesce\",\"command_topic\":\"%s/config/2hSecCoalesce/set\",\"min\":10,\"max\":600,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer-sand\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Smart Logic & Filters
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/switch/%s_trendAdapt/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Trend-Adaptive Anchors\",\"unique_id\":\"%s_trendAdapt\",\"state_topic\":\"%s/config/trendAdapt\",\"command_topic\":\"%s/config/trendAdapt/set\",\"icon\":\"mdi:chart-line-variant\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_upMLMult/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"UP Trend Max Loss Mult\",\"unique_id\":\"%s_upMLMult\",\"state_topic\":\"%s/config/upMLMult\",\"command_topic\":\"%s/config/upMLMult/set\",\"min\":0.5,\"max\":2.0,\"step\":0.01,\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_upTPMult/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"UP Trend Take Profit Mult\",\"unique_id\":\"%s_upTPMult\",\"state_topic\":\"%s/config/upTPMult\",\"command_topic\":\"%s/config/upTPMult/set\",\"min\":0.5,\"max\":2.0,\"step\":0.01,\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_downMLMult/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"DOWN Trend Max Loss Mult\",\"unique_id\":\"%s_downMLMult\",\"state_topic\":\"%s/config/downMLMult\",\"command_topic\":\"%s/config/downMLMult/set\",\"min\":0.5,\"max\":2.0,\"step\":0.01,\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_downTPMult/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"DOWN Trend Take Profit Mult\",\"unique_id\":\"%s_downTPMult\",\"state_topic\":\"%s/config/downTPMult\",\"command_topic\":\"%s/config/downTPMult/set\",\"min\":0.5,\"max\":2.0,\"step\":0.01,\"icon\":\"mdi:chart-line-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/switch/%s_smartConf/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Smart Confluence Mode\",\"unique_id\":\"%s_smartConf\",\"state_topic\":\"%s/config/smartConf\",\"command_topic\":\"%s/config/smartConf/set\",\"icon\":\"mdi:chart-timeline-variant\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/switch/%s_autoVol/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Auto-Volatility Mode\",\"unique_id\":\"%s_autoVol\",\"state_topic\":\"%s/config/autoVol\",\"command_topic\":\"%s/config/autoVol/set\",\"icon\":\"mdi:chart-timeline-variant-shimmer\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_autoVolWin/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Auto-Volatility Window\",\"unique_id\":\"%s_autoVolWin\",\"state_topic\":\"%s/config/autoVolWin\",\"command_topic\":\"%s/config/autoVolWin/set\",\"min\":10,\"max\":120,\"step\":1,\"unit_of_measurement\":\"min\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_autoVolBase/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Auto-Volatility Baseline\",\"unique_id\":\"%s_autoVolBase\",\"state_topic\":\"%s/config/autoVolBase\",\"command_topic\":\"%s/config/autoVolBase/set\",\"min\":0.01,\"max\":1.0,\"step\":0.0001,\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_autoVolMin/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Auto-Volatility Min Mult\",\"unique_id\":\"%s_autoVolMin\",\"state_topic\":\"%s/config/autoVolMin\",\"command_topic\":\"%s/config/autoVolMin/set\",\"min\":0.1,\"max\":1.0,\"step\":0.01,\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_autoVolMax/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Auto-Volatility Max Mult\",\"unique_id\":\"%s_autoVolMax\",\"state_topic\":\"%s/config/autoVolMax\",\"command_topic\":\"%s/config/autoVolMax/set\",\"min\":1.0,\"max\":3.0,\"step\":0.01,\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    // Warm-Start
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/switch/%s_warmStart/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Warm-Start Enabled\",\"unique_id\":\"%s_warmStart\",\"state_topic\":\"%s/config/warmStart\",\"command_topic\":\"%s/config/warmStart/set\",\"icon\":\"mdi:fire\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_ws1mExtra/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Warm-Start 1m Extra\",\"unique_id\":\"%s_ws1mExtra\",\"state_topic\":\"%s/config/ws1mExtra\",\"command_topic\":\"%s/config/ws1mExtra/set\",\"min\":0,\"max\":100,\"step\":1,\"icon\":\"mdi:fire\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_ws5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Warm-Start 5m Candles\",\"unique_id\":\"%s_ws5m\",\"state_topic\":\"%s/config/ws5m\",\"command_topic\":\"%s/config/ws5m/set\",\"min\":2,\"max\":200,\"step\":1,\"icon\":\"mdi:fire\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_ws30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Warm-Start 30m Candles\",\"unique_id\":\"%s_ws30m\",\"state_topic\":\"%s/config/ws30m\",\"command_topic\":\"%s/config/ws30m/set\",\"min\":2,\"max\":200,\"step\":1,\"icon\":\"mdi:fire\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_ws2h/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Warm-Start 2h Candles\",\"unique_id\":\"%s_ws2h\",\"state_topic\":\"%s/config/ws2h\",\"command_topic\":\"%s/config/ws2h/set\",\"min\":2,\"max\":200,\"step\":1,\"icon\":\"mdi:fire\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
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
    // Gebruik dynamische MQTT prefix (gebaseerd op NTFY topic voor unieke identificatie)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    char clientId[64];
    uint32_t macLower = (uint32_t)ESP.getEfuseMac();
    snprintf(clientId, sizeof(clientId), "%s_%08x", mqttPrefix, macLower);
    Serial_printf(F("[MQTT] Connecting to %s:%d as %s...\n"), mqttHost, mqttPort, clientId);
    
    if (mqttClient.connect(clientId, mqttUser, mqttPass)) {
        Serial_println("[MQTT] Connected!");
        mqttConnected = true;
        mqttReconnectAttemptCount = 0; // Reset counter bij succesvolle verbinding
        
        // Geoptimaliseerd: gebruik char arrays i.p.v. String voor subscribe topics
        // Gebruik dynamische MQTT prefix (gebaseerd op NTFY topic voor unieke identificatie)
        char mqttPrefix[64];
        getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
        char topicBuffer[128];
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike1m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/spike5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move30m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown1min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown30min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/binanceSymbol/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ntfyTopic/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        // button/reset subscribe verwijderd - gebruik nu anchorValue number entity
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorTakeProfit/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorMaxLoss/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorValue/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/anchorStrategy/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/language/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/displayRotation/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to 2h alert threshold settings
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hBreakMargin/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hBreakReset/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hBreakCD/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hMeanMinDist/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hMeanTouch/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hMeanCD/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hCompressTh/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hCompressReset/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hCompressCD/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hAnchorMargin/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hAnchorCD/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hTrendHyst/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hThrottleTC/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hThrottleTM/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hThrottleMT/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/2hThrottleComp/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to smart logic & filters
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/trendAdapt/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/upMLMult/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/upTPMult/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/downMLMult/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/downTPMult/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/smartConf/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVol/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVolWin/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVolBase/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVolMin/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVolMax/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to warm-start settings
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/warmStart/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ws1mExtra/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ws5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ws30m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/ws2h/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to cooldown5min (was missing)
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown5min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to move5mAlert (was missing)
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5mAlert/set", mqttPrefix);
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

// ============================================================================
// Web Server Functions
// ============================================================================
// Fase 9: Alle web server functionaliteit verplaatst naar WebServerModule (zie src/WebServer/)

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

// Fase 5.2: Geconsolideerde loop helper voor ring buffer iteratie
// Helper: Iterate through ring buffer and accumulate valid prices
// Geoptimaliseerd: elimineert code duplicatie voor ring buffer loops
static void accumulateValidPricesFromRingBuffer(
    const float* array,
    bool arrayFilled,
    uint16_t currentIndex,
    uint16_t bufferSize,
    uint16_t startOffset,      // Start offset (1 = newest, 2 = one before newest, etc.)
    uint16_t count,             // Number of elements to iterate
    float& sum,
    uint16_t& validCount
)
{
    sum = 0.0f;
    validCount = 0;
    
    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t offset = startOffset + i;
        uint16_t idx;
        
        if (!arrayFilled)
        {
            // Direct mode: check bounds
            if (offset > currentIndex) break;
            idx = currentIndex - offset;
        }
        else
        {
            // Ring buffer mode: use helper
            int32_t idx_temp = getRingBufferIndexAgo(currentIndex, offset, bufferSize);
            if (idx_temp < 0) break;
            idx = (uint16_t)idx_temp;
        }
        
        if (isValidPrice(array[idx]))
        {
            sum += array[idx];
            validCount++;
        }
    }
}

// Fase 5.1: Geconsolideerde berekeningen helpers
// Helper: Calculate available elements in array (elimineert code duplicatie)
// Geoptimaliseerd: elimineert herhaalde "arrayFilled ? arraySize : index" pattern
static inline uint16_t calculateAvailableElements(bool arrayFilled, uint16_t currentIndex, uint16_t arraySize)
{
    return arrayFilled ? arraySize : currentIndex;
}

// Helper: Calculate percentage return (elimineert code duplicatie)
// Geoptimaliseerd: elimineert herhaalde "((priceNow - priceXAgo) / priceXAgo) * 100.0f" pattern
static inline float calculatePercentageReturn(float priceNow, float priceXAgo)
{
    if (priceXAgo == 0.0f || !isValidPrice(priceNow) || !isValidPrice(priceXAgo)) {
        return 0.0f;
    }
    return ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
}

// Helper: Calculate percentage of SOURCE_LIVE entries in the last windowMinutes of minuteAverages
// Returns percentage (0-100) of entries that are SOURCE_LIVE
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Fase 8.5.2: static verwijderd zodat UIController module deze kan gebruiken
uint8_t calcLivePctMinuteAverages(uint16_t windowMinutes)
{
    if (windowMinutes == 0 || windowMinutes > MINUTES_FOR_30MIN_CALC) {
        return 0;
    }
    
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    DataSource* sources = priceData.getMinuteAveragesSource();
    
    // Fase 5.1: Geconsolideerde berekening
    uint8_t availableMinutes = calculateAvailableElements(arrayFilled, index, MINUTES_FOR_30MIN_CALC);
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

// ============================================================================
// Generic Min/Max Finding Helper (Fase 2.1: Geconsolideerde Min/Max Finding)
// ============================================================================

// Generic helper: Find min and max values in an array
// Supports both direct array access and ring buffer access patterns
// Geoptimaliseerd: elimineert code duplicatie tussen findMinMaxInSecondPrices, findMinMaxInLast30Minutes, findMinMaxInLast2Hours
// Fase: static verwijderd zodat AlertEngine module deze functie kan gebruiken voor 5m min/max
bool findMinMaxInArray(
    const float* array,           // Array pointer
    uint16_t arraySize,           // Total array size
    uint16_t currentIndex,        // Current write index (for ring buffer) or count (for direct)
    bool arrayFilled,              // Whether array is filled (ring buffer mode)
    uint16_t elementsToCheck,     // Number of elements to check (0 = all available)
    bool useRingBuffer,           // true = ring buffer with modulo, false = direct indexing
    float &minVal,                // Output: minimum value
    float &maxVal                 // Output: maximum value
)
{
    minVal = 0.0f;
    maxVal = 0.0f;
    
    if (array == nullptr || arraySize == 0) {
        return false;
    }
    
    // Determine count of elements to check
    uint16_t count;
    if (useRingBuffer) {
        // Ring buffer mode: use availableMinutes logic
        uint16_t available = arrayFilled ? arraySize : currentIndex;
        if (available == 0) {
            return false;
        }
        count = (elementsToCheck == 0 || elementsToCheck > available) ? available : elementsToCheck;
    } else {
        // Direct mode: use currentIndex as count
        if (!arrayFilled && array[0] == 0.0f) {
            return false;
        }
        count = arrayFilled ? arraySize : currentIndex;
        if (count == 0) {
            return false;
        }
    }
    
    // Find first valid price to initialize min/max
    bool firstValid = false;
    
    if (useRingBuffer) {
        // Ring buffer mode: iterate backwards from currentIndex
        for (uint16_t i = 1; i <= count; i++) {
            uint16_t idx = (currentIndex - i + arraySize) % arraySize;
            if (isValidPrice(array[idx])) {
                if (!firstValid) {
                    minVal = array[idx];
                    maxVal = array[idx];
                    firstValid = true;
                } else {
                    if (array[idx] < minVal) minVal = array[idx];
                    if (array[idx] > maxVal) maxVal = array[idx];
                }
            }
        }
    } else {
        // Direct mode: iterate from start
        for (uint16_t i = 0; i < count; i++) {
            if (isValidPrice(array[i])) {
                if (!firstValid) {
                    minVal = array[i];
                    maxVal = array[i];
                    firstValid = true;
                } else {
                    if (array[i] < minVal) minVal = array[i];
                    if (array[i] > maxVal) maxVal = array[i];
                }
            }
        }
    }
    
    return firstValid;
}

// Find min and max values in secondPrices array
// Fase 6.1: AlertEngine module gebruikt deze functie (extern declaration in AlertEngine.cpp)
// Fase 2.1: Geoptimaliseerd: gebruikt generic findMinMaxInArray() helper
void findMinMaxInSecondPrices(float &minVal, float &maxVal)
{
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    float* prices = priceData.getSecondPrices();
    bool arrayFilled = priceData.getSecondArrayFilled();
    uint8_t index = priceData.getSecondIndex();
    
    findMinMaxInArray(prices, SECONDS_PER_MINUTE, index, arrayFilled, 0, false, minVal, maxVal);
}

// ============================================================================
// Price Calculation Functions
// ============================================================================

// Calculate 1-minute return: price now vs 60 seconds ago
// Generic return calculation function
// Calculates percentage return: (priceNow - priceXAgo) / priceXAgo * 100
// Supports different array types (uint8_t, uint16_t indices) and optional average calculation
// Fase 2.2: Geoptimaliseerd: geconsolideerde validatie checks, early returns, en average reset logica
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
    // Fase 2.2: Helper: Reset average price index (elimineert code duplicatie)
    auto resetAveragePrice = [averagePriceIndex]() {
        if (averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = 0.0f;
        }
    };
    
    // Fase 2.2: Geconsolideerde early return - check data availability eerst
    if (!arrayFilled && currentIndex < positionsAgo) {
        resetAveragePrice();
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
    
    // Fase 2.2: Geconsolideerde price retrieval - gebruik helper functies
    float priceNow;
    if (arrayFilled) {
        uint16_t lastWrittenIdx = getLastWrittenIndex(currentIndex, arraySize);
        priceNow = priceArray[lastWrittenIdx];
    } else {
        if (currentIndex == 0) {
            resetAveragePrice();
            return 0.0f;
        }
        priceNow = priceArray[currentIndex - 1];
    }
    
    // Fase 2.2: Geconsolideerde price retrieval voor X positions ago
    float priceXAgo;
    if (arrayFilled) {
        int32_t idxXAgo = getRingBufferIndexAgo(currentIndex, positionsAgo, arraySize);
        if (idxXAgo < 0) {
            resetAveragePrice();
            Serial_printf("%s FATAL: idxXAgo invalid, currentIndex=%u\n", logPrefix, currentIndex);
            return 0.0f;
        }
        priceXAgo = priceArray[idxXAgo];
    } else {
        if (currentIndex < positionsAgo) {
            resetAveragePrice();
            return 0.0f;
    }
        priceXAgo = priceArray[currentIndex - positionsAgo];
    }
    
    // Fase 2.2: Geconsolideerde validatie - één check voor beide prijzen
    if (!areValidPrices(priceNow, priceXAgo)) {
        resetAveragePrice();
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
    
    // Fase 2.2: Geconsolideerde return calculation
    // Fase 5.1: Gebruik geconsolideerde percentage berekening helper
    return calculatePercentageReturn(priceNow, priceXAgo);
}

// Fase 4.2.8: calculateReturn1Minute() verplaatst naar PriceData
// Wrapper functie voor backward compatibility
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
float calculateReturn1Minute()
{
    // Fase 4.2.8: Gebruik PriceData::calculateReturn1Minute()
    extern float averagePrices[];
    return priceData.calculateReturn1Minute(averagePrices);
}

// Calculate 5-minute return: price now vs 5 minutes ago
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
float calculateReturn5Minutes()
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
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
float calculateReturn30Minutes()
{
    // Need at least 30 minutes of history
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    // Fase 5.1: Geconsolideerde berekening
    uint8_t availableMinutes = calculateAvailableElements(arrayFilled, index, MINUTES_FOR_30MIN_CALC);
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
    
    // Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Calculate average of last 30 minutes for display (specific to 30m calculation)
    float* averages = priceData.getMinuteAverages();
    bool minuteArrayFilled = arrayFilled;
    uint8_t minuteIndex = index;
    
    // Fase 5.2: Geconsolideerde loop voor ring buffer iteratie
    float last30Sum = 0.0f;
    uint16_t last30Count = 0;
    accumulateValidPricesFromRingBuffer(
        averages,
        minuteArrayFilled,
        minuteIndex,
        MINUTES_FOR_30MIN_CALC,
        1,  // Start vanaf 1 positie terug (nieuwste)
        30, // 30 minuten
        last30Sum,
        last30Count
    );
    if (last30Count > 0)
    {
        averagePrices[2] = last30Sum / last30Count;
    }
    else
    {
        averagePrices[2] = 0.0f;
    }
    
    // Calculate return: use current price vs average of 30 minutes ago
    // Fase 6.1: Geconsolideerde validatie
    float priceNow = prices[0];
    float price30mAgo = averagePrices[2];
    
    // Fase 6.1: Gebruik geconsolideerde validatie helper
    if (!areValidPrices(priceNow, price30mAgo) || price30mAgo <= 0.0f) {
        return 0.0f;
    }
    
    // Fase 5.1: Geconsolideerde percentage berekening
    return calculatePercentageReturn(priceNow, price30mAgo);
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
    
    // Fase 5.1: Geconsolideerde percentage berekening
    return calculatePercentageReturn(currentAvg, prevMinuteAvg);
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
        // Fase 5.1: Geconsolideerde percentage berekening
        return calculatePercentageReturn(currentAvg, firstMinuteAverage);
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
    // Fase 5.2: Geconsolideerde loop voor ring buffer iteratie
    float prev30Sum = 0.0f;
    uint16_t prev30Count = 0;
    accumulateValidPricesFromRingBuffer(
        minuteAverages,
        minuteArrayFilled,
        minuteIndex,
        MINUTES_FOR_30MIN_CALC,
        31, // Start vanaf 31 posities terug
        30, // 30 minuten (tot 60 posities terug)
        prev30Sum,
        prev30Count
    );
    
    // Fase 4.3: Geconsolideerde early return checks
    if (prev30Count == 0 || prev30Sum == 0.0f || last30Count == 0)
    {
        averagePrices[2] = 0.0f; // Reset gemiddelde prijs
        return 0.0f;
    }
    
    float last30Avg = last30Sum / last30Count;
    averagePrices[2] = last30Avg; // Sla gemiddelde prijs op voor 30 minuten
    float prev30Avg = prev30Sum / prev30Count;
    
    // Fase 4.3: Geconsolideerde check (prev30Avg == 0.0f is al gecheckt in prev30Sum == 0.0f)
    // Maar we checken het nog steeds voor veiligheid na deling
    if (prev30Avg == 0.0f)
        return 0.0f;
    
    // Fase 5.1: Geconsolideerde percentage berekening
    return calculatePercentageReturn(last30Avg, prev30Avg);
}

// ret_2h: prijs nu vs 120 minuten (2 uur) geleden (gebruik minuteAverages)
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Calculate 2-hour return: price now vs 120 minutes ago
static float calculateReturn2Hours()
{
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    float* averages = priceData.getMinuteAverages();
    
    // Fase 5.1: Geconsolideerde berekening
    uint8_t availableMinutes = calculateAvailableElements(arrayFilled, index, MINUTES_FOR_30MIN_CALC);
    
    // Bereken gemiddelde van beschikbare minuten voor display (voor 2h box)
    // Dit wordt gedaan ongeacht of er 120 minuten zijn, zodat de waarde getoond kan worden
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    if (availableMinutes > 0) {
        // Fase 5.2: Geconsolideerde loop voor ring buffer iteratie
        float last120Sum = 0.0f;
        uint16_t last120Count = 0;
        uint16_t minutesToUse = (availableMinutes < 120) ? availableMinutes : 120;  // Gebruik beschikbare minuten, max 120
        accumulateValidPricesFromRingBuffer(
            averages,
            arrayFilled,
            index,
            MINUTES_FOR_30MIN_CALC,
            1,  // Start vanaf 1 positie terug (nieuwste)
            minutesToUse,
            last120Sum,
            last120Count
        );
        if (last120Count > 0)
        {
            averagePrices[3] = last120Sum / last120Count;
        }
        else
        {
            averagePrices[3] = 0.0f;
        }
    } else {
        averagePrices[3] = 0.0f;
    }
    #endif
    
    // Als er minder dan 120 minuten zijn, bereken return op basis van beschikbare data
    if (availableMinutes < 120)
    {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // Bereken return op basis van beschikbare minuten (minimaal 2 minuten nodig)
        if (availableMinutes >= 2) {
            // Get current price (last minute average)
            uint8_t lastMinuteIdx;
            if (!arrayFilled)
            {
                if (index == 0) {
                    Serial.printf("[Ret2h] ERROR: index == 0, availableMinutes=%u\n", availableMinutes);
                    return 0.0f;
                }
                lastMinuteIdx = index - 1;
            }
            else
            {
                lastMinuteIdx = getLastWrittenIndex(index, MINUTES_FOR_30MIN_CALC);
            }
            float priceNow = averages[lastMinuteIdx];
            
            // Get price X minutes ago (waar X = availableMinutes - 1, maar min 1)
            uint8_t minutesAgo = (availableMinutes > 1) ? (availableMinutes - 1) : 1;
            uint8_t idxXAgo;
            if (!arrayFilled)
            {
                if (index < minutesAgo) {
                    Serial.printf("[Ret2h] ERROR: index=%u < minutesAgo=%u, availableMinutes=%u\n", index, minutesAgo, availableMinutes);
                    return 0.0f;
                }
                idxXAgo = index - minutesAgo;
            }
            else
            {
                int32_t idxXAgo_temp = getRingBufferIndexAgo(index, minutesAgo, MINUTES_FOR_30MIN_CALC);
                if (idxXAgo_temp < 0) {
                    Serial.printf("[Ret2h] ERROR: idxXAgo_temp < 0, index=%u, minutesAgo=%u\n", index, minutesAgo);
                    return 0.0f;
                }
                idxXAgo = (uint8_t)idxXAgo_temp;
            }
            
            float priceXAgo = averages[idxXAgo];
            
            // Validate prices
            if (priceXAgo <= 0.0f || priceNow <= 0.0f)
            {
                Serial.printf("[Ret2h] ERROR: Invalid prices: priceNow=%.2f, priceXAgo=%.2f\n", priceNow, priceXAgo);
                return 0.0f;
            }
            
            // Return percentage: (now - X ago) / X ago * 100
            float ret = ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
            return ret;
        } else {
            Serial.printf("[Ret2h] ERROR: availableMinutes=%u < 2\n", availableMinutes);
        }
        #endif
        return 0.0f;
    }
    
    // Get current price (last minute average)
    uint8_t lastMinuteIdx;
    if (!arrayFilled)
    {
        if (index == 0) {
            #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
            averagePrices[3] = 0.0f;  // Reset voor 2h box
            #endif
            Serial.printf("[Ret2h] ERROR: index == 0 in 120m path\n");
            return 0.0f;
        }
        lastMinuteIdx = index - 1;
    }
    else
    {
        lastMinuteIdx = getLastWrittenIndex(index, MINUTES_FOR_30MIN_CALC);
    }
    float priceNow = averages[lastMinuteIdx];
    
    // Get price 120 minutes ago
    // Voor een ring buffer: als we 120 posities terug willen in een buffer van 120,
    // dan moeten we de laatste geschreven positie gebruiken en dan 119 posities terug gaan
    uint8_t idx120mAgo;
    if (!arrayFilled)
    {
        if (index < 120) {
            Serial.printf("[Ret2h] ERROR: index=%u < 120 in 120m path\n", index);
            return 0.0f;  // averagePrices[3] is al berekend boven
        }
        idx120mAgo = index - 120;
    }
    else
    {
        // Voor een ring buffer: als we 120 minuten terug willen in een buffer van 120,
        // dan moeten we 119 posities terug gaan vanaf de laatste geschreven positie
        // (omdat we al op de laatste geschreven positie staan)
        uint8_t lastWrittenIdx = getLastWrittenIndex(index, MINUTES_FOR_30MIN_CALC);
        // 119 posities terug (niet 120, omdat positionsAgo >= bufferSize niet toegestaan is)
        int32_t idx120mAgo_temp = getRingBufferIndexAgo(lastWrittenIdx, 119, MINUTES_FOR_30MIN_CALC);
        if (idx120mAgo_temp < 0) {
            Serial.printf("[Ret2h] ERROR: idx120mAgo_temp < 0, index=%u, lastWrittenIdx=%u\n", index, lastWrittenIdx);
            return 0.0f;  // averagePrices[3] is al berekend boven
        }
        idx120mAgo = (uint8_t)idx120mAgo_temp;
    }
    
    float price120mAgo = averages[idx120mAgo];
    
    // Validate prices
    if (price120mAgo <= 0.0f || priceNow <= 0.0f)
    {
        Serial.printf("[Ret2h] ERROR: Invalid prices in 120m path: priceNow=%.2f, price120mAgo=%.2f\n", 
                      priceNow, price120mAgo);
        return 0.0f;  // averagePrices[3] is al berekend boven
    }
    
    // Return percentage: (now - 120m ago) / 120m ago * 100
    float ret = ((priceNow - price120mAgo) / price120mAgo) * 100.0f;
    return ret;
}

// Helper: beschikbare uren in hourly buffer
static inline uint16_t getAvailableHours()
{
    if (hourlyAverages == nullptr) {
        return 0;
    }
    return calculateAvailableElements(hourArrayFilled, hourIndex, HOURS_FOR_7D);
}

// Calculate return based on hourly buffer
static float calculateReturnFromHourly(uint16_t hoursBack)
{
    uint16_t availableHours = getAvailableHours();
    if (availableHours < 2 || hoursBack == 0) {
        return 0.0f;
    }
    
    uint16_t hoursAgo = (availableHours > hoursBack) ? hoursBack : (availableHours - 1);
    if (hoursAgo == 0) {
        return 0.0f;
    }
    
    uint16_t lastHourIdx;
    if (!hourArrayFilled)
    {
        if (hourIndex == 0) {
            return 0.0f;
        }
        lastHourIdx = hourIndex - 1;
        if (lastHourIdx < hoursAgo) {
            return 0.0f;
        }
    }
    else
    {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }
    
    uint16_t idxHoursAgo;
    if (!hourArrayFilled)
    {
        idxHoursAgo = lastHourIdx - hoursAgo;
            }
            else
            {
        int32_t idxHoursAgoTemp = getRingBufferIndexAgo(lastHourIdx, hoursAgo, HOURS_FOR_7D);
        if (idxHoursAgoTemp < 0) {
            return 0.0f;
        }
        idxHoursAgo = (uint16_t)idxHoursAgoTemp;
    }
    
    float priceNow = hourlyAverages[lastHourIdx];
    float priceAgo = hourlyAverages[idxHoursAgo];
    
    return calculatePercentageReturn(priceNow, priceAgo);
}

// ret_1d: prijs nu vs 24 uur geleden (hourly buffer)
static float calculateReturn24Hours()
{
    return calculateReturnFromHourly(24);
}

// ret_7d: prijs nu vs 7 dagen geleden (hourly buffer)
static float calculateReturn7Days()
{
    return calculateReturnFromHourly(HOURS_FOR_7D);
}

// ============================================================================
// Trend Detection Functions
// ============================================================================

// Fase 5.3.3: determineTrendState() wrapper functie verwijderd - alle calls gebruiken nu trendDetector.determineTrendState()

// Voeg absolute 1m return toe aan volatiliteit buffer (wordt elke minuut aangeroepen)
// Geoptimaliseerd: bounds checking en validatie toegevoegd
// Fase 5.3.11: Alle wrapper functies verwijderd - alle calls gebruiken nu directe module calls

// Find min and max values in last 30 minutes of minuteAverages array
// Fase 6.1: AlertEngine module gebruikt deze functie (extern declaration in AlertEngine.cpp)
// Fase 2.1: Geoptimaliseerd: gebruikt generic findMinMaxInArray() helper
void findMinMaxInLast30Minutes(float &minVal, float &maxVal)
{
    findMinMaxInArray(minuteAverages, MINUTES_FOR_30MIN_CALC, minuteIndex, minuteArrayFilled, 30, true, minVal, maxVal);
}

#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
// Find min and max values in last 2 hours (120 minutes) of minuteAverages array
// Alleen voor CYD platforms met 2h box
// Fase 2.1: Geoptimaliseerd: gebruikt generic findMinMaxInArray() helper
void findMinMaxInLast2Hours(float &minVal, float &maxVal)
{
    findMinMaxInArray(minuteAverages, MINUTES_FOR_30MIN_CALC, minuteIndex, minuteArrayFilled, 120, true, minVal, maxVal);
}
#endif

// Compute 2-hour metrics uniformly from existing state
// Gebruikt bestaande berekeningen: averagePrices[3], findMinMaxInLast2Hours() (CYD) of minuteAverages (andere), hasRet2h
TwoHMetrics computeTwoHMetrics()
{
    TwoHMetrics metrics;
    
    // Gebruik bestaande 2h average (wordt berekend in calculateReturn2Hours())
    metrics.avg2h = averagePrices[3];
    
    // Gebruik bestaande findMinMaxInLast2Hours() functie voor CYD platforms
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    findMinMaxInLast2Hours(metrics.low2h, metrics.high2h);
    #else
    // Voor niet-CYD platforms: bereken 2h min/max uit minuteAverages (zoals findMinMaxInLast2Hours doet)
    metrics.low2h = 0.0f;
    metrics.high2h = 0.0f;
    
    uint8_t availableMinutes = 0;
    if (!minuteArrayFilled) {
        availableMinutes = minuteIndex;
    } else {
        availableMinutes = MINUTES_FOR_30MIN_CALC;  // 120 minuten
    }
    
    if (availableMinutes > 0) {
        // Gebruik laatste 120 minuten (of minder als niet beschikbaar)
        uint8_t count = (availableMinutes < 120) ? availableMinutes : 120;
        bool firstValid = false;
        
        for (uint8_t i = 1; i <= count; i++) {
            uint8_t idx = (minuteIndex - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
            if (isValidPrice(minuteAverages[idx])) {
                if (!firstValid) {
                    metrics.low2h = minuteAverages[idx];
                    metrics.high2h = minuteAverages[idx];
                    firstValid = true;
                } else {
                    if (minuteAverages[idx] < metrics.low2h) metrics.low2h = minuteAverages[idx];
                    if (minuteAverages[idx] > metrics.high2h) metrics.high2h = minuteAverages[idx];
                }
            }
        }
    }
    #endif
    
    // Valid check: avg2h > 0, high2h > 0, low2h > 0, high2h >= low2h, en hasRet2h
    metrics.valid = (metrics.avg2h > 0.0f) && 
                    (metrics.high2h > 0.0f) && 
                    (metrics.low2h > 0.0f) && 
                    (metrics.high2h >= metrics.low2h) &&
                    hasRet2h;
    
    // Bereken range percentage: (high2h - low2h) / avg2h * 100
    if (metrics.valid && metrics.avg2h > 0.0f) {
        metrics.rangePct = ((metrics.high2h - metrics.low2h) / metrics.avg2h) * 100.0f;
    } else {
        metrics.rangePct = 0.0f;
    }
    
    // Log metrics (alleen 1x per boot of op debug-flag)
    static bool loggedOnce = false;
    static bool lastValidState = false;
    if (metrics.valid && (!loggedOnce || !lastValidState)) {
        Serial.printf("[2H] avg=%.2f high=%.2f low=%.2f range=%.2f%%\n", 
                      metrics.avg2h, metrics.high2h, metrics.low2h, metrics.rangePct);
        loggedOnce = true;
        lastValidState = true;
    } else if (!metrics.valid && lastValidState) {
        // Reset logged flag als valid weer false wordt (bijvoorbeeld na reset)
        lastValidState = false;
        loggedOnce = false;
    }
    
    return metrics;
}

// Add price to second array (called every second)
// Geoptimaliseerd: bounds checking toegevoegd voor robuustheid
// Fase 4.2.11: Oude addPriceToSecondArray() functie verwijderd
// Gebruik nu priceData.addPriceToSecondArray() in plaats daarvan

// Update minute averages (called every minute)
// Geoptimaliseerd: bounds checking en validatie toegevoegd
static void updateHourlyAverage()
{
    minutesSinceHourUpdate++;
    if (minutesSinceHourUpdate < MINUTES_PER_HOUR) {
        return;
    }
    minutesSinceHourUpdate = 0;
    
    uint16_t availableMinutes = calculateAvailableElements(minuteArrayFilled, minuteIndex, MINUTES_FOR_30MIN_CALC);
    if (availableMinutes < MINUTES_PER_HOUR) {
        return;
    }
    
    float hourSum = 0.0f;
    uint16_t hourCount = 0;
    accumulateValidPricesFromRingBuffer(
        minuteAverages,
        minuteArrayFilled,
        minuteIndex,
        MINUTES_FOR_30MIN_CALC,
        1,
        MINUTES_PER_HOUR,
        hourSum,
        hourCount
    );
    
    if (hourCount == 0) {
        return;
    }
    
    float hourAvg = hourSum / hourCount;
    if (!isValidPrice(hourAvg)) {
        return;
    }

    if (hourlyAverages == nullptr) {
        return;
    }
    hourlyAverages[hourIndex] = hourAvg;
    hourIndex = (hourIndex + 1) % HOURS_FOR_7D;
    if (hourIndex == 0) {
        hourArrayFilled = true;
    }
}

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
    
    // Update hourly aggregate buffer
    updateHourlyAverage();
    
    // Update warm-start status na elke minuut update
    updateWarmStartStatus();
}

// ============================================================================
// Price Fetching and Management Functions
// ============================================================================

// Fetch the symbols' current prices (thread-safe met mutex)
// Fase 8.9.1: static verwijderd zodat UIController module deze kan gebruiken
static void updateLatestKlineMetricsIfNeeded()
{
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    static unsigned long last1mFetchMs = 0;
    static unsigned long last5mFetchMs = 0;
    unsigned long now = millis();
    
    if (last1mFetchMs == 0 || (now - last1mFetchMs) >= 60000UL) {
        float temp1mPrices[2];
        int fetched1m = fetchBinanceKlines(binanceSymbol, "1m", 2, temp1mPrices, nullptr, 2);
        if (fetched1m > 0) {
            last1mFetchMs = now;
        }
    }
    
    if (last5mFetchMs == 0 || (now - last5mFetchMs) >= 300000UL) {
        float temp5mPrices[2];
        int fetched5m = fetchBinanceKlines(binanceSymbol, "5m", 2, temp5mPrices, nullptr, 2);
        if (fetched5m > 0) {
            last5mFetchMs = now;
        }
    }
}

void fetchPrice()
{
    // Controleer eerst of WiFi verbonden is
    if (WiFi.status() != WL_CONNECTED) {
        #if !DEBUG_BUTTON_ONLY
        Serial.println(F("[API] WiFi niet verbonden, skip fetch"));
        #endif
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
        #if !DEBUG_BUTTON_ONLY
        Serial.printf("[API] WARN -> %s leeg response (tijd: %lu ms) - mogelijk timeout of netwerkprobleem\n", binanceSymbol, fetchTime);
        #endif
        // Gebruik laatste bekende prijs als fallback (al ingesteld als fetched = prices[0])
    } else {
        // Succesvol opgehaald (alleen loggen bij langzame calls > 1200ms)
        #if !DEBUG_BUTTON_ONLY
        if (fetchTime > 1200) {
            Serial.printf(F("[API] OK -> %s %.2f (tijd: %lu ms) - langzaam\n"), binanceSymbol, fetched, fetchTime);
        }
        #endif
        
        // Neem mutex voor data updates (timeout verhoogd om mutex conflicts te verminderen)
        // API task heeft prioriteit: verhoogde timeout om mutex te krijgen zelfs als UI bezig is
        #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(500); // TTGO/GEEK: 500ms
        #else
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(400); // CYD/ESP32-S3: 400ms voor betere mutex acquisitie
        #endif
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        // Fase 4.1: Gebruik geconsolideerde mutex timeout handling
        // Fase 4.2: Gebruik geconsolideerde mutex pattern helper
        static uint32_t mutexTimeoutCount = 0;
        if (safeMutexTake(dataMutex, apiMutexTimeout, "apiTask fetchPrice"))
        {
            // Fase 4.2: Geconsolideerde mutex timeout counter reset
            resetMutexTimeoutCounter(mutexTimeoutCount);
            
            if (openPrices[0] == 0)
                openPrices[0] = fetched; // capture session open once
            lastApiMs = millis();
            
            prices[0] = fetched;
            
            // Update anchor min/max als anchor actief is
            // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. directe globale variabele updates
            if (anchorActive && anchorPrice > 0.0f) {
                anchorSystem.updateAnchorMinMax(fetched);
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
            
            // Hourly buffer availability
            uint16_t availableHours = getAvailableHours();
            hasRet4hLive = (availableHours >= 4);
            hasRet4h = hasRet4hWarm || hasRet4hLive;
            if (hasRet4hLive) {
                ret_4h = calculateReturnFromHourly(4);
            } else if (!hasRet4hWarm) {
                ret_4h = 0.0f;
            }
            hasRet1d = hasRet1dWarm || (availableHours >= 24);
            if (availableHours >= 24) {
                ret_1d = calculateReturn24Hours();
            } else if (!hasRet1dWarm) {
                ret_1d = 0.0f;
            }
            bool hasRet7dLive = (availableHours >= HOURS_FOR_7D);
            hasRet7d = hasRet7dWarm || hasRet7dLive;
            if (hasRet7dLive) {
                ret_7d = calculateReturn7Days();
            } else if (!hasRet7dWarm) {
                ret_7d = 0.0f;
            }
            
            // Fase 5.1: Bepaal trend state op basis van 2h return (gebruik TrendDetector module)
            if (hasRet2h && hasRet30m) {
                extern float trendThreshold;
                // Fase 5.3.15: Update module eerst, synchroniseer dan globale variabele
                TrendState newTrendState = trendDetector.determineTrendState(ret_2h, ret_30m, trendThreshold);
                trendDetector.setTrendState(newTrendState);  // Update TrendDetector state
                trendState = newTrendState;  // Synchroniseer globale variabele
            }
            
            // Check trend change en stuur notificatie indien nodig
            // Fase 5.3.2: Directe module call i.p.v. wrapper
            extern float ret_2h;
            extern bool minuteArrayFilled;
            extern uint8_t minuteIndex;
            trendDetector.checkTrendChange(ret_30m, ret_2h, minuteArrayFilled, minuteIndex);
            // Update globale trendState en previousTrendState voor backward compatibility
            extern TrendState trendState;
            extern TrendState previousTrendState;
            trendState = trendDetector.getTrendState();
            previousTrendState = trendDetector.getPreviousTrendState();
            
            // Check medium trend change en stuur notificatie indien nodig
            extern bool hasRet4h;
            extern bool hasRet1d;
            if (hasRet4h && hasRet1d) {
                extern float ret_4h;
                extern float ret_1d;
                const float mediumThreshold = 2.0f;
                trendDetector.checkMediumTrendChange(ret_4h, ret_1d, mediumThreshold);
            }

            // Check lange termijn trend change (7d) en stuur notificatie indien nodig
            extern bool hasRet7d;
            if (hasRet7d) {
                extern float ret_7d;
                const float longTermThreshold = 1.0f;
                trendDetector.checkLongTermTrendChange(ret_7d, longTermThreshold);
            }
            
            // Update volatiliteit buffer elke minuut met absolute 1m return
            // Fase 5.3.5: Directe module call i.p.v. wrapper
            if (minuteUpdate && ret_1m != 0.0f)
            {
                float abs_ret_1m = fabsf(ret_1m);
                volatilityTracker.addAbs1mReturnToVolatilityBuffer(abs_ret_1m);
                
                // Bereken gemiddelde en bepaal volatiliteit state
                // Fase 5.2: Gebruik VolatilityTracker module
                float avg_abs_1m = volatilityTracker.calculateAverageAbs1mReturn();
                // Fase 5.2: Gebruik VolatilityTracker module
                if (avg_abs_1m > 0.0f)
                {
                    extern float volatilityLowThreshold;
                    extern float volatilityHighThreshold;
                    // Fase 5.3.15: Update module eerst, synchroniseer dan globale variabele
                    VolatilityState newVolatilityState = volatilityTracker.determineVolatilityState(avg_abs_1m, volatilityLowThreshold, volatilityHighThreshold);
                    volatilityTracker.setVolatilityState(newVolatilityState);  // Update VolatilityTracker state
                    volatilityState = newVolatilityState;  // Synchroniseer globale variabele
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
            
            // 2h return voor CYD platforms (index 3)
            #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
            // ret_2h wordt nu altijd berekend in calculateReturn2Hours(), ook als er minder dan 120 minuten zijn
            // Het berekent een return op basis van beschikbare data (minimaal 2 minuten nodig)
            prices[3] = ret_2h;
            #endif
            
            // Check thresholds and send notifications if needed (met ret_5m voor extra filtering)
            // Fase 6.1.11: Gebruik AlertEngine module i.p.v. globale functie
            alertEngine.checkAndNotify(ret_1m, ret_5m, ret_30m);
            
            // Check anchor take profit / max loss alerts
            // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
            anchorSystem.checkAnchorAlerts();
            
            // Check 2-hour notifications (breakout, breakdown, compression, mean reversion, anchor context)
            // Wordt aangeroepen na elke price update
            // Functie checkt zelf of anchorPrice > 0 voor anchor context notificaties
            float manualAnchor = anchorActive ? anchorPrice : 0.0f;
            AlertEngine::check2HNotifications(prices[0], manualAnchor);
            
            // Touchscreen anchor set functionaliteit verwijderd - gebruik nu fysieke knop
            
            // Publiceer waarden naar MQTT
            publishMqttValues(fetched, ret_1m, ret_5m, ret_30m);
            
            // Zet flag voor nieuwe data (voor grafiek update)
            newPriceDataAvailable = true;
            
            safeMutexGive(dataMutex, "fetchPrice");  // MUTEX EERST VRIJGEVEN!
            ok = true;
            
            // Periodieke auto anchor update (elke 5 minuten checken)
            // BELANGRIJK: Dit moet BUITEN de mutex gebeuren om deadlocks te voorkomen!
            static unsigned long lastAutoAnchorCheckMs = 0;
            static bool firstAutoAnchorUpdateDone = false;
            unsigned long nowMs = millis();
            
            // Eerste update: wacht 5 seconden na boot om race conditions te voorkomen
            if (!firstAutoAnchorUpdateDone && nowMs >= 5000) {
                Serial.println("[API Task] Eerste auto anchor update (na 5s delay, buiten mutex)...");
                bool result = AlertEngine::maybeUpdateAutoAnchor(true);  // Force update na warm-start
                Serial.printf("[API Task] Auto anchor update result: %s\n", result ? "SUCCESS" : "FAILED");
                firstAutoAnchorUpdateDone = true;
                lastAutoAnchorCheckMs = nowMs;
            } else if (firstAutoAnchorUpdateDone && (nowMs - lastAutoAnchorCheckMs) >= (5UL * 60UL * 1000UL)) {
                // Periodieke updates: elke 5 minuten
                AlertEngine::maybeUpdateAutoAnchor(false);  // Non-force update
                lastAutoAnchorCheckMs = nowMs;
            }
        } else {
            // Fase 4.1: Geconsolideerde mutex timeout handling
            handleMutexTimeout(mutexTimeoutCount, "API", binanceSymbol);
        }
    }
}

// Update the UI (wordt aangeroepen vanuit uiTask met mutex)
// Update UI - Refactored to use helper functions
// UI Update Helper Functions - Split from updateUI() for better organization
// Fase 8.11.2: Oude update functies verwijderd - nu in UIController module
// - updateChartSection()
// - updateHeaderSection()
// - updatePriceCardsSection()
// - updateUI()

// ============================================================================
// ============================================================================
// UI Update Helper Functions
// ============================================================================

// Fase 8: UI functies zijn verplaatst naar UIController module (zie src/UIController/UIController.cpp)
// - createChart(), createHeaderLabels(), createPriceBoxes(), createFooter()
// - updateDateTimeLabels(), updateTrendLabel(), updateVolatilityLabel(), updateWarmStartStatusLabel()
// - updateBTCEURCard(), updateAveragePriceCard(), updatePriceCardColor()
// - updateChartSection(), updateHeaderSection(), updatePriceCardsSection()
// - updateUI(), checkButton(), updateChartRange(), setupLVGL()

// Cache variabelen voor datum/tijd labels (lokaal voor deze functie)
static char lastDateText[11] = {0};  // Cache voor date label
static char lastTimeText[9] = {0};   // Cache voor time label

// ============================================================================
// RGB LED Functions (alleen voor CYD platforms)
// ============================================================================

// Helper functie om footer bij te werken
// Fase 8: updateFooter() gebruikt nog globale pointers (kan later naar UIController module verplaatst worden)
void updateFooter()
{
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
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
    
    // TTGO/GEEK: Update versie label (rechtsonder) - force update als cache leeg is
    if (chartVersionLabel != nullptr) {
        if (strlen(lastVersionText) == 0 || strcmp(lastVersionText, VERSION_STRING) != 0) {
            lv_label_set_text(chartVersionLabel, VERSION_STRING);
            strncpy(lastVersionText, VERSION_STRING, sizeof(lastVersionText) - 1);
            lastVersionText[sizeof(lastVersionText) - 1] = '\0';
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
        // Force update als cache leeg is of waarde veranderd is
        if (strlen(lastVersionText) == 0 || strcmp(lastVersionText, versionBuffer) != 0 || lastRamValue != freeRAM) {
        lv_label_set_text(chartVersionLabel, versionBuffer);
            strncpy(lastVersionText, versionBuffer, sizeof(lastVersionText) - 1);
            lastVersionText[sizeof(lastVersionText) - 1] = '\0';
            lastRamValue = freeRAM;
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
    
    // CYD: Update versie label (rechtsonder, regel 2) - force update als cache leeg is
    if (chartVersionLabel != nullptr) {
        if (strlen(lastVersionText) == 0 || strcmp(lastVersionText, VERSION_STRING) != 0) {
            lv_label_set_text(chartVersionLabel, VERSION_STRING);
            strncpy(lastVersionText, VERSION_STRING, sizeof(lastVersionText) - 1);
            lastVersionText[sizeof(lastVersionText) - 1] = '\0';
        }
    }
    #endif
}

// ============================================================================
// UI Helper Functions - Refactored from buildUI() for better code organization
// ============================================================================

// Helper functie om scroll uit te schakelen voor een object
// Fase 8.11.1: static verwijderd zodat UIController module deze kan gebruiken
void disableScroll(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(obj, 0, LV_PART_SCROLLBAR);
}

// Fase 8.11.1: Oude create functies verwijderd - nu in UIController module
// De volgende functies zijn verplaatst naar src/UIController/UIController.cpp:
// - createChart()
// - createHeaderLabels()
// - createPriceBoxes()
// - createFooter()
// - buildUI()

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
// Fase 8.9.1: static verwijderd zodat UIController module deze kan gebruiken
unsigned long lastButtonPress = 0;
int lastButtonState = HIGH; // Start met HIGH (niet ingedrukt)
// Fase 8.9.1: const verwijderd, nu als #define gebruikt zodat UIController module deze kan gebruiken
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 500  // 500ms debounce
#endif

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
        // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
        if (anchorSystem.setAnchorPrice(0.0f)) {
            // Update UI (this will also take the mutex internally)
            // Fase 8.8.1: Gebruik module versie (parallel - oude functie blijft bestaan)
            uiController.updateUI();
        } else {
            Serial_println("[Button] WARN: Kon anchor niet instellen");
        }
        
        // Publish to MQTT if connected (optional, for logging)
        if (mqttConnected) {
            // button/reset publish verwijderd - gebruik nu anchorValue number entity
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
    // ESP32-S3 fix: Serial moet als ALLER EERSTE worden geïnitialiseerd
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    Serial.begin(115200);
    delay(500); // ESP32-S3 heeft tijd nodig voor Serial stabilisatie
    Serial.println("\n\n=== ESP32-S3 Crypto Monitor Starting ===");
    Serial.flush(); // Force flush om te zorgen dat output wordt verzonden
    #else
    Serial.begin(115200);
    #endif
    
    // Load settings from Preferences
    // Initialize SettingsStore
    settingsStore.begin();
    
    // Fase 4.1: Initialize ApiClient
    apiClient.begin();
    
    // Fase 4.2.1: Initialize PriceData (module structuur)
    // Fase 4.2.5: State variabelen worden geïnitialiseerd in constructor en gesynchroniseerd in begin()
    priceData.begin();  // begin() synchroniseert state met globale variabelen
    
    // Fase 5.1: Initialize TrendDetector (trend detection module)
    trendDetector.begin();  // begin() synchroniseert state met globale variabelen
    
    // Fase 5.2: Initialize VolatilityTracker (volatiliteit tracking module)
    volatilityTracker.begin();  // begin() synchroniseert state met globale variabelen
    
    // Fase 6.1: Initialize AlertEngine (alert detection module)
    alertEngine.begin();
    
    // Fase 6.2.6: Initialiseer AnchorSystem module
    anchorSystem.begin();  // begin() synchroniseert state met globale variabelen
    
    // Load settings
    loadSettings();
    // ESP32-S3 fix: DEV_DEVICE_INIT() overslaan - backlight wordt later ingesteld via setDisplayBrigthness() met PWM
    // Dit voorkomt conflict tussen digitalWrite en ledcAttachChannel
    #if !defined(PLATFORM_ESP32S3_SUPERMINI)
    DEV_DEVICE_INIT();
    #endif
    
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
    // Pas display rotatie toe (0 = normaal, 2 = 180 graden)
    // Alleen 0 en 2 zijn geldig voor 180 graden rotatie
    uint8_t rotation = (displayRotation == 2) ? 2 : 0;
    gfx->setRotation(rotation);
    // TTGO/ESP32-S3: altijd false (ST7789 heeft geen inversie nodig)
    // CYD24/CYD28: standaard false, behalve wanneer INVERT_COLORS flag is gezet
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    gfx->invertDisplay(false); // TTGO/ESP32-S3 T-Display/GEEK heeft geen inversie nodig (ST7789)
    #elif defined(PLATFORM_CYD24_INVERT_COLORS)
    gfx->invertDisplay(true); // CYD24: inverteer kleuren
    #elif defined(PLATFORM_CYD28_INVERT_COLORS)
    gfx->invertDisplay(true); // CYD28 2USB variant: inverteer kleuren
    #else
    gfx->invertDisplay(false); // CYD24/CYD28: geen inversie (standaard)
    #endif
    gfx->fillScreen(RGB565_BLACK);
    
    // ESP32-S3 fix: Backlight moet opnieuw worden ingesteld na display initialisatie
    // DEV_DEVICE_INIT() wordt eerder aangeroepen, maar ledc kan conflicteren
    // GEEK gebruikt digitalWrite (geen PWM), dus alleen voor SUPERMINI
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
    // Zet backlight eerst uit, dan weer aan met PWM
    digitalWrite(GFX_BL, LOW);
    delay(10);
    setDisplayBrigthness();
    #elif defined(PLATFORM_ESP32S3_GEEK)
    // GEEK gebruikt digitalWrite, geen PWM nodig - backlight is al aan via DEV_DEVICE_INIT()
    // Geen extra actie nodig
    #else
    setDisplayBrigthness();
    #endif
    
    // Geef display tijd om te stabiliseren na initialisatie (vooral belangrijk voor CYD displays en ESP32-S3)
    #if !defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
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
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: double buffer alleen als PSRAM beschikbaar is
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
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: 30 regels met PSRAM, 2 zonder
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;
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
    #elif defined(PLATFORM_ESP32S3_GEEK)
        boardName = "ESP32-S3 GEEK";
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
    // ESP32 Arduino core initialiseert de watchdog al, dus eerst deinit dan init
    esp_err_t deinit_err = esp_task_wdt_deinit();
    if (deinit_err != ESP_OK && deinit_err != ESP_ERR_NOT_FOUND) {
        #if !DEBUG_BUTTON_ONLY
        Serial.printf("[WDT] Deinit error: %d\n", deinit_err);
        #endif
    }
    
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_err_t init_err = esp_task_wdt_init(&wdt_config);
    if (init_err != ESP_OK) {
        #if !DEBUG_BUTTON_ONLY
        Serial.printf("[WDT] Init error: %d\n", init_err);
        #endif
    }
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
                        char mqttPrefixTemp[64];
                        getMqttTopicPrefix(mqttPrefixTemp, sizeof(mqttPrefixTemp));
                        snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", mqttPrefixTemp);
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
    // Maak mutexen VOOR we ze gebruiken (moeten eerst aangemaakt worden)
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println(F("[Error] Kon dataMutex niet aanmaken!"));
    }
    
    // S0: Maak netwerk mutex voor HTTP operaties
    gNetMutex = xSemaphoreCreateMutex();
    if (gNetMutex == NULL) {
        Serial.println(F("[Error] Kon gNetMutex niet aanmaken!"));
    } else {
        Serial.println("[FreeRTOS] Mutex aangemaakt");
    }
}

// Alloceer grote arrays dynamisch voor CYD en TTGO zonder PSRAM om DRAM overflow te voorkomen
static void allocateDynamicArrays()
{
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
    // Voor CYD/TTGO zonder PSRAM: alloceer arrays dynamisch
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

    if (hourlyAverages == nullptr) {
        if (hasPSRAM()) {
            hourlyAverages = (float *)heap_caps_malloc(HOURS_FOR_7D * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (!hourlyAverages) {
            hourlyAverages = (float *)heap_caps_malloc(HOURS_FOR_7D * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (!hourlyAverages) {
            Serial.println(F("[Memory] FATAL: Hourly buffer allocatie gefaald!"));
            Serial.printf("[Memory] Free heap: %u bytes\n", ESP.getFreeHeap());
            while (true) {
                /* no need to continue */
            }
        }

        for (uint16_t i = 0; i < HOURS_FOR_7D; i++) {
            hourlyAverages[i] = 0.0f;
        }
        Serial.printf("[Memory] Hourly buffer gealloceerd: hourlyAverages=%u bytes\n",
                      HOURS_FOR_7D * sizeof(float));
    }
}

static void startFreeRTOSTasks()
{
    // M1: Heap telemetry vóór startFreeRTOSTasks
    logHeap("TASKS_START_PRE");
    
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
    
    // M1: Heap telemetry na startFreeRTOSTasks
    logHeap("TASKS_START_POST");
}

void setup()
{
    // Setup in logical sections for better readability and maintainability
    setupSerialAndDevice();
    setupDisplay();
    // Fase 8: LVGL initialisatie via UIController module
    uiController.setupLVGL();
    setupWatchdog();
    setupWiFiEventHandlers();
    setupMutex();  // Mutex moet vroeg aangemaakt worden, maar tasks starten later
    
    // Alloceer dynamische arrays voor CYD zonder PSRAM (moet voor initialisatie)
    allocateDynamicArrays();
    
    // Initialize source tracking arrays (default: all LIVE, wordt overschreven door warm-start)
    for (uint8_t i = 0; i < SECONDS_PER_MINUTE; i++) {
        secondPricesSource[i] = SOURCE_LIVE;
    }
    // Initialize fiveMinutePricesSource (alleen als array bestaat, niet nullptr)
    if (fiveMinutePricesSource != nullptr) {
    for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
        fiveMinutePricesSource[i] = SOURCE_LIVE;
    }
    }
    // Initialize minuteAveragesSource (alleen als array bestaat, niet nullptr)
    if (minuteAveragesSource != nullptr) {
    for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
        minuteAveragesSource[i] = SOURCE_LIVE;
        }
    }
    
    // Initialize lastPriceLblValueArray (cache voor average price labels)
    for (uint8_t i = 0; i < SYMBOL_COUNT; i++) {
        lastPriceLblValueArray[i] = -1.0f;
    }
    
    // WiFi connection and initial data fetch (maakt tijdelijk UI aan)
    wifiConnectionAndFetchPrice();
    
    // Fase 4.2.5: Synchroniseer PriceData state na warm-start (als warm-start is uitgevoerd)
    priceData.syncStateFromGlobals();
    
    // Fase 7.2: Bind WarmStartWrapper dependencies
    CryptoMonitorSettings currentSettings;
    currentSettings.warmStartEnabled = warmStartEnabled;
    currentSettings.warmStart1mExtraCandles = warmStart1mExtraCandles;
    currentSettings.warmStart5mCandles = warmStart5mCandles;
    currentSettings.warmStart30mCandles = warmStart30mCandles;
    currentSettings.warmStart2hCandles = warmStart2hCandles;
    warmWrap.bindSettings(&currentSettings);
    warmWrap.bindLogger(&Serial);
    
    // Fase 7.2b: Warm-start exclusiviteit
    // Warm-start is de enige schrijver tijdens setup (tasks bestaan nog niet, geen race conditions mogelijk)
    // Warm-start: Vul buffers met Binance historische data (als WiFi verbonden is)
    if (WiFi.status() == WL_CONNECTED && warmStartEnabled) {
        warmWrap.beginRun();
        
        // Bereken requested counts (voor logging)
        bool psramAvailable = hasPSRAM();
        uint16_t req1mCandles = calculate1mCandles();
        uint16_t max5m = psramAvailable ? 24 : 12;
        uint16_t max30m = psramAvailable ? 12 : 6;
        uint16_t max2h = psramAvailable ? 8 : 4;
        uint16_t req5mCandles = clampUint16(warmStart5mCandles, 2, max5m);
        uint16_t req30mCandles = clampUint16(warmStart30mCandles, 2, max30m);
        uint16_t req2hCandles = clampUint16(warmStart2hCandles, 2, max2h);
        
        WarmStartMode mode = performWarmStart();
        warmWrap.endRun(mode, warmStartStats, warmStartStatus, 
                       ret_2h, ret_30m, hasRet2h, hasRet30m,
                       req1mCandles, req5mCandles, req30mCandles, req2hCandles);
    }
    
    Serial_println("Setup done");
    fetchPrice();
    
    // Build main UI (verwijdert WiFi UI en bouwt hoofd UI)
    // Fase 8.4.3: Gebruik module versie
    uiController.buildUI();
    
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
    
    // Fase 7.2b: Minimale guard: controleer arrays vóór tasks starten
    // Warm-start is de enige schrijver tijdens setup (tasks bestaan nog niet)
    // Deze check voorkomt dat tasks starten met ongeldige arrays
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    if (!hasPSRAM()) {
        if (fiveMinutePrices == nullptr || fiveMinutePricesSource == nullptr ||
            minuteAverages == nullptr || minuteAveragesSource == nullptr) {
            Serial.println(F("[FATAL] Arrays niet gealloceerd vóór task start!"));
            Serial.printf("[Memory] Free heap: %u bytes\n", ESP.getFreeHeap());
            while (true) {
                delay(1000);
                yield();
                Serial.println(F("[FATAL] Waiting for reset..."));
            }
        }
    }
    #endif
    
    // Start FreeRTOS tasks NA buildUI() en NA warm-start
    // Warm-start heeft exclusieve toegang tijdens setup (geen race conditions mogelijk)
    startFreeRTOSTasks();
}

// Toon verbindingsinfo (SSID en IP-adres) en "Opening Bitvavo Session" op het scherm
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
    
    // Maak spinner voor "Opening Bitvavo Session" (8px naar beneden vanaf midden)
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
    
    // "Opening Bitvavo Session" label (onder de spinner)
    lv_obj_t *binanceLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(binanceLabel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(binanceLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(binanceLabel, 150);
    lv_label_set_long_mode(binanceLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(binanceLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(binanceLabel, "Opening Bitvavo\nSession");
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
        // Toon verbindingsinfo (SSID en IP) en "Opening Bitvavo Session"
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
    // Fase 9.1.2: Gebruik module versie
    webServerModule.setupWebServer();
    
    // Scherm wordt leeggemaakt door buildUI() in setup()
}
// ============================================================================
// FreeRTOS Tasks
// ============================================================================

// FreeRTOS Task: API calls op Core 1 (elke 1.3 seconde)
void apiTask(void *parameter)
{
    const uint32_t apiIntervalMs = UPDATE_API_INTERVAL;
    
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
    
    // M1: Rate-limited heap telemetry (elke 60s)
    static unsigned long lastHeapLog = 0;
    const unsigned long HEAP_LOG_INTERVAL_MS = 60000;  // 60 seconden
    
    for (;;)
    {
        uint32_t t0 = millis();
        
        // M1: Rate-limited heap telemetry in apiTask (elke 60s)
        if ((t0 - lastHeapLog) >= HEAP_LOG_INTERVAL_MS) {
            logHeap("API_TASK_LOOP");
            lastHeapLog = t0;
        }
        
        // Controleer WiFi status voordat we een request doet
        if (WiFi.status() == WL_CONNECTED) {
            // Voer 1 API call uit
            fetchPrice();
            updateLatestKlineMetricsIfNeeded();
            
            // C1: Verwerk pending anchor setting (network-safe: gebeurt in apiTask waar HTTPS calls al zijn)
            if (pendingAnchorSetting.pending) {
                // Thread-safe: kopieer waarde lokaal om race conditions te voorkomen
                float anchorValueToSet = pendingAnchorSetting.value;
                bool useCurrentPrice = pendingAnchorSetting.useCurrentPrice;
                // Reset flag direct om dubbele verwerking te voorkomen
                pendingAnchorSetting.pending = false;
                
                // Bepaal waarde: gebruik zojuist opgehaalde prijs als useCurrentPrice, anders opgegeven waarde
                float valueToSet = useCurrentPrice ? 0.0f : anchorValueToSet;
                
                // C1: Gebruik AnchorSystem module om anchor in te stellen (skipNotifications=false om notificatie te versturen)
                // Notificatie wordt BUITEN mutex verstuurd, dus geen blocking probleem
                if (anchorSystem.setAnchorPrice(valueToSet, false, false)) {
                    // C1: Persist settings na succesvolle anchor set
                    saveSettings();
                    Serial_printf(F("[API Task] Anchor updated via pending request: %.2f\n"), 
                                 useCurrentPrice ? prices[0] : anchorValueToSet);
                } else {
                    #if !DEBUG_BUTTON_ONLY
                    Serial_println(F("[API Task] WARN: Kon anchor niet instellen (pending request) - mutex timeout of geen prijs beschikbaar"));
                    #endif
                }
            }
        } else {
            Serial.println("[API Task] WiFi verbinding verloren, wachten op reconnect...");
            // Wacht tot WiFi weer verbonden is
            while (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wacht 1 seconde
            }
            Serial.println("[API Task] WiFi weer verbonden");
        }
        
        // Duration-aware timing: wacht (interval - callDuration)
        uint32_t dur = millis() - t0;
        uint32_t waitMs = (apiIntervalMs > dur) ? (apiIntervalMs - dur) : 0;
        
        // WARN alleen bij echte overload (dur > apiIntervalMs)
        #if !DEBUG_BUTTON_ONLY
        if (dur > apiIntervalMs) {
            Serial.printf("[API Task] WARN: Call duurde %lu ms (langer dan interval %lu ms)\n", 
                         dur, apiIntervalMs);
        }
        #endif
        
        // Wacht tot volgende interval
        vTaskDelay(pdMS_TO_TICKS(waitMs));
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
            // Fase 4.2: Geconsolideerde mutex timeout counter reset
            resetMutexTimeoutCounter(uiMutexTimeoutCount);
            
            // Fase 8.8.1: Gebruik module versie (parallel - oude functie blijft bestaan)
            uiController.updateUI();
            safeMutexGive(dataMutex, "uiTask updateUI");
        }
        else
        {
            // Fase 4.1: Geconsolideerde mutex timeout handling (UI task: elke 20e, reset bij 100)
            handleMutexTimeout(uiMutexTimeoutCount, "UI Task", nullptr, 20, 100);
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
        
        // C1: Anchor verwerking verplaatst naar apiTask (network-safe: gebeurt waar HTTPS calls al zijn)
        
        // Check physical button (alleen voor TTGO)
        #if HAS_PHYSICAL_BUTTON
        // Fase 8.9.1: Gebruik module versie (parallel - oude functie blijft bestaan)
        uiController.checkButton();
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
        // Fase 9.1.2: Gebruik module versie
        if (WiFi.status() == WL_CONNECTED) {
            webServerModule.handleClient();
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
