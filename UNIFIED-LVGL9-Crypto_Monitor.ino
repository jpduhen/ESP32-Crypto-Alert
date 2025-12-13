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
#define VERSION_MINOR 68
#define VERSION_STRING "3.68"

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
static lv_obj_t *volatilityLabel; // Label voor volatiliteit weergave

static uint32_t lastApiMs = 0; // Time of last api call

// CPU usage measurement (alleen voor web interface)
static float cpuUsagePercent = 0.0f;
static unsigned long loopTimeSum = 0;
static uint16_t loopCount = 0;
static const unsigned long LOOP_PERIOD_MS = UPDATE_UI_INTERVAL; // 1000ms

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

// Settings structs voor betere organisatie
struct AlertThresholds {
    float spike1m;
    float spike5m;
    float move30m;
    float move5m;
    float move5mAlert;
    float threshold1MinUp;
    float threshold1MinDown;
    float threshold30MinUp;
    float threshold30MinDown;
};

struct NotificationCooldowns {
    unsigned long cooldown1MinMs;
    unsigned long cooldown30MinMs;
    unsigned long cooldown5MinMs;
};

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
        Serial_printf("[Anchor Queue] WARN: Ongeldige waarde: %.2f\n", value);
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

// Simple HTTP GET – returns body in buffer, returns true on success
// Geoptimaliseerd: gebruik char array i.p.v. String om geheugenfragmentatie te voorkomen
// bufferSize moet groot genoeg zijn voor de response (minimaal 256 bytes aanbevolen)
// Retry logic: probeer opnieuw bij tijdelijke fouten (timeout, connection refused/lost)
static bool httpGET(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs = HTTP_TIMEOUT_MS)
{
    if (buffer == nullptr || bufferSize == 0) {
        Serial_printf("[HTTP] Invalid buffer parameters\n");
        return false;
    }
    
    buffer[0] = '\0'; // Initialize buffer
    
    const uint8_t MAX_RETRIES = 2; // Max 2 retries (3 pogingen totaal)
    const uint32_t RETRY_DELAY_MS = 500; // 500ms delay tussen retries
    
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        HTTPClient http;
        http.setTimeout(timeoutMs);
        http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS); // Sneller falen bij connect problemen
        // Connection reuse uitgeschakeld - veroorzaakte eerder problemen
        // Bij retries altijd false voor betrouwbaarheid
        http.setReuse(false);
        
        unsigned long requestStart = millis();
        
        if (!http.begin(url))
        {
            http.end();
            if (attempt == MAX_RETRIES) {
                Serial_printf("[HTTP] http.begin() gefaald na %d pogingen voor: %s\n", attempt + 1, url);
                return false;
            }
            // Retry bij begin failure
            delay(RETRY_DELAY_MS);
            continue;
        }
        
        int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
        
        if (code == 200)
        {
            // Get response size first
            int contentLength = http.getSize();
            if (contentLength > 0 && (size_t)contentLength >= bufferSize) {
                Serial_printf("[HTTP] Response te groot: %d bytes (buffer: %zu bytes)\n", contentLength, bufferSize);
                http.end();
                return false;
            }
            
            // Read response into buffer
            WiFiClient *stream = http.getStreamPtr();
            if (stream != nullptr) {
                size_t bytesRead = 0;
                while (stream->available() && bytesRead < bufferSize - 1) {
                    int c = stream->read();
                    if (c < 0) break;
                    buffer[bytesRead++] = (char)c;
                }
                buffer[bytesRead] = '\0';
                
                // Performance monitoring: log langzame calls
                // Normale calls zijn < 500ms, langzaam is > 1000ms
                if (requestTime > 1000) {
                    Serial_printf("[HTTP] Langzame response: %lu ms (poging %d)\n", requestTime, attempt + 1);
                }
            } else {
                // Fallback: gebruik getString() als stream niet beschikbaar is
                String payload = http.getString();
                size_t len = payload.length();
                if (len >= bufferSize) {
                    Serial_printf("[HTTP] Response te groot: %zu bytes (buffer: %zu bytes)\n", len, bufferSize);
                    http.end();
                    return false;
                }
                strncpy(buffer, payload.c_str(), bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
            }
            
            http.end();
            if (attempt > 0) {
                Serial_printf("[HTTP] Succes na retry (poging %d/%d)\n", attempt + 1, MAX_RETRIES + 1);
            }
            return true;
        }
        else
        {
            // Check of dit een retry-waardige fout is
            bool shouldRetry = false;
            if (code == HTTPC_ERROR_CONNECTION_REFUSED || code == HTTPC_ERROR_CONNECTION_LOST) {
                shouldRetry = true;
                Serial_printf("[HTTP] Connectie probleem: code=%d, tijd=%lu ms (poging %d/%d)\n", code, requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code == HTTPC_ERROR_READ_TIMEOUT) {
                shouldRetry = true;
                Serial_printf("[HTTP] Read timeout na %lu ms (poging %d/%d)\n", requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code == HTTPC_ERROR_SEND_HEADER_FAILED || code == HTTPC_ERROR_SEND_PAYLOAD_FAILED) {
                // Send failures kunnen tijdelijk zijn - retry waardig
                shouldRetry = true;
                Serial_printf("[HTTP] Send failure: code=%d, tijd=%lu ms (poging %d/%d)\n", code, requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code < 0) {
                // Andere HTTPClient error codes - retry alleen bij network errors
                shouldRetry = (code == HTTPC_ERROR_CONNECTION_REFUSED || 
                              code == HTTPC_ERROR_CONNECTION_LOST || 
                              code == HTTPC_ERROR_READ_TIMEOUT ||
                              code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                              code == HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                if (!shouldRetry) {
                    Serial_printf("[HTTP] Error code=%d, tijd=%lu ms (geen retry)\n", code, requestTime);
                } else {
                    Serial_printf("[HTTP] Error code=%d, tijd=%lu ms (poging %d/%d)\n", code, requestTime, attempt + 1, MAX_RETRIES + 1);
                }
            } else {
                // HTTP error codes (4xx, 5xx) - geen retry
                Serial_printf("[HTTP] GET gefaald: code=%d, tijd=%lu ms (geen retry)\n", code, requestTime);
            }
            
            http.end();
            
            // Retry bij tijdelijke fouten
            if (shouldRetry && attempt < MAX_RETRIES) {
                delay(RETRY_DELAY_MS);
                continue;
            }
            
            // Geen retry meer mogelijk of niet-retry-waardige fout
            return false;
        }
    }
    
    return false;
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
// ============================================================================
// Helper Functions
// ============================================================================

// Helper: Validate if price is valid (not NaN, Inf, or <= 0)
static bool isValidPrice(float price)
{
    return !isnan(price) && !isinf(price) && price > 0.0f;
}

// Helper: Validate if two prices are valid
static bool areValidPrices(float price1, float price2)
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
        Serial_printf("[Validation] Invalid float value (NaN/Inf): %s\n", str);
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
        Serial_printf("[Mutex] ERROR: Attempt to take nullptr mutex in %s\n", context);
        return false;
    }
    
    // Check if mutex is already held for too long (potential deadlock)
    if (mutexHolderContext != nullptr && mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf("[Mutex] WARNING: Potential deadlock detected! Mutex held for %lu ms by %s\n", 
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
        Serial_printf("[Mutex] ERROR: Attempt to give nullptr mutex in %s\n", context);
        return false;
    }
    
    // Check if mutex was held for too long (potential deadlock)
    if (mutexTakeTime > 0) {
        unsigned long holdTime = millis() - mutexTakeTime;
        if (holdTime > MAX_MUTEX_HOLD_TIME_MS) {
            Serial_printf("[Mutex] WARNING: Mutex held for %lu ms by %s (potential deadlock)\n", 
                         holdTime, mutexHolderContext ? mutexHolderContext : "unknown");
        }
    }
    
    BaseType_t result = xSemaphoreGive(mutex);
    if (result != pdTRUE) {
        Serial_printf("[Mutex] ERROR: xSemaphoreGive failed in %s (result=%d)\n", context, result);
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

// ============================================================================
// Utility Functions
// ============================================================================

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
                Serial_printf("[Anchor] Gebruik huidige prijs als anchor: %.2f\n", priceToSet);
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
        Serial_printf("[Anchor] Anchor set: anchorPrice = %.2f\n", anchorPrice);
        
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
        Serial_printf("[MQTT] Anchor event gepubliceerd: %s (prijs: %.2f, event: %s)\n", 
                     timeStr, anchor_price, event_type);
    } else {
        // Queue message if not connected or publish failed
        enqueueMqttMessage(topic, payload, false);
        Serial_printf("[MQTT] Anchor event in queue: %s (prijs: %.2f, event: %s)\n", 
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
    
    Serial_printf("[Volatility] σ=%.4f%%, volFactor=%.3f, thresholds: 1m=%.3f%%, 5m=%.3f%%, 30m=%.3f%%\n",
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
    
    Serial_printf("[Confluence] Alert verzonden: 1m=%.2f%%, 5m=%.2f%%, trend=%s, ret_30m=%.2f%%\n",
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
        Serial_printf("[Anchor] Take profit notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n", 
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
        Serial_printf("[Anchor] Max loss notificatie verzonden: %.2f%% (threshold: %.2f%%, basis: %.2f%%, trend: %s, anchor: %.2f, prijs: %.2f)\n", 
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
    Serial_printf("[Notify] %s notificatie verstuurd (%d/%d dit uur)\n", alertType, alertsThisHour, maxAlertsPerHour);
    
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
        Serial_printf("[Notify] Uur-tellers gereset\n");
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
            Serial_printf("[Notify] 1m spike: ret_1m=%.2f%%, ret_5m=%.2f%%\n", ret_1m, ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf("[Notify] 1m spike onderdrukt (gebruikt in confluence alert)\n");
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last1mEvent.usedInConfluence) {
                    Serial_printf("[Notify] 1m spike onderdrukt (al gebruikt in confluence)\n");
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
                        Serial_printf("[Notify] 1m spike notificatie verstuurd (%d/%d dit uur)\n", alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR);
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
            Serial_printf("[Notify] 30m move: ret_30m=%.2f%%, ret_5m=%.2f%%\n", ret_30m, ret_5m);
            
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
                Serial_printf("[Notify] 30m move notificatie verstuurd (%d/%d dit uur)\n", alerts30MinThisHour, MAX_30M_ALERTS_PER_HOUR);
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
            Serial_printf("[Notify] 5m move: ret_5m=%.2f%%\n", ret_5m);
            
            // Check for confluence first (Smart Confluence Mode)
            bool confluenceFound = false;
            if (smartConfluenceEnabled) {
                confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);
            }
            
            // Als confluence werd gevonden, skip individuele alert
            if (confluenceFound) {
                Serial_printf("[Notify] 5m move onderdrukt (gebruikt in confluence alert)\n");
            } else {
                // Check of dit event al gebruikt is in confluence (suppress individuele alert)
                if (smartConfluenceEnabled && last5mEvent.usedInConfluence) {
                    Serial_printf("[Notify] 5m move onderdrukt (al gebruikt in confluence)\n");
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
                        Serial_printf("[Notify] 5m move notificatie verstuurd (%d/%d dit uur)\n", alerts5MinThisHour, MAX_5M_ALERTS_PER_HOUR);
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
    preferences.begin("crypto", true); // read-only mode
    
    // Generate default NTFY topic with unique ESP32 device ID
    // Geoptimaliseerd: gebruik char array i.p.v. String
    char defaultTopic[64];
    generateDefaultNtfyTopic(defaultTopic, sizeof(defaultTopic));
    
    // Load NTFY topic from Preferences, or use generated default
    String topic = preferences.getString("ntfyTopic", defaultTopic);
    
    // If the loaded topic is the old default (without device ID), replace it with new format
    // Also migrate old format with prefix (e.g. "crypt-xxxxxx-alert") to new format without prefix
    // BUT: Don't migrate custom topics that users have set manually
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
            }
            // Note: We no longer migrate topics that don't have exactly 8 chars,
            // as users might have set custom topics with different lengths
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
    safeStrncpy(symbolsArray[0], binanceSymbol, sizeof(symbolsArray[0]));
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
    
    // Load trend-adaptive anchor settings
    trendAdaptiveAnchorsEnabled = preferences.getBool("trendAdapt", TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT);
    uptrendMaxLossMultiplier = preferences.getFloat("upMLMult", UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT);
    uptrendTakeProfitMultiplier = preferences.getFloat("upTPMult", UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT);
    downtrendMaxLossMultiplier = preferences.getFloat("downMLMult", DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT);
    downtrendTakeProfitMultiplier = preferences.getFloat("downTPMult", DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT);
    
    // Load Smart Confluence Mode settings
    smartConfluenceEnabled = preferences.getBool("smartConf", SMART_CONFLUENCE_ENABLED_DEFAULT);
    
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
    
    // Save trend-adaptive anchor settings
    preferences.putBool("trendAdapt", trendAdaptiveAnchorsEnabled);
    preferences.putFloat("upMLMult", uptrendMaxLossMultiplier);
    preferences.putFloat("upTPMult", uptrendTakeProfitMultiplier);
    preferences.putFloat("downMLMult", downtrendMaxLossMultiplier);
    preferences.putFloat("downTPMult", downtrendTakeProfitMultiplier);
    
    // Save Smart Confluence Mode settings
    preferences.putBool("smartConf", smartConfluenceEnabled);
    
    // Save Auto-Volatility Mode settings
    preferences.putBool("autoVol", autoVolatilityEnabled);
    preferences.putUChar("autoVolWin", autoVolatilityWindowMinutes);
    preferences.putFloat("autoVolBase", autoVolatilityBaseline1mStdPct);
    preferences.putFloat("autoVolMin", autoVolatilityMinMultiplier);
    preferences.putFloat("autoVolMax", autoVolatilityMaxMultiplier);
    
    // Save trend and volatility settings
    preferences.putFloat("trendTh", trendThreshold);
    preferences.putFloat("volLow", volatilityLowThreshold);
    preferences.putFloat("volHigh", volatilityHighThreshold);
    
    // Save language setting
    preferences.putUChar("language", language);
    
    preferences.end();
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
        Serial_printf("[Overflow] Seconds value too large: %d (max: 4294967)\n", seconds);
        return false;
    }
    
    // Safe multiplication: seconds * 1000UL
    resultMs = (uint32_t)seconds * 1000UL;
    
    // Verify result is reasonable (should be >= 1000 and <= 3,600,000 for our use case)
    if (resultMs < 1000UL || resultMs > 3600000UL) {
        Serial_printf("[Overflow] Invalid result: %lu ms (expected 1000-3600000)\n", resultMs);
        return false;
    }
    
    return true;
}

static bool handleMqttIntSetting(const char* value, uint32_t* targetMs, int minVal, int maxVal, const char* stateTopic, const char* prefix) {
    int seconds = atoi(value);
    if (seconds >= minVal && seconds <= maxVal) {
        uint32_t resultMs;
        if (!safeSecondsToMs(seconds, resultMs)) {
            Serial_printf("[MQTT] Overflow check failed for cooldown: %d seconds\n", seconds);
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
    
    Serial_printf("[MQTT] Message: %s => %s\n", topicBuffer, msgBuffer);
    
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
                Serial_printf("[MQTT] Invalid value for %s: %s\n", floatSettings[i].suffix, msgBuffer);
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
                                        Serial_printf("[MQTT] Overflow check failed for cooldown: %d seconds\n", seconds);
                                        break;
                                    }
                                    *cooldownSettings[i].targetMs = resultMs;
                    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s%s", prefixBuffer, cooldownSettings[i].stateSuffix);
                    snprintf(valueBuffer, sizeof(valueBuffer), "%lu", *cooldownSettings[i].targetMs / 1000);
                    mqttClient.publish(topicBufferFull, valueBuffer, true);
                    settingChanged = true;
                    handled = true;
                } else {
                    Serial_printf("[MQTT] Invalid cooldown value (range: 1-3600 seconds): %s\n", msgBuffer);
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
                                Serial_printf("[MQTT] Anchor setting queued: %.2f\n", val);
                            }
                        } else {
                            Serial_printf("[MQTT] WARN: Ongeldige anchor waarde opgegeven: %s\n", msgBuffer);
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
    Serial_printf("[MQTT] Connecting to %s:%d as %s...\n", mqttHost, mqttPort, clientId);
    
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
        Serial_printf("[MQTT] Connect failed, rc=%d (poging %u)\n", mqttClient.state(), mqttReconnectAttemptCount);
        mqttConnected = false;
    }
}

// Web server HTML page
static String getSettingsHTML()
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
    
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>" + String(binanceSymbol) + " " + String(getText("Instellingen", "Settings")) + "</title>";
    html += "<style>";
    html += "*{box-sizing:border-box;}";
    html += "body{font-family:Arial;margin:0;padding:10px;background:#1a1a1a;color:#fff;}";
    html += ".container{max-width:600px;margin:0 auto;padding:0 10px;}";
    html += "h1{color:#00BCD4;margin:15px 0;font-size:24px;}";
    html += "form{max-width:100%;}";
    html += "label{display:block;margin:15px 0 5px;color:#ccc;}";
    html += "input[type=number],input[type=text],select{width:100%;padding:8px;border:1px solid #444;background:#2a2a2a;color:#fff;border-radius:4px;box-sizing:border-box;}";
    html += "button{background:#00BCD4;color:#fff;padding:12px 24px;border:none;border-radius:4px;cursor:pointer;font-size:16px;margin-top:20px;width:100%;}";
    html += "button:hover{background:#00acc1;}";
    html += ".info{color:#888;font-size:12px;margin-top:5px;}";
    html += ".status-box{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:15px;margin:20px 0;max-width:100%;}";
    html += ".status-row{display:flex;justify-content:space-between;margin:8px 0;padding:8px 0;border-bottom:1px solid #333;flex-wrap:wrap;}";
    html += ".status-label{color:#888;flex:1;min-width:120px;}";
    html += ".status-value{color:#fff;font-weight:bold;text-align:right;flex:1;min-width:100px;}";
    html += ".section-header{background:#2a2a2a;border:1px solid #444;border-radius:4px;padding:12px;margin:15px 0 0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}";
    html += ".section-header:hover{background:#333;}";
    html += ".section-header h3{margin:0;color:#00BCD4;font-size:16px;}";
    html += ".section-content{display:none;padding:15px;background:#1a1a1a;border:1px solid #444;border-top:none;border-radius:0 0 4px 4px;}";
    html += ".section-content.active{display:block;}";
    html += ".section-desc{color:#888;font-size:12px;margin-top:5px;margin-bottom:15px;}";
    html += ".toggle-icon{color:#00BCD4;font-size:18px;flex-shrink:0;margin-left:10px;}";
    html += "@media (max-width:600px){";
    html += "body{padding:5px;}";
    html += ".container{padding:0 5px;}";
    html += "h1{font-size:20px;margin:10px 0;}";
    html += ".status-box{padding:10px;margin:15px 0;}";
    html += ".status-row{flex-direction:column;padding:6px 0;}";
    html += ".status-label{min-width:auto;margin-bottom:3px;}";
    html += ".status-value{text-align:left;min-width:auto;}";
    html += ".section-header{padding:10px;}";
    html += ".section-header h3{font-size:14px;}";
    html += ".section-content{padding:10px;}";
    html += "button{padding:10px 20px;font-size:14px;}";
    html += "label{font-size:14px;}";
    html += "input[type=number],input[type=text],select{font-size:14px;padding:6px;}";
    html += "}";
    html += "</style>";
    html += "<script type='text/javascript'>";
    html += "(function(){";
    html += "function toggleSection(id){";
    html += "var content=document.getElementById('content-'+id);";
    html += "var icon=document.getElementById('icon-'+id);";
    html += "if(!content||!icon)return false;";
    html += "if(content.classList.contains('active')){";
    html += "content.classList.remove('active');";
    html += "icon.innerHTML='&#9654;';";
    html += "}else{";
    html += "content.classList.add('active');";
    html += "icon.innerHTML='&#9660;';";
    html += "}";
    html += "return false;";
    html += "}";
    html += "function setAnchorBtn(e){";
    html += "if(e){e.preventDefault();e.stopPropagation();}";
    html += "var input=document.getElementById('anchorValue');";
    html += "if(!input){alert('Input not found');return false;}";
    html += "var val=input.value||'';";
    html += "var xhr=new XMLHttpRequest();";
    html += "xhr.open('POST','/anchor/set',true);";
    html += "xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');";
    html += "xhr.onreadystatechange=function(){";
    html += "if(xhr.readyState==4){";
    html += "if(xhr.status==200){";
    html += "alert('" + String(getText("Anchor ingesteld!", "Anchor set!")) + "');";
    html += "setTimeout(function(){location.reload();},500);";
    html += "}else{";
    html += "alert('" + String(getText("Fout bij instellen anchor", "Error setting anchor")) + "');";
    html += "}";
    html += "}";
    html += "};";
    html += "xhr.send('value='+encodeURIComponent(val));";
    html += "return false;";
    html += "}";
    html += "window.addEventListener('DOMContentLoaded',function(){";
    html += "var headers=document.querySelectorAll('.section-header');";
    html += "for(var i=0;i<headers.length;i++){";
    html += "headers[i].addEventListener('click',function(e){";
    html += "var id=this.getAttribute('data-section');";
    html += "toggleSection(id);";
    html += "e.preventDefault();";
    html += "return false;";
    html += "});";
    html += "}";
    html += "var basic=document.getElementById('icon-basic');";
    html += "var anchor=document.getElementById('icon-anchor');";
    html += "if(basic)basic.innerHTML='&#9660;';";
    html += "if(anchor)anchor.innerHTML='&#9660;';";
    html += "var anchorBtn=document.getElementById('anchorBtn');";
    html += "if(anchorBtn){";
    html += "anchorBtn.addEventListener('click',setAnchorBtn);";
    html += "}";
    html += "});";
    html += "})();";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>" + String(binanceSymbol) + " " + String(getText("Instellingen", "Settings")) + "</h1>";
    
    // Huidige Status samenvatting (read-only)
    html += "<div class='status-box'>";
    html += "<h2 style='color:#00BCD4;margin-top:0;'>" + String(getText("Huidige Status", "Current Status")) + "</h2>";
    
    // Prijs
    if (currentPrice > 0.0f) {
        html += "<div class='status-row'><span class='status-label'>" + String(getText("Huidige Prijs", "Current Price")) + ":</span><span class='status-value'>" + String(currentPrice, 2) + " EUR</span></div>";
    } else {
        html += "<div class='status-row'><span class='status-label'>" + String(getText("Huidige Prijs", "Current Price")) + ":</span><span class='status-value'>--</span></div>";
    }
    
    // Returns
    html += "<div class='status-row'><span class='status-label'>1m Return:</span><span class='status-value'>" + String(currentRet1m, 2) + "%</span></div>";
    html += "<div class='status-row'><span class='status-label'>5m Return:</span><span class='status-value'>" + String(currentRet5m, 2) + "%</span></div>";
    html += "<div class='status-row'><span class='status-label'>30m Return:</span><span class='status-value'>" + String(currentRet30m, 2) + "%</span></div>";
    
    // Trend
    const char* trendText = "";
    switch (currentTrend) {
        case TREND_UP: trendText = (language == 0) ? "UP" : "UP"; break;
        case TREND_DOWN: trendText = (language == 0) ? "DOWN" : "DOWN"; break;
        case TREND_SIDEWAYS: trendText = (language == 0) ? "SIDEWAYS" : "SIDEWAYS"; break;
    }
    html += "<div class='status-row'><span class='status-label'>" + String(getText("Trend", "Trend")) + ":</span><span class='status-value'>" + String(trendText) + "</span></div>";
    
    // Volatiliteit
    const char* volText = "";
    switch (currentVol) {
        case VOLATILITY_LOW: volText = (language == 0) ? "LAAG" : "LOW"; break;
        case VOLATILITY_MEDIUM: volText = (language == 0) ? "GEMIDDELD" : "MEDIUM"; break;
        case VOLATILITY_HIGH: volText = (language == 0) ? "HOOG" : "HIGH"; break;
    }
    html += "<div class='status-row'><span class='status-label'>" + String(getText("Volatiliteit", "Volatility")) + ":</span><span class='status-value'>" + String(volText) + "</span></div>";
    
    // Anchor
    if (currentAnchorActive && currentAnchorPrice > 0.0f) {
        html += "<div class='status-row'><span class='status-label'>" + String(getText("Anchor", "Anchor")) + ":</span><span class='status-value'>" + String(currentAnchorPrice, 2) + " EUR</span></div>";
        html += "<div class='status-row'><span class='status-label'>" + String(getText("PnL t.o.v. Anchor", "PnL vs Anchor")) + ":</span><span class='status-value'>" + String(currentAnchorPct, 2) + "%</span></div>";
    } else {
        html += "<div class='status-row'><span class='status-label'>" + String(getText("Anchor", "Anchor")) + ":</span><span class='status-value'>" + String(getText("Niet ingesteld", "Not set")) + "</span></div>";
    }
    
    // Features status
    html += "<div class='status-row'><span class='status-label'>" + String(getText("Trend-Adaptive", "Trend-Adaptive")) + ":</span><span class='status-value'>" + String(trendAdaptiveAnchorsEnabled ? getText("Aan", "On") : getText("Uit", "Off")) + "</span></div>";
    html += "<div class='status-row'><span class='status-label'>" + String(getText("Smart Confluence", "Smart Confluence")) + ":</span><span class='status-value'>" + String(smartConfluenceEnabled ? getText("Aan", "On") : getText("Uit", "Off")) + "</span></div>";
    html += "<div class='status-row'><span class='status-label'>" + String(getText("Auto-Volatility", "Auto-Volatility")) + ":</span><span class='status-value'>" + String(autoVolatilityEnabled ? getText("Aan", "On") : getText("Uit", "Off")) + "</span></div>";
    
    html += "</div>";
    
    html += "<form method='POST' action='/save'>";
    
    // ===== SECTIE 1: Basis & Connectiviteit =====
    html += "<div class='section-header' data-section='basic'>";
    html += "<h3>" + String(getText("Basis & Connectiviteit", "Basic & Connectivity")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-basic'>&#9660;</span>";
    html += "</div>";
    html += "<div class='section-content active' id='content-basic'>";
    html += "<div class='section-desc'>" + String(getText("Basisinstellingen voor taal, notificaties en API connectiviteit", "Basic settings for language, notifications and API connectivity")) + "</div>";
    
    html += "<label>" + String(getText("Taal van het systeem", "System Language")) + ":<select name='language'>";
    html += "<option value='0'" + String(language == 0 ? " selected" : "") + ">Nederlands</option>";
    html += "<option value='1'" + String(language == 1 ? " selected" : "") + ">English</option>";
    html += "</select></label>";
    html += "<div class='info'>" + String(getText("Kies de taal voor het scherm en deze webpagina. Dit heeft geen invloed op de werking van het systeem.", "Choose the language for the screen and this webpage. This does not affect the system's operation.")) + "</div>";
    
    html += "<label>" + String(getText("Te volgen markt (Binance trading pair)", "Market to follow (Binance trading pair)")) + ":<input type='text' name='binancesymbol' value='" + String(binanceSymbol) + "' maxlength='15'></label>";
    html += "<div class='info'>" + String(getText("Welke markt moet worden gevolgd, bijvoorbeeld BTCEUR of BTCUSDT. Alle berekeningen en alerts zijn gebaseerd op dit trading pair.", "Which market should be followed, for example BTCEUR or BTCUSDT. All calculations and alerts are based on this trading pair.")) + "</div>";
    
    html += "<label>" + String(getText("Notificatiekanaal (NTFY)", "Notification channel (NTFY)")) + ":<input type='text' name='ntfytopic' value='" + String(ntfyTopic) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("Dit is het kanaal waarop je mobiele meldingen ontvangt via de NTFY-app. Abonneer je in de app op dit topic om alerts te krijgen.", "This is the channel where you receive mobile notifications via the NTFY app. Subscribe to this topic in the app to receive alerts.")) + "</div>";
    
    html += "</div>";
    
    // ===== SECTIE 2: Anchor & Risicokader =====
    html += "<div class='section-header' data-section='anchor'>";
    html += "<h3>" + String(getText("Anchor & Risicokader", "Anchor & Risk Framework")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-anchor'>&#9660;</span>";
    html += "</div>";
    html += "<div class='section-content active' id='content-anchor'>";
    html += "<div class='section-desc'>" + String(getText("Referentieprijs en risicogrenzen voor take profit en stop loss", "Reference price and risk boundaries for take profit and stop loss")) + "</div>";
    
    // Haal huidige prijs op voor default waarde
    String anchorValueStr = "";
    if (currentPrice > 0.0f) {
        anchorValueStr = String(currentPrice, 2);
    } else if (currentAnchorActive && currentAnchorPrice > 0.0f) {
        anchorValueStr = String(currentAnchorPrice, 2);
    }
    
    html += "<label>" + String(getText("Referentieprijs (Anchor)", "Reference price (Anchor)")) + " (EUR):<input type='number' step='0.01' id='anchorValue' value='" + anchorValueStr + "' min='0.01'></label>";
    html += "<button type='button' id='anchorBtn' style='background:#4CAF50;margin-top:10px;'>" + String(getText("Stel Anchor In", "Set Anchor")) + "</button>";
    html += "<div class='info'>" + String(getText("De prijs waartegen winst en verlies worden gemeten. Laat leeg om automatisch de huidige marktprijs te gebruiken.", "The price against which profit and loss are measured. Leave empty to automatically use the current market price.")) + "</div>";
    
    html += "<label>" + String(getText("Winstdoel vanaf anchor (%)", "Profit target from anchor (%)")) + ":<input type='number' step='0.1' name='anchorTP' value='" + String(anchorTakeProfit, 1) + "' min='0.1' max='100'></label>";
    html += "<div class='info'>" + String(getText("Hoeveel procent de prijs moet stijgen vanaf de anchor voordat een \"Take Profit\"-melding wordt gestuurd.", "How many percent the price must rise from the anchor before a \"Take Profit\" notification is sent.")) + "</div>";
    
    html += "<label>" + String(getText("Maximaal verlies vanaf anchor (%)", "Maximum loss from anchor (%)")) + ":<input type='number' step='0.1' name='anchorML' value='" + String(anchorMaxLoss, 1) + "' min='-100' max='-0.1'></label>";
    html += "<div class='info'>" + String(getText("Hoeveel procent de prijs mag dalen vanaf de anchor voordat een \"Max Loss\"-melding wordt gestuurd.", "How many percent the price may fall from the anchor before a \"Max Loss\" notification is sent.")) + "</div>";
    
    // Trend-adaptive anchor settings
    html += "<h4 style='color:#00BCD4;margin-top:20px;'>" + String(getText("Trend-Adaptive Anchors", "Trend-Adaptive Anchors")) + "</h4>";
    html += "<label><input type='checkbox' name='trendAdapt' value='1'" + String(trendAdaptiveAnchorsEnabled ? " checked" : "") + "> " + String(getText("Risico automatisch aanpassen aan trend", "Automatically adjust risk to trend")) + "</label>";
    html += "<div class='info'>" + String(getText("Past je winst- en verliesgrenzen automatisch aan op basis van de marktrichting (stijgend, dalend of zijwaarts).", "Automatically adjusts your profit and loss limits based on market direction (rising, falling or sideways).")) + "</div>";
    
    html += "<label>" + String(getText("Extra ruimte bij stijgende markt (verlies)", "Extra room in rising market (loss)")) + ":<input type='number' step='0.01' name='upMLMult' value='" + String(uptrendMaxLossMultiplier, 2) + "' min='0.5' max='2.0'></label>";
    html += "<div class='info'>" + String(getText("Bij een stijgende trend mag de prijs iets verder tegen je in bewegen voordat een Max Loss-melding komt.", "In a rising trend, the price may move slightly further against you before a Max Loss notification comes.")) + "</div>";
    
    html += "<label>" + String(getText("Meer winst laten lopen bij stijgende markt", "Let more profit run in rising market")) + ":<input type='number' step='0.01' name='upTPMult' value='" + String(uptrendTakeProfitMultiplier, 2) + "' min='0.5' max='2.0'></label>";
    html += "<div class='info'>" + String(getText("Bij een stijgende trend wordt het winstdoel automatisch verhoogd zodat winsten langer kunnen doorlopen.", "In a rising trend, the profit target is automatically increased so profits can run longer.")) + "</div>";
    
    html += "<label>" + String(getText("Sneller beschermen bij dalende markt", "Protect faster in falling market")) + ":<input type='number' step='0.01' name='downMLMult' value='" + String(downtrendMaxLossMultiplier, 2) + "' min='0.5' max='2.0'></label>";
    html += "<div class='info'>" + String(getText("Bij een dalende trend wordt het maximale verlies verkleind om sneller risico te beperken.", "In a falling trend, the maximum loss is reduced to limit risk faster.")) + "</div>";
    
    html += "<label>" + String(getText("Sneller winst nemen bij dalende markt", "Take profit faster in falling market")) + ":<input type='number' step='0.01' name='downTPMult' value='" + String(downtrendTakeProfitMultiplier, 2) + "' min='0.5' max='2.0'></label>";
    html += "<div class='info'>" + String(getText("Bij een dalende trend wordt het winstdoel verlaagd om eerder winst veilig te stellen.", "In a falling trend, the profit target is lowered to secure profit earlier.")) + "</div>";
    
    html += "</div>";
    
    // ===== SECTIE 3: Signaalgeneratie =====
    html += "<div class='section-header' data-section='signals'>";
    html += "<h3>" + String(getText("Signaalgeneratie", "Signal Generation")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-signals'>&#9654;</span>";
    html += "</div>";
    html += "<div class='section-content' id='content-signals'>";
    html += "<div class='section-desc'>" + String(getText("Thresholds voor spike en move detectie op verschillende timeframes", "Thresholds for spike and move detection on different timeframes")) + "</div>";
    
    html += "<label>" + String(getText("Snelle prijsbeweging (1 minuut)", "Fast price movement (1 minute)")) + " (%):<input type='number' step='0.01' name='spike1m' value='" + String(spike1mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("Minimale procentuele beweging binnen 1 minuut om als \"snelle impuls\" te worden gezien.", "Minimum percentage movement within 1 minute to be considered a \"fast impulse\".")) + "</div>";
    
    html += "<label>" + String(getText("Bevestiging door 5 minuten (filter)", "Confirmation by 5 minutes (filter)")) + " (%):<input type='number' step='0.01' name='spike5m' value='" + String(spike5mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("Een 1-minuut spike telt alleen mee als de beweging ook door de 5-minuten trend wordt ondersteund.", "A 1-minute spike only counts if the movement is also supported by the 5-minute trend.")) + "</div>";
    
    html += "<label>" + String(getText("Structurele beweging (5 minuten)", "Structural movement (5 minutes)")) + " (%):<input type='number' step='0.01' name='move5mAlert' value='" + String(move5mAlertThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("Minimale procentuele beweging over 5 minuten om als betekenisvolle beweging te worden gezien.", "Minimum percentage movement over 5 minutes to be considered a meaningful movement.")) + "</div>";
    
    html += "<label>" + String(getText("Grote beweging (30 minuten)", "Large movement (30 minutes)")) + " (%):<input type='number' step='0.01' name='move30m' value='" + String(move30mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("Minimale procentuele beweging over 30 minuten om als grotere marktverplaatsing te gelden.", "Minimum percentage movement over 30 minutes to count as a larger market shift.")) + "</div>";
    
    html += "<label>" + String(getText("Korte-termijn bevestiging (5 minuten)", "Short-term confirmation (5 minutes)")) + " (%):<input type='number' step='0.01' name='move5m' value='" + String(move5mThreshold, 2) + "'></label>";
    html += "<div class='info'>" + String(getText("Een 30-minuten beweging telt alleen mee als de 5-minuten trend dezelfde richting bevestigt.", "A 30-minute movement only counts if the 5-minute trend confirms the same direction.")) + "</div>";
    
    html += "<h4 style='color:#00BCD4;margin-top:20px;'>" + String(getText("Trend & Volatiliteit", "Trend & Volatility")) + "</h4>";
    html += "<label>" + String(getText("Wanneer spreekt het systeem van een trend?", "When does the system speak of a trend?")) + " (%):<input type='number' step='0.1' name='trendTh' value='" + String(trendThreshold, 1) + "' min='0.1' max='10'></label>";
    html += "<div class='info'>" + String(getText("Minimale procentuele beweging over 2 uur om de markt als stijgend of dalend te beschouwen.", "Minimum percentage movement over 2 hours to consider the market as rising or falling.")) + "</div>";
    
    html += "<label>" + String(getText("Rustige markt grens", "Quiet market threshold")) + " (%):<input type='number' step='0.01' name='volLow' value='" + String(volatilityLowThreshold, 2) + "' min='0.01' max='1'></label>";
    html += "<div class='info'>" + String(getText("Onder deze waarde wordt de markt als rustig beschouwd.", "Below this value the market is considered quiet.")) + "</div>";
    
    html += "<label>" + String(getText("Drukke markt grens", "Busy market threshold")) + " (%):<input type='number' step='0.01' name='volHigh' value='" + String(volatilityHighThreshold, 2) + "' min='0.01' max='1'></label>";
    html += "<div class='info'>" + String(getText("Boven deze waarde wordt de markt als zeer beweeglijk beschouwd.", "Above this value the market is considered very volatile.")) + "</div>";
    
    html += "</div>";
    
    // ===== SECTIE 4: Slimme logica & filters =====
    html += "<div class='section-header' data-section='smart'>";
    html += "<h3>" + String(getText("Slimme logica & filters", "Smart Logic & Filters")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-smart'>&#9654;</span>";
    html += "</div>";
    html += "<div class='section-content' id='content-smart'>";
    html += "<div class='section-desc'>" + String(getText("Geavanceerde filtering en automatische aanpassing van thresholds", "Advanced filtering and automatic threshold adjustment")) + "</div>";
    
    // Smart Confluence Mode
    html += "<h4 style='color:#00BCD4;margin-top:10px;'>" + String(getText("Smart Confluence Mode", "Smart Confluence Mode")) + "</h4>";
    html += "<label><input type='checkbox' name='smartConf' value='1'" + String(smartConfluenceEnabled ? " checked" : "") + "> " + String(getText("Smart Confluence Mode Inschakelen", "Enable Smart Confluence Mode")) + "</label>";
    html += "<div class='info'>" + String(getText("Verstuur alleen alerts als er confluence is tussen 1m, 5m en 30m timeframes in dezelfde richting. Dit vermindert het aantal alerts maar verhoogt de betekenisvolheid.", "Only send alerts when there is confluence between 1m, 5m and 30m timeframes in the same direction. This reduces the number of alerts but increases their significance.")) + "</div>";
    
    // Auto-Volatility Mode
    html += "<h4 style='color:#00BCD4;margin-top:20px;'>" + String(getText("Auto-Volatility Mode", "Auto-Volatility Mode")) + "</h4>";
    html += "<label><input type='checkbox' name='autoVol' value='1'" + String(autoVolatilityEnabled ? " checked" : "") + "> " + String(getText("Drempels automatisch aanpassen aan markt", "Automatically adjust thresholds to market")) + "</label>";
    html += "<div class='info'>" + String(getText("Past gevoeligheid automatisch aan: rustige markt → gevoeliger, drukke markt → strenger.", "Automatically adjusts sensitivity: quiet market → more sensitive, busy market → stricter.")) + "</div>";
    
    html += "<label>" + String(getText("Hoe ver terugkijken voor volatiliteit", "How far back to look for volatility")) + " (minuten):<input type='number' step='1' name='autoVolWin' value='" + String(autoVolatilityWindowMinutes) + "' min='10' max='120'></label>";
    html += "<div class='info'>" + String(getText("Aantal minuten dat wordt gebruikt om te bepalen hoe rustig of druk de markt is.", "Number of minutes used to determine how quiet or busy the market is.")) + "</div>";
    
    html += "<label>" + String(getText("Normale marktbeweging (referentie)", "Normal market movement (reference)")) + " (%):<input type='number' step='0.01' name='autoVolBase' value='" + String(autoVolatilityBaseline1mStdPct, 2) + "' min='0.01' max='1.0'></label>";
    html += "<div class='info'>" + String(getText("Dit is wat het systeem beschouwt als \"normale\" 1-minuut beweging. Afwijkingen hiervan maken de drempels strenger of soepeler.", "This is what the system considers \"normal\" 1-minute movement. Deviations from this make thresholds stricter or more lenient.")) + "</div>";
    
    html += "<label>" + String(getText("Minimale gevoeligheid", "Minimum sensitivity")) + ":<input type='number' step='0.1' name='autoVolMin' value='" + String(autoVolatilityMinMultiplier, 1) + "' min='0.1' max='1.0'></label>";
    html += "<div class='info'>" + String(getText("Voorkomt dat het systeem té gevoelig wordt in extreem rustige markten.", "Prevents the system from becoming too sensitive in extremely quiet markets.")) + "</div>";
    
    html += "<label>" + String(getText("Maximale gevoeligheid", "Maximum sensitivity")) + ":<input type='number' step='0.1' name='autoVolMax' value='" + String(autoVolatilityMaxMultiplier, 1) + "' min='1.0' max='3.0'></label>";
    html += "<div class='info'>" + String(getText("Voorkomt dat het systeem té streng wordt in extreem drukke markten.", "Prevents the system from becoming too strict in extremely busy markets.")) + "</div>";
    
    html += "</div>";
    
    // ===== SECTIE 5: Cooldowns =====
    html += "<div class='section-header' data-section='cooldowns'>";
    html += "<h3>" + String(getText("Cooldowns", "Cooldowns")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-cooldowns'>&#9654;</span>";
    html += "</div>";
    html += "<div class='section-content' id='content-cooldowns'>";
    html += "<div class='section-desc'>" + String(getText("Minimale tijd tussen alerts van hetzelfde type om spam te voorkomen", "Minimum time between alerts of the same type to prevent spam")) + "</div>";
    
    html += "<label>" + String(getText("Wachttijd tussen snelle meldingen", "Wait time between fast notifications")) + " (seconden):<input type='number' name='cd1min' value='" + String(notificationCooldown1MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Minimale tijd tussen twee snelle (1m) meldingen om spam te voorkomen.", "Minimum time between two fast (1m) notifications to prevent spam.")) + "</div>";
    
    html += "<label>" + String(getText("Wachttijd tussen 5m meldingen", "Wait time between 5m notifications")) + " (seconden):<input type='number' name='cd5min' value='" + String(notificationCooldown5MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Zorgt ervoor dat structurele bewegingen niet te vaak achter elkaar worden gemeld.", "Ensures that structural movements are not reported too frequently in succession.")) + "</div>";
    
    html += "<label>" + String(getText("Wachttijd tussen grote bewegingen", "Wait time between large movements")) + " (seconden):<input type='number' name='cd30min' value='" + String(notificationCooldown30MinMs / 1000) + "'></label>";
    html += "<div class='info'>" + String(getText("Beperkt hoe vaak meldingen over grote marktbewegingen worden verstuurd.", "Limits how often notifications about large market movements are sent.")) + "</div>";
    
    html += "</div>";
    
    // ===== SECTIE 6: Integratie (MQTT) =====
    html += "<div class='section-header' data-section='mqtt'>";
    html += "<h3>" + String(getText("Integratie (MQTT)", "Integration (MQTT)")) + "</h3>";
    html += "<span class='toggle-icon' id='icon-mqtt'>&#9654;</span>";
    html += "</div>";
    html += "<div class='section-content' id='content-mqtt'>";
    html += "<div class='section-desc'>" + String(getText("MQTT broker configuratie voor Home Assistant integratie", "MQTT broker configuration for Home Assistant integration")) + "</div>";
    
    html += "<label>" + String(getText("MQTT server (IP-adres)", "MQTT server (IP address)")) + ":<input type='text' name='mqtthost' value='" + String(mqttHost) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("IP-adres van de MQTT broker waar status- en eventdata naartoe worden gestuurd.", "IP address of the MQTT broker where status and event data are sent.")) + "</div>";
    
    html += "<label>" + String(getText("MQTT poort", "MQTT port")) + ":<input type='number' name='mqttport' value='" + String(mqttPort) + "' min='1' max='65535'></label>";
    html += "<div class='info'>" + String(getText("Poortnummer van de MQTT broker (meestal 1883).", "Port number of the MQTT broker (usually 1883).")) + "</div>";
    
    html += "<label>" + String(getText("MQTT gebruikersnaam", "MQTT username")) + ":<input type='text' name='mqttuser' value='" + String(mqttUser) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("Gebruikersnaam voor toegang tot de MQTT broker.", "Username for access to the MQTT broker.")) + "</div>";
    
    html += "<label>" + String(getText("MQTT wachtwoord", "MQTT password")) + ":<input type='password' name='mqttpass' value='" + String(mqttPass) + "' maxlength='63'></label>";
    html += "<div class='info'>" + String(getText("Wachtwoord voor toegang tot de MQTT broker.", "Password for access to the MQTT broker.")) + "</div>";
    
    html += "</div>";
    
    html += "<button type='submit'>" + String(getText("Opslaan", "Save")) + "</button></form>";
    html += "</div>";
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
                Serial_printf("[Web] WARN: Ongeldige anchor waarde opgegeven: '%s'\n", anchorValueStr.c_str());
            }
        } else {
            // Leeg veld = gebruik huidige prijs
            pendingAnchorSetting.value = 0.0f;
            pendingAnchorSetting.useCurrentPrice = true;
            // Zet pending flag als laatste (memory barrier effect)
            pendingAnchorSetting.pending = true;
            valid = true;
            Serial_println("[Web] Anchor setting queued: gebruik huidige prijs");
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

static void setupWebServer()
{
    Serial.println("[WebServer] Routes registreren...");
    server.on("/", handleRoot);
    Serial.println("[WebServer] Route '/' geregistreerd");
    server.on("/save", HTTP_POST, handleSave);
    Serial.println("[WebServer] Route '/save' geregistreerd");
    server.on("/anchor/set", HTTP_POST, handleAnchorSet);
    Serial.println("[WebServer] Route '/anchor/set' geregistreerd");
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
static bool parsePrice(const char *body, float &out)
{
    // Check of body niet leeg is
    if (body == nullptr || strlen(body) == 0)
        return false;
    
    const char *priceStart = strstr(body, "\"price\":\"");
    if (priceStart == nullptr)
        return false;
    
    priceStart += 9; // skip to first digit after "price":""
    
    const char *priceEnd = strchr(priceStart, '"');
    if (priceEnd == nullptr)
        return false;
    
    // Valideer lengte
    size_t priceLen = priceEnd - priceStart;
    if (priceLen == 0 || priceLen > 20) // Max 20 karakters voor prijs (veiligheidscheck)
        return false;
    
    // Extract price string
    char priceStr[32];
    if (priceLen >= sizeof(priceStr)) {
        return false;
    }
    strncpy(priceStr, priceStart, priceLen);
    priceStr[priceLen] = '\0';
    
    // Convert to float
    float val;
    if (!safeAtof(priceStr, val)) {
        return false;
    }
    
    // Validate that we got a valid price
    if (!isValidPrice(val))
        return false;
    
    out = val;
    return true;
}

// Calculate average of array (optimized: single loop)
static float calculateAverage(float *array, uint8_t size, bool filled)
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
static int32_t getRingBufferIndexAgo(uint32_t currentIndex, uint32_t positionsAgo, uint32_t bufferSize)
{
    if (positionsAgo >= bufferSize) return -1;
    // Safe modulo calculation: (currentIndex - positionsAgo + bufferSize * 2) % bufferSize
    int32_t idx = ((int32_t)currentIndex - (int32_t)positionsAgo + (int32_t)bufferSize * 2) % (int32_t)bufferSize;
    if (idx < 0 || idx >= (int32_t)bufferSize) return -1;
    return idx;
}

// Helper: Get last written index in ringbuffer (currentIndex points to next write position)
static uint32_t getLastWrittenIndex(uint32_t currentIndex, uint32_t bufferSize)
{
    return (currentIndex == 0) ? (bufferSize - 1) : (currentIndex - 1);
}

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
        if (isValidPrice(secondPrices[i]))
        {
            if (secondPrices[i] < minVal) minVal = secondPrices[i];
            if (secondPrices[i] > maxVal) maxVal = secondPrices[i];
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
            // For 1m: use calculateAverage helper
            averagePrices[1] = calculateAverage(secondPrices, SECONDS_PER_MINUTE, secondArrayFilled);
        } else if (averagePriceIndex == 2) {
            // For 30m: calculate average of last 30 minutes (handled separately in calculateReturn30Minutes)
            // This is a placeholder - actual calculation is done in the wrapper function
        }
    }
    
    // Return percentage: (now - X ago) / X ago * 100
    return ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
}

static float calculateReturn1Minute()
{
    float ret = calculateReturnGeneric(
        secondPrices,
        SECONDS_PER_MINUTE,
        secondIndex,
        secondArrayFilled,
        VALUES_FOR_1MIN_RETURN,
        "[Ret1m]",
        10000,  // Log every 10 seconds
        1       // Update averagePrices[1]
    );
    return ret;
}

// Calculate 5-minute return: price now vs 5 minutes ago
static float calculateReturn5Minutes()
{
    return calculateReturnGeneric(
        fiveMinutePrices,
        SECONDS_PER_5MINUTES,
        fiveMinuteIndex,
        fiveMinuteArrayFilled,
        VALUES_FOR_5MIN_RETURN,
        "[Ret5m]",
        30000,  // Log every 30 seconds
        255     // Don't update averagePrices
    );
}

// Calculate 30-minute return: price now vs 30 minutes ago (using minute averages)
static float calculateReturn30Minutes()
{
    // Need at least 30 minutes of history
    uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
    if (availableMinutes < 30)
    {
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        if (now - lastLogTime > 60000) {
            Serial_printf("[Ret30m] Wachten op data: minuteIndex=%u (nodig: 30, available=%u)\n", 
                         minuteIndex, availableMinutes);
            lastLogTime = now;
        }
        averagePrices[2] = 0.0f;
        return 0.0f;
    }
    
    // Use generic function for return calculation
    float ret = calculateReturnGeneric(
        minuteAverages,
        MINUTES_FOR_30MIN_CALC,
        minuteIndex,
        minuteArrayFilled,
        30,  // 30 minutes ago
        "[Ret30m]",
        60000,  // Log every 60 seconds
        255     // Don't update averagePrices here (we do it manually below)
    );
    
    // Calculate average of last 30 minutes for display (specific to 30m calculation)
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
        if (isValidPrice(minuteAverages[idx]))
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
// Calculate 2-hour return: price now vs 120 minutes ago
static float calculateReturn2Hours()
{
    uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
    if (availableMinutes < 120)
    {
        return 0.0f;
    }
    
    // Get current price (last minute average)
    uint8_t lastMinuteIdx;
    if (!minuteArrayFilled)
    {
        if (minuteIndex == 0) return 0.0f;
        lastMinuteIdx = minuteIndex - 1;
    }
    else
    {
        lastMinuteIdx = getLastWrittenIndex(minuteIndex, MINUTES_FOR_30MIN_CALC);
    }
    float priceNow = minuteAverages[lastMinuteIdx];
    
    // Get price 120 minutes ago
    uint8_t idx120mAgo;
    if (!minuteArrayFilled)
    {
        if (minuteIndex < 120) return 0.0f;
        idx120mAgo = minuteIndex - 120;
    }
    else
    {
        int32_t idx120mAgo_temp = getRingBufferIndexAgo(minuteIndex, 120, MINUTES_FOR_30MIN_CALC);
        if (idx120mAgo_temp < 0) return 0.0f;
        idx120mAgo = (uint8_t)idx120mAgo_temp;
    }
    
    float price120mAgo = minuteAverages[idx120mAgo];
    
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
static void addPriceToSecondArray(float price)
{
    // Validate input
    if (!isValidPrice(price))
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
    char responseBuffer[512]; // Buffer voor API response (Binance responses zijn klein, ~100 bytes)
    snprintf(url, sizeof(url), "%s%s", BINANCE_API, binanceSymbol);
    
    bool httpSuccess = httpGET(url, responseBuffer, sizeof(responseBuffer), HTTP_TIMEOUT_MS);
    unsigned long fetchTime = millis() - fetchStart;
    
    if (!httpSuccess || strlen(responseBuffer) == 0) {
        // Leeg response - kan komen door timeout of netwerkproblemen
        Serial.printf("[API] WARN -> %s leeg response (tijd: %lu ms) - mogelijk timeout of netwerkprobleem\n", binanceSymbol, fetchTime);
        // Gebruik laatste bekende prijs als fallback (al ingesteld als fetched = prices[0])
    } else if (!parsePrice(responseBuffer, fetched)) {
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
        const TickType_t apiMutexTimeout = pdMS_TO_TICKS(200); // CYD/ESP32-S3: 200ms voor snellere UI updates
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
            
            safeMutexGive(dataMutex, "fetchPrice");
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
        Serial_println("[UI] WARN: Chart of dataSeries is null, skip update");
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
            #ifdef PLATFORM_TTGO
            // TTGO: compact formaat dd-mm-yy voor lagere resolutie
            char dateStr[9]; // dd-mm-yy + null terminator = 9 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
            #else
            // CYD/ESP32-S3: volledig formaat dd-mm-yyyy voor hogere resolutie
            char dateStr[11]; // dd-mm-yyyy + null terminator = 11 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
            #endif
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
static void updateBTCEURCard(bool hasNewData)
{
    if (priceTitle[0] != nullptr) {
        lv_label_set_text(priceTitle[0], "BTCEUR");
    }
    
    lv_label_set_text_fmt(priceLbl[0], "%.2f", prices[0]);
    
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
            // Gebruik dynamische max loss waarde
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            lv_label_set_text_fmt(anchorMinLabel, "%.2f", stopLossPrice);
        } else {
            lv_label_set_text(anchorMinLabel, "");
        }
    }
    #else
    if (anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Toon dynamische take profit waarde (effectief percentage)
            float takeProfitPrice = anchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            lv_label_set_text_fmt(anchorMaxLabel, "+%.2f%% %.2f", effAnchorUI.takeProfitPct, takeProfitPrice);
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
            // Toon dynamische max loss waarde (effectief percentage)
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            lv_label_set_text_fmt(anchorMinLabel, "%.2f%% %.2f", effAnchorUI.maxLossPct, stopLossPrice);
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
    lv_label_set_text_fmt(chartVersionLabel, "%s", VERSION_STRING);
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
    // Buffer grootte - platform-specifiek
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI)
    // TTGO/ESP32-S3: 30 regels voor RAM besparing
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
}

static void setupMutex()
{
    // Maak mutex VOOR we het gebruiken (moet eerst aangemaakt worden)
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("[Error] Kon mutex niet aanmaken!");
    } else {
        Serial.println("[FreeRTOS] Mutex aangemaakt");
    }
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
    
    // WiFi connection and initial data fetch (maakt tijdelijk UI aan)
    wifiConnectionAndFetchPrice();
    
    Serial_println("Setup done");
    fetchPrice();
    
    // Build main UI (verwijdert WiFi UI en bouwt hoofd UI)
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
        
        // Geoptimaliseerd: betere mutex timeout handling met retry logica
        // Neem mutex voor data lezen (timeout verhoogd voor CYD om haperingen te voorkomen)
        // CYD heeft grotere buffer en meer rendering overhead, dus iets langere timeout
        #ifdef PLATFORM_TTGO
        const TickType_t mutexTimeout = pdMS_TO_TICKS(50); // TTGO: korte timeout
        #else
        const TickType_t mutexTimeout = pdMS_TO_TICKS(100); // CYD/ESP32-S3: langere timeout voor betere grafiek updates
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