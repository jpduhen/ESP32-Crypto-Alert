// Tutorial : https://youtu.be/JqQEG0eipic
// Unified Crypto Monitor — ESP32-S3 platforms (o.a. GEEK, Super Mini, LCDWIKI 2.8, JC3248W535, AMOLED)
// Select platform in platform_config.h

#define LV_CONF_INCLUDE_SIMPLE // Use the lv_conf.h included in this project, to configure see https://docs.lvgl.io/master/get-started/platforms/arduino.html

// Platform config moet als eerste, definieert platform-specifieke instellingen
// MODULE_INCLUDE is NIET gedefinieerd, zodat PINS files worden geïncludeerd
#include "platform_config.h"

#include <WiFi.h>                   // Included with Espressif ESP32 Dev Module
#include <WiFiClientSecure.h>       // TLS client for HTTPS (NTFY)
#include <HTTPClient.h>             // Included with Espressif ESP32 Dev Module
#include <WiFiManager.h>            // Install "WiFiManager" with the Library Manager
#include <WebServer.h>              // Included with Espressif ESP32 Dev Module
#include <Preferences.h>            // Included with Espressif ESP32 Dev Module
#include <PubSubClient.h>           // Install "PubSubClient3" from https://github.com/hmueller01/pubsubclient3
#include "atomic.h"                 // Included in this project
#include <lvgl.h>                   // Install "lvgl" with the Library Manager (last tested on v9.2.2)
#include "Arduino.h"
#include <cstring>

// Lokale secrets (optioneel, niet in git opnemen). Gebruikt om NTFY access token veilig in te laden.
// Verwacht in secrets_local.h bijvoorbeeld:
//   #define NTFY_ACCESS_TOKEN "tk_..."
#if defined(__has_include)
    #if __has_include("secrets_local.h")
        #include "secrets_local.h"
    #endif
#endif

// Default: geen Bearer auth (gedraagt zich identiek aan de huidige setup).
#ifndef NTFY_ACCESS_TOKEN
    #define NTFY_ACCESS_TOKEN ""
#endif

#if WS_ENABLED
    #include <WebSocketsClient.h>
    #define WS_LIB_AVAILABLE 1
#endif

// Touchscreen functionaliteit volledig verwijderd - gebruik nu fysieke boot knop (GPIO 0)
#include <SPI.h>
#include <time.h>                   // For time functions
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#if __has_include(<esp_cache.h>)
#include <esp_cache.h>
#define CRYPTO_ALERT_LVGL_HAS_ESP_CACHE 1
#endif

// Display backend abstraction (Arduino_GFX vs esp_lcd for JC3248W535).
#include "src/display/DisplayBackend.h"
#include "src/display/DisplayBackend_Factory.h"
#include "src/display/DisplayBackend_ArduinoGFX.h"

DisplayBackend *g_displayBackend = nullptr;

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
#include "src/RegimeEngine/RegimeEngine.h"

// Memory module (M1: heap telemetry voor geheugenfragmentatie audit)
#include "src/Memory/HeapMon.h"

// Forward declaration needed for Arduino auto-prototypes.
// (Arduino preprocessor may generate function prototypes before the struct body below.)
struct NtfyPendingItem;

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

#if WS_ENABLED && !WS_LIB_AVAILABLE
    #warning "WS_ENABLED is 1 maar WebSocketsClient.h ontbreekt. Installeer de WebSockets library of zet WS_ENABLED op 0."
#endif

// --- Debug Configuration ---
// DEBUG_BUTTON_ONLY en DEBUG_CALCULATIONS worden gedefinieerd in platform_config.h
// Hier alleen de macro's voor Serial output

#if DEBUG_BUTTON_ONLY
    // Disable all Serial output except button actions
    #define Serial_printf(...) ((void)0)
    #define Serial_println(...) ((void)0)
    #define Serial_print(...) ((void)0)
    
    // DEBUG_CALCULATIONS logging werkt altijd, ongeacht DEBUG_BUTTON_ONLY
    // WAARSCHUWING: DEBUG_CALCULATIONS gebruikt DRAM voor debug strings
    // Bij gebrek aan PSRAM is uitgebreide debug meestal uit (DRAM); zie platform_config.h
    #if DEBUG_CALCULATIONS
        #undef Serial_printf
        #undef Serial_println
        #undef Serial_print
        // Gebruik Serial.printf_P() voor Flash strings (PROGMEM) - bespaart DRAM
        // Maar Serial.printf_P() heeft beperkte ondersteuning, dus gebruik Serial.printf() met F()
        // Let op: F() strings worden nog steeds in DRAM opgeslagen bij Serial.printf()
        #define Serial_printf(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
        #define Serial_println(...) Serial.println(__VA_ARGS__)
        #define Serial_print(...) Serial.print(__VA_ARGS__)
    #endif
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
#define BITVAVO_API_BASE "https://api.bitvavo.com/v2"  // Bitvavo API base URL
#define BITVAVO_SYMBOL_DEFAULT "BTC-EUR"  // Default Bitvavo symbol (format: BASE-QUOTE met streepje)
// T1: Verhoogde connect/read timeouts voor betere stabiliteit
#define HTTP_CONNECT_TIMEOUT_MS 2000  // Connect timeout (2000ms - geoptimaliseerd)
#define HTTP_READ_TIMEOUT_MS 2500     // Read timeout (2500ms - geoptimaliseerd voor kleine responses)
#define HTTP_TIMEOUT_MS HTTP_READ_TIMEOUT_MS  // Backward compatibility: totale timeout = read timeout

// --- Chart Configuration ---
#define PRICE_RANGE 200         // The range of price for the chart, adjust as needed
#define POINTS_TO_CHART 60      // Number of points on the chart (60 points = 4 minutes at 4000ms API interval)

// --- Timing Configuration ---
#define UPDATE_UI_INTERVAL 1000   // UI update in ms (elke seconde)
#define UPDATE_API_INTERVAL 4000   // API poll in ms (afgestemd op typische HTTPS-duur; connect/read timeouts ongewijzigd)
// 1 Hz sampler voor korte horizons (secondPrices / fiveMinutePrices); los van API-poll
#define PRICE_SAMPLE_INTERVAL_MS 1000
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
#define NIGHT_MODE_ENABLED_DEFAULT false  // Profiel 5F: nachtstand uit (S3-GEEK-5-basis)
#define NIGHT_MODE_START_HOUR_DEFAULT 23
#define NIGHT_MODE_END_HOUR_DEFAULT 7
#define NIGHT_MODE_SPIKE5M_THRESHOLD_DEFAULT 0.60f
#define NIGHT_MODE_MOVE5M_ALERT_THRESHOLD_DEFAULT 0.55f
#define NIGHT_MODE_MOVE30M_THRESHOLD_DEFAULT 0.45f
#define NIGHT_MODE_COOLDOWN_5M_SEC_DEFAULT 900
#define NIGHT_MODE_AUTO_VOL_MIN_MULTIPLIER_DEFAULT 0.90f
#define NIGHT_MODE_AUTO_VOL_MAX_MULTIPLIER_DEFAULT 1.80f
#define CONFLUENCE_TIME_WINDOW_MS 300000UL     // 5 minuten tijdshorizon voor confluence (1m en 5m events moeten binnen ±5 minuten liggen)

// --- Warm-Start Configuration ---
#define WARM_START_ENABLED_DEFAULT true  // Default: warm-start met Binance historische data aan
#define WARM_START_1M_EXTRA_CANDLES_DEFAULT 15  // Extra 1m candles bovenop volatility window
#define WARM_START_5M_CANDLES_DEFAULT 12  // Aantal 5m candles (default: 12 = 1 uur)
#define WARM_START_30M_CANDLES_DEFAULT 8  // Aantal 30m candles (default: 8 = 4 uur)
#define WARM_START_2H_CANDLES_DEFAULT 6  // Aantal 2h candles (default: 6 = 12 uur)
// Optimalisatie: skip warm-start voor 1m/5m als live data snel genoeg is
#define WARM_START_SKIP_1M_DEFAULT false
#define WARM_START_SKIP_5M_DEFAULT false
// Bitvavo candlestick endpoint: /{market}/candles (wordt dynamisch gebouwd)
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

// --- NTFY: productie vs. diagnostiek (Fase 1 tracker) ---
// Productie-alertdelivery: altijd via queue + exclusive flow; niet afhankelijk van onderstaande vlaggen.
//
// CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME — hoofdschakelaar voor optionele test-/diagnose-runtime (standaard UIT).
// Alleen als deze 1 is, kunnen startup/periodic/deferred/WS-health paden actief worden (per sub-vlag hieronder).
#ifndef CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME
#define CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME 0
#endif

// Sub-vlaggen: alleen effect als CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME==1 (handmatige test onder belasting).
#ifndef CRYPTO_ALERT_NTFY_STARTUP_TEST
#define CRYPTO_ALERT_NTFY_STARTUP_TEST 1
#endif

#ifndef CRYPTO_ALERT_NTFY_PERIODIC_TEST
#define CRYPTO_ALERT_NTFY_PERIODIC_TEST 0
#endif

#ifndef CRYPTO_ALERT_NTFY_PERIODIC_TEST_MS
#define CRYPTO_ALERT_NTFY_PERIODIC_TEST_MS 30000UL
#endif

// Spike/Move alert thresholds — centrale code-basis profiel 5F (S3-GEEK-5 + 5F-tuning)
#define SPIKE_1M_THRESHOLD_DEFAULT 0.16f
#define SPIKE_5M_THRESHOLD_DEFAULT 0.36f
#define MOVE_30M_THRESHOLD_DEFAULT 0.80f
#define MOVE_5M_THRESHOLD_DEFAULT 0.36f
#define MOVE_5M_ALERT_THRESHOLD_DEFAULT 0.50f

// Cooldown tijden (ms) — profiel 5F
#define NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT 90000UL
#define NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT 90000UL
#define NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT 150000UL

// Max alerts per uur
#define MAX_1M_ALERTS_PER_HOUR 3
#define MAX_30M_ALERTS_PER_HOUR 2
#define MAX_5M_ALERTS_PER_HOUR 3

// --- MQTT Configuration ---
#define MQTT_HOST_DEFAULT "192.168.68.3"  // Standaard MQTT broker IP (pas aan naar jouw MQTT broker)
#define MQTT_PORT_DEFAULT 1883             // Standaard MQTT poort
#define MQTT_USER_DEFAULT "mosquitto"       // Standaard MQTT gebruiker (pas aan)
#define MQTT_PASS_DEFAULT "mqtt_password"  // Standaard MQTT wachtwoord (pas aan)
#define MQTT_VALUES_PUBLISH_INTERVAL_MS 30000UL
#define MQTT_IP_PUBLISH_INTERVAL_MS 600000UL

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
// Aantal waarden nodig voor return berekeningen gebaseerd op UPDATE_API_INTERVAL (4000ms)
// 1 minuut = 60000ms / 4000ms = 15 waarden
// 5 minuten = 300000ms / 4000ms = 75 waarden
#define VALUES_FOR_1MIN_RETURN ((60000UL) / (UPDATE_API_INTERVAL))
#define VALUES_FOR_5MIN_RETURN ((300000UL) / (UPDATE_API_INTERVAL))

// --- CPU Measurement Configuration ---
#define CPU_MEASUREMENT_SAMPLES 20  // Meet over 20 loops voor gemiddelde


// ============================================================================
// Global Variables
// ============================================================================

// Touchscreen: niet gebruikt; fysieke boot-knop (GPIO 0) — zie ook comment bij includes.

// LVGL Display global variables
// Fase 8: Display state - gebruikt door UIController module
lv_display_t *disp;
lv_color_t *disp_draw_buf = nullptr;  // Draw buffer pointer (één keer gealloceerd bij init)
size_t disp_draw_buf_size = 0;  // Buffer grootte in bytes (voor logging)

// Widgets LVGL global variables
// Fase 8: UI object pointers - gebruikt door UIController module (zie src/UIController/UIController.h)
lv_obj_t *chart;
lv_chart_series_t *dataSeries;     // Blauwe serie voor alle punten
lv_obj_t *lblFooterLine1; // Footer regel 1 (2-regel footer: RSSI links op regel 1)
lv_obj_t *lblFooterLine2; // Footer regel 2 (IP op regel 2; versie via chartVersionLabel)
lv_obj_t *ramLabel; // RAM rechts op regel 1 (2-regel footer-layout)

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

// Symbols array - eerste element wordt dynamisch ingesteld via bitvavoSymbol
// Fase 8: UI data - gebruikt door UIController module
#if defined(PLATFORM_ESP32S3_JC3248W535)
char symbol0[16] = "BTC-EUR";
extern const char *const symbols[SYMBOL_COUNT] = {symbol0, SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL, SYMBOL_2H_LABEL, SYMBOL_5M_LABEL, SYMBOL_1D_LABEL, SYMBOL_7D_LABEL};
#elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
char symbol0[16] = "BTC-EUR";
extern const char *const symbols[SYMBOL_COUNT] = {symbol0, SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL, SYMBOL_2H_LABEL};
#else
char symbol0[16] = "BTC-EUR";
extern const char *const symbols[SYMBOL_COUNT] = {symbol0, SYMBOL_1MIN_LABEL, SYMBOL_30MIN_LABEL};
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
// Langere returns: semantiek is verspreid over warm-start en live buffers.
// ret_2h: trend-% (2h-candles of minuteAverages); niet hetzelfde als EUR avg/high/low/range in computeTwoHMetrics.
// ret_1d: bij warm-start kan het eindigen op het 1h/24h-regressiepad (na een eerste 1d-candlepad).
// ret_7d: bij warm-start kan de waarde van 1W-regressie naar 7× daily gaan als dat beschikbaar is.
float ret_2h = 0.0f;  // 2-hour return percentage
float ret_30m = 0.0f;  // 30-minute return percentage (calculated from minuteAverages or warm-start data)
float ret_4h = 0.0f;  // 4-hour return percentage (calculated from API during warm-start)
float ret_1d = 0.0f;  // 1-day return percentage (warm-start en/of live 24h-buffer)
float ret_7d = 0.0f;  // 7-day return percentage (warm-start en/of live hourly buffer)
// Fase 8: UI state - gebruikt door UIController module
bool hasRet2hWarm = false;  // Flag: ret_2h beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet30mWarm = false;  // Flag: ret_30m beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet4hWarm = false;  // Flag: ret_4h beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet1dWarm = false;  // Flag: ret_1d beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet7dWarm = false;  // Flag: ret_7d beschikbaar vanuit warm-start (minimaal 2 candles)
bool hasRet2hLive = false;  // Flag: ret_2h kan worden berekend uit live data (minuteIndex >= 120)
bool hasRet30mLive = false;  // Flag: ret_30m kan worden berekend uit live data (minuteIndex >= 30)
uint8_t livePct5m = 0;  // % SOURCE_LIVE in actief 5m-secondenvenster (fiveMinutePrices)
bool hasRet5mLive = false;  // true als 5m-ring gevuld en ≥80% van samples SOURCE_LIVE
bool hasRet4hLive = false;  // Flag: ret_4h kan worden berekend uit live data (hourly buffer >= 4)
bool hasRet7dLive = false;  // Flag: 7d-ret uit live uurbuffer (availableHours >= HOURS_FOR_7D); UI JC3248 grafiekbadge
// Combined flags: beschikbaar vanuit warm-start OF live data
bool hasRet2h = false;  // hasRet2hWarm || hasRet2hLive
bool hasRet30m = false;  // hasRet30mWarm || hasRet30mLive
bool hasRet4h = false;  // hasRet4hWarm || hasRet4hLive
bool hasRet1d = false;  // hasRet1dWarm || live 24h-ret wanneer buffer genoeg uren heeft (≥24)
bool hasRet7d = false;  // hasRet7dWarm (warm-start) of live hourly buffer
// Warm-start 1d stats voor vroege UI weergave (tot hourly buffer gevuld is)
bool warmStart1dValid = false;
float warmStart1dMin = 0.0f;
float warmStart1dMax = 0.0f;
float warmStart1dAvg = 0.0f;
bool warmStart2hValid = false;
float warmStart2hMin = 0.0f;
float warmStart2hMax = 0.0f;
float warmStart2hAvg = 0.0f;
bool warmStart7dValid = false;
float warmStart7dMin = 0.0f;
float warmStart7dMax = 0.0f;
float warmStart7dAvg = 0.0f;

// Warm-start buffers voor 7d (voorkomt grote stack-allocaties)
static float warmStartTemp1h7dPrices[168];
static unsigned long warmStartTemp1h7dTimes[168];
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

// Regime-engine (Fase A: snapshot; instellingen gesynchroniseerd in loadSettings/saveSettings)
bool regimeEngineEnabled = true;
uint32_t regimeMinDwellSec = 180u;
float regimeEnergeticEnter = 0.95f;
float regimeEnergeticExit = 0.78f;
float regimeSlapEnter = 0.38f;
float regimeSlapExit = 0.52f;
float regimeLoadedFloor = 0.45f;
float regimeLoadedDrop = 0.35f;
float regimeDirDeadband1mPct = 0.05f;
float regimeDirDeadband5mPct = 0.10f;
float regimeDirDeadband30mPct = 0.15f;
float regimeDirDeadband2hPct = 0.25f;
float regime2hCompressMinPct = 0.35f;
float regime2hCompressMaxPct = 1.10f;

// Regime Fase B: alert threshold/cooldown multipliers (AlertEngine)
float regimeSlapSpike1mMult = 1.18f;
float regimeSlapMove5mAlertMult = 1.10f;
float regimeSlapMove30mMult = 1.08f;
float regimeSlapCooldown1mMult = 1.50f;
float regimeSlapCooldown5mMult = 1.20f;
float regimeSlapCooldown30mMult = 1.10f;
float regimeGeladenSpike1mMult = 0.98f;
float regimeGeladenMove5mAlertMult = 0.95f;
float regimeGeladenMove30mMult = 1.00f;
float regimeGeladenCooldown1mMult = 1.00f;
float regimeGeladenCooldown5mMult = 0.95f;
float regimeGeladenCooldown30mMult = 1.00f;
float regimeEnergiekSpike1mMult = 0.85f;
float regimeEnergiekMove5mAlertMult = 0.88f;
float regimeEnergiekMove30mMult = 1.05f;
float regimeEnergiekCooldown1mMult = 0.70f;
float regimeEnergiekCooldown5mMult = 0.80f;
float regimeEnergiekCooldown30mMult = 1.20f;
bool regimeEnergiekAllowStandalone1mBurst = true;
float regimeEnergiekStandalone1mFactor = 1.20f;
float regimeEnergiekMinDirectionStrength = 0.60f;

unsigned long lastTrendChangeNotification = 0;  // Timestamp van laatste trend change notificatie (backward compatibility)

// Smart Confluence Mode state
// Fase 6.1: AlertEngine module gebruikt deze variabele (extern declaration in AlertEngine.cpp)
bool smartConfluenceEnabled = SMART_CONFLUENCE_ENABLED_DEFAULT;
bool nightModeEnabled = NIGHT_MODE_ENABLED_DEFAULT;
uint8_t nightModeStartHour = NIGHT_MODE_START_HOUR_DEFAULT;
uint8_t nightModeEndHour = NIGHT_MODE_END_HOUR_DEFAULT;
float nightSpike5mThreshold = NIGHT_MODE_SPIKE5M_THRESHOLD_DEFAULT;
float nightMove5mAlertThreshold = NIGHT_MODE_MOVE5M_ALERT_THRESHOLD_DEFAULT;
float nightMove30mThreshold = NIGHT_MODE_MOVE30M_THRESHOLD_DEFAULT;
uint16_t nightCooldown5mSec = NIGHT_MODE_COOLDOWN_5M_SEC_DEFAULT;
float nightAutoVolMinMultiplier = NIGHT_MODE_AUTO_VOL_MIN_MULTIPLIER_DEFAULT;
float nightAutoVolMaxMultiplier = NIGHT_MODE_AUTO_VOL_MAX_MULTIPLIER_DEFAULT;
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

// WebSocket (stap-voor-stap migratie)
static bool wsInitialized = false;
bool wsConnected = false;
bool wsConnecting = false;
unsigned long wsConnectStartMs = 0;
unsigned long wsConnectedMs = 0;
static uint32_t wsMsgCount = 0;
static unsigned long wsLastLogMs = 0;
static float wsLastPrice = 0.0f;
static unsigned long wsLastPriceMs = 0;
static float wsLastBid = 0.0f;
static float wsLastAsk = 0.0f;
static float wsLastSpread = 0.0f;
static unsigned long wsLastBidAskMs = 0;
struct WsSecondAggregateState {
    float secondOpen = 0.0f;
    float secondHigh = 0.0f;
    float secondLow = 0.0f;
    float secondClose = 0.0f;
    uint32_t secondTickCount = 0;
    float secondSpreadLast = 0.0f;
    float secondSpreadMax = 0.0f;
    uint32_t secondBucket = 0;
    bool valid = false;
};
static WsSecondAggregateState wsSecondAggCurrent;
static WsSecondAggregateState wsSecondAggLastClosed;
bool getWsSecondLastClosedQuality(uint32_t& tickCount, float& spreadMax, bool& valid, bool& fresh) {
    valid = wsSecondAggLastClosed.valid;
    fresh = false;
    if (!valid) {
        tickCount = 0;
        spreadMax = 0.0f;
        return false;
    }
    tickCount = wsSecondAggLastClosed.secondTickCount;
    spreadMax = wsSecondAggLastClosed.secondSpreadMax;
    const uint32_t nowBucket = (uint32_t)(millis() / 1000UL);
    const uint32_t ageBuckets = (nowBucket >= wsSecondAggLastClosed.secondBucket)
        ? (nowBucket - wsSecondAggLastClosed.secondBucket)
        : (UINT32_MAX - wsSecondAggLastClosed.secondBucket + nowBucket + 1U);
    fresh = (ageBuckets <= 1U);
    return true;
}

// Live anchor-check huidige prijs bron voor responsiviteit: laatst afgesloten 1s close
// Alleen geldig als snapshot zowel valid als fresh is.
bool getWsSecondLastClosedCloseFresh(float& close, bool& ok) {
    close = 0.0f;
    ok = false;
    if (!wsSecondAggLastClosed.valid) return false;

    const uint32_t nowBucket = (uint32_t)(millis() / 1000UL);
    const uint32_t ageBuckets = (nowBucket >= wsSecondAggLastClosed.secondBucket)
        ? (nowBucket - wsSecondAggLastClosed.secondBucket)
        : (UINT32_MAX - wsSecondAggLastClosed.secondBucket + nowBucket + 1U);
    if (ageBuckets > 1U) return false;

    if (wsSecondAggLastClosed.secondClose > 0.0f) {
        close = wsSecondAggLastClosed.secondClose;
        ok = true;
        return true;
    }
    return false;
}
static unsigned long wsLastCandle1mMs = 0;
static unsigned long wsLastCandle5mMs = 0;
static unsigned long wsLastCandle4hMs = 0;
static unsigned long wsLastCandle1dMs = 0;

struct WsCandleState {
    unsigned long openTime = 0;
    float lastClose = 0.0f;
    bool has = false;
};
static WsCandleState wsCandle4h;
static WsCandleState wsCandle1d;
static EmaAccumulator wsEma4h;
static EmaAccumulator wsEma1d;
static uint8_t wsEma4hN = 0;
static uint8_t wsEma1dN = 0;
static bool wsEma4hInit = false;
static bool wsEma1dInit = false;
float wsAnchorEma4hLive = 0.0f;
float wsAnchorEma1dLive = 0.0f;
bool wsAnchorEma4hValid = false;
bool wsAnchorEma1dValid = false;
static bool wsAutoAnchorTrigger = false;
static unsigned long wsLastCandleLogMs = 0;
#if WS_ENABLED && WS_LIB_AVAILABLE
static WebSocketsClient* wsClientPtr = nullptr;
#endif
static bool wsPending = false;
static size_t wsPendingLen = 0;
static char wsPendingBuf[360];

// Exclusive NTFY + loop(): tijdelijk geen ws.loop() wanneer apiTask WS stopt voor HTTPS-send.
static volatile bool wsPauseForNtfySend = false;

// NTFY extra health ping: eenmalig pas na de eerste echte WS live message (+ delay)
static bool wsHasSeenFirstLiveMessage = false;
static unsigned long wsLiveSinceMs = 0;
static bool wsLiveNtfyTestSent = true;
static const unsigned long WS_LIVE_NTFY_HEALTH_PING_DELAY_MS = 15000UL;

static const char* WS_HOST = "ws.bitvavo.com";
static const uint16_t WS_PORT = 443;
static const char* WS_PATH = "/v2/";

// NTFY exclusive network mode (alleen apiTask wisselt modus; loop() respecteert vlag)
enum NetExclusiveNtfyMode : uint8_t {
    NET_MODE_NORMAL = 0,
    NET_MODE_NTFY_EXCLUSIVE_STOPPING_WS = 1,
    NET_MODE_NTFY_EXCLUSIVE_SENDING = 2,
    NET_MODE_NTFY_EXCLUSIVE_RESTARTING_WS = 3,
};
volatile uint8_t g_netExclusiveNtfyMode = NET_MODE_NORMAL;
static unsigned long s_netExclusiveDeadlineMs = 0;
static const unsigned long NTFY_EXCL_WS_STOP_MS = 8000UL;
static const unsigned long NTFY_EXCL_SEND_MS = 30000UL;
static const unsigned long NTFY_EXCL_WS_RESTART_MS = 20000UL;
static const unsigned long NTFY_EXCL_WS_RESTART_WAIT_LOG_MS = 12000UL;
// Aantal timeout-vensters waarbij we disconnect + beginSSL herhalen voordat we exclusive verlaten (eerste poging telt niet mee als "retry"-log)
static const uint8_t NTFY_EXCL_WS_RESTART_MAX_RETRIES = 3;
// Meerdere ws.loop()-calls per apiTask-cyclus: TLS/SSL vordert sneller dan één pump per interval (loop() is tijdens exclusive uit).
static const uint8_t NTFY_EXCL_WS_RESTART_PUMP_BURST = 40;
volatile bool g_wsSubscribeSentAfterConnect = false;

static void processWsTextMessage(const char* wsBuf, size_t length);


// Fase 8: UI state - gebruikt door UIController module
uint8_t symbolIndexToChart = 0; // The symbol index to chart
uint32_t maxRange;
uint32_t minRange;
// chartMaxLabel verwijderd - niet meer nodig

// Fase 8: UI object pointers - gebruikt door UIController module (zie src/UIController/UIController.h)
lv_obj_t *chartTitle;     // Label voor chart titel / device-id (platforms met deze header)
lv_obj_t *chartVersionLabel; // Label voor versienummer (rechts bovenste regel)
lv_obj_t *chartDateLabel; // Label voor datum rechtsboven (vanaf pixel 180)
lv_obj_t *chartTimeLabel; // Label voor tijd rechtsboven
lv_obj_t *chartBeginLettersLabel; // Label voor beginletters (compacte layouts, o.a. GEEK)
lv_obj_t *ipLabel; // IP-adres label (footer, platform-afhankelijk)
lv_obj_t *price1MinMaxLabel; // Label voor max waarde in 1 min buffer
lv_obj_t *price1MinMinLabel; // Label voor min waarde in 1 min buffer
lv_obj_t *price1MinDiffLabel; // Label voor verschil tussen max en min in 1 min buffer
lv_obj_t *price30MinMaxLabel; // Label voor max waarde in 30 min buffer
lv_obj_t *price30MinMinLabel; // Label voor min waarde in 30 min buffer
lv_obj_t *price30MinDiffLabel; // Label voor verschil tussen max en min in 30 min buffer
lv_obj_t *price2HMaxLabel = nullptr; // 2h min/max/diff (LCDWIKI_28 / JC3248W535; nullptr op 3-symbol boards)
lv_obj_t *price2HMinLabel = nullptr;
lv_obj_t *price2HDiffLabel = nullptr;
#if defined(PLATFORM_ESP32S3_JC3248W535)
lv_obj_t *price5mMaxLabel = nullptr; // 5m min/max/diff (alleen JC3248 5-kaart UI)
lv_obj_t *price5mMinLabel = nullptr;
lv_obj_t *price5mDiffLabel = nullptr;
lv_obj_t *price1dMaxLabel = nullptr; // 1d min/max/diff (JC3248, index 5)
lv_obj_t *price1dMinLabel = nullptr;
lv_obj_t *price1dDiffLabel = nullptr;
lv_obj_t *price7dMaxLabel = nullptr; // 7d min/max/diff (JC3248, data-index 6)
lv_obj_t *price7dMinLabel = nullptr;
lv_obj_t *price7dDiffLabel = nullptr;
#endif
lv_obj_t *anchorLabel; // Label voor anchor price info (rechts midden, met percentage verschil)
lv_obj_t *anchorMaxLabel; // Label voor "Pak winst" (rechts, groen, boven)
lv_obj_t *anchorMinLabel; // Label voor "Stop loss" (rechts, rood, onder)
static lv_obj_t *anchorDeltaLabel; // Label voor anchor delta % (compacte layouts, rechts)
lv_obj_t *trendLabel; // Label voor trend weergave
lv_obj_t *warmStartStatusLabel; // Label voor warm-start status weergave (rechts bovenin chart)
lv_obj_t *volatilityLabel; // Label voor volatiliteit weergave
lv_obj_t *mediumTrendLabel; // Label voor medium trend weergave (4h + 1d)
lv_obj_t *longTermTrendLabel; // Label voor lange termijn trend weergave (7d)

// Fase 8: UI state - gebruikt door UIController module
uint32_t lastApiMs = 0; // Time of last api call

// latestKnownPrice + 1 Hz sampler vullen secondPrices/fiveMinutePrices; API-poll blijft onafhankelijk
float lastFetchedPrice = 0.0f; // Laatste succesvol opgehaalde prijs (UI + samplerbron-sync)
// Optie B: gedeelde staat — alleen bijgewerkt door REST/WS; 1 Hz sampler schrijft buffers
float latestKnownPrice = 0.0f;
unsigned long latestKnownPriceMs = 0;
#define LKP_SRC_NONE 0
#define LKP_SRC_REST 1
#define LKP_SRC_WS   2
uint8_t latestKnownPriceSource = LKP_SRC_NONE;
unsigned long lastPriceRepeatMs = 0; // Timestamp van laatste prijs herhaling

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
static char httpResponseBuffer[240];  // Buffer voor HTTP responses (NTFY, etc.) - verkleind naar 240 bytes (bespaart 8 bytes DRAM)

// M2: Globale herbruikbare buffer voor HTTP responses (voorkomt String allocaties)
// Note: Niet static zodat ApiClient.cpp er toegang toe heeft via extern declaratie in ApiClient.h
// Verkleind van 2048 naar 512 bytes (genoeg voor price responses, ~100 bytes)
char gApiResp[296];  // Verkleind naar 296 bytes (bespaart 8 bytes DRAM) // Buffer voor API price responses (M2: streaming)
// gKlinesResp verwijderd: fetchBitvavoCandles gebruikt streaming parsing met bitvavoStreamBuffer

// Streaming buffer voor Bitvavo candlestick parsing (heap-first, fallback naar kleine static buffer)
#define BITVAVO_STREAM_BUFFER_HEAP_SIZE 512
static char bitvavoStreamBufferFallback[128];
static char* bitvavoStreamBuffer = bitvavoStreamBufferFallback;
static size_t bitvavoStreamBufferSize = sizeof(bitvavoStreamBufferFallback);

// LVGL UI buffers en cache (voorkomt herhaalde allocaties en onnodige updates)
// Fase 8.6.1: static verwijderd zodat UIController module deze kan gebruiken
char priceLblBuffer[18];  // Buffer voor price label (%.2f format, max: "12345.67" = ~8 chars)
char anchorMaxLabelBuffer[28];  // o.a. "+%.2f%% %.2f"; sync met ANCHOR_MAX_LABEL_BUFFER_SIZE in UIController.cpp
char anchorLabelBuffer[32];  // o.a. "%c%.2f%% %.2f"
char anchorMinLabelBuffer[32];  // o.a. "%.2f%% %.2f"
// Fase 8.6.2: static verwijderd zodat UIController module deze kan gebruiken
char priceTitleBuffer[SYMBOL_COUNT][40];  // Buffers voor price titles (48→40 bytes om DRAM te sparen)
char price1MinMaxLabelBuffer[18];  // Buffer voor 1m max label (max: "12345.67" = ~8 chars)
char price1MinMinLabelBuffer[18];  // Buffer voor 1m min label (max: "12345.67" = ~8 chars)
char price1MinDiffLabelBuffer[18];  // Buffer voor 1m diff label (max: "12345.67" = ~8 chars)
char price30MinMaxLabelBuffer[18];  // Buffer voor 30m max label (max: "12345.67" = ~8 chars)
char price30MinMinLabelBuffer[20];  // Buffer voor 30m min label (max: "12345.67" = ~8 chars)
char price30MinDiffLabelBuffer[20];  // Buffer voor 30m diff label (max: "12345.67" = ~8 chars)
char price2HMaxLabelBuffer[20];  // Buffer voor 2h max label (max: "12345.67" = ~8 chars, altijd gedefinieerd)
char price2HMinLabelBuffer[20];  // Buffer voor 2h min label (max: "12345.67" = ~8 chars, altijd gedefinieerd)
char price2HDiffLabelBuffer[20];  // Buffer voor 2h diff label (max: "12345.67" = ~8 chars, altijd gedefinieerd)
#if defined(PLATFORM_ESP32S3_JC3248W535)
char price5mMaxLabelBuffer[20];
char price5mMinLabelBuffer[20];
char price5mDiffLabelBuffer[20];
char price1dMaxLabelBuffer[20];
char price1dMinLabelBuffer[20];
char price1dDiffLabelBuffer[20];
char price7dMaxLabelBuffer[20];
char price7dMinLabelBuffer[20];
char price7dDiffLabelBuffer[20];
#endif

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
float lastPrice2HMaxValue = -1.0f;  // Cache voor 2h max (4-symbol boards met 2h-kaart)
float lastPrice2HMinValue = -1.0f;
float lastPrice2HDiffValue = -1.0f;
#if defined(PLATFORM_ESP32S3_JC3248W535)
float lastPrice5mMaxValue = -1.0f;
float lastPrice5mMinValue = -1.0f;
float lastPrice5mDiffValue = -1.0f;
float lastPrice1dMaxValue = -1.0f;
float lastPrice1dMinValue = -1.0f;
float lastPrice1dDiffValue = -1.0f;
float lastPrice7dMaxValue = -1.0f;
float lastPrice7dMinValue = -1.0f;
float lastPrice7dDiffValue = -1.0f;
#endif
char lastPriceTitleText[SYMBOL_COUNT][32] = {""};  // Cache voor price titles (max: "30 min  +12.34%" = ~20 chars, verkleind van 48 naar 32 bytes)
char priceLblBufferArray[SYMBOL_COUNT][24];  // Buffers voor average price labels (max: "12345.67" = ~8 chars)
static char footerRssiBuffer[10];  // Buffer voor footer RSSI
static char footerRamBuffer[10];  // Buffer voor footer RAM
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
// Dynamisch gealloceerd: zonder PSRAM → INTERNAL; met PSRAM → SPIRAM (bespaart DRAM)
float *fiveMinutePrices = nullptr;
DataSource *fiveMinutePricesSource = nullptr;
uint16_t fiveMinuteIndex = 0;
bool fiveMinuteArrayFilled = false;

// Array van 120 posities voor laatste 120 minuten (2 uur)
// Elke minuut wordt het gemiddelde van de 60 seconden opgeslagen
// Dynamisch gealloceerd: zonder PSRAM → INTERNAL; met PSRAM → SPIRAM (bespaart DRAM)
float *minuteAverages = nullptr;
DataSource *minuteAveragesSource = nullptr;
// Fase 4.2.9: static verwijderd zodat PriceData getters deze kunnen gebruiken
uint8_t minuteIndex = 0;
bool minuteArrayFilled = false;
static unsigned long lastMinuteUpdate = 0;
static float firstMinuteAverage = 0.0f; // Eerste minuut gemiddelde prijs als basis voor 30-min berekening
// Uur-aggregatie buffer voor lange perioden (max 7 dagen)
float *hourlyAverages = nullptr;
DataSource *hourlyAveragesSource = nullptr;  // bron per uur (UI 1d/7d: % SOURCE_LIVE in venster)
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
uint8_t warmStart5mCandles = WARM_START_5M_CANDLES_DEFAULT;  // UI/MQTT/telemetry; performWarmStart gebruikt dit niet meer (5m-seed uit 1m)
uint8_t warmStart30mCandles = WARM_START_30M_CANDLES_DEFAULT;
uint8_t warmStart2hCandles = WARM_START_2H_CANDLES_DEFAULT;
bool warmStartSkip1m = WARM_START_SKIP_1M_DEFAULT;
bool warmStartSkip5m = WARM_START_SKIP_5M_DEFAULT;
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
volatile bool pendingDisplayRotationApply = false;
volatile uint8_t pendingDisplayRotationValue = 0;
volatile bool pendingMqttReconnect = false;
volatile bool pendingIpPublish = false;
char pendingIpBuffer[16] = "";

// Forward declarations (moet vroeg in het bestand staan)
static bool enqueueMqttMessage(const char* topic, const char* payload, bool retained);
void publishMqttAnchorEvent(float anchor_price, const char* event_type);
void apiTask(void *parameter);
void uiTask(void *parameter);
void webTask(void *parameter);
void priceRepeatTask(void *parameter); // Aparte task voor periodieke prijs herhaling
void wifiConnectionAndFetchPrice();
void setDisplayBrigthness();
void requestDisplayRotation(uint8_t rotation);
void requestMqttReconnect();

// Settings structs voor betere organisatie
// NOTE: AlertThresholds en NotificationCooldowns zijn nu gedefinieerd in SettingsStore.h

// Instelbare grenswaarden (worden geladen uit Preferences)
// Note: ntfyTopic wordt geïnitialiseerd in loadSettings() met unieke ESP32 ID
// Fase 8.7.1: static verwijderd zodat UIController module deze kan gebruiken
char ntfyTopic[64] = "";  // NTFY topic (max 63 karakters)
// NTFY backoff om fout-stormen te voorkomen
static unsigned long ntfyNextAllowedMs = 0;
static uint8_t ntfyFailStreak = 0;

// apiTask-handle: direct wakker maken na enqueue (leeg→niet-leeg) i.p.v. volledige UPDATE_API_INTERVAL wachten
static TaskHandle_t s_apiTaskHandle = nullptr;
#ifndef DEBUG_NTFY_API_WAKE
#define DEBUG_NTFY_API_WAKE 0
#endif

// Diagnostiek: laatste NTFY poging kreeg HTTP 429.
// Wordt alleen gezet wanneer er effectief een HTTP response code 429 is ontvangen.
static bool ntfyLastSendAttemptWas429 = false;

// Diagnostiek: NTFY send werd vroeg afgebroken door lokale backoff (geen HTTP response).
static bool ntfyLastSendAttemptWasRateLimitedByBackoff = false;

// Startup NTFY: uitgestelde retry bij 429.
static bool ntfyStartupTestDeferredPending = false;
static unsigned long ntfyStartupTestDeferredNextMs = 0;
static bool ntfyStartupTestDeferredRetryAttempted = false;

// NTFY test scheduling (alleen gebruikt als CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME actief is)
static bool ntfyStartupTestSent = false;
static unsigned long ntfyPeriodicTestNextMs = 0;
// Candles backoff om connect errors te temperen
static unsigned long candlesNextAllowedMs = 0;
static uint8_t candlesFailStreak = 0;
// REST-candles: reconnect-grace en connect-failure backoff (alleen updateLatestKlineMetricsIfNeeded / fetchBitvavoCandles)
static unsigned long lastWsDisconnectMs = 0;
static unsigned long lastWsReconnectMs = 0;
static unsigned long lastCandleRestFailMs = 0;
static const unsigned long WS_RECONNECT_GRACE_CANDLES_MS = 15000UL;
static const unsigned long CANDLE_REST_CONNECT_BACKOFF_MS = 30000UL;
// Fase 5.1: static verwijderd zodat TrendDetector module deze variabele kan gebruiken
char bitvavoSymbol[16] = BITVAVO_SYMBOL_DEFAULT;  // Bitvavo symbool (max 15 karakters, bijv. BTC-EUR, ETH-EUR)

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
// 2h-alert defaults — profiel 5F (volledige struct; gelijk aan SettingsStore-basis voor nieuwe/lege NVS)
Alert2HThresholds alert2HThresholds = {
    .breakMarginPct = 0.15f,
    .breakResetMarginPct = 0.10f,
    .breakCooldownMs = 10800000UL,
    .meanMinDistancePct = 0.80f,
    .meanTouchBandPct = 0.10f,
    .meanCooldownMs = 10800000UL,
    .compressThresholdPct = 0.70f,
    .compressResetPct = 1.10f,
    .compressCooldownMs = 18000000UL,
    .anchorOutsideMarginPct = 0.25f,
    .anchorCooldownMs = 10800000UL,
    .trendHysteresisFactor = 0.65f,
    .throttlingTrendChangeMs = 10800000UL,
    .throttlingTrendToMeanMs = 3600000UL,
    .throttlingMeanTouchMs = 7200000UL,
    .throttlingCompressMs = 10800000UL,
    .twoHSecondaryGlobalCooldownSec = 14400UL,
    .twoHSecondaryCoalesceWindowSec = 180UL,
    .anchorSourceMode = 0,
    .autoAnchorLastValue = 0.0f,
    .autoAnchorLastUpdateEpoch = 0,
    .autoAnchorUpdateMinutes = 120,
    .autoAnchorForceUpdateMinutes = 720,
    .autoAnchor4hCandles = 24,
    .autoAnchor1dCandles = 14,
    .autoAnchorMinUpdatePct_x100 = 15,
    .autoAnchorTrendPivotPct_x100 = 100,
    .autoAnchorW4hBase_x100 = 35,
    .autoAnchorW4hTrendBoost_x100 = 35,
    .autoAnchorFlags = 0
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

// Boot-netwerk fasering: MQTT en WS iets uit fase om opstart-storm (connection refused) te verminderen.
#ifndef CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS
#define CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS 4000UL
#endif
#ifndef CRYPTO_ALERT_BOOTNET_WS_EXTRA_DELAY_MS
#define CRYPTO_ALERT_BOOTNET_WS_EXTRA_DELAY_MS 2000UL
#endif
static unsigned long s_bootNetMqttGateUntilMs = 0; // 0 = geen gate (of al voorbij)
static unsigned long s_bootNetWsGateUntilMs = 0;

// API boot-settle: korte pauze vóór vroege Bitvavo/HTTP om connection refused te verminderen.
#ifndef CRYPTO_ALERT_BOOTNET_API_SETTLE_MS
#define CRYPTO_ALERT_BOOTNET_API_SETTLE_MS 1200UL
#endif
#ifndef CRYPTO_ALERT_BOOTNET_WARMSTART_API_DELAY_MS
#define CRYPTO_ALERT_BOOTNET_WARMSTART_API_DELAY_MS 600UL
#endif
static unsigned long s_bootNetApiGateUntilMs = 0; // 0 = geen actieve gate

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
static bool mqttDiscoveryPublished = false;
static unsigned long mqttLastDiscoveryMs = 0;
static unsigned long mqttLastSettingsPublishMs = 0;

// WiFi reconnect controle
// Geoptimaliseerd: betere reconnect logica met retry counter en exponential backoff
static bool wifiReconnectEnabled = false;
static unsigned long lastReconnectAttempt = 0;
static bool wifiInitialized = false;
static uint8_t reconnectAttemptCount = 0;
static const uint8_t MAX_RECONNECT_ATTEMPTS = 5; // Max aantal reconnect pogingen voordat we exponential backoff starten
static const uint8_t RECONNECT_ATTEMPTS_BEFORE_AP = 8; // Na zoveel mislukte reconnects: start CryptoAlert AP om WiFi opnieuw in te stellen
static bool apStartedForReconnect = false; // Of de AP al is gestart na herhaaldelijk reconnect-falen

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
// FASE 1.2: Debug logging toegevoegd voor verificatie
static bool parseKlineEntry(const char* jsonStr, float* closePrice, unsigned long* openTime)
{
    if (jsonStr == nullptr || closePrice == nullptr || openTime == nullptr) {
        return false;
    }
    
    // Skip opening bracket
    const char* ptr = jsonStr;
    while (*ptr && (*ptr == '[' || *ptr == ' ')) ptr++;
    if (*ptr == '\0') {
        return false;
    }
    
    // Parse openTime (eerste veld)
    uint64_t time = 0;
    while (*ptr && *ptr != ',') {
        if (*ptr >= '0' && *ptr <= '9') {
            time = time * 10 + (*ptr - '0');
        }
        ptr++;
    }
    if (*ptr != ',') {
        return false;
    }
    if (time > 2000000000ULL) {
        time /= 1000ULL; // converteer ms -> s
    }
    *openTime = (unsigned long)time;
    ptr++; // Skip comma
    
    
    // Skip open, high, low (velden 2-4)
    for (int i = 0; i < 3; i++) {
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr != ',') {
            return false;
        }
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
    if (!safeAtof(priceStr, price)) {
        return false;
    }
    
    if (!isValidPrice(price)) {
        return false;
    }
    
    *closePrice = price;
    return true;
}

// Haal Binance klines op voor een specifiek timeframe
// Memory efficient: streaming parsing, bewaar alleen laatste maxCount candles
// Returns: aantal candles opgehaald, of -1 bij fout
int fetchBitvavoCandles(const char* symbol, const char* interval, uint16_t limit, float* prices, unsigned long* timestamps, uint16_t maxCount, float* highs = nullptr, float* lows = nullptr, float* volumes = nullptr)
{
    if (symbol == nullptr || interval == nullptr || prices == nullptr || maxCount == 0) {
        return -1;
    }

    // Vroege boot: geen candle-HTTP gelijk met MQTT-connect (zelfde netwerk-slot / connection refused).
    if (s_bootNetMqttGateUntilMs != 0 && millis() < s_bootNetMqttGateUntilMs) {
        static unsigned long s_lastBootNetCandleMqttGateLogMs = 0;
        const unsigned long now = millis();
        if (now - s_lastBootNetCandleMqttGateLogMs >= 4000UL) {
            Serial.println(F("[BootNet] Candle fetch delayed due to MQTT boot gate"));
            s_lastBootNetCandleMqttGateLogMs = now;
        }
        return 0;
    }

    // M1: Heap telemetry vóór URL build
    logHeap("CANDLES_URL_BUILD");
    
    // Build Bitvavo URL: https://api.bitvavo.com/v2/{market}/candles?interval={interval}&limit={limit}
    char url[256];
    int urlLen = snprintf(url, sizeof(url), "%s/%s/candles?interval=%s&limit=%u", 
                         BITVAVO_API_BASE, symbol, interval, limit);
    if (urlLen < 0 || urlLen >= (int)sizeof(url)) {
        return -1;
    }
    
    // M1: Heap telemetry vóór HTTP GET
    logHeap("KLINES_GET_PRE");
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("fetchBitvavoCandles");
    
    int result = -1;
    HTTPClient http;
        
        // S2: do-while(0) patroon voor consistente cleanup
        do {
            // N1: Expliciete connect/read timeout settings (geoptimaliseerd: 2000ms connect, 2500ms read)
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
        http.setTimeout(WARM_START_TIMEOUT_MS > HTTP_READ_TIMEOUT_MS ? WARM_START_TIMEOUT_MS : HTTP_READ_TIMEOUT_MS);
    http.setReuse(false);
        
        unsigned long requestStart = millis();
    
        // N2: Voeg User-Agent header toe VOOR http.begin() om Cloudflare blocking te voorkomen
        // Headers moeten worden toegevoegd voordat de verbinding wordt geopend
        http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
        http.addHeader(F("Accept"), F("application/json"));
    
    if (!http.begin(url)) {
            Serial.println(F("[Candles] http.begin() gefaald"));
            lastCandleRestFailMs = millis();
            if (candlesFailStreak < 6) candlesFailStreak++;
            uint32_t backoffMs = 5000UL << (candlesFailStreak - 1);
            if (backoffMs > 60000UL) backoffMs = 60000UL;
            candlesNextAllowedMs = millis() + backoffMs;
                break;
            }
    
    int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
            
            // M1: Heap telemetry na HTTP GET
            logHeap("CANDLES_GET_POST");
            
            if (code != 200) {
                // Fase 6.2: Geconsolideerde error logging - gebruik ApiClient helpers
                const char* phase = ApiClient::detectHttpErrorPhase(code);
            ApiClient::logHttpError(code, phase, requestTime, 0, 1, "[Candles]");
                const bool connectFail =
                    (code < 0) ||
                    (phase != nullptr && strcmp(phase, "connect") == 0) ||
                    (code == HTTPC_ERROR_CONNECTION_REFUSED) ||
                    (code == HTTPC_ERROR_CONNECTION_LOST);
                if (connectFail) {
                    lastCandleRestFailMs = millis();
                }
                if (candlesFailStreak < 6) candlesFailStreak++;
                uint32_t backoffMs = 5000UL << (candlesFailStreak - 1);
                if (backoffMs > 60000UL) backoffMs = 60000UL;
                candlesNextAllowedMs = millis() + backoffMs;
                break;
            }
            
    WiFiClient* stream = http.getStreamPtr();
            if (stream == nullptr) {
                Serial.println(F("[Candles] Stream pointer is null"));
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
        if (bitvavoStreamBuffer == nullptr || bitvavoStreamBufferSize < 2) {
            break;
        }
        const size_t BUFFER_SIZE = bitvavoStreamBufferSize;
        
        // Feed watchdog tijdens parsing
        unsigned long lastWatchdogFeed = millis();
        const unsigned long WATCHDOG_FEED_INTERVAL = 1000; // Feed elke seconde
        
        // Timeout voor parsing
        unsigned long parseStartTime = millis();
        const unsigned long PARSE_TIMEOUT_MS = 8000;
        unsigned long lastDataTime = millis();
        const unsigned long DATA_TIMEOUT_MS = 2000;
        
        // M1: Heap telemetry vóór JSON parse
        logHeap("CANDLES_PARSE_PRE");
    
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
                bufferLen = stream->readBytes((uint8_t*)bitvavoStreamBuffer, BUFFER_SIZE - 1);
                bitvavoStreamBuffer[bufferLen] = '\0';
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
        
        char c = bitvavoStreamBuffer[bufferPos++];
        
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
                        uint64_t time = 0;
                        for (int i = 0; fieldBuf[i] != '\0'; i++) {
                            if (fieldBuf[i] >= '0' && fieldBuf[i] <= '9') {
                                time = time * 10 + (fieldBuf[i] - '0');
                            }
                        }
                        if (time > 2000000000ULL) {
                            time /= 1000ULL; // converteer ms -> s
                        }
                        openTime = (unsigned long)time;
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
    logHeap("CANDLES_PARSE_POST");
    
    if (lastParsedKline.valid && interval != nullptr) {
        if (strcmp(interval, "1m") == 0) {
            lastKline1m = lastParsedKline;
        } else if (strcmp(interval, "5m") == 0) {
            lastKline5m = lastParsedKline;
        }
    }
    // Reset backoff bij succesvolle parse
    candlesFailStreak = 0;
    candlesNextAllowedMs = 0;
    lastCandleRestFailMs = 0;
    
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
    netMutexUnlock("fetchBitvavoCandles");
    
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
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
        if (psramAvailable) {
            maxCandles = 240;  // S3 + PSRAM: hogere warm-start limiet
        }
    #endif
    return clampUint16(baseCandles, 30, maxCandles);
}

// Helper: lineaire regressie % over reeks (x = uur index, y = prijs)
static bool computeRegressionPctFromSeries(const float* prices, int count, float stepHours, float totalHours, float &outPct)
{
    outPct = 0.0f;
    if (prices == nullptr || count < 2 || stepHours <= 0.0f || totalHours <= 0.0f) {
        return false;
    }
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    int validPoints = 0;
    for (int i = 0; i < count; i++) {
        float price = prices[i];
        if (!isValidPrice(price)) {
            continue;
        }
        float x = (float)i * stepHours;
        sumX += x;
        sumY += price;
        sumXY += x * price;
        sumX2 += x * x;
        validPoints++;
    }
    if (validPoints < 2) {
        return false;
    }
    float denom = (validPoints * sumX2 - sumX * sumX);
    if (fabsf(denom) < 1e-6f) {
        return false;
    }
    float slope = (validPoints * sumXY - sumX * sumY) / denom; // prijs per uur
    float avgPrice = sumY / (float)validPoints;
    if (avgPrice <= 0.0f) {
        return false;
    }
    outPct = (slope * totalHours / avgPrice) * 100.0f;
    return true;
}

// Helper: lineaire regressie % op basis van timestamps (x = uren sinds start)
static bool computeRegressionPctFromSeriesWithTimes(const float* prices, const unsigned long* times, int count, float totalHours, float &outPct)
{
    outPct = 0.0f;
    if (prices == nullptr || times == nullptr || count < 2) {
        return false;
    }
    unsigned long minTime = 0xFFFFFFFFUL;
    unsigned long maxTime = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    int validPoints = 0;
    for (int i = 0; i < count; i++) {
        float price = prices[i];
        unsigned long t = times[i];
        if (!isValidPrice(price) || t == 0) {
            continue;
        }
        if (t < minTime) minTime = t;
        if (t > maxTime) maxTime = t;
    }
    if (minTime == 0xFFFFFFFFUL || maxTime <= minTime) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        float price = prices[i];
        unsigned long t = times[i];
        if (!isValidPrice(price) || t == 0) {
            continue;
        }
        float x = (float)(t - minTime) / 3600.0f; // uren sinds start
        sumX += x;
        sumY += price;
        sumXY += x * price;
        sumX2 += x * x;
        validPoints++;
    }
    if (validPoints < 2) {
        return false;
    }
    float denom = (validPoints * sumX2 - sumX * sumX);
    if (fabsf(denom) < 1e-6f) {
        return false;
    }
    float slope = (validPoints * sumXY - sumX * sumY) / denom; // prijs per uur
    float avgPrice = sumY / (float)validPoints;
    if (avgPrice <= 0.0f) {
        return false;
    }
    if (totalHours <= 0.0f) {
        totalHours = (float)(maxTime - minTime) / 3600.0f;
        if (totalHours <= 0.0f) {
            return false;
        }
    }
    outPct = (slope * totalHours / avgPrice) * 100.0f;
    return true;
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

    bootNetArmApiGateMs(CRYPTO_ALERT_BOOTNET_WARMSTART_API_DELAY_MS, true);
    bootNetWaitApiGateIfNeeded("warmStart");

    // Bereken dynamische candle limits (PSRAM-aware clamping)
    bool psramAvailable = hasPSRAM();
    uint16_t req1mCandles = warmStartSkip1m ? 0 : calculate1mCandles();  // PSRAM-aware (max 150 met PSRAM, 80 zonder)
    uint16_t max30m = psramAvailable ? 12 : 6;  // Met PSRAM: 12, zonder: 6
    uint16_t max2h = psramAvailable ? 8 : 4;  // Met PSRAM: 8, zonder: 4
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
        if (psramAvailable) {
            max30m = 24;
            max2h = 12;
        }
    #endif
    uint16_t req30mCandles = clampUint16(warmStart30mCandles, 2, max30m);
    uint16_t req2hCandles = clampUint16(warmStart2hCandles, 2, max2h);
    
    float temp1mPrices[SECONDS_PER_MINUTE];
    int count1m = 0;
    
    // 1. Vul 1m buffer voor volatiliteit (returns-only: alleen laatste closes nodig)
    if (warmStartSkip1m) {
        warmStartStats.loaded1m = 0;
        warmStartStats.warmStartOk1m = true;  // Skip is OK
        Serial.println(F("[WarmStart][1m] SKIPPED (warmStartSkip1m=1)"));
    } else {
    // Memory efficient: alleen laatste SECONDS_PER_MINUTE closes bewaren
    lv_timer_handler();  // Update spinner animatie vóór fetch
    count1m = fetchBitvavoCandles(bitvavoSymbol, "1m", req1mCandles, temp1mPrices, nullptr, SECONDS_PER_MINUTE);
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
        #if DEBUG_CALCULATIONS
        Serial.printf(F("[WarmStart][1m] FAILED: count1m=%d\n"), count1m);
        #endif
        }
    }
    
    // 2. Vul 5m-buffer: 300s uit laatste 5 opeenvolgende 1m-closes — per minuut lineair tussen vorige close en huidige close (geen echte OHLC open; geen aparte 5m API)
    if (warmStartSkip5m) {
        warmStartStats.loaded5m = 0;
        warmStartStats.warmStartOk5m = true;  // Skip is OK
        Serial.println(F("[WarmStart][5m] SKIPPED (warmStartSkip5m=1)"));
    } else if (count1m >= 5 && fiveMinutePrices != nullptr && fiveMinutePricesSource != nullptr) {
        const int base = count1m - 5;
        int outIdx = 0;
        for (int j = 0; j < 5; j++) {
            float closeVal = temp1mPrices[base + j];
            float prevCloseVal = (base + j > 0) ? temp1mPrices[base + j - 1] : closeVal;
            for (int s = 0; s < 60; s++) {
                float tt = (float)s / 59.0f;
                float p = prevCloseVal + (closeVal - prevCloseVal) * tt;
                fiveMinutePrices[outIdx] = p;
                fiveMinutePricesSource[outIdx] = SOURCE_BINANCE;
                outIdx++;
            }
        }
        fiveMinuteIndex = 0;
        fiveMinuteArrayFilled = true;
        priceData.syncStateFromGlobals();
        warmStartStats.loaded5m = 5;
        warmStartStats.warmStartOk5m = true;
    } else {
        warmStartStats.loaded5m = 0;
        warmStartStats.warmStartOk5m = false;
    }
    
    yield();
    delay(0);
    
    // 3. Haal 1w candles op voor lange termijn trend (fallback)
    float temp1wPrices[2];
    unsigned long temp1wTimes[2];
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
        count1w = fetchBitvavoCandles(bitvavoSymbol, "1W", 2, temp1wPrices, temp1wTimes, 2);  // Bitvavo gebruikt "1W" (hoofdletter W)
        lv_timer_handler();
        if (count1w >= 2) {
            break;
        }
    }

    // Provenance ret_7d (warm-start): eerste waarde uit 1W-regressie; kan later vervangen worden door 7d daily (bewuste volgorde).
    if (count1w >= 2) {
        float spanHours = (float)(count1w - 1) * 168.0f;
        float totalHours = (spanHours > 168.0f || spanHours <= 0.0f) ? 168.0f : spanHours;
        if (computeRegressionPctFromSeriesWithTimes(temp1wPrices, temp1wTimes, count1w, totalHours, ret_7d)) {
            hasRet7dWarm = true;
        } else {
            hasRet7dWarm = false;
        }
    } else {
        hasRet7dWarm = false;
    }

    // 4. Haal 1d candles op voor lange termijn trend (regressie over 7 dagen)
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    float temp1dPrices[30];
    unsigned long temp1dTimes[30];
    const uint8_t max1dCandles = 30;
    #else
    float temp1dPrices[8];
    unsigned long temp1dTimes[8];
    const uint8_t max1dCandles = 8;
    #endif
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
        count1d = fetchBitvavoCandles(bitvavoSymbol, "1d", max1dCandles, temp1dPrices, temp1dTimes, max1dCandles);
        lv_timer_handler();
        if (count1d >= 2) {
            break;
        }
    }
    
    // Sorteer 1d candles op tijd (oudste -> nieuwste) zodat trendrichting klopt
    if (count1d > 1) {
        for (int i = 1; i < count1d; i++) {
            unsigned long t = temp1dTimes[i];
            float p = temp1dPrices[i];
            int j = i - 1;
            while (j >= 0 && temp1dTimes[j] > t) {
                temp1dTimes[j + 1] = temp1dTimes[j];
                temp1dPrices[j + 1] = temp1dPrices[j];
                j--;
            }
            temp1dTimes[j + 1] = t;
            temp1dPrices[j + 1] = p;
        }
    }

    // Provenance ret_1d (warm-start): eerste schrijf via 1d-candles; kan worden overschreven door 1h/24h-blok hieronder.
    if (count1d >= 2) {
        float spanHours = (float)(count1d - 1) * 24.0f;
        float totalHours = (spanHours > 24.0f || spanHours <= 0.0f) ? 24.0f : spanHours;
        if (computeRegressionPctFromSeriesWithTimes(temp1dPrices, temp1dTimes, count1d, totalHours, ret_1d)) {
            hasRet1dWarm = true;
            #if DEBUG_CALCULATIONS
            Serial_printf(F("[WarmStart][1d] ret_1d=%.4f%% (regressie)\n"), ret_1d);
            #endif
        } else {
            hasRet1dWarm = false;
            #if DEBUG_CALCULATIONS
            Serial_printf(F("[WarmStart][1d] ERROR: regressie mislukt\n"));
            #endif
        }
    } else {
        hasRet1dWarm = false;
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[WarmStart][1d] ERROR: count1d=%d < 2\n"), count1d);
        #endif
    }

    // 4b. Haal 1h candles op voor echte 24h UI stats (min/max/avg + ret_1d)
    float temp1hPrices[24];
    unsigned long temp1hTimes[24];
    int count1h = 0;
    const int maxRetries1h = 2;
    for (int retry = 0; retry < maxRetries1h; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 1h retry %d/%d...\n"), retry, maxRetries1h - 1);
            yield();
            delay(400);
            lv_timer_handler();
        }
        lv_timer_handler();
        count1h = fetchBitvavoCandles(bitvavoSymbol, "1h", 24, temp1hPrices, temp1hTimes, 24);
        lv_timer_handler();
        if (count1h >= 2) {
            break;
        }
    }

    if (count1h >= 2) {
        // Sorteer 1h candles op tijd (oudste -> nieuwste)
        for (int i = 1; i < count1h; i++) {
            unsigned long t = temp1hTimes[i];
            float p = temp1hPrices[i];
            int j = i - 1;
            while (j >= 0 && temp1hTimes[j] > t) {
                temp1hTimes[j + 1] = temp1hTimes[j];
                temp1hPrices[j + 1] = temp1hPrices[j];
                j--;
            }
            temp1hTimes[j + 1] = t;
            temp1hPrices[j + 1] = p;
        }
        float sum1h = 0.0f;
        float min1h = 0.0f;
        float max1h = 0.0f;
        bool firstValid1h = false;
        for (int i = 0; i < count1h; i++) {
            float price = temp1hPrices[i];
            if (!isValidPrice(price)) {
                continue;
            }
            if (!firstValid1h) {
                min1h = price;
                max1h = price;
                firstValid1h = true;
            } else {
                if (price < min1h) min1h = price;
                if (price > max1h) max1h = price;
            }
            sum1h += price;
        }
        if (firstValid1h) {
            warmStart1dMin = min1h;
            warmStart1dMax = max1h;
            warmStart1dAvg = sum1h / (float)count1h;
            warmStart1dValid = true;
        } else {
            warmStart1dValid = false;
        }
        // Definitieve warm-start ret_1d als beide paden slagen: deze 1h/24h-regressie (overschrijft 1d-candlepad hierboven).
        float spanHours1d = (count1h > 1) ? (float)(count1h - 1) * 1.0f : 0.0f;
        float totalHours1d = (spanHours1d > 24.0f || spanHours1d <= 0.0f) ? 24.0f : spanHours1d;
        if (computeRegressionPctFromSeriesWithTimes(temp1hPrices, temp1hTimes, count1h, totalHours1d, ret_1d)) {
            hasRet1dWarm = true;
            Serial_printf(F("[WarmStart][1h] ret_1d=%.4f%% (regressie over 24h)\n"),
                          ret_1d);
        } else {
            hasRet1dWarm = false;
        }
        Serial_printf(F("[WarmStart][1h] count=%d, valid=%d, min=%.2f, max=%.2f, avg=%.2f\n"),
                      count1h, warmStart1dValid ? 1 : 0, warmStart1dMin, warmStart1dMax, warmStart1dAvg);
    } else {
        Serial_printf(F("[WarmStart] 1h fetch onvoldoende candles (%d)\n"), count1h);
    }

    // Vervangt 1W-ret_7d indien count1d>=7 en regressie slaagt (bewuste downstream-volgorde).
    // Warm-start 7d: zelfde 7× daily venster als live fill168HourlyStatsFor7dUi / ret_7d-daily — min/max/gem. voor JC3248-fallback.
    if (count1d >= 7) {
        const int startIdx = count1d - 7;  // laatste 7 candles (oudste -> nieuwste)
        float sum7d = 0.0f;
        float min7d = 0.0f;
        float max7d = 0.0f;
        bool first7d = false;
        uint16_t cnt7d = 0;
        for (int i = 0; i < 7; i++) {
            float price = temp1dPrices[startIdx + i];
            if (!isValidPrice(price)) {
                continue;
            }
            sum7d += price;
            cnt7d++;
            if (!first7d) {
                min7d = max7d = price;
                first7d = true;
            } else {
                if (price < min7d) {
                    min7d = price;
                }
                if (price > max7d) {
                    max7d = price;
                }
            }
        }
        if (first7d && cnt7d > 0) {
            warmStart7dMin = min7d;
            warmStart7dMax = max7d;
            warmStart7dAvg = sum7d / (float)cnt7d;
            warmStart7dValid = true;
        } else {
            warmStart7dValid = false;
        }
        const float stepHours = 24.0f;
        const float totalHours = 168.0f;
        float ret7dDaily = 0.0f;
        if (computeRegressionPctFromSeries(&temp1dPrices[startIdx], 7, stepHours, totalHours, ret7dDaily)) {
            ret_7d = ret7dDaily;
            hasRet7dWarm = true;
            #if DEBUG_CALCULATIONS
            Serial_printf(F("[WarmStart][7d] ret_7d=%.4f%% (regressie over 7d daily)\n"), ret_7d);
            #endif
        }
        Serial_printf(F("[WarmStart][7d] daily window: valid=%u, min=%.2f, max=%.2f, avg=%.2f (warmValid=%d)\n"),
                      (unsigned)cnt7d, warmStart7dMin, warmStart7dMax, warmStart7dAvg,
                      warmStart7dValid ? 1 : 0);
        // 7× daily *close* min/max kan smaller zijn dan 24×1h intraday — union met 1d warm zodat nested TF logisch blijft
        if (warmStart7dValid && warmStart1dValid) {
            if (warmStart1dMax > warmStart7dMax) {
                warmStart7dMax = warmStart1dMax;
            }
            if (warmStart1dMin < warmStart7dMin) {
                warmStart7dMin = warmStart1dMin;
            }
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
        count4h = fetchBitvavoCandles(bitvavoSymbol, "4h", 2, temp4hPrices, nullptr, 2);
        lv_timer_handler();
        if (count4h >= 2) {
            break;
        }
    }
    
    if (count4h >= 2) {
        float spanHours = (float)(count4h - 1) * 4.0f;
        float totalHours = (spanHours > 4.0f || spanHours <= 0.0f) ? 4.0f : spanHours;
        if (computeRegressionPctFromSeries(temp4hPrices, count4h, 4.0f, totalHours, ret_4h)) {
            hasRet4hWarm = true;
        } else {
            hasRet4hWarm = false;
        }
    } else {
        hasRet4hWarm = false;
    }
    
    // 6. Warme ret_30m uit 30m API; minuutbuffer uit 1m-closes (geen platte 30m-close over 120 slots)
    // Retry-logica: probeer maximaal 3 keer als eerste poging faalt
    float temp30mPrices[2];
    unsigned long temp30mTimes[2];
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
        count30m = fetchBitvavoCandles(bitvavoSymbol, "30m", req30mCandles, temp30mPrices, temp30mTimes, 2);
        lv_timer_handler();  // Update spinner animatie na fetch
        if (count30m >= 2) {
            break;  // Succes, stop retries
        }
    }
    
    if (count30m >= 2) {
        // Sorteer 30m candles op tijd (oudste -> nieuwste)
        for (int i = 1; i < count30m; i++) {
            unsigned long t = temp30mTimes[i];
            float p = temp30mPrices[i];
            int j = i - 1;
            while (j >= 0 && temp30mTimes[j] > t) {
                temp30mTimes[j + 1] = temp30mTimes[j];
                temp30mPrices[j + 1] = temp30mPrices[j];
                j--;
            }
            temp30mTimes[j + 1] = t;
            temp30mPrices[j + 1] = p;
        }
        float spanHours = (float)(count30m - 1) * 0.5f;
        float totalHours = (spanHours > 0.5f || spanHours <= 0.0f) ? 0.5f : spanHours;
        if (computeRegressionPctFromSeriesWithTimes(temp30mPrices, temp30mTimes, count30m, totalHours, ret_30m)) {
            hasRet30mWarm = true;
        } else {
            hasRet30mWarm = false;
        }
        
        // Minuutbuffer: seed uit bestaande 1m-closes (chronologisch), geen platte vulling met 30m-close
        if (minuteAverages != nullptr && minuteAveragesSource != nullptr) {
            if (count1m > 0) {
                int seedCount = (count1m < MINUTES_FOR_30MIN_CALC) ? count1m : MINUTES_FOR_30MIN_CALC;
                int startIdx = count1m - seedCount;
                for (int m = 0; m < MINUTES_FOR_30MIN_CALC; m++) {
                    if (m < seedCount) {
                        minuteAverages[m] = temp1mPrices[startIdx + m];
                        minuteAveragesSource[m] = SOURCE_BINANCE;
                    } else {
                        minuteAverages[m] = 0.0f;
                        minuteAveragesSource[m] = SOURCE_BINANCE;
                    }
                }
                minuteIndex = (uint8_t)seedCount;
                minuteArrayFilled = (seedCount == MINUTES_FOR_30MIN_CALC);
                firstMinuteAverage = (seedCount > 0) ? minuteAverages[0] : 0.0f;
            } else {
                for (int m = 0; m < MINUTES_FOR_30MIN_CALC; m++) {
                    minuteAverages[m] = 0.0f;
                    minuteAveragesSource[m] = SOURCE_BINANCE;
                }
                minuteIndex = 0;
                minuteArrayFilled = false;
                firstMinuteAverage = 0.0f;
            }
        }
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
    
    // 7. Initieer 2h trend berekening
    // Retry-logica: probeer maximaal 3 keer als eerste poging faalt
    float temp2hPrices[12];
    unsigned long temp2hTimes[12];
    int count2h = 0;
    const int maxRetries2h = 3;
    uint16_t req2hFetch = (req2hCandles > 12) ? 12 : req2hCandles;
    for (int retry = 0; retry < maxRetries2h; retry++) {
        if (retry > 0) {
            Serial_printf(F("[WarmStart] 2h retry %d/%d...\n"), retry, maxRetries2h - 1);
            yield();
            delay(500);  // Korte delay tussen retries
            lv_timer_handler();  // Update spinner animatie
        }
        lv_timer_handler();  // Update spinner animatie vóór fetch
        count2h = fetchBitvavoCandles(bitvavoSymbol, "2h", req2hFetch, temp2hPrices, temp2hTimes, 12);
        lv_timer_handler();  // Update spinner animatie na fetch
        if (count2h >= 2) {
            break;  // Succes, stop retries
        }
    }
    
    if (count2h >= 2) {
        // Sorteer 2h candles op tijd (oudste -> nieuwste)
        for (int i = 1; i < count2h; i++) {
            unsigned long t = temp2hTimes[i];
            float p = temp2hPrices[i];
            int j = i - 1;
            while (j >= 0 && temp2hTimes[j] > t) {
                temp2hTimes[j + 1] = temp2hTimes[j];
                temp2hPrices[j + 1] = temp2hPrices[j];
                j--;
            }
            temp2hTimes[j + 1] = t;
            temp2hPrices[j + 1] = p;
        }
        // Bereken 2h min/max/avg voor UI (warm-start fallback)
        float sum2h = 0.0f;
        float min2h = 0.0f;
        float max2h = 0.0f;
        bool firstValid2h = false;
        int valid2h = 0;
        for (int i = 0; i < count2h; i++) {
            float price = temp2hPrices[i];
            if (!isValidPrice(price)) {
                continue;
            }
            valid2h++;
            if (!firstValid2h) {
                min2h = price;
                max2h = price;
                firstValid2h = true;
            } else {
                if (price < min2h) min2h = price;
                if (price > max2h) max2h = price;
            }
            sum2h += price;
        }
        if (firstValid2h) {
            warmStart2hMin = min2h;
            warmStart2hMax = max2h;
            warmStart2hAvg = sum2h / (float)valid2h;
            warmStart2hValid = true;
        } else {
            warmStart2hValid = false;
        }
        float spanHours = (float)(count2h - 1) * 2.0f;
        float totalHours = (spanHours > 2.0f || spanHours <= 0.0f) ? 2.0f : spanHours;
        // Warm-start ret_2h: globaal 2h-trend-% (API 2h candles); los van computeTwoHMetrics (EUR avg/high/low).
        if (computeRegressionPctFromSeriesWithTimes(temp2hPrices, temp2hTimes, count2h, totalHours, ret_2h)) {
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
        warmStart2hValid = false;
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
    hasRet4h = hasRet4hWarm;  // 4h alleen via warm-start
    hasRet1d = hasRet1dWarm;  // 1d alleen via warm-start
    hasRet7d = hasRet7dWarm;  // 7d via warm-start of live hourly buffer
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[WarmStart] Combined flags: hasRet1d=%d (warm=%d), hasRet7d=%d (warm=%d), ret_1d=%.4f%%, ret_7d=%.4f%%\n"),
                  hasRet1d ? 1 : 0, hasRet1dWarm ? 1 : 0,
                  hasRet7d ? 1 : 0, hasRet7dWarm ? 1 : 0,
                  ret_1d, ret_7d);
    #endif
    
    // Initialiseer prices array met warm-start waarden (voor directe UI weergave)
    #if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
    if (hasRet2h) {
        prices[3] = ret_2h;  // Zet 2h return direct na warm-start
    }
    #endif
    
#if SYMBOL_COUNT > 3
    // Bereken 2h gemiddelde na warm-start (voor UI weergave)
    if (hasRet2h && (minuteArrayFilled || minuteIndex > 0)) {
        uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
        if (availableMinutes > 0) {
            float last120Sum = 0.0f;
            uint16_t last120Count = 0;
            uint16_t minutesToUse = (availableMinutes < 120) ? availableMinutes : 120;
            accumulateValidPricesFromRingBuffer(
                minuteAverages,
                minuteArrayFilled,
                minuteIndex,
                MINUTES_FOR_30MIN_CALC,
                1,  // Start vanaf 1 positie terug (nieuwste)
                minutesToUse,
                last120Sum,
                last120Count
            );
            if (last120Count > 0) {
                averagePrices[3] = last120Sum / last120Count;
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[WarmStart][2h] averagePrices[3]=%.2f, availableMinutes=%u, minutesToUse=%u, last120Count=%u\n"),
                             averagePrices[3], availableMinutes, minutesToUse, last120Count);
                #endif
            } else {
                averagePrices[3] = 0.0f;
            }
        }
    }
#endif
    if (hasRet30m) {
        prices[2] = ret_30m;  // Zet 30m return direct na warm-start
    }
#if defined(PLATFORM_ESP32S3_JC3248W535)
    if (hasRet1d) {
        prices[5] = ret_1d;  // 1d-return op data-index 5 (zelfde semantiek als ret_1d elders)
    }
    if (warmStart1dValid) {
        averagePrices[5] = warmStart1dAvg;  // 24h gemiddelde uit warm-start 1h-candles (sluit aan op hasRet1d / warmStart1d*)
    }
    if (hasRet7d) {
        prices[6] = ret_7d;
    }
    if (warmStart7dValid) {
        averagePrices[6] = warmStart7dAvg;
    }
#endif
    
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
    bool ok1m = warmStartSkip1m ? true : warmStartStats.warmStartOk1m;
    bool ok5m = warmStartSkip5m ? true : warmStartStats.warmStartOk5m;
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
    if (warmStartSkip1m || warmStartSkip5m) {
        Serial_printf(F("[WarmStart] Skips: 1m=%d 5m=%d\n"),
                      warmStartSkip1m ? 1 : 0, warmStartSkip5m ? 1 : 0);
    }
    
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

// Send notification via Ntfy.sh (productie-HTTPS delivery).
// colorTag: "green_square" / "red_square" / "blue_square" / …
// Logging (Fase 2 tracker): [NTFY] = delivery/HTTP; [NTFY][diag] = optionele DNS; [WS] = WS lifecycle (los van NTFY HTTPS).

static void ntfyLogTopicDigest(char *out, size_t outSz, const char *topic) {
    if (out == nullptr || outSz == 0) return;
    size_t n = strlen(topic);
    if (n == 0) {
        snprintf(out, outSz, "empty");
        return;
    }
    if (n <= 8) {
        snprintf(out, outSz, "len=%u", (unsigned)n);
        return;
    }
    snprintf(out, outSz, "len=%u head=%.3s...tail=%.3s", (unsigned)n, topic, topic + n - 3);
}

// Wachttijd na HTTP 429: Retry-After (seconden) indien aanwezig, anders 60 s. Max 1 uur.
static uint32_t ntfy429WaitMs(HTTPClient &http, bool *usedRetryAfterHeader) {
    const uint32_t kDefaultMs = 60000UL;
    if (usedRetryAfterHeader) {
        *usedRetryAfterHeader = false;
    }
    String ra = http.header("Retry-After");
    if (ra.length() == 0) {
        return kDefaultMs;
    }
    int sec = ra.toInt();
    if (sec <= 0) {
        // Geen parsebare delta-seconds (bijv. HTTP-date) — veilige default
        return kDefaultMs;
    }
    if (usedRetryAfterHeader) {
        *usedRetryAfterHeader = true;
    }
    uint64_t ms = (uint64_t)sec * 1000ULL;
    if (ms > 3600000ULL) {
        ms = 3600000ULL;
    }
    return (uint32_t)ms;
}

static const char *ntfyPhaseForClientCode(int code) {
    if (code > 0) return "http_status";
    switch (code) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "connect_refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "send_header";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "send_body";
    case HTTPC_ERROR_NOT_CONNECTED: return "not_connected";
    case HTTPC_ERROR_CONNECTION_LOST: return "connection_lost";
    case HTTPC_ERROR_NO_STREAM: return "no_stream";
    case HTTPC_ERROR_NO_HTTP_SERVER: return "dns_or_host";
    case HTTPC_ERROR_TOO_LESS_RAM: return "out_of_ram";
    case HTTPC_ERROR_ENCODING: return "encoding";
    case HTTPC_ERROR_STREAM_WRITE: return "stream_write";
    case HTTPC_ERROR_READ_TIMEOUT: return "read_timeout";
#ifdef HTTPC_ERROR_SSL_NOT_AVAILABLE
    case HTTPC_ERROR_SSL_NOT_AVAILABLE: return "tls_ssl";
#endif
#ifdef HTTPC_ERROR_CONNECTION_FAILED
    case HTTPC_ERROR_CONNECTION_FAILED: return "connect_failed";
#endif
    default: return "transport_unknown";
    }
}

// --- Productie NTFY delivery (enkel pad) — Fase 3 tracker ---
// 1) sendNotification() → enqueueNtfyPending()
// 2) apiTask: bij pending → exclusive modus (STOPPING_WS → SEND → RESTARTING_WS)
// 3) ntfyExclusiveSendOnePendingFromQueue() → sendNtfyNotification() = validatie + deze HTTPS-transporthelper
// WS stop/restart: uitsluitend apiTask + wsStopForNtfyExclusive / restartWebSocketAfterNtfyExclusive (niet in sendNtfyNotification).

/** Alleen HTTPS POST naar ntfy.sh: retries, globale backoff/streak. Caller houdt netMutex. */
static bool ntfyHttpsPostNtfyAlertBody(
    const char *url, int urlLen,
    const char *title, const char *message, const char *colorTag,
    const char *topicDigest, size_t titleLen, size_t msgLen,
    unsigned long nowMsAtSendStart)
{
    const uint8_t MAX_RETRIES = 1;
    const uint32_t RETRY_DELAYS[] = {250, 750};
    bool ok = false;

    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        bool attemptOk = false;
        bool shouldRetry = false;
        int lastCode = 0;
        HTTPClient http;
        WiFiClientSecure ntfyClient;
        ntfyClient.setInsecure();

        Serial_printf(
            F("[NTFY] send start attempt=%u/%u proto=HTTPS host=ntfy.sh port=443 path=/%s full_url_len=%d title_len=%u body_len=%u tags=%s\n"),
            (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1),
            topicDigest,
            urlLen,
            (unsigned)titleLen,
            (unsigned)msgLen,
            (colorTag != nullptr && colorTag[0] != '\0') ? "yes" : "no");

        do {
            http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
            http.setTimeout(HTTP_READ_TIMEOUT_MS);
            http.setReuse(false);

            if (!http.begin(ntfyClient, url)) {
                Serial_println(F("[NTFY] FAIL phase=http_begin detail=HTTPClient.begin() false (TLS/client prep)"));
                lastCode = 0;
                shouldRetry = (attempt < MAX_RETRIES);
                break;
            }

            static const char *ntfyResponseHeaderKeys[] = {"Retry-After"};
            http.collectHeaders(ntfyResponseHeaderKeys, 1);

            if (NTFY_ACCESS_TOKEN[0] != '\0') {
                Serial_println(F("[NTFY] HTTPS: Bearer auth enabled"));
                char authHeader[160];
                snprintf(authHeader, sizeof(authHeader), "Bearer %s", NTFY_ACCESS_TOKEN);
                http.addHeader(F("Authorization"), authHeader);
            }

            http.addHeader("Title", title);
            http.addHeader("Priority", "high");
            if (colorTag != nullptr && strlen(colorTag) > 0 && strlen(colorTag) <= 64) {
                http.addHeader(F("Tags"), colorTag);
            }

            http.addHeader(F("Connection"), F("close"));

            int code = http.POST(message);
            lastCode = code;
            String err = HTTPClient().errorToString(code);

            if (code == 200 || code == 201) {
                ntfyLastSendAttemptWas429 = false;
                ntfyLastSendAttemptWasRateLimitedByBackoff = false;
                WiFiClient *stream = http.getStreamPtr();
                size_t totalLen = 0;
                if (stream != nullptr) {
                    while (stream->available() && totalLen < (sizeof(httpResponseBuffer) - 1)) {
                        size_t bytesRead = stream->readBytes((uint8_t *)(httpResponseBuffer + totalLen),
                                                             sizeof(httpResponseBuffer) - 1 - totalLen);
                        totalLen += bytesRead;
                    }
                } else {
                    const size_t CHUNK_SIZE = 256;
                    while (http.connected() && totalLen < (sizeof(httpResponseBuffer) - 1)) {
                        size_t remaining = sizeof(httpResponseBuffer) - 1 - totalLen;
                        size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
                        WiFiClient *client = http.getStreamPtr();
                        if (client == nullptr) break;
                        size_t bytesRead = client->readBytes((uint8_t *)(httpResponseBuffer + totalLen), chunkSize);
                        if (bytesRead == 0) {
                            if (!client->available()) break;
                            delay(10);
                            continue;
                        }
                        totalLen += bytesRead;
                    }
                }
                httpResponseBuffer[totalLen] = '\0';

                Serial_printf(F("[NTFY] OK attempt=%u/%u http=%d response_bytes=%u\n"),
                              (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1), code, (unsigned)totalLen);
                ntfyFailStreak = 0;
                ntfyNextAllowedMs = 0;
                attemptOk = true;
                ok = true;
            } else if (code == 429) {
                ntfyLastSendAttemptWas429 = true;
                ntfyLastSendAttemptWasRateLimitedByBackoff = false;
                bool usedRa = false;
                uint32_t waitMs = ntfy429WaitMs(http, &usedRa);
                unsigned long t = millis();
                ntfyNextAllowedMs = t + waitMs;
                Serial_printf(
                    F("[NTFY] FAIL attempt=%u/%u phase=rate_limited http_status=429 wait_ms=%lu source=%s err=%s\n"),
                    (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1), (unsigned long)waitMs,
                    usedRa ? "Retry-After" : "default_60s", err.c_str());
                shouldRetry = false;
            } else {
                const char *phase = (code < 0) ? ntfyPhaseForClientCode(code) : "http_status";
                if (code < 0) {
                    Serial_printf(F("[NTFY] FAIL attempt=%u/%u phase=%s client_code=%d err=%s\n"),
                                  (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1), phase, code, err.c_str());
                } else {
                    Serial_printf(F("[NTFY] FAIL attempt=%u/%u phase=%s http_status=%d err=%s\n"),
                                  (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1), phase, code, err.c_str());
                }
                shouldRetry = (code == HTTPC_ERROR_CONNECTION_REFUSED ||
                               code == HTTPC_ERROR_CONNECTION_LOST ||
                               code == HTTPC_ERROR_READ_TIMEOUT ||
                               code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                               code == HTTPC_ERROR_SEND_PAYLOAD_FAILED ||
                               (code >= 500 && code < 600));
            }
        } while (0);

        WiFiClient *stream = http.getStreamPtr();
        if (stream != nullptr) {
            stream->stop();
        }
        http.end();
        ntfyClient.stop();

        if (attemptOk) {
            if (attempt > 0) {
                Serial_printf(F("[NTFY] success after retry (attempt %u/%u)\n"),
                              (unsigned)(attempt + 1), (unsigned)(MAX_RETRIES + 1));
            }
            break;
        }

        if (lastCode == 429) {
            break;
        }

        if (!attemptOk && attempt >= MAX_RETRIES) {
            if (lastCode != 429) {
                if (ntfyFailStreak < 6) ntfyFailStreak++;
                uint32_t backoffMs = 5000UL << (ntfyFailStreak - 1);
                if (backoffMs > 300000UL) backoffMs = 300000UL;
                ntfyNextAllowedMs = nowMsAtSendStart + backoffMs;
                Serial_printf(F("[NTFY] scheduling backoff %lu ms (streak=%u)\n"),
                              (unsigned long)backoffMs, (unsigned)ntfyFailStreak);
            }
        }
        if (shouldRetry && attempt < MAX_RETRIES) {
            uint32_t backoffDelay = (attempt < sizeof(RETRY_DELAYS) / sizeof(RETRY_DELAYS[0])) ? RETRY_DELAYS[attempt] : 500;
            Serial_printf(F("[NTFY] retry reason=transient_error next_delay_ms=%lu (attempt %u->%u)\n"),
                          (unsigned long)backoffDelay, (unsigned)(attempt + 1), (unsigned)(attempt + 2));
            delay(backoffDelay);
        }
    }

    return ok;
}

static bool sendNtfyNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    unsigned long nowMs = millis();
    // Reset diagnose-vlag per send-actie (anders kan een eerdere 429 blijven hangen).
    ntfyLastSendAttemptWas429 = false;
    ntfyLastSendAttemptWasRateLimitedByBackoff = false;
    // NTFY netwerk-diagnose (rate-limited): alleen buiten exclusive-modus (geen extra DNS tijdens NTFY-slot)
    static unsigned long lastNtfyNetLogMs = 0;
    if (g_netExclusiveNtfyMode == NET_MODE_NORMAL && nowMs - lastNtfyNetLogMs >= 60000UL) {
        lastNtfyNetLogMs = nowMs;
        IPAddress ntfyIp;
        bool resolved = WiFi.hostByName("ntfy.sh", ntfyIp);
        IPAddress dnsIp = WiFi.dnsIP();
        IPAddress gwIp = WiFi.gatewayIP();
        Serial_printf(F("[NTFY][diag] RSSI=%d DNS=%u.%u.%u.%u GW=%u.%u.%u.%u ntfy.sh=%u.%u.%u.%u resolved=%d\n"),
                      WiFi.RSSI(),
                      dnsIp[0], dnsIp[1], dnsIp[2], dnsIp[3],
                      gwIp[0], gwIp[1], gwIp[2], gwIp[3],
                      ntfyIp[0], ntfyIp[1], ntfyIp[2], ntfyIp[3],
                      resolved ? 1 : 0);
        if (!resolved) {
            Serial_println(F("[NTFY][diag] hint: hostByName(ntfy.sh) failed -> DNS/proxy/firewall?"));
        }
    }
    if (ntfyNextAllowedMs != 0 && nowMs < ntfyNextAllowedMs) {
        static unsigned long lastBackoffLogMs = 0;
        if (nowMs - lastBackoffLogMs > 10000) {
            Serial_printf(F("[NTFY] skip send: backoff %lu ms remaining\n"),
                          (unsigned long)(ntfyNextAllowedMs - nowMs));
            lastBackoffLogMs = nowMs;
        }
        ntfyLastSendAttemptWasRateLimitedByBackoff = true;
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial_println(F("[NTFY] abort: WiFi not connected"));
        return false;
    }
    if (strlen(ntfyTopic) == 0) {
        Serial_println(F("[NTFY] abort: topic empty (configure in web UI)"));
        return false;
    }
    if (title == nullptr || message == nullptr) {
        Serial_println(F("[NTFY] abort: null title or message"));
        return false;
    }
    const size_t titleLen = strlen(title);
    const size_t msgLen = strlen(message);
    if (titleLen > 64 || msgLen > 512) {
        Serial_printf(F("[NTFY] abort: title_len=%u or body_len=%u exceeds limit 64/512\n"),
                      (unsigned)titleLen, (unsigned)msgLen);
        return false;
    }
    if (msgLen == 0) {
        Serial_println(F("[NTFY] warn: empty POST body"));
    }

    char url[128];
    int urlLen = snprintf(url, sizeof(url), "https://ntfy.sh/%s", ntfyTopic);
    if (urlLen < 0 || urlLen >= (int)sizeof(url)) {
        Serial_printf(F("[NTFY] abort: URL overflow (snprintf need %d, max %u)\n"),
                      urlLen, (unsigned)(sizeof(url) - 1));
        return false;
    }

    char topicDigest[56];
    ntfyLogTopicDigest(topicDigest, sizeof(topicDigest), ntfyTopic);

    if (g_netExclusiveNtfyMode == NET_MODE_NORMAL) {
        Serial_println(F("[NTFY] send: HTTPS POST (prod; WS state on other lines)"));
    }

    netMutexLock("[NTFY] sendNtfyNotification");
    const bool ok = ntfyHttpsPostNtfyAlertBody(url, urlLen, title, message, colorTag, topicDigest, titleLen, msgLen, nowMs);
    netMutexUnlock("[NTFY] sendNtfyNotification");

    return ok;
}

// WebSocket init (alleen na warm-start, met heap guards)
static void maybeInitWebSocketAfterWarmStart()
{
#if !WS_ENABLED
    Serial.println(F("[WS] Disabled (WS_ENABLED=0)"));
    return;
#else
    if (wsInitialized) {
        return;
    }
    logHeap("WS_INIT_PRE");
#if !WS_LIB_AVAILABLE
    Serial.println(F("[WS] Library ontbreekt (WebSocketsClient.h)"));
    return;
#else
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const uint32_t wsMinFreeHeap = 20000;
    const uint32_t wsMinLargestBlock = 2000;
    if (freeHeap < wsMinFreeHeap || largestBlock < wsMinLargestBlock) {
        Serial_printf(F("[WS] Skip init: low heap (free=%u, largest=%u, minFree=%u, minLargest=%u)\n"),
                      freeHeap, largestBlock, wsMinFreeHeap, wsMinLargestBlock);
        return;
    }
    if (wsClientPtr == nullptr) {
        wsClientPtr = new WebSocketsClient();
        if (wsClientPtr == nullptr) {
            Serial.println(F("[WS] ERROR: alloc failed"));
            return;
        }
    }
    const char* wsHost = "ws.bitvavo.com";
    const uint16_t wsPort = 443;
    const char* wsPath = "/v2/";
    wsClientPtr->onEvent([](WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                Serial.println(F("[WS] Connected"));
                wsConnected = true;
                wsConnecting = false;
                wsConnectedMs = millis();
                // Subscribe op ticker + candles (1m/5m) voor volume/range confirmatie
                if (wsClientPtr != nullptr) {
                    char payloadBuf[256];
                    snprintf(payloadBuf, sizeof(payloadBuf),
                             "{\"action\":\"subscribe\",\"channels\":[{\"name\":\"ticker\",\"markets\":[\"%s\"]},{\"name\":\"candles\",\"interval\":[\"1m\",\"5m\",\"4h\",\"1d\"],\"markets\":[\"%s\"]}]}",
                             bitvavoSymbol,
                             bitvavoSymbol);
                    wsClientPtr->sendTXT(payloadBuf);
                    Serial.println(F("[WS] Subscribe sent"));
                    g_wsSubscribeSentAfterConnect = true;
                }
                lastWsReconnectMs = millis();
                break;
            case WStype_DISCONNECTED:
                Serial.print(F("[WS] Disconnected"));
                Serial_printf(F(" len=%u"), (unsigned)length);
                if (payload != nullptr && length >= 2) {
                    const uint16_t code =
                        (uint16_t)(((uint16_t)payload[0] << 8) | (uint16_t)payload[1]);
                    Serial_printf(F(" close=%u"), (unsigned)code);
                }
                Serial.println();
                wsConnected = false;
                wsConnecting = false;
                lastWsDisconnectMs = millis();
                break;
            case WStype_ERROR:
                Serial.println(F("[WS] Error"));
                wsConnecting = false;
                break;
            case WStype_TEXT: {
                wsMsgCount++;
                if (payload != nullptr && length > 0) {
                    size_t copyLen = (length < (sizeof(wsPendingBuf) - 1)) ? length : (sizeof(wsPendingBuf) - 1);
                    memcpy(wsPendingBuf, payload, copyLen);
                    wsPendingBuf[copyLen] = '\0';
                    wsPendingLen = copyLen;
                    wsPending = true;
                }
                break;
            }
            default:
                break;
        }
    });
    wsClientPtr->setReconnectInterval(5000);
    Serial.printf("[WS] Connecting to wss://%s%s\n", wsHost, wsPath);
    // Regie: beginSSL kan intern een (her)connect start triggeren.
    netMutexLock("[WS] beginSSL");
    wsClientPtr->beginSSL(wsHost, wsPort, wsPath);
    netMutexUnlock("[WS] beginSSL");
    wsConnecting = true;
    wsConnectStartMs = millis();
    wsInitialized = true;
    logHeap("WS_INIT_POST");
#endif
#endif
}

static void processWsTextMessage(const char* wsBuf, size_t length)
{
    if (wsBuf == nullptr || length == 0) {
        return;
    }

    // Candle updates (WS) -> update lastKline1m/5m voor volume UI
    if (strstr(wsBuf, "\"event\":\"candle\"") != nullptr) {
        const char* keyInterval = "\"interval\":\"";
        const char* keyMarket = "\"market\":\"";
        const char* keyCandle = "\"candle\":[";
        const char* posInterval = strstr(wsBuf, keyInterval);
        const char* posMarket = strstr(wsBuf, keyMarket);
        const char* posCandle = strstr(wsBuf, keyCandle);
        if (posInterval && posMarket && posCandle) {
            char intervalBuf[4] = {0};
            char marketBuf[16] = {0};
            posInterval += strlen(keyInterval);
            posMarket += strlen(keyMarket);
            size_t i = 0;
            while (i < sizeof(intervalBuf) - 1 && posInterval[i] && posInterval[i] != '"') {
                intervalBuf[i] = posInterval[i];
                i++;
            }
            intervalBuf[i] = '\0';
            i = 0;
            while (i < sizeof(marketBuf) - 1 && posMarket[i] && posMarket[i] != '"') {
                marketBuf[i] = posMarket[i];
                i++;
            }
            marketBuf[i] = '\0';

            // Parse candle array: [timestamp, open, high, low, close, volume]
            posCandle += strlen(keyCandle);
            char fieldBuf[20];
            uint64_t openTimeMs = 0;
            unsigned long openTime = 0;
            float open = 0.0f, high = 0.0f, low = 0.0f, close = 0.0f, volume = 0.0f;
            bool ok = true;
            for (uint8_t f = 0; f < 6; f++) {
                // Skip whitespace/quotes/brackets
                while (*posCandle == ' ' || *posCandle == '"' || *posCandle == '[') {
                    posCandle++;
                }
                size_t idx = 0;
                while (*posCandle && *posCandle != '"' && *posCandle != ',' && *posCandle != ']'
                       && idx < sizeof(fieldBuf) - 1) {
                    fieldBuf[idx++] = *posCandle++;
                }
                fieldBuf[idx] = '\0';
                while (*posCandle && *posCandle != ',' && *posCandle != ']') posCandle++;
                while (*posCandle == ',' || *posCandle == ']' || *posCandle == ' ' || *posCandle == '[') {
                    posCandle++;
                }
                if (idx == 0) { ok = false; break; }
                if (f == 0) {
                    openTimeMs = strtoull(fieldBuf, nullptr, 10);
                } else if (f == 1) {
                    ok = safeAtof(fieldBuf, open);
                } else if (f == 2) {
                    ok = safeAtof(fieldBuf, high);
                } else if (f == 3) {
                    ok = safeAtof(fieldBuf, low);
                } else if (f == 4) {
                    ok = safeAtof(fieldBuf, close);
                } else if (f == 5) {
                    ok = safeAtof(fieldBuf, volume);
                }
                if (!ok) break;
            }

            if (ok && strcmp(marketBuf, bitvavoSymbol) == 0) {
                if (openTimeMs > 0) {
                    openTime = (unsigned long)(openTimeMs / 1000ULL);
                } else {
                    openTime = 0;
                }
                KlineMetrics kline;
                kline.openTime = openTime;
                if (high <= 0.0f) high = close;
                if (low <= 0.0f) low = close;
                if (high < low) {
                    float tmp = high;
                    high = low;
                    low = tmp;
                }
                kline.high = high;
                kline.low = low;
                kline.close = close;
                kline.volume = volume;
                kline.valid = (openTime > 0 && close > 0.0f);
                if (strcmp(intervalBuf, "1m") == 0) {
                    lastKline1m = kline;
                    wsLastCandle1mMs = millis();
                    if (!wsHasSeenFirstLiveMessage) {
                        wsHasSeenFirstLiveMessage = true;
                        wsLiveSinceMs = wsLastCandle1mMs;
                    }
                } else if (strcmp(intervalBuf, "5m") == 0) {
                    lastKline5m = kline;
                    wsLastCandle5mMs = millis();
                    if (!wsHasSeenFirstLiveMessage) {
                        wsHasSeenFirstLiveMessage = true;
                        wsLiveSinceMs = wsLastCandle5mMs;
                    }
                } else if (strcmp(intervalBuf, "4h") == 0) {
                    wsLastCandle4hMs = millis();
                    if (!wsHasSeenFirstLiveMessage) {
                        wsHasSeenFirstLiveMessage = true;
                        wsLiveSinceMs = wsLastCandle4hMs;
                    }
                } else if (strcmp(intervalBuf, "1d") == 0) {
                    wsLastCandle1dMs = millis();
                    if (!wsHasSeenFirstLiveMessage) {
                        wsHasSeenFirstLiveMessage = true;
                        wsLiveSinceMs = wsLastCandle1dMs;
                    }
                }

                // Rate-limited log om WS candle flow te verifiëren
                unsigned long nowMs = millis();
                if (nowMs - wsLastCandleLogMs >= 60000UL) {
                    wsLastCandleLogMs = nowMs;
                    Serial_printf(F("[WS][Candle] %s close=%.2f vol=%.4f\n"),
                                 intervalBuf, close, volume);
                }

                // Update EMA voor auto-anchor op basis van WS candles (4h/1d)
                extern Alert2HThresholds alert2HThresholds;
                if (strcmp(intervalBuf, "4h") == 0) {
                    uint8_t n = alert2HThresholds.autoAnchor4hCandles;
                    if (!wsEma4hInit || wsEma4hN != n) {
                        wsEma4h.begin(n);
                        wsEma4hInit = true;
                        wsEma4hN = n;
                    }
                    if (wsCandle4h.has && openTime != wsCandle4h.openTime) {
                        wsEma4h.push(wsCandle4h.lastClose);
                        wsAnchorEma4hValid = wsEma4h.isValid();
                        wsAnchorEma4hLive = wsEma4h.ema;
                        wsAutoAnchorTrigger = true;
                    }
                    wsCandle4h.openTime = openTime;
                    wsCandle4h.lastClose = close;
                    wsCandle4h.has = true;
                    if (wsEma4h.isValid()) {
                        wsAnchorEma4hLive = (wsEma4h.alpha * close) + ((1.0f - wsEma4h.alpha) * wsEma4h.ema);
                        wsAnchorEma4hValid = true;
                    }
                } else if (strcmp(intervalBuf, "1d") == 0) {
                    uint8_t n = alert2HThresholds.autoAnchor1dCandles;
                    if (!wsEma1dInit || wsEma1dN != n) {
                        wsEma1d.begin(n);
                        wsEma1dInit = true;
                        wsEma1dN = n;
                    }
                    if (wsCandle1d.has && openTime != wsCandle1d.openTime) {
                        wsEma1d.push(wsCandle1d.lastClose);
                        wsAnchorEma1dValid = wsEma1d.isValid();
                        wsAnchorEma1dLive = wsEma1d.ema;
                        wsAutoAnchorTrigger = true;
                    }
                    wsCandle1d.openTime = openTime;
                    wsCandle1d.lastClose = close;
                    wsCandle1d.has = true;
                    if (wsEma1d.isValid()) {
                        wsAnchorEma1dLive = (wsEma1d.alpha * close) + ((1.0f - wsEma1d.alpha) * wsEma1d.ema);
                        wsAnchorEma1dValid = true;
                    }
                }
            }
        }
    }

    auto parseWsPriceField = [&](const char* key, float& out) -> bool {
        char* pos = strstr(wsBuf, key);
        if (pos == nullptr) return false;
        pos += strlen(key);
        char valBuf[24];
        size_t idx = 0;
        while (idx < sizeof(valBuf) - 1 && pos[idx] != '\0' && pos[idx] != '"') {
            valBuf[idx] = pos[idx];
            idx++;
        }
        valBuf[idx] = '\0';
        float parsed = 0.0f;
        if (!safeAtof(valBuf, parsed) || parsed <= 0.0f) return false;
        out = parsed;
        return true;
    };

    float parsedLast = 0.0f;
    float parsedBid = 0.0f;
    float parsedAsk = 0.0f;
    const bool hasLast = parseWsPriceField("\"lastPrice\":\"", parsedLast);
    const bool hasBid = parseWsPriceField("\"bestBid\":\"", parsedBid);
    const bool hasAsk = parseWsPriceField("\"bestAsk\":\"", parsedAsk);

    const unsigned long wsNowMs = millis();
    bool spreadValidThisTick = false;
    float spreadThisTick = 0.0f;
    if (hasBid || hasAsk) {
        if (hasBid) wsLastBid = parsedBid;
        if (hasAsk) wsLastAsk = parsedAsk;
        if (wsLastBid > 0.0f && wsLastAsk > 0.0f && wsLastAsk >= wsLastBid) {
            wsLastSpread = wsLastAsk - wsLastBid;
            spreadValidThisTick = true;
            spreadThisTick = wsLastSpread;
        }
        wsLastBidAskMs = wsNowMs;
    }

    float chosenPrice = 0.0f;
    if (hasLast) {
        chosenPrice = parsedLast;   // Primair: trade lastPrice
    } else if (hasBid) {
        chosenPrice = parsedBid;    // Fallback: bestBid
    } else if (hasAsk) {
        chosenPrice = parsedAsk;    // Fallback: bestAsk
    }

    if (chosenPrice > 0.0f) {
        wsLastPrice = chosenPrice;
        wsLastPriceMs = wsNowMs;

        const uint32_t currentSecondBucket = (uint32_t)(wsNowMs / 1000UL);
        if (!wsSecondAggCurrent.valid || wsSecondAggCurrent.secondBucket != currentSecondBucket) {
            // Sluit vorige seconde af: freeze volledige snapshot in lastClosed.
            if (wsSecondAggCurrent.valid) {
                wsSecondAggLastClosed = wsSecondAggCurrent;
                wsSecondAggLastClosed.valid = true;
            }
            wsSecondAggCurrent.valid = true;
            wsSecondAggCurrent.secondBucket = currentSecondBucket;
            wsSecondAggCurrent.secondOpen = chosenPrice;
            wsSecondAggCurrent.secondHigh = chosenPrice;
            wsSecondAggCurrent.secondLow = chosenPrice;
            wsSecondAggCurrent.secondClose = chosenPrice;
            wsSecondAggCurrent.secondTickCount = 1;
            wsSecondAggCurrent.secondSpreadLast = spreadValidThisTick ? spreadThisTick : 0.0f;
            wsSecondAggCurrent.secondSpreadMax = spreadValidThisTick ? spreadThisTick : 0.0f;
        } else {
            if (chosenPrice > wsSecondAggCurrent.secondHigh) wsSecondAggCurrent.secondHigh = chosenPrice;
            if (chosenPrice < wsSecondAggCurrent.secondLow) wsSecondAggCurrent.secondLow = chosenPrice;
            wsSecondAggCurrent.secondClose = chosenPrice;
            wsSecondAggCurrent.secondTickCount++;
            if (spreadValidThisTick) {
                wsSecondAggCurrent.secondSpreadLast = spreadThisTick;
                if (spreadThisTick > wsSecondAggCurrent.secondSpreadMax) wsSecondAggCurrent.secondSpreadMax = spreadThisTick;
            }
        }

        // Alleen gedeelde prijsstaat — geen directe buffer-write (sampler schrijft 1 Hz)
        if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(50), "WS latestKnownPrice")) {
            latestKnownPrice = wsLastPrice;
            latestKnownPriceMs = wsLastPriceMs;
            latestKnownPriceSource = LKP_SRC_WS;
            lastFetchedPrice = wsLastPrice;
            safeMutexGive(dataMutex, "WS latestKnownPrice");
        }

        // Markeer "WS live" bij een echte price update.
        if (!wsHasSeenFirstLiveMessage) {
            wsHasSeenFirstLiveMessage = true;
            wsLiveSinceMs = wsNowMs;
        }
    }

    unsigned long now = millis();
    if (now - wsLastLogMs >= 5000) {
        wsLastLogMs = now;
        char snippet[96];
        size_t copyLen = (length < (sizeof(snippet) - 1)) ? length : (sizeof(snippet) - 1);
        memcpy(snippet, wsBuf, copyLen);
        snippet[copyLen] = '\0';
        Serial_printf(F("[WS] Msgs=%lu price=%.2f sample=%s\n"),
                     (unsigned long)wsMsgCount, wsLastPrice, snippet);
    }
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

// Rond prijzen af op hele euro's (0.50 -> omhoog)
static float roundToEuro(float price)
{
    if (price <= 0.0f) {
        return price;
    }
    return (float)((uint32_t)(price + 0.5f));
}

// Zelfde regel als UI: alleen BTC-quote op hele euro; overige markten op centen (REST vs grafiek consistent)
static bool isBtcBaseMarket(const char* symbol)
{
    if (symbol == nullptr) {
        return false;
    }
    const char* dash = strchr(symbol, '-');
    if (dash == nullptr) {
        return false;
    }
    const size_t baseLen = static_cast<size_t>(dash - symbol);
    return baseLen == 3 && strncmp(symbol, "BTC", 3) == 0;
}

static float roundToCent(float price)
{
    if (price <= 0.0f) {
        return price;
    }
    const float scaled = price * 100.0f;
    const float rounded = (price >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f);
    return (float)((int)rounded) / 100.0f;
}

// REST-tickerprijs: align met weergave in de prijsboxen (fetchPrice schrijft prices[0] / latestKnownPrice)
static float roundRestFetchedQuotePrice(float price)
{
    if (isBtcBaseMarket(bitvavoSymbol)) {
        return roundToEuro(price);
    }
    return roundToCent(price);
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

// Deferred actions from other tasks (thread-safe flags)
void requestDisplayRotation(uint8_t rotation)
{
    pendingDisplayRotationValue = rotation;
    pendingDisplayRotationApply = true;
}

void requestMqttReconnect()
{
    pendingMqttReconnect = true;
}

static void applyPendingDisplayRotation()
{
    if (!pendingDisplayRotationApply) {
        return;
    }
    pendingDisplayRotationApply = false;
    if (g_displayBackend == nullptr) {
        return;
    }
    // Wis scherm voor/na rotatie om residu te voorkomen
    g_displayBackend->fillScreen(0 /* RGB565_BLACK */);
    g_displayBackend->setRotation(pendingDisplayRotationValue);
    g_displayBackend->fillScreen(0 /* RGB565_BLACK */);
}

static void applyPendingMqttReconnect()
{
    if (!pendingMqttReconnect) {
        return;
    }
    pendingMqttReconnect = false;
    if (mqttConnected) {
        mqttClient.disconnect();
        mqttConnected = false;
        lastMqttReconnectAttempt = 0;
        mqttReconnectAttemptCount = 0; // Reset counter bij disconnect
    }
}

// Deadlock detection: Track mutex hold times
static unsigned long mutexTakeTime = 0;
static const char* mutexHolderContext = nullptr;
static const unsigned long MAX_MUTEX_HOLD_TIME_MS = 2000; // Max 2 seconden hold time (deadlock threshold)

// Fase 4.1: Geconsolideerde mutex timeout handling
// Helper: Handle mutex timeout with rate-limited logging
// Geoptimaliseerd: elimineert code duplicatie voor mutex timeout handling
static inline const char* safeLogStr(const char* p)
{
    return (p != nullptr) ? p : "?";
}

static void handleMutexTimeout(uint32_t& timeoutCount, const char* context, const char* symbol = nullptr, uint32_t logInterval = 10, uint32_t resetThreshold = 50)
{
    timeoutCount++;
    // Log alleen bij eerste timeout of elke N-de timeout (rate limiting)
    if (timeoutCount == 1 || timeoutCount % logInterval == 0) {
        if (symbol) {
            Serial_printf(F("[%s] WARN -> %s mutex timeout (count: %lu)\n"), safeLogStr(context), safeLogStr(symbol), timeoutCount);
        } else {
            Serial_printf(F("[%s] WARN: mutex timeout (count: %lu)\n"), safeLogStr(context), timeoutCount);
        }
    }
    // Reset counter na te veel timeouts (mogelijk deadlock)
    if (timeoutCount > resetThreshold) {
        if (symbol) {
            Serial_printf(F("[%s] CRIT -> %s mutex timeout te vaak, mogelijk deadlock!\n"), safeLogStr(context), safeLogStr(symbol));
        } else {
            Serial_printf(F("[%s] CRIT: mutex timeout te vaak, mogelijk deadlock!\n"), safeLogStr(context));
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
        Serial_printf(F("[Mutex] ERROR: Attempt to take nullptr mutex in %s\n"), safeLogStr(context));
        return false;
    }
    
    // Check if mutex is already held for too long (potential deadlock)
    if (mutexHolderContext != nullptr && mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf(F("[Mutex] WARNING: Potential deadlock detected! Mutex held for %lu ms by %s\n"), 
                         holdTime, safeLogStr(mutexHolderContext));
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
        Serial_printf(F("[Mutex] ERROR: Attempt to give nullptr mutex in %s\n"), safeLogStr(context));
        return;
    }
    
    // Check if mutex was held for too long (potential deadlock)
    if (mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf(F("[Mutex] WARNING: Mutex held for %lu ms by %s (potential deadlock)\n"), 
                         holdTime, safeLogStr(mutexHolderContext));
        }
    }
    
    BaseType_t result = xSemaphoreGive(mutex);
    if (result != pdTRUE) {
        Serial_printf(F("[Mutex] ERROR: xSemaphoreGive failed in %s (result=%d)\n"), safeLogStr(context), result);
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
        const char* safeTaskName = (taskName != nullptr) ? taskName : "net";
        Serial.printf(F("[NetMutex] WARN: gNetMutex is NULL, HTTP operatie zonder mutex (by %s)\n"), safeTaskName);
        #endif
        return;
    }
    
    // C2: Debug logging met task name en core id (uitgeschakeld voor minder logging)
    // #if !DEBUG_BUTTON_ONLY
    // BaseType_t coreId = xPortGetCoreID();
    // Serial.printf(F("[NetMutex] lock by %s (core %d)\n"), taskName, coreId);
    // #endif
    
    const char* safeTaskName = (taskName != nullptr) ? taskName : "net";
    const bool suppressWsReconnectAcqRel = (safeTaskName != nullptr && strstr(safeTaskName, "reconnect loop") != nullptr);
    // Rate-limited waiting logs om connect-heavy acties te kunnen correleren.
    uint32_t lastWaitLogMs = 0;
    const uint32_t waitLogEveryMs = 2000;
    for (;;) {
        if (xSemaphoreTake(gNetMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            #if !DEBUG_BUTTON_ONLY
            if (!suppressWsReconnectAcqRel) {
                Serial.printf(F("[NET] acquire %s\n"), safeTaskName);
            }
            #endif
            return;
        }
        uint32_t now = millis();
        if (now - lastWaitLogMs >= waitLogEveryMs) {
            lastWaitLogMs = now;
            #if !DEBUG_BUTTON_ONLY
            if (!suppressWsReconnectAcqRel) {
                Serial.printf(F("[NET] waiting for network slot %s\n"), safeTaskName);
            }
            #endif
        }
    }
}

void netMutexUnlock(const char* taskName)
{
    if (gNetMutex == NULL) {
        return;
    }
    
    // C2: Debug logging met task name en core id (uitgeschakeld voor minder logging)
    // #if !DEBUG_BUTTON_ONLY
    // BaseType_t coreId = xPortGetCoreID();
    // Serial.printf(F("[NetMutex] unlock by %s (core %d)\n"), taskName, coreId);
    // #endif
    
    const char* safeTaskName = (taskName != nullptr) ? taskName : "net";
    xSemaphoreGive(gNetMutex);
    #if !DEBUG_BUTTON_ONLY
    const bool suppressWsReconnectRelease = (safeTaskName != nullptr && strstr(safeTaskName, "reconnect loop") != nullptr);
    if (!suppressWsReconnectRelease) {
        Serial.printf(F("[NET] release %s\n"), safeTaskName);
    }
    #endif
}

// C2: Non-blocking net mutex probeer-lock (optioneel gebruikt in WS steady loop)
static bool netMutexTryLock()
{
    if (gNetMutex == NULL) return true;
    return xSemaphoreTake(gNetMutex, 0) == pdTRUE;
}

// Forward declarations
// Fase 6.1: AlertEngine module gebruikt deze functies (extern declarations in AlertEngine.cpp)
void findMinMaxInSecondPrices(float &minVal, float &maxVal);
void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
#if defined(PLATFORM_ESP32S3_JC3248W535)
void findMinMaxInFiveMinutePrices(float &minVal, float &maxVal);
bool uiFiveMinuteHasMinimalData(void);
void findMinMaxInLast24Hours(float &minVal, float &maxVal);
void findMinMaxInLast7Days(float &minVal, float &maxVal);
#endif
#if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
void findMinMaxInLast2Hours(float &minVal, float &maxVal);
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

// --------------------------------------------------------------------------
// NTFY pending queue (noodpatch)
// --------------------------------------------------------------------------
#define NTFY_PENDING_Q_SIZE 8
#define NTFY_TITLE_MAX  65   // sendNtfyNotification() limiet: 64
#define NTFY_BODY_MAX   513  // sendNtfyNotification() limiet: 512
#define NTFY_TAG_MAX    65   // sendNtfyNotification() tags max: 64
#define NTFY_SEQUENCE_MAX 40

enum NtfyPriority : uint8_t {
    NTFY_PRIO_LOW = 0,
    NTFY_PRIO_MEDIUM = 1,
    NTFY_PRIO_HIGH = 2,
};

struct NtfyPendingItem {
    bool used = false;
    bool delivered = false;
    uint8_t priority = (uint8_t)NTFY_PRIO_LOW;
    uint8_t retryCount = 0;
    uint32_t auditId = 0;
    uint32_t createdMs = 0;
    uint32_t lastAttemptMs = 0;
    uint32_t nextAttemptMs = 0;
    char title[NTFY_TITLE_MAX] = {0};
    char body[NTFY_BODY_MAX] = {0};
    char colorTag[NTFY_TAG_MAX] = {0};
    char sequenceId[NTFY_SEQUENCE_MAX] = {0};
};

static NtfyPendingItem s_ntfyQ[NTFY_PENDING_Q_SIZE];
static SemaphoreHandle_t s_ntfyQMutex = NULL;
static uint32_t s_ntfyAuditCounter = 0;

static uint32_t ntfyNextAuditId(void) {
    s_ntfyAuditCounter++;
    if (s_ntfyAuditCounter == 0) {
        s_ntfyAuditCounter = 1;
    }
    return s_ntfyAuditCounter;
}

// Meetpatch: correlatie via ntfyAuditLog(); geen invloed op queue-/retry-beleid.
static void ntfyAuditLog(const __FlashStringHelper* event,
                          uint32_t auditId,
                          const char* sequenceId,
                          uint8_t prio,
                          uint8_t retryCount,
                          uint8_t qsize,
                          uint32_t createdMs,
                          uint32_t queueWaitMs,
                          uint32_t sendDurationMs,
                          uint32_t nextAttemptMs,
                          const char* reason) {
    const char* seq = (sequenceId != nullptr && sequenceId[0] != '\0') ? sequenceId : "-";
    const char* rs = (reason != nullptr && reason[0] != '\0') ? reason : "-";
    Serial.print(F("[NTFY][AUDIT] event="));
    Serial.print(event);
    Serial.printf(
        " audit=%lu seq=%s prio=%u retry=%u qsize=%u created_ms=%lu queue_wait_ms=%lu send_duration_ms=%lu next_attempt_ms=%lu reason=%s\n",
        (unsigned long)auditId, seq, (unsigned)prio, (unsigned)retryCount, (unsigned)qsize,
        (unsigned long)createdMs, (unsigned long)queueWaitMs, (unsigned long)sendDurationMs,
        (unsigned long)nextAttemptMs, rs);
}

static inline bool ntfyBackoffActive(unsigned long nowMs) {
    return (ntfyNextAllowedMs != 0 && nowMs < ntfyNextAllowedMs);
}

static uint32_t ntfyRetryDelayMs(uint8_t retryCount) {
    // poging 1 direct (0), poging 2 500ms, poging 3 2000ms, poging 4 5000ms, daarna 30000ms
    if (retryCount <= 0) return 0;
    if (retryCount == 1) return 500;
    if (retryCount == 2) return 2000;
    if (retryCount == 3) return 5000;
    return 30000UL;
}

static bool ntfyPendingEquals(const NtfyPendingItem& it,
                              const char* title, const char* body, const char* tag) {
    if (!it.used || it.delivered) return false;
    if (title == nullptr || body == nullptr) return false;
    if (strcmp(it.title, title) != 0) return false;
    if (strcmp(it.body, body) != 0) return false;
    const char* t = (tag != nullptr) ? tag : "";
    return strcmp(it.colorTag, t) == 0;
}

static bool ntfyQueueIsEmpty_NoLock() {
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (s_ntfyQ[i].used && !s_ntfyQ[i].delivered) return false;
    }
    return true;
}

static uint8_t ntfyQueuePendingCount_NoLock() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (s_ntfyQ[i].used && !s_ntfyQ[i].delivered) n++;
    }
    return n;
}

static bool ntfyContainsCaseInsensitive(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr) return false;
    const size_t nLen = strlen(needle);
    if (nLen == 0) return false;
    for (const char* p = text; *p; p++) {
        size_t i = 0;
        while (i < nLen) {
            char a = p[i];
            char b = needle[i];
            if (a == '\0') return false;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            i++;
        }
        if (i == nLen) return true;
    }
    return false;
}

static int ntfyDirectionFromBodyLine(const char* body, const char* marker) {
    // returns +1 up, -1 down, 0 unknown
    if (body == nullptr || marker == nullptr) return 0;
    const char* p = strstr(body, marker);
    if (p == nullptr) return 0;
    p += strlen(marker);
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '+') return +1;
    if (*p == '-') return -1;
    return 0;
}

void ntfyBuildSequenceId(const char* title, const char* body, char* outSeq, size_t outSeqSize) {
    if (outSeq == nullptr || outSeqSize == 0) return;
    outSeq[0] = '\0';
    if (title == nullptr) title = "";
    if (body == nullptr) body = "";

    if (ntfyContainsCaseInsensitive(title, "2h") &&
        ntfyContainsCaseInsensitive(title, "breakout")) {
        safeStrncpy(outSeq, "btc-2h-breakout", outSeqSize);
        return;
    }
    if (ntfyContainsCaseInsensitive(title, "2h") &&
        ntfyContainsCaseInsensitive(title, "breakdown")) {
        safeStrncpy(outSeq, "btc-2h-breakdown", outSeqSize);
        return;
    }

    const int d30 = ntfyDirectionFromBodyLine(body, "30m:");
    if (d30 > 0) { safeStrncpy(outSeq, "btc-30m-up", outSeqSize); return; }
    if (d30 < 0) { safeStrncpy(outSeq, "btc-30m-down", outSeqSize); return; }

    const int d5 = ntfyDirectionFromBodyLine(body, "5m:");
    if (d5 > 0) { safeStrncpy(outSeq, "btc-5m-up", outSeqSize); return; }
    if (d5 < 0) { safeStrncpy(outSeq, "btc-5m-down", outSeqSize); return; }

    const int d1 = ntfyDirectionFromBodyLine(body, "1m:");
    if (d1 > 0) { safeStrncpy(outSeq, "btc-1m-up", outSeqSize); return; }
    if (d1 < 0) { safeStrncpy(outSeq, "btc-1m-down", outSeqSize); return; }
}

void alertAuditPriceSnapshot(float* outPrice, const char** outSrcTag, uint32_t* outAgeMs) {
    const unsigned long now = millis();
    float p = latestKnownPrice;
    if (!(p > 0.0f) && lastFetchedPrice > 0.0f) {
        p = lastFetchedPrice;
    }
    if (outPrice != nullptr) {
        *outPrice = p;
    }
    const char* src = "UNKNOWN";
    switch (latestKnownPriceSource) {
        case LKP_SRC_WS:
            src = "WS";
            break;
        case LKP_SRC_REST:
            src = "REST";
            break;
        default:
            if (p > 0.0f || lastFetchedPrice > 0.0f) {
                src = "FALLBACK";
            }
            break;
    }
    if (outSrcTag != nullptr) {
        *outSrcTag = src;
    }
    uint32_t age = 0;
    if (latestKnownPriceMs != 0UL && now >= latestKnownPriceMs) {
        age = (uint32_t)(now - latestKnownPriceMs);
    }
    if (outAgeMs != nullptr) {
        *outAgeMs = age;
    }
}

void alertAuditLog(const char* rule, const char* seqNullable, float price, const char* price_src,
                   uint32_t price_age_ms, const char* metric, float threshold,
                   const char* context1, const char* context2) {
    const char* sq = (seqNullable != nullptr && seqNullable[0] != '\0') ? seqNullable : "-";
    const char* ps = (price_src != nullptr && price_src[0] != '\0') ? price_src : "UNKNOWN";
    const char* m = (metric != nullptr && metric[0] != '\0') ? metric : "-";
    const char* c1 = (context1 != nullptr && context1[0] != '\0') ? context1 : "-";
    const char* c2 = (context2 != nullptr && context2[0] != '\0') ? context2 : "-";
    const char* ru = (rule != nullptr && rule[0] != '\0') ? rule : "-";
    Serial.printf(
        "[ALERT][AUDIT] rule=%s seq=%s price=%.2f price_src=%s price_age_ms=%lu metric=%s threshold=%.4f context1=%s context2=%s reason=emit\n",
        ru, sq, (double)price, ps, (unsigned long)price_age_ms, m, (double)threshold, c1, c2);
}

static int ntfyFindFreeSlot_NoLock() {
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) return (int)i;
    }
    return -1;
}

static int ntfyFindEvictCandidate_NoLock(uint8_t incomingPrio) {
    // Evict laagste prioriteit, oudste createdMs eerst.
    int best = -1;
    uint8_t bestPrio = 255;
    uint32_t bestCreated = 0;
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) continue;
        uint8_t p = s_ntfyQ[i].priority;
        if (p > incomingPrio) continue; // never evict higher prio than incoming
        if (best < 0 || p < bestPrio || (p == bestPrio && s_ntfyQ[i].createdMs < bestCreated)) {
            best = (int)i;
            bestPrio = p;
            bestCreated = s_ntfyQ[i].createdMs;
        }
    }
    return best;
}

static void ntfyWakeApiTaskAfterEnqueueFromEmpty(void) {
    if (s_apiTaskHandle == nullptr) {
        return;
    }
#if DEBUG_NTFY_API_WAKE
    Serial.println(F("[NTFY][Q] wake api_task (enqueue from empty)"));
#endif
    xTaskNotifyGive(s_apiTaskHandle);
}

static bool enqueueNtfyPending(const char* title, const char* body, const char* colorTag, uint8_t priority, const char* sequenceId = nullptr)
{
    if (title == nullptr || body == nullptr) return false;
    const char* tag = (colorTag != nullptr) ? colorTag : "";
    const char* seq = (sequenceId != nullptr) ? sequenceId : "";

    if (s_ntfyQMutex == NULL) {
        // Safety: no queue without mutex.
        return false;
    }
    if (xSemaphoreTake(s_ntfyQMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }

    if (seq[0] != '\0') {
        for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
            if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) continue;
            if (strcmp(s_ntfyQ[i].sequenceId, seq) != 0) continue;
            NtfyPendingItem& ex = s_ntfyQ[i];
            safeStrncpy(ex.title, title, sizeof(ex.title));
            safeStrncpy(ex.body, body, sizeof(ex.body));
            safeStrncpy(ex.colorTag, tag, sizeof(ex.colorTag));
            if (priority > ex.priority) ex.priority = priority;
            ex.nextAttemptMs = 0;
            const uint32_t nowUp = millis();
            const uint32_t qWaitUp = (nowUp >= ex.createdMs) ? (nowUp - ex.createdMs) : 0;
            const uint8_t qsUp = ntfyQueuePendingCount_NoLock();
            const uint32_t snapAudit = ex.auditId;
            const uint8_t snapPrio = ex.priority;
            const uint8_t snapRetry = ex.retryCount;
            const uint32_t snapCreated = ex.createdMs;
            const uint32_t snapNext = ex.nextAttemptMs;
            xSemaphoreGive(s_ntfyQMutex);
            ntfyAuditLog(F("update_pending"), snapAudit, seq, snapPrio, snapRetry, qsUp,
                          snapCreated, qWaitUp, 0, snapNext, "seq_coalesce");
            return true;
        }
    }

    // Coalescing: skip duplicate pending.
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (ntfyPendingEquals(s_ntfyQ[i], title, body, tag)) {
            const NtfyPendingItem& dup = s_ntfyQ[i];
            const uint32_t nowDup = millis();
            const uint32_t qWaitDup = (nowDup >= dup.createdMs) ? (nowDup - dup.createdMs) : 0;
            const uint8_t qsDup = ntfyQueuePendingCount_NoLock();
            const uint32_t dAudit = dup.auditId;
            const uint8_t dPrio = dup.priority;
            const uint8_t dRetry = dup.retryCount;
            const uint32_t dCreated = dup.createdMs;
            const uint32_t dNext = dup.nextAttemptMs;
            char dSeq[NTFY_SEQUENCE_MAX];
            safeStrncpy(dSeq, dup.sequenceId, sizeof(dSeq));
            xSemaphoreGive(s_ntfyQMutex);
            ntfyAuditLog(F("duplicate_skipped"), dAudit,
                          (dSeq[0] != '\0') ? dSeq : nullptr,
                          dPrio, dRetry, qsDup, dCreated, qWaitDup, 0, dNext,
                          "same_title_body_tag");
            return true;
        }
    }

    bool evictedSlot = false;
    uint32_t evictAudit = 0;
    uint8_t evictPrio = 0;
    uint8_t evictRetry = 0;
    uint8_t evictQs = 0;
    uint32_t evictCreated = 0;
    uint32_t evictQWait = 0;
    uint32_t evictNext = 0;
    char evictSeq[NTFY_SEQUENCE_MAX] = {0};

    int slot = ntfyFindFreeSlot_NoLock();
    if (slot < 0) {
        // Queue vol: drop LOW/MEDIUM eerst; HIGH probeert te verdringen.
        slot = ntfyFindEvictCandidate_NoLock(priority);
        if (slot < 0) {
            const uint8_t qsDrop = ntfyQueuePendingCount_NoLock();
            xSemaphoreGive(s_ntfyQMutex);
            ntfyAuditLog(F("drop_queue_full"), 0, (seq[0] != '\0') ? seq : nullptr, priority, 0, qsDrop,
                          0, 0, 0, 0, "no_evict_candidate");
            return false;
        }
        evictedSlot = true;
        {
            const NtfyPendingItem& victim = s_ntfyQ[slot];
            const uint32_t nowEv = millis();
            evictQWait = (nowEv >= victim.createdMs) ? (nowEv - victim.createdMs) : 0;
            evictQs = ntfyQueuePendingCount_NoLock();
            evictAudit = victim.auditId;
            evictPrio = victim.priority;
            evictRetry = victim.retryCount;
            evictCreated = victim.createdMs;
            evictNext = victim.nextAttemptMs;
            safeStrncpy(evictSeq, victim.sequenceId, sizeof(evictSeq));
        }
        // Evict bestaande (laag of gelijk), incoming blijft.
        s_ntfyQ[slot].used = false;
        s_ntfyQ[slot].delivered = false;
    }

    const bool insertFromEmptyQueue = (ntfyQueuePendingCount_NoLock() == 0);

    NtfyPendingItem& it = s_ntfyQ[slot];
    it.used = true;
    it.delivered = false;
    it.priority = priority;
    it.retryCount = 0;
    it.auditId = ntfyNextAuditId();
    it.createdMs = millis();
    it.lastAttemptMs = 0;
    it.nextAttemptMs = 0; // direct eligible
    safeStrncpy(it.title, title, sizeof(it.title));
    safeStrncpy(it.body, body, sizeof(it.body));
    safeStrncpy(it.colorTag, tag, sizeof(it.colorTag));
    safeStrncpy(it.sequenceId, seq, sizeof(it.sequenceId));

    const uint8_t qSize = ntfyQueuePendingCount_NoLock();
    const uint32_t enqAudit = it.auditId;
    const uint32_t enqCreated = it.createdMs;
    xSemaphoreGive(s_ntfyQMutex);
    if (insertFromEmptyQueue) {
        ntfyWakeApiTaskAfterEnqueueFromEmpty();
    }
    if (evictedSlot) {
        ntfyAuditLog(F("evict_for_new_item"), evictAudit,
                      (evictSeq[0] != '\0') ? evictSeq : nullptr,
                      evictPrio, evictRetry, evictQs, evictCreated, evictQWait, 0, evictNext,
                      "incoming_takes_slot");
    }
    ntfyAuditLog(F("enqueue"), enqAudit, (seq[0] != '\0') ? seq : nullptr, priority, 0, qSize,
                  enqCreated, 0, 0, 0, evictedSlot ? "after_evict" : "new_slot");
    return true;
}

static bool ntfyPickNextPending_NoLock(uint8_t& outIdx)
{
    const uint32_t nowMs = millis();
    int best = -1;
    uint8_t bestPrio = 0;
    uint32_t bestCreated = 0;
    for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
        if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) continue;
        if (s_ntfyQ[i].nextAttemptMs != 0 && nowMs < s_ntfyQ[i].nextAttemptMs) continue;
        uint8_t p = s_ntfyQ[i].priority;
        if (best < 0 || p > bestPrio || (p == bestPrio && s_ntfyQ[i].createdMs < bestCreated)) {
            best = (int)i;
            bestPrio = p;
            bestCreated = s_ntfyQ[i].createdMs;
        }
    }
    if (best < 0) return false;
    outIdx = (uint8_t)best;
    return true;
}

static bool ntfyHasFlushablePendingForExclusive(void)
{
    if (s_ntfyQMutex == NULL) return false;
    if (xSemaphoreTake(s_ntfyQMutex, pdMS_TO_TICKS(0)) != pdTRUE) return false;
    const uint32_t nowMs = millis();
    bool has = false;
    if (!ntfyBackoffActive(nowMs)) {
        for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
            if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) continue;
            if (s_ntfyQ[i].nextAttemptMs != 0 && nowMs < s_ntfyQ[i].nextAttemptMs) continue;
            has = true;
            break;
        }
    }
    xSemaphoreGive(s_ntfyQMutex);
    return has;
}

// --- Exclusive NTFY orchestration (apiTask only) — productie-WS rond delivery ---
static bool ntfyExclusiveShouldSkipWsStopPhase(void)
{
#if !WS_ENABLED || !WS_LIB_AVAILABLE
    return true;
#else
    return (!wsInitialized || wsClientPtr == nullptr);
#endif
}

static void wsExclusiveWsPumpOnce(void)
{
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsInitialized && wsClientPtr != nullptr && WiFi.status() == WL_CONNECTED) {
        netMutexLock("[NTFY][EXCL] ws pump");
        wsClientPtr->loop();
        netMutexUnlock("[NTFY][EXCL] ws pump");
    }
#endif
}

/** Alleen RESTARTING_WS: gebundelde pumps zodat WS/TLS binnen exclusive venster kan voltooien (non-blocking, vast aantal iteraties). */
static void wsExclusiveWsPumpRestartBurst(void)
{
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsInitialized && wsClientPtr != nullptr && WiFi.status() == WL_CONNECTED) {
        netMutexLock("[NTFY][EXCL] ws pump burst");
        for (uint8_t i = 0; i < NTFY_EXCL_WS_RESTART_PUMP_BURST; i++) {
            wsClientPtr->loop();
        }
        netMutexUnlock("[NTFY][EXCL] ws pump burst");
    }
#endif
}

static void wsStopForNtfyExclusive(void)
{
    Serial_println(F("[NTFY][EXCL] ws stop requested"));
    wsPauseForNtfySend = true;
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsInitialized && wsClientPtr != nullptr) {
        netMutexLock("[NTFY][EXCL] ws stop disconnect");
        wsClientPtr->disconnect();
        netMutexUnlock("[NTFY][EXCL] ws stop disconnect");
        wsConnected = false;
        wsConnecting = false;
    }
#endif
}

static void restartWebSocketAfterNtfyExclusive(void)
{
#if !WS_ENABLED || !WS_LIB_AVAILABLE
    (void)0;
#else
    g_wsSubscribeSentAfterConnect = false;
    if (!wsInitialized) {
        maybeInitWebSocketAfterWarmStart();
        return;
    }
    if (wsClientPtr == nullptr) {
        return;
    }
    netMutexLock("[NTFY][EXCL] ws restart beginSSL");
    wsClientPtr->setReconnectInterval(5000);
    wsClientPtr->beginSSL(WS_HOST, WS_PORT, WS_PATH);
    netMutexUnlock("[NTFY][EXCL] ws restart beginSSL");
    wsConnecting = true;
#endif
}

// Eén pending alert versturen (alleen vanuit exclusive SEND-state).
// Slot wordt na pick bij index gehouden; na HTTPS wordt dezelfde index geüpdatet (geen createdMs-match alleen — voorkomt verkeerde slot + mutex-timeout die "send fail" logde bij geslaagde POST).
static bool ntfyExclusiveSendOnePendingFromQueue(void)
{
    const uint32_t nowMs = millis();
    if (ntfyBackoffActive(nowMs)) {
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (s_ntfyQMutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_ntfyQMutex, pdMS_TO_TICKS(0)) != pdTRUE) {
        return false;
    }
    uint8_t idx = 0;
    if (!ntfyPickNextPending_NoLock(idx)) {
        xSemaphoreGive(s_ntfyQMutex);
        return false;
    }
    NtfyPendingItem it = s_ntfyQ[idx];
    const uint8_t qszPick = ntfyQueuePendingCount_NoLock();
    xSemaphoreGive(s_ntfyQMutex);

    const uint32_t pickMs = millis();
    const uint32_t qwaitPick = (pickMs >= it.createdMs) ? (pickMs - it.createdMs) : 0;
    ntfyAuditLog(F("flush_start"), it.auditId,
                  (it.sequenceId[0] != '\0') ? it.sequenceId : nullptr,
                  it.priority, it.retryCount, qszPick, it.createdMs, qwaitPick, 0, it.nextAttemptMs,
                  "exclusive_send");

    const unsigned long sendT0 = millis();
    const bool ok = sendNtfyNotification(it.title, it.body, it.colorTag);
    const unsigned long sendT1 = millis();
    const uint32_t sendDur = (sendT1 >= sendT0) ? (uint32_t)(sendT1 - sendT0) : 0;

    if (xSemaphoreTake(s_ntfyQMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        Serial_println(F("[NTFY][Q] WARN: queue lock after send timed out (bookkeeping skipped)"));
        return ok;
    }

    const uint32_t bookMs = millis();
    const uint32_t qwaitBook = (bookMs >= it.createdMs) ? (bookMs - it.createdMs) : 0;

    int found = -1;
    if (idx < NTFY_PENDING_Q_SIZE) {
        NtfyPendingItem& cand = s_ntfyQ[idx];
        if (cand.used && !cand.delivered &&
            strcmp(cand.title, it.title) == 0 && strcmp(cand.body, it.body) == 0 &&
            strcmp(cand.colorTag, it.colorTag) == 0) {
            found = (int)idx;
        }
    }
    if (found < 0) {
        for (uint8_t i = 0; i < NTFY_PENDING_Q_SIZE; i++) {
            if (!s_ntfyQ[i].used || s_ntfyQ[i].delivered) continue;
            if (strcmp(s_ntfyQ[i].title, it.title) != 0) continue;
            if (strcmp(s_ntfyQ[i].body, it.body) != 0) continue;
            if (strcmp(s_ntfyQ[i].colorTag, it.colorTag) != 0) continue;
            found = (int)i;
            break;
        }
    }
    if (found >= 0) {
        NtfyPendingItem& ref = s_ntfyQ[found];
        ref.lastAttemptMs = nowMs;
        const uint8_t qsBook = ntfyQueuePendingCount_NoLock();
        if (ok) {
            ntfyAuditLog(F("send_result"), it.auditId,
                          (it.sequenceId[0] != '\0') ? it.sequenceId : nullptr,
                          it.priority, it.retryCount, qsBook, it.createdMs, qwaitBook, sendDur, 0, "ok");
            ref.delivered = true;
            ref.used = false;
            const uint8_t qSize = ntfyQueuePendingCount_NoLock();
            ntfyAuditLog(F("delivered"), it.auditId,
                          (it.sequenceId[0] != '\0') ? it.sequenceId : nullptr,
                          it.priority, it.retryCount, qSize, it.createdMs, qwaitBook, sendDur, 0,
                          "slot_cleared");
        } else {
            ref.retryCount++;
            uint32_t delayMs = ntfyRetryDelayMs(ref.retryCount);
            uint32_t nextMs = nowMs + delayMs;
            if (ntfyNextAllowedMs != 0 && ntfyNextAllowedMs > nextMs) nextMs = ntfyNextAllowedMs;
            ref.nextAttemptMs = nextMs;
            ntfyAuditLog(F("send_result"), it.auditId,
                          (it.sequenceId[0] != '\0') ? it.sequenceId : nullptr,
                          it.priority, ref.retryCount, qsBook, it.createdMs, qwaitBook, sendDur, ref.nextAttemptMs,
                          "fail");
            const uint8_t qsRetry = ntfyQueuePendingCount_NoLock();
            ntfyAuditLog(F("retry_scheduled"), it.auditId,
                          (it.sequenceId[0] != '\0') ? it.sequenceId : nullptr,
                          it.priority, ref.retryCount, qsRetry, it.createdMs, qwaitBook, sendDur, ref.nextAttemptMs,
                          "queue_backoff");
        }
    } else if (ok) {
        Serial_println(F("[NTFY][Q] WARN: send ok but no matching queue slot (duplicate delivery risk)"));
    }
    xSemaphoreGive(s_ntfyQMutex);
    return ok;
}

static bool s_ntfyExclusiveSendDoneThisCycle = false;
static bool s_ntfyExclusiveRestartBegun = false;
static uint8_t s_ntfyExclWsRestartRetriesUsed = 0;
static unsigned long s_ntfyExclRsWaitLogMs = 0;

// Productie: exclusive NTFY-sequencing in apiTask. Prefix [NTFY][EXCL] — los van [WS] init/reconnect in loop/maybeInit.
static void apiTaskNtfyExclusiveStateMachine(void)
{
    const unsigned long now = millis();

    switch (g_netExclusiveNtfyMode) {
        case NET_MODE_NTFY_EXCLUSIVE_STOPPING_WS: {
            wsExclusiveWsPumpOnce();
            if (!wsConnected && !wsConnecting) {
                Serial_println(F("[NTFY][EXCL] ws stopped"));
                g_netExclusiveNtfyMode = NET_MODE_NTFY_EXCLUSIVE_SENDING;
                s_netExclusiveDeadlineMs = now + NTFY_EXCL_SEND_MS;
                s_ntfyExclusiveSendDoneThisCycle = false;
            } else if (now > s_netExclusiveDeadlineMs) {
                Serial_printf(F("[NTFY][EXCL] timeout in state STOPPING_WS\n"));
                g_netExclusiveNtfyMode = NET_MODE_NTFY_EXCLUSIVE_SENDING;
                s_netExclusiveDeadlineMs = now + NTFY_EXCL_SEND_MS;
                s_ntfyExclusiveSendDoneThisCycle = false;
            }
            break;
        }
        case NET_MODE_NTFY_EXCLUSIVE_SENDING: {
            if (!s_ntfyExclusiveSendDoneThisCycle) {
                s_ntfyExclusiveSendDoneThisCycle = true;
                Serial_println(F("[NTFY][EXCL] send start"));
                const bool ok = ntfyExclusiveSendOnePendingFromQueue();
                if (ok) {
                    Serial_println(F("[NTFY][EXCL] send ok"));
                } else {
                    Serial_println(F("[NTFY][EXCL] send fail"));
                }
                g_netExclusiveNtfyMode = NET_MODE_NTFY_EXCLUSIVE_RESTARTING_WS;
                s_netExclusiveDeadlineMs = now + NTFY_EXCL_WS_RESTART_MS;
                s_ntfyExclusiveRestartBegun = false;
                s_ntfyExclWsRestartRetriesUsed = 0;
                s_ntfyExclRsWaitLogMs = now;
            }
            break;
        }
        case NET_MODE_NTFY_EXCLUSIVE_RESTARTING_WS: {
            if (!s_ntfyExclusiveRestartBegun) {
                s_ntfyExclusiveRestartBegun = true;
                Serial_println(F("[NTFY][EXCL] ws restart requested"));
                restartWebSocketAfterNtfyExclusive();
                Serial_println(F("[NTFY][EXCL] ws restart waiting"));
            }
            wsExclusiveWsPumpRestartBurst();
            if (!(wsConnected && g_wsSubscribeSentAfterConnect) &&
                (now - s_ntfyExclRsWaitLogMs >= NTFY_EXCL_WS_RESTART_WAIT_LOG_MS)) {
                s_ntfyExclRsWaitLogMs = now;
                Serial_println(F("[NTFY][EXCL] ws restart still waiting"));
            }
            if (wsConnected && g_wsSubscribeSentAfterConnect) {
                Serial_println(F("[NTFY][EXCL] ws restored"));
                wsPauseForNtfySend = false;
                g_netExclusiveNtfyMode = NET_MODE_NORMAL;
                Serial_println(F("[NTFY][EXCL] exit"));
                s_ntfyExclusiveRestartBegun = false;
                s_ntfyExclWsRestartRetriesUsed = 0;
                s_ntfyExclRsWaitLogMs = 0;
            } else if (now > s_netExclusiveDeadlineMs) {
                if (s_ntfyExclWsRestartRetriesUsed < NTFY_EXCL_WS_RESTART_MAX_RETRIES) {
                    s_ntfyExclWsRestartRetriesUsed++;
                    Serial_println(F("[NTFY][EXCL] ws restart retry"));
#if WS_ENABLED && WS_LIB_AVAILABLE
                    if (wsClientPtr != nullptr) {
                        netMutexLock("[NTFY][EXCL] ws retry disconnect");
                        wsClientPtr->disconnect();
                        netMutexUnlock("[NTFY][EXCL] ws retry disconnect");
                    }
                    wsConnected = false;
                    wsConnecting = false;
#endif
                    g_wsSubscribeSentAfterConnect = false;
                    s_ntfyExclusiveRestartBegun = false;
                    s_netExclusiveDeadlineMs = now + NTFY_EXCL_WS_RESTART_MS;
                    s_ntfyExclRsWaitLogMs = now;
                } else {
                    Serial_println(F("[NTFY][EXCL] timeout in state RESTARTING_WS (abandon)"));
#if WS_ENABLED && WS_LIB_AVAILABLE
                    if (wsClientPtr != nullptr) {
                        netMutexLock("[NTFY][EXCL] abandon disconnect");
                        wsClientPtr->disconnect();
                        netMutexUnlock("[NTFY][EXCL] abandon disconnect");
                    }
                    wsConnected = false;
                    wsConnecting = false;
#endif
                    g_wsSubscribeSentAfterConnect = false;
                    wsPauseForNtfySend = false;
                    g_netExclusiveNtfyMode = NET_MODE_NORMAL;
#if WS_ENABLED && WS_LIB_AVAILABLE
                    if (wsInitialized && wsClientPtr != nullptr) {
                        restartWebSocketAfterNtfyExclusive();
                    }
#endif
                    Serial_println(F("[NTFY][EXCL] exit"));
                    s_ntfyExclusiveRestartBegun = false;
                    s_ntfyExclWsRestartRetriesUsed = 0;
                    s_ntfyExclRsWaitLogMs = 0;
                }
            }
            break;
        }
        default:
            break;
    }
}

// Best-effort notification log (ringbuffer, fixed size, try-lock only in writer)
#define NOTIF_LOG_TITLE_MAX   48
#define NOTIF_LOG_MSG_MAX    160
#define NOTIF_LOG_TAG_MAX     24
#define NOTIF_LOG_SIZE        64

struct NotificationLogEntry {
    uint32_t timeMs;
    uint8_t  sent;   // 0 = send failed, 1 = send success
    char title[NOTIF_LOG_TITLE_MAX];
    char message[NOTIF_LOG_MSG_MAX];
    char colorTag[NOTIF_LOG_TAG_MAX];
};

static NotificationLogEntry s_notificationLog[NOTIF_LOG_SIZE];
static uint8_t s_notificationLogHead   = 0;
static uint8_t s_notificationLogCount  = 0;
static SemaphoreHandle_t s_notifLogMutex = NULL;

// Best-effort, strictly non-blocking writer. Try-lock only; if mutex NULL of lock faalt, skip append.
static void appendNotificationLog(const char *title, const char *message, const char *colorTag, uint8_t sent) {
    if (s_notifLogMutex == NULL) return;  // Safety: no unsafe write without mutex
    if (xSemaphoreTake(s_notifLogMutex, 0) != pdTRUE) return;  // Try-lock only; never wait
    uint8_t idx = s_notificationLogHead % NOTIF_LOG_SIZE;
    s_notificationLog[idx].timeMs = millis();
    s_notificationLog[idx].sent   = sent;
    safeStrncpy(s_notificationLog[idx].title,   title   ? title   : "", sizeof(s_notificationLog[idx].title));
    safeStrncpy(s_notificationLog[idx].message, message ? message : "", sizeof(s_notificationLog[idx].message));
    safeStrncpy(s_notificationLog[idx].colorTag, colorTag ? colorTag : "", sizeof(s_notificationLog[idx].colorTag));
    s_notificationLogHead++;
    if (s_notificationLogCount < NOTIF_LOG_SIZE) s_notificationLogCount++;
    xSemaphoreGive(s_notifLogMutex);
}

// Read helpers voor Web UI (mag kort blokkeren met timeout)
uint8_t getNotificationLogCount(void) {
    if (s_notifLogMutex != NULL && xSemaphoreTake(s_notifLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        uint8_t n = s_notificationLogCount;
        xSemaphoreGive(s_notifLogMutex);
        return n;
    }
    return s_notificationLogCount;  // best-effort when mutex busy or NULL
}

bool getNotificationLogEntry(uint8_t index,
                             char *titleOut, size_t titleSize,
                             char *messageOut, size_t messageSize,
                             char *colorTagOut, size_t colorTagSize,
                             uint32_t *timeMsOut,
                             uint8_t *sentOut) {
    if (titleOut == nullptr || messageOut == nullptr) return false;
    if (s_notifLogMutex != NULL && xSemaphoreTake(s_notifLogMutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return false;  // best-effort: skip read if mutex busy
    if (index >= s_notificationLogCount) {
        if (s_notifLogMutex != NULL) xSemaphoreGive(s_notifLogMutex);
        return false;
    }
    // Newest first: index 0 = meest recente entry
    uint8_t pos = (s_notificationLogHead + NOTIF_LOG_SIZE - 1 - index) % NOTIF_LOG_SIZE;
    const NotificationLogEntry *e = &s_notificationLog[pos];
    safeStrncpy(titleOut,   e->title,   titleSize);
    safeStrncpy(messageOut, e->message, messageSize);
    if (colorTagOut && colorTagSize) safeStrncpy(colorTagOut, e->colorTag, colorTagSize);
    if (timeMsOut) *timeMsOut = e->timeMs;
    if (sentOut) *sentOut = e->sent;
    if (s_notifLogMutex != NULL) xSemaphoreGive(s_notifLogMutex);
    return true;
}

// Send notification via NTFY — productie-ingang: alleen enqueue (delivery = exclusive pad in apiTask).
// Fase 5.1: static verwijderd zodat TrendDetector module deze functie kan aanroepen (later verplaatst naar AlertEngine)
// Phase 1: logging gebeurt NA sendNtfyNotification() en is best-effort
bool sendNotification(const char *title, const char *message, const char *colorTag = nullptr)
{
    // Alertpaden blokkeren niet op HTTPS; apiTask leegt de queue via exclusive flow.
    char seqId[NTFY_SEQUENCE_MAX] = {0};
    ntfyBuildSequenceId(title, message, seqId, sizeof(seqId));
    const bool accepted = enqueueNtfyPending(
        title, message, colorTag, NTFY_PRIO_HIGH,
        (seqId[0] != '\0') ? seqId : nullptr
    );
    appendNotificationLog(title, message, colorTag, accepted ? 1u : 0u);
    return accepted;
}

// ============================================================================
// NTFY: gedeelde tijd-string helpers + diagnostische tests (CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME)
// ============================================================================
static bool formatLocalTimeHHMMSS(char *out, size_t outSz)
{
    if (out == nullptr || outSz < 9) return false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        // Geen geldige tijd: fallback zodat we toch een vaste string kunnen loggen
        strncpy(out, "--:--:--", outSz - 1);
        out[outSz - 1] = '\0';
        return false;
    }
    strftime(out, outSz, "%H:%M:%S", &timeinfo);
    return true;
}

static bool waitLocalTimeHHMMSS(char *out, size_t outSz, uint32_t timeoutMs)
{
    const uint32_t start = millis();
    for (;;) {
        if (formatLocalTimeHHMMSS(out, outSz) && out[0] != '-') {
            return true;
        }
        if (millis() - start >= timeoutMs) {
            return false;
        }
        delay(250);
    }
}

static bool isNetMutexBusy()
{
    if (gNetMutex == nullptr) return false;
    if (xSemaphoreTake(gNetMutex, 0) == pdTRUE) {
        xSemaphoreGive(gNetMutex);
        return false;
    }
    return true;
}

static void ntfyStartupTestIfEnabled(void)
{
#if CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME && CRYPTO_ALERT_NTFY_STARTUP_TEST
    if (ntfyStartupTestSent) return;

    if (WiFi.status() != WL_CONNECTED) {
        Serial_println(F("[NTFY][test] startup test skipped: WiFi not connected"));
        return;
    }

    const unsigned long now = millis();
    if (ntfyBackoffActive(now)) {
        Serial_println(F("[NTFY][test] skip startup: backoff"));
        ntfyStartupTestDeferredPending = true;
        ntfyStartupTestDeferredRetryAttempted = false;
        ntfyStartupTestDeferredNextMs = ntfyNextAllowedMs;
        if (ntfyStartupTestDeferredNextMs == 0) {
            ntfyStartupTestDeferredNextMs = now + 60000UL;
        }
        return;
    }

    char hhmmss[10] = {0};
    (void)waitLocalTimeHHMMSS(hhmmss, sizeof(hhmmss), 15000UL);

    char title[96];
    char body[96];
    snprintf(title, sizeof(title), "Reboot - %s", VERSION_STRING);
    snprintf(body, sizeof(body), "[%s] Setup compleet", hhmmss);

    Serial_println(F("[NTFY][test] startup test queued"));

    const char *tag = "☑️♻️";
    const bool accepted = enqueueNtfyPending(title, body, tag, NTFY_PRIO_LOW);
    if (accepted) {
        ntfyStartupTestSent = true;
        Serial_println(F("[NTFY][test] startup test accepted"));
    } else {
        Serial_println(F("[NTFY][test] startup test enqueue failed"));
    }
#else
    (void)0;
#endif
}

// Diagnostiek: eenmalige extra NTFY na eerste WS-live (alleen als DIAGNOSTICS_RUNTIME + STARTUP_TEST).
static void ntfyWsLiveHealthPingIfDue(void)
{
#if CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME && CRYPTO_ALERT_NTFY_STARTUP_TEST
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (!wsHasSeenFirstLiveMessage || wsLiveNtfyTestSent) return;
    if (wsLiveSinceMs == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    const unsigned long now = millis();
    if (ntfyBackoffActive(now)) {
        Serial_println(F("[NTFY][test] skip ws-live ping: backoff"));
        return;
    }
    if ((now - wsLiveSinceMs) < WS_LIVE_NTFY_HEALTH_PING_DELAY_MS) return;

    wsLiveNtfyTestSent = true; // 1x per boot

    Serial.println(F("[NTFY][test] ws-live ping queued"));
    const char* tag = "☑️";
    const char* text = "WS live health ping";
    const bool accepted = enqueueNtfyPending(text, text, tag, NTFY_PRIO_LOW);
    if (accepted) {
        Serial.println(F("[NTFY][test] ws-live ping enqueue ok"));
    } else {
        Serial.println(F("[NTFY][test] ws-live ping enqueue failed"));
    }
#else
    (void)0;
#endif
#else
    (void)0;
#endif
}

static void ntfyPeriodicTestIfEnabled(void)
{
#if CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME && CRYPTO_ALERT_NTFY_PERIODIC_TEST
    if (WiFi.status() != WL_CONNECTED) return;

    const unsigned long now = millis();
    if (ntfyPeriodicTestNextMs == 0) {
        ntfyPeriodicTestNextMs = now + CRYPTO_ALERT_NTFY_PERIODIC_TEST_MS;
        return;
    }
    if (now < ntfyPeriodicTestNextMs) return;

    Serial_println(F("[NTFY][test] periodic test due"));

    if (ntfyBackoffActive(now)) {
        Serial_println(F("[NTFY][test] skip periodic: backoff"));
        ntfyPeriodicTestNextMs = ntfyNextAllowedMs;
        return;
    }

    char hhmmss[10] = {0};
    (void)formatLocalTimeHHMMSS(hhmmss, sizeof(hhmmss));

    char text[96];
    snprintf(text, sizeof(text), "[%s] Controle NTFY", hhmmss);

    const char *tag = "☑️";
    const bool accepted = enqueueNtfyPending(text, text, tag, NTFY_PRIO_LOW);
    if (accepted) {
        Serial_println(F("[NTFY][test] periodic test accepted"));
    } else {
        Serial_println(F("[NTFY][test] periodic test enqueue failed"));
    }

    unsigned long desiredNext = now + CRYPTO_ALERT_NTFY_PERIODIC_TEST_MS;
    if (ntfyNextAllowedMs != 0 && ntfyNextAllowedMs > desiredNext) {
        desiredNext = ntfyNextAllowedMs;
    }
    ntfyPeriodicTestNextMs = desiredNext;
#else
    (void)0;
#endif
}

// Diagnostiek: deferred retry voor startup-NTFY bij rate-limit (429) — zelfde master-vlag als startup test.
static void ntfyDeferredStartupTestIfPending(void)
{
#if CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME && CRYPTO_ALERT_NTFY_STARTUP_TEST
    if (!ntfyStartupTestDeferredPending) return;
    if (ntfyStartupTestDeferredRetryAttempted) return;
    const unsigned long now = millis();

    unsigned long dueMs = ntfyStartupTestDeferredNextMs;
    if (ntfyNextAllowedMs != 0 && ntfyNextAllowedMs > dueMs) dueMs = ntfyNextAllowedMs;
    if (now < dueMs) return;
    if (ntfyBackoffActive(now)) {
        Serial_println(F("[NTFY][test] skip deferred retry: backoff"));
        ntfyStartupTestDeferredNextMs = ntfyNextAllowedMs;
        return;
    }

    Serial_println(F("[NTFY][test] deferred startup retry due"));

    char hhmmss[10] = {0};
    (void)formatLocalTimeHHMMSS(hhmmss, sizeof(hhmmss));

    char title[96];
    char body[96];
    snprintf(title, sizeof(title), "Reboot - %s", VERSION_STRING);
    snprintf(body, sizeof(body), "[%s] Setup compleet", hhmmss);

    const char *tag = "☑️♻️";
    const bool accepted = enqueueNtfyPending(title, body, tag, NTFY_PRIO_LOW);
    ntfyStartupTestDeferredRetryAttempted = true;
    ntfyStartupTestDeferredPending = false;
    if (accepted) {
        ntfyStartupTestSent = true;
        Serial_println(F("[NTFY][test] deferred startup test accepted"));
    } else {
        Serial_println(F("[NTFY][test] deferred startup test enqueue failed"));
    }
#else
    (void)0;
#endif
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
    float anchorRounded = roundToEuro(anchor_price);
    
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
             "{\"time\":\"%s\",\"price\":%.0f,\"event\":\"%s\"}",
             timeStr, anchorRounded, event_type);
    
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char topic[128];
    // Gebruik dynamische MQTT prefix (gebaseerd op NTFY topic)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    snprintf(topic, sizeof(topic), "%s/anchor/event", mqttPrefix);
    
    // Try direct publish, queue if failed
    if (mqttConnected && mqttClient.publish(topic, payload, false)) {
        Serial_printf(F("[MQTT] Anchor event gepubliceerd: %s (prijs: %.0f, event: %s)\n"),
                     timeStr, anchorRounded, event_type);
    } else {
        // Queue message if not connected or publish failed
        enqueueMqttMessage(topic, payload, false);
        Serial_printf(F("[MQTT] Anchor event in queue: %s (prijs: %.0f, event: %s)\n"),
                     timeStr, anchorRounded, event_type);
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
    safeStrncpy(bitvavoSymbol, settings.bitvavoSymbol, sizeof(bitvavoSymbol));
    // Update symbols array with the loaded bitvavo symbol
    safeStrncpy(symbol0, bitvavoSymbol, sizeof(symbol0));
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
    nightModeEnabled = settings.nightModeEnabled;
    nightModeStartHour = settings.nightModeStartHour;
    nightModeEndHour = settings.nightModeEndHour;
    nightSpike5mThreshold = settings.nightSpike5mThreshold;
    nightMove5mAlertThreshold = settings.nightMove5mAlertThreshold;
    nightMove30mThreshold = settings.nightMove30mThreshold;
    nightCooldown5mSec = settings.nightCooldown5mSec;
    nightAutoVolMinMultiplier = settings.nightAutoVolMinMultiplier;
    nightAutoVolMaxMultiplier = settings.nightAutoVolMaxMultiplier;
    
    // Copy Warm-Start settings
    warmStartEnabled = settings.warmStartEnabled;
    warmStart1mExtraCandles = settings.warmStart1mExtraCandles;
    warmStart5mCandles = settings.warmStart5mCandles;
    warmStart30mCandles = settings.warmStart30mCandles;
    warmStart2hCandles = settings.warmStart2hCandles;
    warmStartSkip1m = settings.warmStartSkip1m;
    warmStartSkip5m = settings.warmStartSkip5m;
    
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

    // Regime-engine (Fase A)
    regimeEngineEnabled = settings.regimeEngineEnabled;
    regimeMinDwellSec = settings.regimeMinDwellSec;
    regimeEnergeticEnter = settings.regimeEnergeticEnter;
    regimeEnergeticExit = settings.regimeEnergeticExit;
    regimeSlapEnter = settings.regimeSlapEnter;
    regimeSlapExit = settings.regimeSlapExit;
    regimeLoadedFloor = settings.regimeLoadedFloor;
    regimeLoadedDrop = settings.regimeLoadedDrop;
    regimeDirDeadband1mPct = settings.regimeDirDeadband1mPct;
    regimeDirDeadband5mPct = settings.regimeDirDeadband5mPct;
    regimeDirDeadband30mPct = settings.regimeDirDeadband30mPct;
    regimeDirDeadband2hPct = settings.regimeDirDeadband2hPct;
    regime2hCompressMinPct = settings.regime2hCompressMinPct;
    regime2hCompressMaxPct = settings.regime2hCompressMaxPct;

    regimeSlapSpike1mMult = settings.regimeSlapSpike1mMult;
    regimeSlapMove5mAlertMult = settings.regimeSlapMove5mAlertMult;
    regimeSlapMove30mMult = settings.regimeSlapMove30mMult;
    regimeSlapCooldown1mMult = settings.regimeSlapCooldown1mMult;
    regimeSlapCooldown5mMult = settings.regimeSlapCooldown5mMult;
    regimeSlapCooldown30mMult = settings.regimeSlapCooldown30mMult;
    regimeGeladenSpike1mMult = settings.regimeGeladenSpike1mMult;
    regimeGeladenMove5mAlertMult = settings.regimeGeladenMove5mAlertMult;
    regimeGeladenMove30mMult = settings.regimeGeladenMove30mMult;
    regimeGeladenCooldown1mMult = settings.regimeGeladenCooldown1mMult;
    regimeGeladenCooldown5mMult = settings.regimeGeladenCooldown5mMult;
    regimeGeladenCooldown30mMult = settings.regimeGeladenCooldown30mMult;
    regimeEnergiekSpike1mMult = settings.regimeEnergiekSpike1mMult;
    regimeEnergiekMove5mAlertMult = settings.regimeEnergiekMove5mAlertMult;
    regimeEnergiekMove30mMult = settings.regimeEnergiekMove30mMult;
    regimeEnergiekCooldown1mMult = settings.regimeEnergiekCooldown1mMult;
    regimeEnergiekCooldown5mMult = settings.regimeEnergiekCooldown5mMult;
    regimeEnergiekCooldown30mMult = settings.regimeEnergiekCooldown30mMult;
    regimeEnergiekAllowStandalone1mBurst = settings.regimeEnergiekAllowStandalone1mBurst;
    regimeEnergiekStandalone1mFactor = settings.regimeEnergiekStandalone1mFactor;
    regimeEnergiekMinDirectionStrength = settings.regimeEnergiekMinDirectionStrength;
    
    Serial_printf(F("[Settings] Loaded: topic=%s, symbol=%s, 1min trend=%.2f/%.2f%%/min, 30min trend=%.2f/%.2f%%/uur, cooldown=%lu/%lu ms\n"),
                  ntfyTopic, bitvavoSymbol, threshold1MinUp, threshold1MinDown, threshold30MinUp, threshold30MinDown,
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
    safeStrncpy(settings.bitvavoSymbol, bitvavoSymbol, sizeof(settings.bitvavoSymbol));
    settings.language = language;
    settings.displayRotation = displayRotation;
    
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
    settings.nightModeEnabled = nightModeEnabled;
    settings.nightModeStartHour = nightModeStartHour;
    settings.nightModeEndHour = nightModeEndHour;
    settings.nightSpike5mThreshold = nightSpike5mThreshold;
    settings.nightMove5mAlertThreshold = nightMove5mAlertThreshold;
    settings.nightMove30mThreshold = nightMove30mThreshold;
    settings.nightCooldown5mSec = nightCooldown5mSec;
    settings.nightAutoVolMinMultiplier = nightAutoVolMinMultiplier;
    settings.nightAutoVolMaxMultiplier = nightAutoVolMaxMultiplier;
    
    // Copy Warm-Start settings
    settings.warmStartEnabled = warmStartEnabled;
    settings.warmStart1mExtraCandles = warmStart1mExtraCandles;
    settings.warmStart5mCandles = warmStart5mCandles;
    settings.warmStart30mCandles = warmStart30mCandles;
    settings.warmStart2hCandles = warmStart2hCandles;
    settings.warmStartSkip1m = warmStartSkip1m;
    settings.warmStartSkip5m = warmStartSkip5m;
    
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

    // Regime-engine (Fase A)
    settings.regimeEngineEnabled = regimeEngineEnabled;
    settings.regimeMinDwellSec = regimeMinDwellSec;
    settings.regimeEnergeticEnter = regimeEnergeticEnter;
    settings.regimeEnergeticExit = regimeEnergeticExit;
    settings.regimeSlapEnter = regimeSlapEnter;
    settings.regimeSlapExit = regimeSlapExit;
    settings.regimeLoadedFloor = regimeLoadedFloor;
    settings.regimeLoadedDrop = regimeLoadedDrop;
    settings.regimeDirDeadband1mPct = regimeDirDeadband1mPct;
    settings.regimeDirDeadband5mPct = regimeDirDeadband5mPct;
    settings.regimeDirDeadband30mPct = regimeDirDeadband30mPct;
    settings.regimeDirDeadband2hPct = regimeDirDeadband2hPct;
    settings.regime2hCompressMinPct = regime2hCompressMinPct;
    settings.regime2hCompressMaxPct = regime2hCompressMaxPct;

    settings.regimeSlapSpike1mMult = regimeSlapSpike1mMult;
    settings.regimeSlapMove5mAlertMult = regimeSlapMove5mAlertMult;
    settings.regimeSlapMove30mMult = regimeSlapMove30mMult;
    settings.regimeSlapCooldown1mMult = regimeSlapCooldown1mMult;
    settings.regimeSlapCooldown5mMult = regimeSlapCooldown5mMult;
    settings.regimeSlapCooldown30mMult = regimeSlapCooldown30mMult;
    settings.regimeGeladenSpike1mMult = regimeGeladenSpike1mMult;
    settings.regimeGeladenMove5mAlertMult = regimeGeladenMove5mAlertMult;
    settings.regimeGeladenMove30mMult = regimeGeladenMove30mMult;
    settings.regimeGeladenCooldown1mMult = regimeGeladenCooldown1mMult;
    settings.regimeGeladenCooldown5mMult = regimeGeladenCooldown5mMult;
    settings.regimeGeladenCooldown30mMult = regimeGeladenCooldown30mMult;
    settings.regimeEnergiekSpike1mMult = regimeEnergiekSpike1mMult;
    settings.regimeEnergiekMove5mAlertMult = regimeEnergiekMove5mAlertMult;
    settings.regimeEnergiekMove30mMult = regimeEnergiekMove30mMult;
    settings.regimeEnergiekCooldown1mMult = regimeEnergiekCooldown1mMult;
    settings.regimeEnergiekCooldown5mMult = regimeEnergiekCooldown5mMult;
    settings.regimeEnergiekCooldown30mMult = regimeEnergiekCooldown30mMult;
    settings.regimeEnergiekAllowStandalone1mBurst = regimeEnergiekAllowStandalone1mBurst;
    settings.regimeEnergiekStandalone1mFactor = regimeEnergiekStandalone1mFactor;
    settings.regimeEnergiekMinDirectionStrength = regimeEnergiekMinDirectionStrength;
    
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
        {"/config/nightSpike5m/set", true, 0.01f, 10.0f, &nightSpike5mThreshold, "/config/nightSpike5m"},
        {"/config/nightMove5m/set", true, 0.01f, 10.0f, &nightMove5mAlertThreshold, "/config/nightMove5m"},
        {"/config/nightMove30m/set", true, 0.01f, 20.0f, &nightMove30mThreshold, "/config/nightMove30m"},
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
        {"/config/autoVolMax/set", true, 1.0f, 3.0f, &autoVolatilityMaxMultiplier, "/config/autoVolMax"},
        {"/config/nightAvMin/set", true, 0.1f, 3.0f, &nightAutoVolMinMultiplier, "/config/nightAvMin"},
        {"/config/nightAvMax/set", true, 0.1f, 5.0f, &nightAutoVolMaxMultiplier, "/config/nightAvMax"}
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
        {"/config/ws2h/set", &warmStart2hCandles, 2, 200, "/config/ws2h"},
        {"/config/nightStartHour/set", &nightModeStartHour, 0, 23, "/config/nightStartHour"},
        {"/config/nightEndHour/set", &nightModeEndHour, 0, 23, "/config/nightEndHour"}
    };
    
    // Boolean settings (switch entities)
    static const struct {
        const char* suffix;
        bool* targetVar;
        const char* stateSuffix;
    } boolSettings[] = {
        {"/config/trendAdapt/set", &trendAdaptiveAnchorsEnabled, "/config/trendAdapt"},
        {"/config/smartConf/set", &smartConfluenceEnabled, "/config/smartConf"},
        {"/config/nightMode/set", &nightModeEnabled, "/config/nightMode"},
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
    
    // Nacht: 5m cooldown in seconden (aparte setting)
    if (!handled) {
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/nightCd5m/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            int seconds = atoi(msgBuffer);
            if (seconds >= 60 && seconds <= 7200) {
                nightCooldown5mSec = static_cast<uint16_t>(seconds);
                snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/nightCd5m", prefixBuffer);
                snprintf(valueBuffer, sizeof(valueBuffer), "%u", nightCooldown5mSec);
                mqttClient.publish(topicBufferFull, valueBuffer, true);
                settingChanged = true;
                handled = true;
            } else {
                Serial_printf(F("[MQTT] Invalid nightCd5m value (range: 60-7200 seconds): %s\n"), msgBuffer);
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
        // bitvavoSymbol - speciale logica (uppercase + symbolsArray update)
        snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/bitvavoSymbol/set", prefixBuffer);
        if (strcmp(topicBuffer, topicBufferFull) == 0) {
            if (handleMqttStringSetting(msgBuffer, msgLen, bitvavoSymbol, sizeof(bitvavoSymbol), true, "/config/bitvavoSymbol", prefixBuffer)) {
                safeStrncpy(symbol0, bitvavoSymbol, sizeof(symbol0));
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
                    // displayRotation - speciale logica (saveSettings call + deferred apply)
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/displayRotation/set", prefixBuffer);
                    if (strcmp(topicBuffer, topicBufferFull) == 0) {
                        uint8_t newRotation = atoi(msgBuffer);
                        if (newRotation == 0 || newRotation == 2) {
                            displayRotation = newRotation;
                            requestDisplayRotation(newRotation);
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
        // Queue is vol: probeer eerst queue te legen voordat we bericht verliezen
        // Dit voorkomt message loss bij tijdelijke queue overflow
        if (mqttConnected) {
            processMqttQueue();  // Probeer queue te legen
        }
        
        // Check opnieuw na processing
        if (mqttQueueCount >= MQTT_QUEUE_SIZE) {
            // Queue nog steeds vol, log warning (maar niet te vaak)
            static unsigned long lastQueueFullWarning = 0;
            unsigned long now = millis();
            if (now - lastQueueFullWarning > 5000) {  // Max 1 warning per 5 seconden
        Serial_printf("[MQTT Queue] Queue vol, bericht verloren: %s\n", topic);
                lastQueueFullWarning = now;
            }
            return false; // Queue nog steeds vol
        }
        // Queue heeft nu ruimte, val door naar normale enqueue
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
    
    // Process max 5 messages per call om queue sneller leeg te maken (was 3)
    uint8_t processed = 0;
    while (mqttQueueCount > 0 && processed < 5) {
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
    publishMqttUint("nightStartHour", nightModeStartHour);
    publishMqttUint("nightEndHour", nightModeEndHour);
    publishMqttFloat("nightSpike5m", nightSpike5mThreshold);
    publishMqttFloat("nightMove5m", nightMove5mAlertThreshold);
    publishMqttFloat("nightMove30m", nightMove30mThreshold);
    publishMqttUint("nightCd5m", nightCooldown5mSec);
    publishMqttFloat("nightAvMin", nightAutoVolMinMultiplier);
    publishMqttFloat("nightAvMax", nightAutoVolMaxMultiplier);
    
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
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightMode", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", nightModeEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/autoVol", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", autoVolatilityEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/warmStart", mqttPrefix);
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", warmStartEnabled ? "ON" : "OFF");
    enqueueMqttMessage(topicBuffer, valueBuffer, true);
    
    // String settings
    publishMqttString("bitvavoSymbol", bitvavoSymbol);
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
        float anchorRounded = roundToEuro(anchorValueToPublish);
        snprintf(valueBufferAnchor, sizeof(valueBufferAnchor), "%.0f", anchorRounded);
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
    
    static unsigned long lastValuesPublishMs = 0;
    unsigned long nowMs = millis();
    if (lastValuesPublishMs != 0 && (nowMs - lastValuesPublishMs) < MQTT_VALUES_PUBLISH_INTERVAL_MS) {
        return;
    }
    lastValuesPublishMs = nowMs;

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
        static char lastIp[16] = {0};
        static unsigned long lastIpPublishMs = 0;
        if (strncmp(lastIp, ipBuffer, sizeof(lastIp)) != 0 ||
            lastIpPublishMs == 0 ||
            (nowMs - lastIpPublishMs) >= MQTT_IP_PUBLISH_INTERVAL_MS) {
            snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", mqttPrefix);
            mqttClient.publish(topicBuffer, ipBuffer, false);
            strncpy(lastIp, ipBuffer, sizeof(lastIp) - 1);
            lastIp[sizeof(lastIp) - 1] = '\0';
            lastIpPublishMs = nowMs;
        }
    }
}

// Publiceer MQTT Discovery berichten voor Home Assistant
// Geoptimaliseerd: gebruik char arrays i.p.v. String om geheugenfragmentatie te voorkomen
static void jsonEscapeString(const char* input, char* output, size_t outputSize) {
    if (output == nullptr || outputSize == 0) return;
    output[0] = '\0';
    if (input == nullptr) return;

    size_t out = 0;
    for (size_t i = 0; input[i] != '\0' && out + 1 < outputSize; ++i) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            if (out + 2 >= outputSize) break;
            output[out++] = '\\';
            output[out++] = c;
        } else {
            output[out++] = c;
        }
    }
    output[out] = '\0';
}

void publishMqttDiscovery() {
    if (!mqttConnected) return;
    
    // Generate device ID and device JSON string (char arrays)
    char deviceId[64];
    getMqttDeviceId(deviceId, sizeof(deviceId));
    
    // Haal MQTT prefix op (gebaseerd op NTFY topic voor unieke identificatie)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    
    char escapedDeviceName[96];
    char escapedDeviceModel[96];
    jsonEscapeString(DEVICE_NAME, escapedDeviceName, sizeof(escapedDeviceName));
    jsonEscapeString(DEVICE_MODEL, escapedDeviceModel, sizeof(escapedDeviceModel));

    char deviceJson[256];
    snprintf(deviceJson, sizeof(deviceJson), 
        "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"JanP\",\"model\":\"%s\"}",
        deviceId, escapedDeviceName, escapedDeviceModel);
    
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
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/text/%s_bitvavoSymbol/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Bitvavo Market\",\"unique_id\":\"%s_bitvavoSymbol\",\"state_topic\":\"%s/config/bitvavoSymbol\",\"command_topic\":\"%s/config/bitvavoSymbol/set\",\"icon\":\"mdi:currency-btc\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
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
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"2h Breakout Cooldown\",\"unique_id\":\"%s_2hBreakCD\",\"state_topic\":\"%s/config/2hBreakCD\",\"command_topic\":\"%s/config/2hBreakCD/set\",\"min\":1,\"max\":18000,\"step\":1,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
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
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/switch/%s_nightMode/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night Mode\",\"unique_id\":\"%s_nightMode\",\"state_topic\":\"%s/config/nightMode\",\"command_topic\":\"%s/config/nightMode/set\",\"icon\":\"mdi:weather-night\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
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
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightStartHour/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night Start Hour\",\"unique_id\":\"%s_nightStartHour\",\"state_topic\":\"%s/config/nightStartHour\",\"command_topic\":\"%s/config/nightStartHour/set\",\"min\":0,\"max\":23,\"step\":1,\"unit_of_measurement\":\"h\",\"icon\":\"mdi:clock-start\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightEndHour/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night End Hour\",\"unique_id\":\"%s_nightEndHour\",\"state_topic\":\"%s/config/nightEndHour\",\"command_topic\":\"%s/config/nightEndHour/set\",\"min\":0,\"max\":23,\"step\":1,\"unit_of_measurement\":\"h\",\"icon\":\"mdi:clock-end\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);

    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightSpike5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night 5m Spike Filter\",\"unique_id\":\"%s_nightSpike5m\",\"state_topic\":\"%s/config/nightSpike5m\",\"command_topic\":\"%s/config/nightSpike5m/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:filter\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightMove5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night 5m Move Threshold\",\"unique_id\":\"%s_nightMove5m\",\"state_topic\":\"%s/config/nightMove5m\",\"command_topic\":\"%s/config/nightMove5m/set\",\"min\":0.01,\"max\":10.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightMove30m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night 30m Move Threshold\",\"unique_id\":\"%s_nightMove30m\",\"state_topic\":\"%s/config/nightMove30m\",\"command_topic\":\"%s/config/nightMove30m/set\",\"min\":0.01,\"max\":20.0,\"step\":0.01,\"unit_of_measurement\":\"%%\",\"icon\":\"mdi:trending-up\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightCd5m/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night 5m Cooldown\",\"unique_id\":\"%s_nightCd5m\",\"state_topic\":\"%s/config/nightCd5m\",\"command_topic\":\"%s/config/nightCd5m/set\",\"min\":60,\"max\":7200,\"step\":10,\"unit_of_measurement\":\"s\",\"icon\":\"mdi:timer\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightAvMin/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night Auto-Vol Min\",\"unique_id\":\"%s_nightAvMin\",\"state_topic\":\"%s/config/nightAvMin\",\"command_topic\":\"%s/config/nightAvMin/set\",\"min\":0.1,\"max\":3.0,\"step\":0.01,\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    snprintf(topicBuffer, sizeof(topicBuffer), "homeassistant/number/%s_nightAvMax/config", deviceId);
    snprintf(payloadBuffer, sizeof(payloadBuffer), "{\"name\":\"Night Auto-Vol Max\",\"unique_id\":\"%s_nightAvMax\",\"state_topic\":\"%s/config/nightAvMax\",\"command_topic\":\"%s/config/nightAvMax/set\",\"min\":0.1,\"max\":5.0,\"step\":0.01,\"icon\":\"mdi:chart-timeline-variant\",\"mode\":\"box\",%s}", deviceId, mqttPrefix, mqttPrefix, deviceJson);
    mqttClient.publish(topicBuffer, payloadBuffer, true);
    delay(50);
    
    Serial_println("[MQTT] Discovery messages published");
}

// MQTT connect functie (niet-blokkerend)
void connectMQTT() {
    if (mqttConnected) return;
    // Eerste MQTT-connect uitstellen tot na warmstart/setup (loop + WiFi-reconnect pad).
    if (s_bootNetMqttGateUntilMs != 0 && millis() < s_bootNetMqttGateUntilMs) {
        return;
    }
    if (s_bootNetMqttGateUntilMs != 0) {
        Serial.println(F("[BootNet] MQTT start now"));
        s_bootNetMqttGateUntilMs = 0;
    }

    mqttClient.setServer(mqttHost, mqttPort);
    mqttClient.setCallback(mqttCallback);
    // Geef MQTT meer ademruimte bij lange API-calls
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(10);
    
    // Geoptimaliseerd: gebruik char array i.p.v. String
    // Gebruik dynamische MQTT prefix (gebaseerd op NTFY topic voor unieke identificatie)
    char mqttPrefix[64];
    getMqttTopicPrefix(mqttPrefix, sizeof(mqttPrefix));
    char clientId[64];
    uint32_t macLower = (uint32_t)ESP.getEfuseMac();
    snprintf(clientId, sizeof(clientId), "%s_%08x", mqttPrefix, macLower);
    Serial_printf(F("[MQTT] Connecting to %s:%d as %s...\n"), mqttHost, mqttPort, clientId);
    
    // Regie: MQTT connect zware actie (socket/TLS) via dezelfde netwerkmutex als NTFY/API.
    netMutexLock("[MQTT] connectMQTT");
    bool connectedNow = mqttClient.connect(clientId, mqttUser, mqttPass);
    netMutexUnlock("[MQTT] connectMQTT");

    if (connectedNow) {
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
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightSpike5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightMove5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightMove30m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown1min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown30min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/bitvavoSymbol/set", mqttPrefix);
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
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightMode/set", mqttPrefix);
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
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightAvMin/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightAvMax/set", mqttPrefix);
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
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightStartHour/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightEndHour/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/nightCd5m/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to cooldown5min (was missing)
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/cooldown5min/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        // Subscribe to move5mAlert (was missing)
        snprintf(topicBuffer, sizeof(topicBuffer), "%s/config/move5mAlert/set", mqttPrefix);
        mqttClient.subscribe(topicBuffer);
        
        unsigned long nowMs = millis();
        // Settings: rate-limit om queue druk te voorkomen (1x per 10 min)
        if (mqttLastSettingsPublishMs == 0 || (nowMs - mqttLastSettingsPublishMs) >= 600000UL) {
            publishMqttSettings();
            mqttLastSettingsPublishMs = nowMs;
        } else {
            Serial_println("[MQTT] Settings publish skipped (rate-limited)");
        }
        // Discovery is zwaar: publiceer 1x per boot of max 1x per 6 uur
        if (!mqttDiscoveryPublished || (nowMs - mqttLastDiscoveryMs) >= 21600000UL) {
            publishMqttDiscovery();
            mqttDiscoveryPublished = true;
            mqttLastDiscoveryMs = nowMs;
        } else {
            Serial_println("[MQTT] Discovery skipped (recently published)");
        }
        
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

// Parse Bitvavo JSON functies zijn verwijderd - nu via ApiClient::parseBitvavoPrice()

// Calculate average of array (optimized: single loop)
// Fase 4.2.8: static verwijderd zodat PriceData.cpp deze functie kan aanroepen
// FIX: calculateAverage moet currentIndex gebruiken voor correcte ring buffer iteratie
float calculateAverage(float *array, uint8_t size, bool filled, uint8_t currentIndex)
{
    // Gebruik accumulateValidPricesFromRingBuffer helper voor correcte ring buffer iteratie
    float sum = 0.0f;
    uint16_t validCount = 0;
    
    // Bereken beschikbare elementen
    uint16_t availableElements = calculateAvailableElements(filled, currentIndex, size);
    if (availableElements == 0) {
        return 0.0f;
    }
    
    // Gebruik laatste 'availableElements' elementen (max size)
    uint16_t elementsToUse = (availableElements < size) ? availableElements : size;
    
    // Gebruik helper functie voor correcte ring buffer iteratie
    accumulateValidPricesFromRingBuffer(
        array,
        filled,
        currentIndex,
        size,
        1,  // Start vanaf 1 positie terug (nieuwste)
        elementsToUse,
        sum,
        validCount
    );
    
    if (validCount == 0) {
        return 0.0f;
    }
    
    float avg = sum / validCount;
    return avg;
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
    float ret = ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
    if (isnan(ret) || isinf(ret)) {
        return 0.0f;
    }
    return ret;
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

// Percentage SOURCE_LIVE in het actieve fiveMinutePrices-venster (zelfde count als calculateReturn5Minutes)
uint8_t calcLivePctFiveMinuteWindow()
{
    DataSource* sources = priceData.getFiveMinutePricesSource();
    if (sources == nullptr) {
        return 0;
    }
    bool filled = priceData.getFiveMinuteArrayFilled();
    uint16_t idx = priceData.getFiveMinuteIndex();
    uint16_t count = filled ? (uint16_t)SECONDS_PER_5MINUTES : idx;
    if (count == 0) {
        return 0;
    }
    uint16_t liveCount = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (sources[i] == SOURCE_LIVE) {
            liveCount++;
        }
    }
    return (uint8_t)((liveCount * 100U) / count);
}

// % SOURCE_LIVE in actief 1m-secondenvenster (min/max-kaart bronstatus)
uint8_t calcLivePctSecondWindow()
{
    uint16_t count = priceData.getSecondArrayFilled()
                         ? (uint16_t)SECONDS_PER_MINUTE
                         : (uint16_t)priceData.getSecondIndex();
    if (count == 0) {
        return 0;
    }
    uint16_t liveCount = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (secondPricesSource[i] == SOURCE_LIVE) {
            liveCount++;
        }
    }
    return (uint8_t)((liveCount * 100U) / count);
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
    
    bool result = findMinMaxInArray(prices, SECONDS_PER_MINUTE, index, arrayFilled, 0, false, minVal, maxVal);
}

#if defined(PLATFORM_ESP32S3_JC3248W535)
// 5m-venster: zelfde ringbuffer als calculateReturn5Minutes() / ret_5m
void findMinMaxInFiveMinutePrices(float &minVal, float &maxVal)
{
    const float* arr = priceData.getFiveMinutePrices();
    if (arr == nullptr) {
        minVal = maxVal = 0.0f;
        return;
    }
    uint16_t idx = priceData.getFiveMinuteIndex();
    bool filled = priceData.getFiveMinuteArrayFilled();
    findMinMaxInArray(arr, SECONDS_PER_5MINUTES, idx, filled, 0, true, minVal, maxVal);
}

bool uiFiveMinuteHasMinimalData(void)
{
    return priceData.getFiveMinuteArrayFilled() || priceData.getFiveMinuteIndex() >= 2;
}

static void refreshAveragePrice5mForUi(void)
{
    float* arr = priceData.getFiveMinutePrices();
    if (arr == nullptr) {
        averagePrices[4] = 0.0f;
        return;
    }
    uint16_t idx = priceData.getFiveMinuteIndex();
    bool filled = priceData.getFiveMinuteArrayFilled();
    uint16_t avail = calculateAvailableElements(filled, idx, SECONDS_PER_5MINUTES);
    if (avail == 0) {
        averagePrices[4] = 0.0f;
        return;
    }
    float sum = 0.0f;
    uint16_t validCount = 0;
    accumulateValidPricesFromRingBuffer(arr, filled, idx, SECONDS_PER_5MINUTES, 1, avail, sum, validCount);
    averagePrices[4] = (validCount > 0) ? (sum / (float)validCount) : 0.0f;
}
#endif

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
            // FIX: Geef secondIndex door voor correcte ring buffer iteratie
            averagePrices[1] = calculateAverage(priceData.getSecondPrices(), SECONDS_PER_MINUTE, priceData.getSecondArrayFilled(), priceData.getSecondIndex());
        } else if (averagePriceIndex == 2) {
            // For 30m: calculate average of last 30 minutes (handled separately in calculateReturn30Minutes)
            // This is a placeholder - actual calculation is done in the wrapper function
        }
    }
    
    // Fase 2.2: Geconsolideerde return calculation
    // Fase 5.1: Gebruik geconsolideerde percentage berekening helper
    float returnValue = calculatePercentageReturn(priceNow, priceXAgo);
    
    
    return returnValue;
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

// Read-only: geen update van averagePrices[1] (o.a. /status)
float calculateReturn1MinuteReadOnly()
{
    return priceData.calculateReturn1Minute(nullptr);
}

// Calculate 5-minute return: price now vs 5 minutes ago
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
float calculateReturn5Minutes()
{
    const float* prices = priceData.getFiveMinutePrices();
    if (prices == nullptr) {
        return 0.0f;
    }
    uint16_t arraySize = SECONDS_PER_5MINUTES;
    uint16_t index = priceData.getFiveMinuteIndex();
    bool filled = priceData.getFiveMinuteArrayFilled();
    uint16_t count = filled ? arraySize : index;
    if (count < 2) {
        return 0.0f;
    }
    float stepHours = (5.0f / 60.0f) / (float)SECONDS_PER_5MINUTES;
    float spanHours = (count > 1) ? (float)(count - 1) * stepHours : 0.0f;
    float totalHours = (spanHours > (5.0f / 60.0f) || spanHours <= 0.0f) ? (5.0f / 60.0f) : spanHours;
    float ret5m = 0.0f;
    // Ringbuffer: bij gevulde buffer is index 0..count-1 niet chronologisch. Regressie vereist oudste→nieuwste.
    static float s_chron5m[SECONDS_PER_5MINUTES];
    const float* series = prices;
    if (filled) {
        for (uint16_t i = 0; i < count; i++) {
            s_chron5m[i] = prices[(index + i) % arraySize];
        }
        series = s_chron5m;
    }
    if (computeRegressionPctFromSeries(series, count, stepHours, totalHours, ret5m)) {
        return ret5m;
    }
    return 0.0f;
}

// Forward decl: definitie volgt na calculateReturn30Minutes-read-only wrapper
static float calculateLinearTrend30Minutes(bool updateAveragePriceCache);

// Calculate 30-minute return: price now vs 30 minutes ago (using minute averages)
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Fase 9.1.4: static verwijderd zodat WebServerModule deze functie kan aanroepen
float calculateReturn30Minutes()
{
    return calculateLinearTrend30Minutes(true);
}

// Read-only: geen update van averagePrices[2] (o.a. /status)
float calculateReturn30MinutesReadOnly()
{
    return calculateLinearTrend30Minutes(false);
}

// Bereken lineaire regressie (trend) over de laatste 30 minuten
// Retourneert de helling (slope) als percentage per 30 minuten
// Positieve waarde = stijgende trend, negatieve waarde = dalende trend
static float calculateLinearTrend30Minutes(bool updateAveragePriceCache)
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
        if (updateAveragePriceCache) {
            averagePrices[2] = 0.0f;
        }
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
            // Loop gaat van nieuwste -> oudste, dus keer om voor juiste richting
            float x = (float)(30 - 1 - i); // 0 = oudste, 29 = nieuwste
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
        if (updateAveragePriceCache) {
            averagePrices[2] = 0.0f;
        }
        return 0.0f;
    }
    
    // Bereken gemiddelde prijs voor weergave
    float last30Avg = last30Sum / last30Count;
    if (updateAveragePriceCache) {
        averagePrices[2] = last30Avg;
    }
    
    // Bereken slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f) // Voorkom deling door nul
    {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is nu de prijsverandering per minuut
    // Omzetten naar percentage per 30 minuten: (slope * 30) / gemiddelde_prijs * 100
    if (last30Avg > 0.0f)
    {
        float slopePer30m = slope * 30.0f; // Prijsverandering per 30 minuten
        float pctPer30m = (slopePer30m / last30Avg) * 100.0f;
        return pctPer30m;
    }
    
    return 0.0f;
}

// ret_2h: prijs nu vs 120 minuten (2 uur) geleden (gebruik minuteAverages)
// Fase 4.2.9: Gebruik PriceData getters (parallel, arrays blijven globaal)
// Calculate linear trend over last 2 hours (120 minutes) using linear regression
// Returns slope as percentage per hour
// Positive value = rising trend, negative value = falling trend
// This is more robust than simple 2-point comparison as it uses all data points
static float calculateLinearTrend2Hours()
{
    bool arrayFilled = priceData.getMinuteArrayFilled();
    uint8_t index = priceData.getMinuteIndex();
    float* averages = priceData.getMinuteAverages();
    
    uint8_t availableMinutes = calculateAvailableElements(arrayFilled, index, MINUTES_FOR_30MIN_CALC);
    
    // We need at least 10 minutes for a reliable trend
    uint8_t minutesToUse = (availableMinutes < 120) ? availableMinutes : 120;
    if (minutesToUse < 10) {
        return 0.0f;  // Not enough data
    }
    
    // Linear regression: y = a + b*x
    // x = time (0 to minutesToUse-1), y = price
    // b (slope) = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint8_t validPoints = 0;
    float avgSum = 0.0f;
    uint8_t avgCount = 0;
    
    // Loop through last minutesToUse minutes
    // Start from the last written position (newest) and work backwards
    uint8_t lastWrittenIdx;
    if (!arrayFilled)
    {
        if (index == 0) {
            return 0.0f;  // No data yet
        }
        lastWrittenIdx = index - 1;
    }
    else
    {
        lastWrittenIdx = getLastWrittenIndex(index, MINUTES_FOR_30MIN_CALC);
    }
    
        for (uint8_t i = 0; i < minutesToUse; i++)
        {
            uint8_t idx;
            if (!arrayFilled)
            {
            if (i >= index) break;  // Not enough data
            idx = index - 1 - i;  // Start at last minute and work backwards
            }
            else
            {
            // Ring buffer mode: use helper, starting from lastWrittenIdx
            int32_t idx_temp = getRingBufferIndexAgo(lastWrittenIdx, i, MINUTES_FOR_30MIN_CALC);
                if (idx_temp < 0) break;
                idx = (uint8_t)idx_temp;
            }
        
        float price = averages[idx];
        if (isValidPrice(price))
        {
            // Time index should increase from oldest -> newest.
            // Loop iterates newest -> oldest, so reverse index for correct slope sign.
            float x = (float)(minutesToUse - 1 - i);  // 0 = oldest, minutesToUse-1 = newest
            float y = price;
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            avgSum += price;
            avgCount++;
            validPoints++;
        }
    }
    
    if (validPoints < 2)
    {
        return 0.0f;
    }
    
    // Calculate average price for display (update averagePrices[3]) — JC3248/LCDWIKI: computeTwoHMetrics-pad
    
    // Calculate slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f)  // Prevent division by zero
    {
                    return 0.0f;
                }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is now price change per minute
    // Convert to percentage per hour: (slope * 60) / average_price * 100
    // Then multiply by 2 to get percentage per 2 hours
    if (avgCount > 0 && avgSum > 0.0f)
    {
        float avgPrice = avgSum / avgCount;
        float slopePerHour = slope * 60.0f;  // Price change per hour
        float pctPerHour = (slopePerHour / avgPrice) * 100.0f;
        float pctPer2Hours = pctPerHour * 2.0f;  // Extrapolate to 2 hours
        return pctPer2Hours;
    }
    
                    return 0.0f;
                }

// Calculate 2-hour return: now uses linear regression for better trend detection
// NOTE: This function now uses linear regression instead of simple 2-point comparison
static float calculateReturn2Hours()
{
    // Use linear regression for more robust trend detection
    return calculateLinearTrend2Hours();
}

// Helper: beschikbare uren in hourly buffer
static inline uint16_t getAvailableHours()
{
    if (hourlyAverages == nullptr) {
        return 0;
    }
    return calculateAvailableElements(hourArrayFilled, hourIndex, HOURS_FOR_7D);
}

// % SOURCE_LIVE in de laatste windowHours uren van hourlyAveragesSource (zelfde ring als hourlyAverages)
uint8_t calcLivePctHourlyLastN(uint16_t windowHours)
{
    if (hourlyAverages == nullptr || hourlyAveragesSource == nullptr) {
        return 0;
    }
    if (windowHours == 0 || windowHours > HOURS_FOR_7D) {
        return 0;
    }
    uint16_t availableHours = getAvailableHours();
    if (availableHours == 0) {
        return 0;
    }
    uint16_t use = (availableHours < windowHours) ? availableHours : windowHours;

    uint16_t lastHourIdx;
    if (!hourArrayFilled) {
        if (hourIndex == 0) {
            return 0;
        }
        lastHourIdx = hourIndex - 1;
    } else {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }

    uint16_t liveCount = 0;
    for (uint16_t k = 0; k < use; k++) {
        uint16_t positionsAgo = (use - 1 - k);
        int32_t idx_temp = getRingBufferIndexAgo(lastHourIdx, positionsAgo, HOURS_FOR_7D);
        if (idx_temp < 0) {
            continue;
        }
        uint16_t idx = (uint16_t)idx_temp;
        if (hourlyAveragesSource[idx] == SOURCE_LIVE) {
            liveCount++;
        }
    }
    return (uint8_t)((liveCount * 100U) / use);
}

#if UI_HAS_TF_MINMAX_STATUS_UI
// Min/max snapshot vóór nested-chain (UIController); bron: 0=— 1=LIVE 2=WARM 3=MIX
float g_uiTfRawMin[7];
float g_uiTfRawMax[7];
bool g_uiTfRawValid[7];
uint8_t g_uiTfMinMaxSrc[7];

void uiResetTfMinMaxSnapshot(void)
{
    for (uint8_t i = 0; i < 7; i++) {
        g_uiTfRawValid[i] = false;
        g_uiTfMinMaxSrc[i] = 0;
    }
}
#endif

#if defined(PLATFORM_ESP32S3_JC3248W535)
// 1d-kaart (index 5): 24h gemiddelde + min/max — zelfde uurvenster als calculateLinearTrend1d / ret_1d; fallback naar warmStart1d*
static bool fill24HourlyStatsFor1dUi(float &outMin, float &outMax, float &outAvg)
{
    outMin = outMax = outAvg = 0.0f;
    if (hourlyAverages == nullptr) {
        return false;
    }
    uint16_t availableHours = getAvailableHours();
    uint16_t hoursToUse = (availableHours < 24) ? availableHours : 24;
    if (hoursToUse < 6) {
        return false;  // zelfde minimum als calculateLinearTrend1Day()
    }
    uint16_t lastHourIdx;
    if (!hourArrayFilled) {
        if (hourIndex == 0) {
            return false;
        }
        lastHourIdx = hourIndex - 1;
    } else {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }
    bool first = false;
    float sum = 0.0f;
    uint16_t cnt = 0;
    for (uint16_t k = 0; k < hoursToUse; k++) {
        uint16_t idx;
        if (!hourArrayFilled) {
            if (hourIndex < hoursToUse) {
                return false;
            }
            idx = (hourIndex - hoursToUse) + k;
        } else {
            uint16_t positionsAgo = (hoursToUse - 1 - k);
            int32_t idx_temp = getRingBufferIndexAgo(lastHourIdx, positionsAgo, HOURS_FOR_7D);
            if (idx_temp < 0) {
                return false;
            }
            idx = (uint16_t)idx_temp;
        }
        float price = hourlyAverages[idx];
        if (isValidPrice(price)) {
            sum += price;
            cnt++;
            if (!first) {
                outMin = outMax = price;
                first = true;
            } else {
                if (price < outMin) {
                    outMin = price;
                }
                if (price > outMax) {
                    outMax = price;
                }
            }
        }
    }
    if (cnt == 0 || !first) {
        return false;
    }
    outAvg = sum / (float)cnt;
    return true;
}

static void refreshAveragePrice1dForUi(void)
{
    float mn, mx, av;
    if (fill24HourlyStatsFor1dUi(mn, mx, av)) {
        averagePrices[5] = av;
        return;
    }
    if (warmStart1dValid) {
        averagePrices[5] = warmStart1dAvg;
    } else {
        averagePrices[5] = 0.0f;
    }
}

// UI-debug (JC3248): welke bron vormde laatste 1d/7d min/max (0=geen, 1=hourly, 2=warmStart)
uint8_t g_uiLastMinMaxSource1d = 0;
uint8_t g_uiLastMinMaxSource7d = 0;

void findMinMaxInLast24Hours(float &minVal, float &maxVal)
{
    float av;
    g_uiLastMinMaxSource1d = 0;
    if (fill24HourlyStatsFor1dUi(minVal, maxVal, av)) {
        g_uiLastMinMaxSource1d = 1;
        return;
    }
    if (warmStart1dValid) {
        g_uiLastMinMaxSource1d = 2;
        minVal = warmStart1dMin;
        maxVal = warmStart1dMax;
    } else {
        minVal = maxVal = 0.0f;
    }
}

// 7d-kaart (index 6): gemiddelde + min/max over hetzelfde uurvenster als calculateLinearTrend7Days / ret_7d; fallback warmStart7d*
static bool fill168HourlyStatsFor7dUi(float &outMin, float &outMax, float &outAvg)
{
    outMin = outMax = outAvg = 0.0f;
    if (hourlyAverages == nullptr) {
        return false;
    }
    uint16_t availableHours = getAvailableHours();
    uint16_t hoursToUse = (availableHours < HOURS_FOR_7D) ? availableHours : HOURS_FOR_7D;
    if (hoursToUse < 24) {
        return false;  // zelfde minimum als calculateLinearTrend7Days()
    }
    uint16_t lastHourIdx;
    if (!hourArrayFilled) {
        if (hourIndex == 0) {
            return false;
        }
        lastHourIdx = hourIndex - 1;
    } else {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }
    bool first = false;
    float sum = 0.0f;
    uint16_t cnt = 0;
    for (uint16_t k = 0; k < hoursToUse; k++) {
        uint16_t idx;
        if (!hourArrayFilled) {
            if (hourIndex < hoursToUse) {
                return false;
            }
            idx = (hourIndex - hoursToUse) + k;
        } else {
            uint16_t positionsAgo = (hoursToUse - 1 - k);
            int32_t idx_temp = getRingBufferIndexAgo(lastHourIdx, positionsAgo, HOURS_FOR_7D);
            if (idx_temp < 0) {
                return false;
            }
            idx = (uint16_t)idx_temp;
        }
        float price = hourlyAverages[idx];
        if (isValidPrice(price)) {
            sum += price;
            cnt++;
            if (!first) {
                outMin = outMax = price;
                first = true;
            } else {
                if (price < outMin) {
                    outMin = price;
                }
                if (price > outMax) {
                    outMax = price;
                }
            }
        }
    }
    if (cnt == 0 || !first) {
        return false;
    }
    outAvg = sum / (float)cnt;
    return true;
}

static void refreshAveragePrice7dForUi(void)
{
    float mn, mx, av;
    if (fill168HourlyStatsFor7dUi(mn, mx, av)) {
        averagePrices[6] = av;
        return;
    }
    if (warmStart7dValid) {
        averagePrices[6] = warmStart7dAvg;
    } else {
        averagePrices[6] = 0.0f;
    }
}

void findMinMaxInLast7Days(float &minVal, float &maxVal)
{
    float av;
    g_uiLastMinMaxSource7d = 0;
    if (fill168HourlyStatsFor7dUi(minVal, maxVal, av)) {
        g_uiLastMinMaxSource7d = 1;
        // 168h ⊃ laatste 24h uit dezelfde hourly buffer — geen nested-clamp nodig
        return;
    }
    if (warmStart7dValid) {
        g_uiLastMinMaxSource7d = 2;
        minVal = warmStart7dMin;
        maxVal = warmStart7dMax;
    } else {
        minVal = maxVal = 0.0f;
    }
}
#endif

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

// Calculate linear trend over last 24 hours (1 day) using linear regression
// Returns slope as percentage per day
// Positive value = rising trend, negative value = falling trend
static float calculateLinearTrend1Day()
{
    if (hourlyAverages == nullptr) {
        return 0.0f;
    }
    
    uint16_t availableHours = getAvailableHours();
    
    // We need at least 6 hours for a reliable trend
    uint16_t hoursToUse = (availableHours < 24) ? availableHours : 24;
    if (hoursToUse < 6) {
        return 0.0f;  // Not enough data
    }
    
    // Linear regression: y = a + b*x
    // x = time (0 to hoursToUse-1), y = price
    // b (slope) = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint16_t validPoints = 0;
    float avgSum = 0.0f;
    uint16_t avgCount = 0;
    
    uint16_t lastHourIdx;
    if (!hourArrayFilled)
    {
        if (hourIndex == 0) {
            return 0.0f;
        }
        lastHourIdx = hourIndex - 1;
    }
    else
    {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }
    
    // Loop through last hoursToUse hours
    // Gebruik altijd de volgorde: oudste -> nieuwste
    for (uint16_t k = 0; k < hoursToUse; k++)
    {
        uint16_t idx;
        if (!hourArrayFilled)
        {
            if (hourIndex < hoursToUse) break;
            idx = (hourIndex - hoursToUse) + k;
        }
        else
        {
            uint16_t positionsAgo = (hoursToUse - 1 - k);
            int32_t idx_temp = getRingBufferIndexAgo(lastHourIdx, positionsAgo, HOURS_FOR_7D);
            if (idx_temp < 0) break;
            idx = (uint16_t)idx_temp;
        }
        
        float price = hourlyAverages[idx];
        if (isValidPrice(price))
        {
            float x = (float)k;  // 0 = oudste, hoursToUse-1 = nieuwste
            float y = price;
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            avgSum += price;
            avgCount++;
            validPoints++;
        }
    }
    
    if (validPoints < 2)
    {
        return 0.0f;
    }
    
    // Calculate average price
    float avgPrice = avgSum / avgCount;
    
    // Calculate slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f)  // Prevent division by zero
    {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is now price change per hour
    // Convert to percentage per day: (slope * 24) / average_price * 100
    if (avgPrice > 0.0f)
    {
        float slopePerDay = slope * 24.0f;  // Price change per day
        float pctPerDay = (slopePerDay / avgPrice) * 100.0f;
        return pctPerDay;
    }
    
    return 0.0f;
}

// Calculate linear trend over last 7 days (168 hours) using linear regression
// Returns slope as percentage per week
// Positive value = rising trend, negative value = falling trend
static float calculateLinearTrend7Days()
{
    if (hourlyAverages == nullptr) {
        return 0.0f;
    }
    
    uint16_t availableHours = getAvailableHours();
    
    // We need at least 24 hours (1 day) for a reliable trend
    uint16_t hoursToUse = (availableHours < HOURS_FOR_7D) ? availableHours : HOURS_FOR_7D;
    if (hoursToUse < 24) {
        return 0.0f;  // Not enough data
    }
    
    // Linear regression: y = a + b*x
    // x = time (0 to hoursToUse-1), y = price
    // b (slope) = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint16_t validPoints = 0;
    float avgSum = 0.0f;
    uint16_t avgCount = 0;
    
    uint16_t lastHourIdx;
    if (!hourArrayFilled)
    {
        if (hourIndex == 0) {
            return 0.0f;
        }
        lastHourIdx = hourIndex - 1;
    }
    else
    {
        lastHourIdx = getLastWrittenIndex(hourIndex, HOURS_FOR_7D);
    }
    
    // Loop through last hoursToUse hours
    // Gebruik altijd de volgorde: oudste -> nieuwste
    for (uint16_t k = 0; k < hoursToUse; k++)
    {
        uint16_t idx;
        if (!hourArrayFilled)
        {
            if (hourIndex < hoursToUse) break;
            idx = (hourIndex - hoursToUse) + k;
        }
        else
        {
            uint16_t positionsAgo = (hoursToUse - 1 - k);
            int32_t idx_temp = getRingBufferIndexAgo(lastHourIdx, positionsAgo, HOURS_FOR_7D);
            if (idx_temp < 0) break;
            idx = (uint16_t)idx_temp;
        }
        
        float price = hourlyAverages[idx];
        if (isValidPrice(price))
        {
            float x = (float)k;  // 0 = oudste, hoursToUse-1 = nieuwste
            float y = price;
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            avgSum += price;
            avgCount++;
            validPoints++;
        }
    }
    
    if (validPoints < 2)
    {
        return 0.0f;
    }
    
    // Calculate average price
    float avgPrice = avgSum / avgCount;
    
    // Calculate slope (b)
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    
    if (fabsf(denominator) < 0.0001f)  // Prevent division by zero
    {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;
    
    // Slope is now price change per hour
    // Convert to percentage per week: (slope * 168) / average_price * 100
    if (avgPrice > 0.0f)
    {
        float slopePerWeek = slope * 168.0f;  // Price change per week
        float pctPerWeek = (slopePerWeek / avgPrice) * 100.0f;
        return pctPerWeek;
    }
    
    return 0.0f;
}

// ret_1d: prijs nu vs 24 uur geleden - now uses linear regression
static float calculateReturn24Hours()
{
    return calculateLinearTrend1Day();
}

// ret_7d: prijs nu vs 7 dagen geleden - via regressie over hourly buffer
static float calculateReturn7Days()
{
    return calculateLinearTrend7Days();
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
    bool result = findMinMaxInArray(minuteAverages, MINUTES_FOR_30MIN_CALC, minuteIndex, minuteArrayFilled, 30, true, minVal, maxVal);
}

#if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
// Find min and max values in last 2 hours (120 minutes) of minuteAverages array
// Platforms met 2h-box (LCDWIKI / JC3248)
// Fase 2.1: Geoptimaliseerd: gebruikt generic findMinMaxInArray() helper
void findMinMaxInLast2Hours(float &minVal, float &maxVal)
{
    bool result = findMinMaxInArray(minuteAverages, MINUTES_FOR_30MIN_CALC, minuteIndex, minuteArrayFilled, 120, true, minVal, maxVal);
}
#endif

// Compute 2-hour metrics: EUR avg/high/low/range uit minuteAverages; hasRet2h alleen voor metrics.valid.
// Schrijft globale ret_2h niet (ret_2h = apart % uit warm-start/fetch -> calculateReturn2Hours).
TwoHMetrics computeTwoHMetrics()
{
    TwoHMetrics metrics;
    
    // JC3248 / LCDWIKI: bereken 2h avg/min/max uit minuteAverages
    metrics.avg2h = 0.0f;
    metrics.low2h = 0.0f;
    metrics.high2h = 0.0f;
    
    uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
    
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
        
        float last120Sum = 0.0f;
        uint16_t last120Count = 0;
        accumulateValidPricesFromRingBuffer(
            minuteAverages,
            minuteArrayFilled,
            minuteIndex,
            MINUTES_FOR_30MIN_CALC,
            1,  // Start vanaf 1 positie terug (nieuwste)
            count,
            last120Sum,
            last120Count
        );
        if (last120Count > 0) {
            metrics.avg2h = last120Sum / last120Count;
        }
    }
    
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
// Korte buffers: priceData.addPriceToSecondArray() alleen vanuit priceRepeatTask (1 Hz sampler)

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
    if (hourlyAveragesSource != nullptr) {
        uint16_t liveMinCnt = 0;
        for (uint16_t i = 1; i <= MINUTES_PER_HOUR; i++) {
            int32_t midx = getRingBufferIndexAgo(minuteIndex, i, MINUTES_FOR_30MIN_CALC);
            if (midx >= 0 && midx < MINUTES_FOR_30MIN_CALC && minuteAveragesSource[midx] == SOURCE_LIVE) {
                liveMinCnt++;
            }
        }
        hourlyAveragesSource[hourIndex] = (liveMinCnt * 100U / MINUTES_PER_HOUR >= 80U) ? SOURCE_LIVE : SOURCE_BINANCE;
    }
    hourIndex = (hourIndex + 1) % HOURS_FOR_7D;
    if (hourIndex == 0) {
        hourArrayFilled = true;
    }
}

// Periodieke sanity check voor live minutenbuffer (voor 1m/5m/30m/2h)
static void checkLiveMinuteBuffer()
{
    static unsigned long lastCheckMs = 0;
    const unsigned long CHECK_INTERVAL_MS = 60000UL; // elke minuut
    unsigned long now = millis();
    if (now - lastCheckMs < CHECK_INTERVAL_MS) {
        return;
    }
    lastCheckMs = now;

    uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
    if (availableMinutes == 0) {
        Serial_printf(F("[MinuteBuf] Geen data: minuteIndex=%u, filled=%d\n"),
                      minuteIndex, minuteArrayFilled ? 1 : 0);
        return;
    }

    uint8_t count = availableMinutes;
    float minVal = 0.0f;
    float maxVal = 0.0f;
    float sum = 0.0f;
    uint16_t valid = 0;
    bool firstValid = false;
    for (uint8_t i = 1; i <= count; i++) {
        uint8_t idx = (minuteIndex - i + MINUTES_FOR_30MIN_CALC) % MINUTES_FOR_30MIN_CALC;
        float price = minuteAverages[idx];
        if (!isValidPrice(price)) {
            continue;
        }
        if (!firstValid) {
            minVal = price;
            maxVal = price;
            firstValid = true;
        } else {
            if (price < minVal) minVal = price;
            if (price > maxVal) maxVal = price;
        }
        sum += price;
        valid++;
    }

    if (!firstValid || valid == 0) {
        Serial_printf(F("[MinuteBuf] Geen geldige prijzen: avail=%u, idx=%u\n"),
                      availableMinutes, minuteIndex);
        return;
    }

    float avg = sum / (float)valid;
    uint8_t livePct30 = calcLivePctMinuteAverages(30);
    uint8_t livePct120 = calcLivePctMinuteAverages(120);
    Serial_printf(F("[MinuteBuf] idx=%u filled=%d avail=%u valid=%u min=%.2f max=%.2f avg=%.2f live30=%u%% live120=%u%%\n"),
                  minuteIndex, minuteArrayFilled ? 1 : 0, availableMinutes, valid, minVal, maxVal, avg,
                  livePct30, livePct120);

    if (avg < minVal || avg > maxVal || maxVal < minVal) {
        Serial_printf(F("[MinuteBuf] WARN: avg buiten range (min=%.2f max=%.2f avg=%.2f)\n"),
                      minVal, maxVal, avg);
    }
}

static void updateMinuteAverage()
{
    // Boot: geen WARN — eerste fetch kan vóór 1 Hz-sampler nog geen second-samples hebben
    if (priceData.getSecondIndex() == 0 && !priceData.getSecondArrayFilled()) {
        return;
    }
    // Fase 4.2.7: Gebruik PriceData getters (parallel, arrays blijven globaal)
    // Bereken gemiddelde van de 60 seconden
    // FIX: Geef secondIndex door voor correcte ring buffer iteratie
    float minuteAvg = calculateAverage(priceData.getSecondPrices(), SECONDS_PER_MINUTE, priceData.getSecondArrayFilled(), priceData.getSecondIndex());
    
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
    
    // Bounds check voor minuteAverages array (array heeft indices 0-119, dus max index is 119)
    // Als minuteIndex >= MINUTES_FOR_30MIN_CALC (120), reset naar 0 en markeer buffer als vol
    if (minuteIndex >= MINUTES_FOR_30MIN_CALC)
    {
        Serial_printf("[Array] ERROR: minuteIndex buiten bereik: %u >= %u, reset naar 0\n", minuteIndex, MINUTES_FOR_30MIN_CALC);
        minuteIndex = 0; // Reset naar veilige waarde
        minuteArrayFilled = true; // Buffer is vol (wraparound)
    }
    
    // Sla op in minute array (minuteIndex is nu gegarandeerd < MINUTES_FOR_30MIN_CALC)
    uint8_t oldMinuteIndex = minuteIndex;
    minuteAverages[minuteIndex] = minuteAvg;
    minuteAveragesSource[minuteIndex] = SOURCE_LIVE;  // Mark as live data
    
    // Verhoog index met modulo om wraparound te garanderen
    bool wasMinuteFilled = minuteArrayFilled;
    minuteIndex = (minuteIndex + 1) % MINUTES_FOR_30MIN_CALC;
    if (minuteIndex == 0)
        minuteArrayFilled = true;
    
    
    // Update hourly aggregate buffer
    updateHourlyAverage();
    
    // Update warm-start status na elke minuut update
    updateWarmStartStatus();

    // Periodieke sanity check voor live minutenbuffer
    checkLiveMinuteBuffer();
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
    const unsigned long now = millis();
    if (candlesNextAllowedMs != 0 && now < candlesNextAllowedMs) {
        return;
    }
    if (lastCandleRestFailMs != 0 && (now - lastCandleRestFailMs) < CANDLE_REST_CONNECT_BACKOFF_MS) {
        static unsigned long lastCandleRestBackoffLogMs = 0;
        if (now - lastCandleRestBackoffLogMs >= 5000UL) {
            lastCandleRestBackoffLogMs = now;
            Serial.println(F("[Candles][REST backoff] Skip REST"));
        }
        return;
    }
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsConnected) {
        // Net verbonden: geef WS even de tijd om eerste candles te leveren
        if (wsLastCandle1mMs == 0 && (now - wsConnectedMs) < 20000UL) {
            return;
        }
        bool recent1m = (wsLastCandle1mMs != 0 && (now - wsLastCandle1mMs) < 90000UL);
        bool recent5m = (wsLastCandle5mMs != 0 && (now - wsLastCandle5mMs) < 360000UL);
        if (recent1m && recent5m) {
            static unsigned long lastWsSkipLogMs = 0;
            if (now - lastWsSkipLogMs >= 120000UL) {
                lastWsSkipLogMs = now;
                Serial_println(F("[Candles][WS fresh] Skip REST"));
            }
            return; // WS candles up-to-date, skip REST
        }
    }
#endif
    // Tijdens WS handshake: geen extra REST connecties
#if WS_ENABLED && WS_LIB_AVAILABLE
    extern bool wsConnecting;
    extern unsigned long wsConnectStartMs;
    if (wsConnecting && (now - wsConnectStartMs) < 15000UL) {
        return;
    }
#endif
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (lastWsDisconnectMs != 0 && (now - lastWsDisconnectMs) < WS_RECONNECT_GRACE_CANDLES_MS) {
        static unsigned long lastDiscGraceLogMs = 0;
        if (now - lastDiscGraceLogMs >= 8000UL) {
            lastDiscGraceLogMs = now;
            Serial.println(F("[Candles][Reconnect grace] Skip REST"));
        }
        return;
    }
    if (lastWsReconnectMs != 0 && (now - lastWsReconnectMs) < WS_RECONNECT_GRACE_CANDLES_MS) {
        static unsigned long lastRecoGraceLogMs = 0;
        if (now - lastRecoGraceLogMs >= 8000UL) {
            lastRecoGraceLogMs = now;
            Serial.println(F("[Candles][Reconnect grace] Skip REST"));
        }
        return;
    }
#endif

    static unsigned long last1mFetchMs = 0;
    static unsigned long last5mFetchMs = 0;

#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsConnected) {
        if (!(wsLastCandle1mMs == 0 && (now - wsConnectedMs) < 20000UL)) {
            bool recent1m = (wsLastCandle1mMs != 0 && (now - wsLastCandle1mMs) < 90000UL);
            bool recent5m = (wsLastCandle5mMs != 0 && (now - wsLastCandle5mMs) < 360000UL);
            if (!(recent1m && recent5m)) {
                static unsigned long lastWsStaleLogMs = 0;
                if (now - lastWsStaleLogMs >= 12000UL) {
                    lastWsStaleLogMs = now;
                    Serial.println(F("[Candles][WS stale] REST fallback"));
                }
            }
        }
    }
#endif

    if (last1mFetchMs == 0 || (now - last1mFetchMs) >= 60000UL) {
        float temp1mPrices[2];
        int fetched1m = fetchBitvavoCandles(bitvavoSymbol, "1m", 2, temp1mPrices, nullptr, 2);
        if (fetched1m > 0) {
            last1mFetchMs = now;
        }
    }
    
    if (last5mFetchMs == 0 || (now - last5mFetchMs) >= 300000UL) {
        float temp5mPrices[2];
        int fetched5m = fetchBitvavoCandles(bitvavoSymbol, "5m", 2, temp5mPrices, nullptr, 2);
        if (fetched5m > 0) {
            last5mFetchMs = now;
        }
    }
}

// Korte API boot-gate: wacht tot TCP/DNS rustiger is vóór Bitvavo HTTP (fetchPrice + warmstart candles).
static void bootNetArmApiGateMs(unsigned long ms, bool logWarmStartLine) {
    s_bootNetApiGateUntilMs = millis() + ms;
    if (logWarmStartLine) {
        Serial.printf(F("[BootNet] WarmStart API gate active +%lums\n"), (unsigned long)ms);
    }
}

static void bootNetWaitApiGateIfNeeded(const char *reason) {
    if (s_bootNetApiGateUntilMs == 0) {
        return;
    }
    const unsigned long now = millis();
    if (now >= s_bootNetApiGateUntilMs) {
        s_bootNetApiGateUntilMs = 0;
        return;
    }
    Serial.printf(F("[BootNet] API delayed start (%s, wait ~%lu ms)\n"), reason ? reason : "?",
                  (unsigned long)(s_bootNetApiGateUntilMs - now));
    while (millis() < s_bootNetApiGateUntilMs) {
        lv_timer_handler();
        delay(10);
    }
    Serial.println(F("[BootNet] API start now"));
    s_bootNetApiGateUntilMs = 0;
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

    bootNetWaitApiGateIfNeeded("fetchPrice");

    unsigned long fetchStart = millis();
    float fetched = prices[0]; // Start met huidige waarde als fallback
    bool ok = false;
    bool usedWs = false;

    // WS voorkeur: als er een recente WS prijs is, gebruik die en skip REST
#if WS_ENABLED && WS_LIB_AVAILABLE
    if (wsConnecting && (millis() - wsConnectStartMs) < 15000UL) {
        // Tijdens handshake: vermijd REST connects
        return;
    }
    if (wsConnected && wsLastPriceMs == 0 && (millis() - wsConnectedMs) < 10000UL) {
        // Net verbonden maar nog geen WS prijs: kort wachten om REST conflicts te voorkomen
        return;
    }
    if (wsConnected && wsLastPrice > 0.0f && (millis() - wsLastPriceMs) < 30000UL) {
        fetched = wsLastPrice;
        usedWs = true;
    }
#endif

    bool httpSuccess = false;
    unsigned long fetchTime = 0;
    if (!usedWs) {
    // Fase 4.1.7: Gebruik hoog-niveau fetchBitvavoPrice() method
        httpSuccess = apiClient.fetchBitvavoPrice(bitvavoSymbol, fetched);
        fetchTime = millis() - fetchStart;
    } else {
        httpSuccess = true;
        fetchTime = millis() - fetchStart;
    }
    
    if (!httpSuccess) {
        // Leeg response - kan komen door timeout of netwerkproblemen
        #if !DEBUG_BUTTON_ONLY
        Serial.printf("[API] WARN -> %s leeg response (tijd: %lu ms) - mogelijk timeout of netwerkprobleem\n", bitvavoSymbol, fetchTime);
        #endif
        // Gebruik laatste bekende prijs als fallback (al ingesteld als fetched = prices[0])
    } else {
        // Succesvol opgehaald: alleen REST-prijs afronden (WS blijft ongerond). Niet-BTC: centen i.p.v. hele euro.
        if (!usedWs) {
            fetched = roundRestFetchedQuotePrice(fetched);
        }
        #if !DEBUG_BUTTON_ONLY
        if (!usedWs && fetchTime > 1200) {
            Serial.printf(F("[API] OK -> %s %.2f (tijd: %lu ms) - langzaam\n"), bitvavoSymbol, fetched, fetchTime);
        }
        #endif
        
        // Neem mutex voor data updates (timeout verhoogd om mutex conflicts te verminderen)
        // API task heeft prioriteit: verhoogde timeout om mutex te krijgen zelfs als UI bezig is
        #if defined(PLATFORM_ESP32S3_GEEK)
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(500); // GEEK: 500ms
        #else
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(400); // Standaard S3: 400ms voor betere mutex-acquisitie
        #endif
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        // Fase 4.1: Gebruik geconsolideerde mutex timeout handling
        // Fase 4.2: Gebruik geconsolideerde mutex pattern helper
        static uint32_t mutexTimeoutCount = 0;
        if (safeMutexTake(dataMutex, apiMutexTimeout, usedWs ? "apiTask fetchPrice (ws)" : "apiTask fetchPrice"))
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
            
            // Korte buffers: alleen priceRepeatTask (1 Hz) roept addPriceToSecondArray aan — hier alleen latestKnownPrice
            latestKnownPrice = fetched;
            latestKnownPriceMs = millis();
            latestKnownPriceSource = usedWs ? (uint8_t)LKP_SRC_WS : (uint8_t)LKP_SRC_REST;
            if (fetched > 0.0f) {
                lastFetchedPrice = fetched;
            }
            
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
            
            // Update live availability flags: gebaseerd op data beschikbaarheid EN percentage live data
            // hasRet30mLive: true zodra er minimaal 30 minuten data is EN ≥80% daarvan SOURCE_LIVE is
            uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
            uint8_t livePct30 = calcLivePctMinuteAverages(30);
            hasRet30mLive = (availableMinutes >= 30 && livePct30 >= 80);
            
            // hasRet2hLive: true zodra er minimaal 120 minuten data is EN ≥80% daarvan SOURCE_LIVE is
            uint8_t livePct120 = calcLivePctMinuteAverages(120);
            hasRet2hLive = (availableMinutes >= 120 && livePct120 >= 80);
            
            livePct5m = calcLivePctFiveMinuteWindow();
            hasRet5mLive = (priceData.getFiveMinuteArrayFilled() && livePct5m >= 80);
            
            // Update combined flags: beschikbaar vanuit warm-start OF live data
            hasRet2h = hasRet2hWarm || hasRet2hLive;
            hasRet30m = hasRet30mWarm || hasRet30mLive;
            
            // Bereken ret_30m: herbereken zodra er 30+ minuten data zijn (ook als mix van warm-start en live)
            // Dit zorgt ervoor dat de waarde wordt bijgewerkt naarmate er meer live data binnenkomt
            if (availableMinutes >= 30) {
                // Genoeg data beschikbaar: herbereken met beschikbare data (kan mix zijn)
                ret_30m = calculateReturn30Minutes();
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][30m] Recalculated (regressie): ret_30m=%.4f%%, availableMinutes=%u, livePct=%u%%\n"),
                             ret_30m, availableMinutes, livePct30);
                #endif
            } else if (hasRet30mWarm) {
                // Niet genoeg data, maar warm-start beschikbaar: behoud warm-start waarde
                // ret_30m blijft de warm-start waarde
            } else {
                // Niet genoeg data en geen warm-start: reset naar 0
                ret_30m = 0.0f;
            }
            
            // Update hasRet30m: beschikbaar als warm-start OF live OF 30+ minuten data beschikbaar
            hasRet30m = hasRet30mWarm || hasRet30mLive || (availableMinutes >= 30);
            
            // Live ret_2h: globaal % uit calculateReturn2Hours() (minuteAverages); computeTwoHMetrics schrijft ret_2h niet.
            // Bereken ret_2h: herbereken wanneer live 2h-data beschikbaar is; anders warm-start behouden
            if (hasRet2hLive) {
                ret_2h = calculateReturn2Hours();
            } else if (!hasRet2hWarm) {
                ret_2h = 0.0f;
            }
            
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
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][1d] Live calculation: ret_1d=%.4f%%, availableHours=%u\n"),
                             ret_1d, availableHours);
                #endif
            } else if (!hasRet1dWarm) {
                ret_1d = 0.0f;
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][1d] Reset to 0: hasRet1dWarm=%d, availableHours=%u\n"),
                             hasRet1dWarm ? 1 : 0, availableHours);
                #endif
            } else {
                // Behoud warm-start waarde
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][1d] Keep warm-start: ret_1d=%.4f%%, hasRet1dWarm=%d, availableHours=%u\n"),
                             ret_1d, hasRet1dWarm ? 1 : 0, availableHours);
                #endif
            }
            hasRet7dLive = (availableHours >= HOURS_FOR_7D);
            hasRet7d = hasRet7dWarm || hasRet7dLive;
            if (hasRet7dLive) {
                ret_7d = calculateReturn7Days();
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][7d] Live calculation: ret_7d=%.4f%%, availableHours=%u\n"),
                             ret_7d, availableHours);
                #endif
            } else if (!hasRet7dWarm) {
                ret_7d = 0.0f;
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][7d] Reset to 0: hasRet7dWarm=%d, availableHours=%u\n"),
                             hasRet7dWarm ? 1 : 0, availableHours);
                #endif
            } else {
                // Behoud warm-start waarde
                #if DEBUG_CALCULATIONS
                Serial_printf(F("[API][7d] Keep warm-start: ret_7d=%.4f%%, hasRet7dWarm=%d, availableHours=%u\n"),
                             ret_7d, hasRet7dWarm ? 1 : 0, availableHours);
                #endif
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
                bool wsQualitySkipVolInput = false;
#ifndef ENABLE_WS_SECOND_QUALITY_GUARD_VOLATILITY
#define ENABLE_WS_SECOND_QUALITY_GUARD_VOLATILITY 1
#endif
#if ENABLE_WS_SECOND_QUALITY_GUARD_VOLATILITY
                {
                    uint32_t wsSecTicks = 0;
                    float wsSecSpreadMax = 0.0f;
                    bool wsSecValid = false;
                    bool wsSecFresh = false;
                    if (getWsSecondLastClosedQuality(wsSecTicks, wsSecSpreadMax, wsSecValid, wsSecFresh) &&
                        wsSecValid && wsSecFresh)
                    {
                        wsQualitySkipVolInput =
                            (wsSecTicks < 2U) ||
                            (wsSecSpreadMax > 35.0f);
                    }
                }
#endif

                if (!wsQualitySkipVolInput) {
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
                } else {
#if !DEBUG_BUTTON_ONLY
                    static unsigned long s_lastWsQualitySkipVolLogMs = 0;
                    if (now - s_lastWsQualitySkipVolLogMs >= 5000UL) {
                        s_lastWsQualitySkipVolLogMs = now;
                        Serial_printf(F("[VOL][WS quality guard] Skip 1m input\n"));
                    }
#endif
                }
            }
            
            // Voor weergave op scherm gebruiken we ret_1m en ret_30m
            // Alleen zetten als er data is, anders blijven ze 0.0f (wat wordt geïnterpreteerd als "geen data")
            if (secondArrayFilled) {
                prices[1] = ret_1m;
            } else {
                prices[1] = 0.0f; // Reset naar 0 om aan te geven dat er nog geen data is
            }
            // Update prices array met berekende returns
            if (hasRet30m) {
                prices[2] = ret_30m;
            } else {
                prices[2] = 0.0f; // Reset naar 0 om aan te geven dat er nog geen data is
            }
            
            // 2h return op index 3 (4-symbol platforms)
            #if defined(PLATFORM_ESP32S3_JC3248W535) || defined(PLATFORM_ESP32S3_LCDWIKI_28)
            // ret_2h wordt nu altijd berekend in calculateReturn2Hours(), ook als er minder dan 120 minuten zijn
            // Het berekent een return op basis van beschikbare data (minimaal 2 minuten nodig)
            if (hasRet2h) {
            prices[3] = ret_2h;
            } else {
                prices[3] = 0.0f; // Reset naar 0 om aan te geven dat er nog geen data is
            }
            #endif
            #if defined(PLATFORM_ESP32S3_JC3248W535)
            if (uiFiveMinuteHasMinimalData()) {
                prices[4] = ret_5m;
            } else {
                prices[4] = 0.0f;
            }
            refreshAveragePrice5mForUi();
            if (hasRet1d) {
                prices[5] = ret_1d;
            } else {
                prices[5] = 0.0f;
            }
            refreshAveragePrice1dForUi();
            if (hasRet7d) {
                prices[6] = ret_7d;
            } else {
                prices[6] = 0.0f;
            }
            refreshAveragePrice7dForUi();
            #endif
            
            // Zet flag voor nieuwe data (voor grafiek update)
            newPriceDataAvailable = true;
            
            // Kopieer waarden voor gebruik BUITEN mutex
            float fetchedLocal = fetched;
            float ret1mLocal = ret_1m;
            float ret5mLocal = ret_5m;
            float ret30mLocal = ret_30m;
            float manualAnchorLocal = anchorActive ? anchorPrice : 0.0f;

            // Fase A regime-engine: na return-berekening, vóór alertEngine (geen wijziging aan alertlogica)
            TwoHMetrics regimeTwoH = computeTwoHMetrics();
            regimeEngineTick(millis(), ret_1m, ret_5m, ret_30m, ret_2h,
                             regimeTwoH.rangePct, regimeTwoH.valid);
            
            // Phase 1: Auto-anchor uitgeschakeld (alleen manual anchor)
            safeMutexGive(dataMutex, "fetchPrice");  // MUTEX EERST VRIJGEVEN!
            ok = true;
            
            // Check thresholds and send notifications if needed (met ret_5m voor extra filtering)
            // Fase 6.1.11: Gebruik AlertEngine module i.p.v. globale functie
            alertEngine.checkAndNotify(ret1mLocal, ret5mLocal, ret30mLocal);
            
            // Check anchor take profit / max loss alerts
            // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
            anchorSystem.checkAnchorAlerts();
            
            // Check 2-hour notifications (breakout, breakdown, compression, mean reversion, anchor context)
            // Wordt aangeroepen na elke price update
            AlertEngine::check2HNotifications(fetchedLocal, manualAnchorLocal);
            
            // Publiceer waarden naar MQTT
            publishMqttValues(fetchedLocal, ret1mLocal, ret5mLocal, ret30mLocal);
        } else {
            // Fase 4.1: Geconsolideerde mutex timeout handling
            handleMutexTimeout(mutexTimeoutCount, "API", bitvavoSymbol);
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
// updateFooter() — IP / RSSI / RAM / versie in de footer (platform-afhankelijk)
// ============================================================================

// Helper functie om footer bij te werken
// Fase 8: updateFooter() gebruikt nog globale pointers (kan later naar UIController module verplaatst worden)
void updateFooter()
{
    #if defined(PLATFORM_ESP32S3_GEEK)
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
    
    // GEEK: Update versie label (rechtsonder) - force update als cache leeg is
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
    
    // 2-regel footer: versie label rechtsonder — force update als cache leeg is
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
    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);
    const size_t len = (size_t)w * (size_t)h * sizeof(uint16_t);

    // ESP32-S3: als px_map in PSRAM zit en de display-driver DMA leest, moet de data-cache
    // naar geheugen worden teruggeschreven — anders willekeurige gekleurde blokjes (~cacheline).
#if defined(CRYPTO_ALERT_LVGL_HAS_ESP_CACHE) && (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV))
    if (len > 0 && esp_ptr_external_ram(px_map)) {
        int msync_flags = ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
#if defined(ESP_CACHE_MSYNC_FLAG_UNALIGNED)
        msync_flags |= ESP_CACHE_MSYNC_FLAG_UNALIGNED;
#endif
        (void)esp_cache_msync((void *)px_map, len, msync_flags);
    }
#endif

    if (g_displayBackend) {
        g_displayBackend->flush(area, px_map);
    }

    lv_disp_flush_ready(disp);
}

// Physical button check function (boards met HAS_PHYSICAL_BUTTON)
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
        
        // Phase 1: Anchor set in apiTask context (queue; geen HTTPS in button/UI task)
        if (queueAnchorSetting(0.0f, true)) {
            uiController.updateUI();
        } else {
            Serial_println("[Button] WARN: Kon anchor niet in queue zetten");
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
static unsigned long bootStartMs = 0;
static unsigned long lastBootLogMs = 0;

static void logBootStage(const char* stage)
{
    unsigned long now = millis();
    if (bootStartMs == 0) {
        bootStartMs = now;
    }
    unsigned long sinceStart = now - bootStartMs;
    unsigned long sinceLast = (lastBootLogMs == 0) ? sinceStart : (now - lastBootLogMs);
    lastBootLogMs = now;
    Serial_printf(F("[Boot] %s @%lu ms (+%lu)\n"), stage, sinceStart, sinceLast);
}

static void setupSerialAndDevice()
{
    // ESP32-S3 fix: Serial moet als ALLER EERSTE worden geïnitialiseerd
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK) || defined(ARDUINO_ESP32S3_DEV)
    Serial.begin(115200);
    delay(800); // ESP32-S3 heeft tijd nodig voor Serial stabilisatie
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
    #if !defined(PLATFORM_ESP32S3_SUPERMINI) && !defined(PLATFORM_ESP32S3_LCDWIKI_28) && !defined(PLATFORM_ESP32S3_JC3248W535)
    DEV_DEVICE_INIT();
    #endif
    
    // Initialiseer fysieke reset button (boards met HAS_PHYSICAL_BUTTON)
    #if HAS_PHYSICAL_BUTTON
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    #endif
    delay(2000); // For debugging, give time for the board to reconnect to com port

    Serial_println("Arduino_GFX LVGL_Arduino_v9 example ");
    String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial_println(LVGL_Arduino);
    bootStartMs = millis();
    lastBootLogMs = 0;
    logBootStage("serial+device");
}

static void setupDisplay()
{
    // Init Display (via backend abstraction)
    if (g_displayBackend == nullptr) {
        g_displayBackend = createDisplayBackendForCurrentPlatform();
    }

    uint32_t speed = 0;
#ifdef GFX_SPEED
    speed = GFX_SPEED;
#endif

    if (g_displayBackend == nullptr || !g_displayBackend->begin(speed)) {
#if defined(CRYPTO_ALERT_FORCE_AXS15231B_BACKEND) && defined(PLATFORM_ESP32S3_JC3248W535)
        Serial_println("AXS15231B backend forced, but init failed. Geen fallback (debug).");
        while (true) { /* no need to continue */ }
#else
        // Safe fallback: ensure stable boards don't get broken.
        // For JC3248, the esp_lcd backend may be unavailable in the current Arduino core.
        delete g_displayBackend;
        g_displayBackend = new DisplayBackend_ArduinoGFX();
        if (g_displayBackend == nullptr || !g_displayBackend->begin(speed)) {
            Serial_println("Display backend init failed (Arduino_GFX fallback also failed)!");
            while (true) {
                /* no need to continue */
            }
        }
#endif
    }
    // Pas display rotatie toe (0 = normaal, 2 = 180 graden)
    // Alleen 0 en 2 zijn geldig voor 180 graden rotatie
    uint8_t rotation = (displayRotation == 2) ? 2 : 0;
    g_displayBackend->setRotation(rotation);
#if defined(PLATFORM_ESP32S3_LCDWIKI_28)
    // Arduino_ILI9341::setRotation zet rotatie 0 op MX|BGR; vendor/docs gebruiken BGR-only (0x08) — MX geeft
    // horizontale spiegeling. Overschrijf MADCTL na setRotation (zie docs/ILI9341V_Init.txt LCD_direction).
    extern Arduino_DataBus *bus;
    {
        uint8_t madctl;
        if (rotation == 2) {
            madctl = ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR;
        } else {
            madctl = ILI9341_MADCTL_BGR;
        }
        bus->beginWrite();
        bus->writeC8D8(ILI9341_MADCTL, madctl);
        bus->endWrite();
    }
#endif
    // ESP32-S3 ST7789-boards: standaard geen inversie
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    g_displayBackend->invertDisplay(false); // Super Mini / GEEK: geen inversie nodig (ST7789)
    #elif defined(PLATFORM_LCDWIKI28_INVERT_COLORS)
    g_displayBackend->invertDisplay(true); // LCDWIKI 2.8: kleurinversie nodig
    #else
    g_displayBackend->invertDisplay(false);
    #endif
    g_displayBackend->fillScreen(0 /* RGB565_BLACK */);
    
    // ESP32-S3 fix: Backlight moet opnieuw worden ingesteld na display initialisatie
    // DEV_DEVICE_INIT() wordt eerder aangeroepen, maar ledc kan conflicteren
    // GEEK gebruikt digitalWrite (geen PWM), dus alleen voor SUPERMINI
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_LCDWIKI_28)
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
    
    // Geef display tijd om te stabiliseren na initialisatie (ESP32-S3 / diverse panelen)
    delay(200); // ESP32-S3 heeft extra tijd nodig voor SPI stabilisatie
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

    uint32_t screenWidth = g_displayBackend ? g_displayBackend->width() : 0;
    uint32_t screenHeight = g_displayBackend ? g_displayBackend->height() : 0;
    #ifdef LVGL_SCREEN_WIDTH
        screenWidth = LVGL_SCREEN_WIDTH;
    #endif
    #ifdef LVGL_SCREEN_HEIGHT
        screenHeight = LVGL_SCREEN_HEIGHT;
    #endif
    
    // Detecteer PSRAM beschikbaarheid
    bool psramAvailable = hasPSRAM();

#if defined(PLATFORM_ESP32S3_JC3248W535)
    const bool lvglDrawBufForceInternal = true;
#else
    const bool lvglDrawBufForceInternal = false;
#endif
    
    // Bepaal useDoubleBuffer: board-aware
    bool useDoubleBuffer;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
        useDoubleBuffer = false;
    #else
        // Fallback: double buffer alleen met PSRAM
        useDoubleBuffer = psramAvailable;
    #endif
    
    // Bepaal buffer lines per board
    uint8_t bufLines;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
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
    #else
        // Fallback
        bufLines = psramAvailable ? 30 : 2;
    #endif
    
    // ESP32-S3 met PSRAM (2MB): gebruik volledige frame-buffer als het scherm klein genoeg is
    // Full frame = vloeiendere updates, één flush per frame, minder overhead. Past ruim in 2MB PSRAM.
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    if (psramAvailable) {
        const size_t fullFrameBytes = (size_t)screenWidth * screenHeight * sizeof(lv_color_t) * 2;  // double buffer
        if (fullFrameBytes <= 400000u) {  // max ~400 KB voor full frame (ruim onder 2MB PSRAM)
            bufLines = (uint8_t)screenHeight;
        }
    }
    #endif
    
    uint32_t bufSize = screenWidth * bufLines;
    uint8_t numBuffers = useDoubleBuffer ? 2 : 1;  // 1 of 2 buffers afhankelijk van useDoubleBuffer
    size_t bufSizeBytes = bufSize * sizeof(lv_color_t) * numBuffers;
    bool useFullFrame = (bufLines >= screenHeight);
    
    const char* bufferLocation;
    uint32_t freeHeapBefore = ESP.getFreeHeap();
    size_t largestFreeBlockBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Bepaal board naam voor logging
    const char* boardName;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
        boardName = "ESP32-S3";
    #elif defined(PLATFORM_ESP32S3_GEEK)
        boardName = "ESP32-S3 GEEK";
    #elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
        boardName = "LCDWIKI28";
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
        boardName = "JC3248W535";
    #elif defined(PLATFORM_ESP32S3_AMOLED_206)
        boardName = "AMOLED206";
    #else
        boardName = "Unknown";
    #endif
    
    // Alloceer buffer één keer bij init (niet herhaald)
    if (disp_draw_buf == nullptr) {
        if (psramAvailable && !lvglDrawBufForceInternal) {
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
            bufferLocation = lvglDrawBufForceInternal ? "INTERNAL+DMA (QSPI/DMA-safe)" : "INTERNAL+DMA";
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
    
    // LVGL buffer setup: single of double buffering, full of partial frame
    // LVGL 9.0+ verwacht buffer size in BYTES, niet pixels
    size_t bufSizePixels = bufSize;  // Aantal pixels in buffer
    size_t bufSizeBytesPerBuffer = bufSizePixels * sizeof(lv_color_t);  // Bytes per buffer
    lv_display_render_mode_t renderMode = useFullFrame ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL;
    
    void *buf2 = nullptr;
    if (useDoubleBuffer) {
        buf2 = (uint8_t *)disp_draw_buf + bufSizeBytesPerBuffer;
    }
    lv_display_set_buffers(disp, disp_draw_buf, buf2, bufSizeBytesPerBuffer, renderMode);
}

static void setupWatchdog()
{
    // Standaard task watchdog (ESP32-S3, …): timeout 10 seconden.
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
                    strncpy(pendingIpBuffer, ipBuffer, sizeof(pendingIpBuffer) - 1);
                    pendingIpBuffer[sizeof(pendingIpBuffer) - 1] = '\0';
                    pendingIpPublish = true;
                }
                wifiReconnectEnabled = false;
                wifiInitialized = true;
                reconnectAttemptCount = 0; // Reset reconnect counter bij succesvolle verbinding
                if (apStartedForReconnect) {
                    WiFi.mode(WIFI_STA); // AP uitzetten na verbinding
                    apStartedForReconnect = false;
                }
                // Start MQTT connectie na WiFi verbinding
                if (!mqttConnected) {
                    requestMqttReconnect();
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

    // Notification log mutex (try-lock only in writer; reader mag korte timeout gebruiken)
    s_notifLogMutex = xSemaphoreCreateMutex();
    if (s_notifLogMutex == NULL) {
        Serial.println(F("[Notify] Log mutex niet aangemaakt; notification log append uit."));
    }

    // NTFY pending queue mutex (noodpatch)
    s_ntfyQMutex = xSemaphoreCreateMutex();
    if (s_ntfyQMutex == NULL) {
        Serial.println(F("[NTFY][Q] WARN: mutex not created; pending queue disabled"));
    }
}

// Alloceer ringbuffer-arrays (alle platforms): met PSRAM in SPIRAM, zonder PSRAM in INTERNAL heap
static void allocateDynamicArrays()
{
    if (fiveMinutePrices == nullptr) {
        const uint32_t caps = hasPSRAM() ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        fiveMinutePrices = (float *)heap_caps_malloc(SECONDS_PER_5MINUTES * sizeof(float), caps);
        fiveMinutePricesSource = (DataSource *)heap_caps_malloc(SECONDS_PER_5MINUTES * sizeof(DataSource), caps);
        minuteAverages = (float *)heap_caps_malloc(MINUTES_FOR_30MIN_CALC * sizeof(float), caps);
        minuteAveragesSource = (DataSource *)heap_caps_malloc(MINUTES_FOR_30MIN_CALC * sizeof(DataSource), caps);

        if (!fiveMinutePrices || !fiveMinutePricesSource || !minuteAverages || !minuteAveragesSource) {
            Serial.println(F("[Memory] FATAL: Ringbuffer array allocatie gefaald!"));
            Serial.printf("[Memory] Free heap: %u bytes\n", ESP.getFreeHeap());
            while (true) {
                /* no need to continue */
            }
        }

        for (uint16_t i = 0; i < SECONDS_PER_5MINUTES; i++) {
            fiveMinutePrices[i] = 0.0f;
            fiveMinutePricesSource[i] = SOURCE_LIVE;
        }
        for (uint8_t i = 0; i < MINUTES_FOR_30MIN_CALC; i++) {
            minuteAverages[i] = 0.0f;
            minuteAveragesSource[i] = SOURCE_LIVE;
        }

        Serial.printf("[Memory] Ringbuffers: fiveMinutePrices=%u, minuteAverages=%u bytes (%s)\n",
                     (unsigned)(SECONDS_PER_5MINUTES * sizeof(float) + SECONDS_PER_5MINUTES * sizeof(DataSource)),
                     (unsigned)(MINUTES_FOR_30MIN_CALC * sizeof(float) + MINUTES_FOR_30MIN_CALC * sizeof(DataSource)),
                     hasPSRAM() ? "PSRAM" : "DRAM");
    }

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
    if (hourlyAverages != nullptr && hourlyAveragesSource == nullptr) {
        if (hasPSRAM()) {
            hourlyAveragesSource = (DataSource *)heap_caps_malloc(HOURS_FOR_7D * sizeof(DataSource), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (!hourlyAveragesSource) {
            hourlyAveragesSource = (DataSource *)heap_caps_malloc(HOURS_FOR_7D * sizeof(DataSource), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (!hourlyAveragesSource) {
            Serial.println(F("[Memory] WARN: hourlyAveragesSource alloc failed; 1d/7d TF-kleur valt terug"));
        } else {
            for (uint16_t i = 0; i < HOURS_FOR_7D; i++) {
                hourlyAveragesSource[i] = SOURCE_BINANCE;
            }
            Serial.printf("[Memory] hourlyAveragesSource=%u bytes\n",
                          (unsigned)(HOURS_FOR_7D * sizeof(DataSource)));
        }
    }
}

static void startFreeRTOSTasks()
{
    // M1: Heap telemetry vóór startFreeRTOSTasks
    logHeap("TASKS_START_PRE");
    
    // FreeRTOS Tasks voor multi-core processing
    // ESP32-S3 heeft mogelijk meer stack ruimte nodig
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    const uint32_t apiTaskStack = 10240;  // ESP32-S3: meer stack voor API task
    const uint32_t uiTaskStack = 10240;   // ESP32-S3: meer stack voor UI task
    const uint32_t webTaskStack = 6144;   // ESP32-S3: meer stack voor web task
    #else
    const uint32_t apiTaskStack = 8192;   // ESP32: standaard stack
    const uint32_t uiTaskStack = 8192;    // ESP32: standaard stack
    const uint32_t webTaskStack = 5120;   // ESP32: verhoogd voor debug logging (was 4096)
    #endif
    
    // Core 1: API calls (elke seconde)
    xTaskCreatePinnedToCore(
        apiTask,           // Task function
        "API_Task",        // Task name
        apiTaskStack,      // Stack size (platform-specifiek)
        NULL,              // Parameters
        1,                 // Priority
        &s_apiTaskHandle,  // Task handle (o.a. ntfy wake na enqueue)
        1                  // Core 1
    );

    // Core 1: Periodieke prijs herhaling (elke 2 seconden) - onafhankelijk van API calls
    // Stack size moet groot genoeg zijn voor mutex calls en Serial.printf
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    const uint32_t priceRepeatTaskStack = 4096; // ESP32-S3: meer stack
    #else
    const uint32_t priceRepeatTaskStack = 3072; // ESP32: voldoende stack voor mutex en debug logging
    #endif
    xTaskCreatePinnedToCore(
        priceRepeatTask,   // Task function
        "PriceRepeat",     // Task name
        priceRepeatTaskStack, // Stack size
        NULL,              // Parameters
        1,                 // Priority (zelfde als API task)
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
    logBootStage("after serial+device");
    setupDisplay();
    logBootStage("after display");
    // Fase 8: LVGL initialisatie via UIController module
    uiController.setupLVGL();
    logBootStage("after lvgl");
    setupWatchdog();
    logBootStage("after watchdog");
    setupWiFiEventHandlers();
    logBootStage("after wifi events");
    setupMutex();  // Mutex moet vroeg aangemaakt worden, maar tasks starten later
    logBootStage("after mutex");
    
    // Ringbuffers en gerelateerde heap-arrays voor alle platforms (vroeg in setup, vóór verdere init)
    allocateDynamicArrays();
    logBootStage("after arrays");
    
    // Allocate Bitvavo streaming buffer on heap (fallback naar static)
    if (bitvavoStreamBuffer == bitvavoStreamBufferFallback) {
        void* buf = nullptr;
        if (hasPSRAM()) {
            buf = heap_caps_malloc(BITVAVO_STREAM_BUFFER_HEAP_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (buf == nullptr) {
            buf = heap_caps_malloc(BITVAVO_STREAM_BUFFER_HEAP_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (buf != nullptr) {
            bitvavoStreamBuffer = reinterpret_cast<char*>(buf);
            bitvavoStreamBufferSize = BITVAVO_STREAM_BUFFER_HEAP_SIZE;
            Serial.printf("[Memory] Bitvavo stream buffer heap: %u bytes\n", (unsigned)bitvavoStreamBufferSize);
        } else {
            Serial.printf("[Memory] Bitvavo stream buffer fallback: %u bytes\n", (unsigned)bitvavoStreamBufferSize);
        }
    }
    
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
    logBootStage("after wifi+initial price");
    
    // Diagnostiek: periodic-test timer alleen als runtime-diagnostiek + periodic sub-vlag aan staan.
    #if CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME && CRYPTO_ALERT_NTFY_PERIODIC_TEST
    ntfyPeriodicTestNextMs = millis() + CRYPTO_ALERT_NTFY_PERIODIC_TEST_MS;
    #endif
    
    // Fase 4.2.5: Synchroniseer PriceData state na warm-start (als warm-start is uitgevoerd)
    priceData.syncStateFromGlobals();
    
    // Fase 7.2: Bind WarmStartWrapper dependencies
    CryptoMonitorSettings currentSettings;
    currentSettings.warmStartEnabled = warmStartEnabled;
    currentSettings.warmStart1mExtraCandles = warmStart1mExtraCandles;
    currentSettings.warmStart5mCandles = warmStart5mCandles;
    currentSettings.warmStart30mCandles = warmStart30mCandles;
    currentSettings.warmStart2hCandles = warmStart2hCandles;
    currentSettings.warmStartSkip1m = warmStartSkip1m;
    currentSettings.warmStartSkip5m = warmStartSkip5m;
    warmWrap.bindSettings(&currentSettings);
    warmWrap.bindLogger(&Serial);
    
    // Fase 7.2b: Warm-start exclusiviteit
    // Warm-start is de enige schrijver tijdens setup (tasks bestaan nog niet, geen race conditions mogelijk)
    // Warm-start: Vul buffers met Binance historische data (als WiFi verbonden is)
    if (WiFi.status() == WL_CONNECTED && warmStartEnabled) {
        warmWrap.beginRun();
        
        // Bereken requested counts (voor logging)
        bool psramAvailable = hasPSRAM();
        uint16_t req1mCandles = warmStartSkip1m ? 0 : calculate1mCandles();
        uint16_t max5m = psramAvailable ? 24 : 12;
        uint16_t max30m = psramAvailable ? 12 : 6;
        uint16_t max2h = psramAvailable ? 8 : 4;
        #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
            if (psramAvailable) {
                max5m = 60;
                max30m = 24;
                max2h = 12;
            }
        #endif
        uint16_t req5mCandles = warmStartSkip5m ? 0 : clampUint16(warmStart5mCandles, 2, max5m);
        uint16_t req30mCandles = clampUint16(warmStart30mCandles, 2, max30m);
        uint16_t req2hCandles = clampUint16(warmStart2hCandles, 2, max2h);
        
        WarmStartMode mode = performWarmStart();
        warmWrap.endRun(mode, warmStartStats, warmStartStatus, 
                       ret_2h, ret_30m, hasRet2h, hasRet30m,
                       req1mCandles, req5mCandles, req30mCandles, req2hCandles);
    }
    logBootStage("after warmstart");
    
    // Diagnostiek: startup-NTFY-test na warmstart (no-op tenzij CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME + STARTUP_TEST).
    ntfyStartupTestIfEnabled();
    
    // WS init wordt uitgesteld tot na de eerste succesvolle API-prijs
    
    Serial_println("Setup done");
    fetchPrice();
    logBootStage("after fetchPrice");
    
    // Build main UI (verwijdert WiFi UI en bouwt hoofd UI)
    // Fase 8.4.3: Gebruik module versie
    uiController.buildUI();
    logBootStage("after buildUI");
    
    // Force LVGL to render immediately after UI creation
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        delay(DELAY_LVGL_RENDER_MS);
    }
    logBootStage("after first render");
    
    // Start FreeRTOS tasks NA buildUI() en NA warm-start
    // Warm-start heeft exclusieve toegang tijdens setup (geen race conditions mogelijk)
    startFreeRTOSTasks();
    logBootStage("after tasks");

    {
        const unsigned long t = millis();
        s_bootNetMqttGateUntilMs = t + CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS;
        s_bootNetWsGateUntilMs = t + CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS + CRYPTO_ALERT_BOOTNET_WS_EXTRA_DELAY_MS;
        Serial.printf(
            F("[BootNet] staged startup: MQTT +%lums, WS +%lums (extra) after setup\n"),
            (unsigned long)CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS,
            (unsigned long)(CRYPTO_ALERT_BOOTNET_MQTT_DELAY_MS + CRYPTO_ALERT_BOOTNET_WS_EXTRA_DELAY_MS));
    }
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
    
    String apSSID = "CryptoAlert";
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
        
        Serial_printf("AP IP: %s\n", apIP);
        
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
        s_bootNetApiGateUntilMs = millis() + CRYPTO_ALERT_BOOTNET_API_SETTLE_MS;
        Serial.printf(
            F("[BootNet] API delayed start +%lums (settle before first fetch)\n"),
            (unsigned long)CRYPTO_ALERT_BOOTNET_API_SETTLE_MS);
        // Toon verbindingsinfo (SSID en IP) en "Opening Bitvavo Session"
        showConnectionInfo();

        // Haal eerste prijs op (fetchPrice wacht intern op API-gate indien actief)
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

// FreeRTOS Task: API calls op Core 1 (interval = UPDATE_API_INTERVAL)
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
    
    static bool wsInitAttempted = false;
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
            if (g_netExclusiveNtfyMode != NET_MODE_NORMAL) {
                apiTaskNtfyExclusiveStateMachine();
            } else {
                if (ntfyHasFlushablePendingForExclusive()) {
                    Serial_println(F("[NTFY][EXCL] enter"));
                    if (ntfyExclusiveShouldSkipWsStopPhase()) {
                        g_netExclusiveNtfyMode = NET_MODE_NTFY_EXCLUSIVE_SENDING;
                        s_netExclusiveDeadlineMs = millis() + NTFY_EXCL_SEND_MS;
                        s_ntfyExclusiveSendDoneThisCycle = false;
                    } else {
                        g_netExclusiveNtfyMode = NET_MODE_NTFY_EXCLUSIVE_STOPPING_WS;
                        s_netExclusiveDeadlineMs = millis() + NTFY_EXCL_WS_STOP_MS;
                        wsStopForNtfyExclusive();
                    }
                } else {
                    // Voer 1 API call uit (alleen buiten NTFY-exclusive slot)
                    fetchPrice();

                    // WS init pas na eerste succesvolle API-prijs (en warm-start klaar), en na boot-net gate (na MQTT-fase).
                    if (!wsInitAttempted && lastApiMs > 0) {
                        if (s_bootNetWsGateUntilMs != 0 && millis() < s_bootNetWsGateUntilMs) {
                            static bool s_bootNetWsHoldLogged = false;
                            if (!s_bootNetWsHoldLogged) {
                                Serial.printf(
                                    F("[BootNet] WS delayed start (wait ~%lu ms)\n"),
                                    (unsigned long)(s_bootNetWsGateUntilMs - millis()));
                                s_bootNetWsHoldLogged = true;
                            }
                        } else {
                            if (s_bootNetWsGateUntilMs != 0) {
                                Serial.println(F("[BootNet] WS start now"));
                                s_bootNetWsGateUntilMs = 0;
                            }
                            maybeInitWebSocketAfterWarmStart();
                            wsInitAttempted = true;
                        }
                    }
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
        
        // Duration-aware timing: wacht tot volgende interval OF vroege wake (ntfy enqueue leeg→pending).
        // ulTaskNotifyTake verkort queue_wait_ms t.o.v. blind wachten op volledige apiIntervalMs.
        if (waitMs > 0) {
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
        } else {
            vTaskDelay(1);
        }
    }
}

// FreeRTOS Task: 1 Hz sampler — enige schrijver naar secondPrices/fiveMinutePrices (addPriceToSecondArray)
// Bron: primair laatst afgesloten WS-seconde-close, fallback latestKnownPrice; API-poll blijft UPDATE_API_INTERVAL
void priceRepeatTask(void *parameter)
{
    // Wacht tot WiFi verbonden is
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    Serial.println(F("[PriceSample] 1 Hz task gestart (PRICE_SAMPLE_INTERVAL_MS)"));
    
    for (;;)
    {
        if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "priceRepeatTask")) {
            float p = latestKnownPrice;
            const uint32_t nowBucket = (uint32_t)(millis() / 1000UL);
            if (wsSecondAggLastClosed.valid) {
                const uint32_t ageBuckets = (nowBucket >= wsSecondAggLastClosed.secondBucket)
                    ? (nowBucket - wsSecondAggLastClosed.secondBucket)
                    : (UINT32_MAX - wsSecondAggLastClosed.secondBucket + nowBucket + 1U);
                if (ageBuckets <= 1U && wsSecondAggLastClosed.secondClose > 0.0f) {
                    p = wsSecondAggLastClosed.secondClose;
                }
            }
            if (p > 0.0f) {
                priceData.addPriceToSecondArray(p);
            }
            safeMutexGive(dataMutex, "priceRepeatTask");
        }

        vTaskDelay(pdMS_TO_TICKS(PRICE_SAMPLE_INTERVAL_MS));
    }
}

// FreeRTOS Task: UI updates op Core 0 (elke seconde)
void uiTask(void *parameter)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(UPDATE_UI_INTERVAL);
    // LVGL task handler: vaker aanroepen voor vloeiende rendering op ESP32-S3
    const TickType_t lvglFrequency = pdMS_TO_TICKS(3); // elke 3 ms
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
        
        // Apply deferred display rotation on UI core
        applyPendingDisplayRotation();
        
        // Roep LVGL task handler regelmatig aan om IDLE task tijd te geven
        TickType_t currentTime = xTaskGetTickCount();
        if ((currentTime - lastLvglTime) >= lvglFrequency) {
            lv_task_handler();
            lastLvglTime = currentTime;
        }
        
        // Geoptimaliseerd: betere mutex timeout handling
        // UI task heeft lagere prioriteit: kortere timeout zodat API task voorrang krijgt
        // Als mutex niet beschikbaar is, skip deze update (UI kan volgende keer opnieuw proberen)
        const TickType_t mutexTimeout = pdMS_TO_TICKS(50); // Korte timeout: API-task heeft voorrang
        
        // UI mag niet lang in een mutex zitten (LVGL kan blocken).
        // We checken alleen kort of er geen writer actief is, en updaten daarna zonder lock.
        if (dataMutex == nullptr || xSemaphoreTake(dataMutex, 0) == pdTRUE) {
            if (dataMutex != nullptr) {
                xSemaphoreGive(dataMutex);
            }
            // Fase 8.8.1: Gebruik module versie (parallel - oude functie blijft bestaan)
            uiController.updateUI();
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
        
        // Check physical button (boards met HAS_PHYSICAL_BUTTON)
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

    // Tijdens NTFY exclusive: geen uitgaande MQTT/diag/WS uit loop() — apiTask regisseert WS-pump daar.
    if (g_netExclusiveNtfyMode == NET_MODE_NORMAL) {
        // Diagnostiek (standaard uit): deferred startup retry / periodic test / WS-live health ping — zie CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME
        ntfyDeferredStartupTestIfPending();
        ntfyPeriodicTestIfEnabled();
        ntfyWsLiveHealthPingIfDue();

        // Deferred MQTT reconnect (thread-safe)
        applyPendingMqttReconnect();

        // Deferred IP publish (thread-safe, avoids arduino_events stack use)
        if (pendingIpPublish && mqttConnected) {
            char topicBuffer[128];
            char mqttPrefixTemp[64];
            getMqttTopicPrefix(mqttPrefixTemp, sizeof(mqttPrefixTemp));
            snprintf(topicBuffer, sizeof(topicBuffer), "%s/values/ip_address", mqttPrefixTemp);
            mqttClient.publish(topicBuffer, pendingIpBuffer, false);
            pendingIpPublish = false;
        }

        // MQTT loop (moet regelmatig worden aangeroepen)
        if (mqttConnected) {
            if (!mqttClient.loop()) {
                // Verbinding verloren, probeer reconnect met backoff
                mqttConnected = false;
                lastMqttReconnectAttempt = millis(); // voorkom immediate reconnect storm
                // mqttReconnectAttemptCount wordt NIET gereset, zodat exponential backoff blijft werken
            } else {
                // Process queued messages when connected
                processMqttQueue();
            }
        } else if (WiFi.status() == WL_CONNECTED) {
            unsigned long now = millis();
            // Boot: MQTT-connect nog niet — wacht tot gate voorbij is (geen reconnect-teller verhogen).
            if (s_bootNetMqttGateUntilMs != 0 && now < s_bootNetMqttGateUntilMs) {
                static bool s_bootNetMqttHoldLogged = false;
                if (!s_bootNetMqttHoldLogged) {
                    Serial.printf(
                        F("[BootNet] MQTT delayed start (wait ~%lu ms)\n"),
                        (unsigned long)(s_bootNetMqttGateUntilMs - now));
                    s_bootNetMqttHoldLogged = true;
                }
            } else {
                // Probeer MQTT reconnect als WiFi verbonden is (met exponential backoff)
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
        }
    }

#if WS_ENABLED && WS_LIB_AVAILABLE
    if (g_netExclusiveNtfyMode == NET_MODE_NORMAL && wsInitialized && wsClientPtr != nullptr) {
        if (WiFi.status() == WL_CONNECTED) {
            if (wsPauseForNtfySend) {
                static unsigned long s_wsPauseInNormalLogMs = 0;
                const unsigned long n = millis();
                if (n - s_wsPauseInNormalLogMs >= 15000UL) {
                    s_wsPauseInNormalLogMs = n;
                    Serial_println(F("[WS][WARN] wsPauseForNtfySend while NET_MODE_NORMAL"));
                }
            }
            // Regie: tijdens (re)connect nemen we de netwerkmutex zodat NTFY/MQTT/API niet tegelijk connect zware acties doen.
            static unsigned long wsReconnectLastPollMs = 0;
            static unsigned long wsReconnectLastWaitLogMs = 0;
            static unsigned long wsReconnectSessionStartMs = 0;
            static bool wsReconnectSessionActive = false;
            static bool wsReconnectTimeoutLogged = false;

            const unsigned long WS_RECONNECT_POLL_THROTTLE_MS = 250UL;
            const unsigned long WS_RECONNECT_TIMEOUT_MS = 15000UL;
            const unsigned long WS_RECONNECT_WAIT_LOG_EVERY_MS = 3000UL;

            if (wsPauseForNtfySend) {
                // Pauze: we doen geen wsClientPtr->loop() calls om NTFY/HTTP volledig te isoleren.
                wsReconnectSessionActive = false;
            } else if (!wsConnected) {
                const unsigned long nowWs = millis();

                if (wsReconnectSessionActive && !wsConnecting) {
                    // Reconnect sessie gestopt door event handler; geen extra reconnect-logging meer.
                    wsReconnectSessionActive = false;
                    wsReconnectTimeoutLogged = false;
                }

                if (wsConnecting && !wsReconnectSessionActive) {
                    wsReconnectSessionActive = true;
                    wsReconnectSessionStartMs = nowWs;
                    wsReconnectTimeoutLogged = false;
                    wsReconnectLastWaitLogMs = 0;
                    Serial_println(F("[WS] reconnect start"));
                }

                if (wsReconnectSessionActive && wsConnecting && !wsReconnectTimeoutLogged) {
                    if ((nowWs - wsReconnectSessionStartMs) >= WS_RECONNECT_TIMEOUT_MS) {
                        Serial_println(F("[WS] reconnect timeout"));
                        wsReconnectTimeoutLogged = true;
                    } else if ((nowWs - wsReconnectLastWaitLogMs) >= WS_RECONNECT_WAIT_LOG_EVERY_MS) {
                        wsReconnectLastWaitLogMs = nowWs;
                        Serial_println(F("[WS] reconnect waiting..."));
                    }
                }

                // Throttle wsClientPtr->loop() tijdens reconnect zodat we niet in een te strakke lus hangen.
                if ((nowWs - wsReconnectLastPollMs) >= WS_RECONNECT_POLL_THROTTLE_MS) {
                    netMutexLock("[WS] reconnect loop");
                    wsClientPtr->loop();
                    netMutexUnlock("[WS] reconnect loop");
                    wsReconnectLastPollMs = nowWs;
                }
                if (lastWsDisconnectMs != 0 && (nowWs - lastWsDisconnectMs) > 45000UL) {
                    static unsigned long s_wsDiscStallLogMs = 0;
                    if (nowWs - s_wsDiscStallLogMs >= 30000UL) {
                        s_wsDiscStallLogMs = nowWs;
                        Serial_printf(
                            F("[WS][stall] disc_age_ms=%lu excl=%u pause=%u cg=%d cn=%d\n"),
                            (unsigned long)(nowWs - lastWsDisconnectMs),
                            (unsigned)g_netExclusiveNtfyMode,
                            (unsigned)(wsPauseForNtfySend ? 1u : 0u),
                            wsConnecting ? 1 : 0,
                            wsConnected ? 1 : 0);
                    }
                }
            } else {
                if (wsReconnectSessionActive) {
                    Serial_println(F("[WS] reconnect complete"));
                    wsReconnectSessionActive = false;
                    wsReconnectTimeoutLogged = false;
                }
                // WS steady-state loop: alleen draaien als netwerkmutex beschikbaar is.
                if (netMutexTryLock()) {
                    wsClientPtr->loop();
                    // Silent unlock to avoid per-iteration [NET] release logspam.
                    if (gNetMutex != NULL) {
                        xSemaphoreGive(gNetMutex);
                    }
                }
            }
            if (wsPending) {
                wsPending = false;
                processWsTextMessage(wsPendingBuf, wsPendingLen);
            }
        }
    }
#endif
    
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
                if (apStartedForReconnect) {
                    WiFi.mode(WIFI_STA); // AP uitzetten na succesvolle reconnect
                    apStartedForReconnect = false;
                }
                // Probeer MQTT reconnect na WiFi reconnect
                if (!mqttConnected) {
                    connectMQTT();
                }
            } else {
                Serial.printf("[WiFi] Reconnect timeout (poging %u)\n", reconnectAttemptCount);
                // Na veel mislukte reconnects: start CryptoAlert AP zodat gebruiker WiFi opnieuw kan instellen
                if (reconnectAttemptCount >= RECONNECT_ATTEMPTS_BEFORE_AP && !apStartedForReconnect) {
                    apStartedForReconnect = true;
                    WiFi.mode(WIFI_AP_STA);
                    WiFi.softAP("CryptoAlert", "");
                    Serial.println(F("[WiFi] CryptoAlert AP gestart (192.168.4.1) – verbind om WiFi opnieuw in te stellen"));
                } else if (reconnectAttemptCount >= MAX_RECONNECT_ATTEMPTS) {
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
