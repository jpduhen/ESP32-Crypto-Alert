#include "SettingsStore.h"
#include <Arduino.h>
#include <esp_system.h>

// Forward declaration voor hasPSRAM (gedefinieerd in main sketch)
extern bool hasPSRAM();

// Include default constants (moet vanuit hoofdbestand komen)
// Voor nu gebruiken we de waarden direct - deze worden later via includes opgelost
#ifndef THRESHOLD_1MIN_UP_DEFAULT
#define THRESHOLD_1MIN_UP_DEFAULT 0.5f
#endif
#ifndef THRESHOLD_1MIN_DOWN_DEFAULT
#define THRESHOLD_1MIN_DOWN_DEFAULT -0.5f
#endif
#ifndef THRESHOLD_30MIN_UP_DEFAULT
#define THRESHOLD_30MIN_UP_DEFAULT 2.0f
#endif
#ifndef THRESHOLD_30MIN_DOWN_DEFAULT
#define THRESHOLD_30MIN_DOWN_DEFAULT -2.0f
#endif
#ifndef SPIKE_1M_THRESHOLD_DEFAULT
#define SPIKE_1M_THRESHOLD_DEFAULT 0.31f
#endif
#ifndef SPIKE_5M_THRESHOLD_DEFAULT
#define SPIKE_5M_THRESHOLD_DEFAULT 0.65f
#endif
#ifndef MOVE_30M_THRESHOLD_DEFAULT
#define MOVE_30M_THRESHOLD_DEFAULT 1.3f
#endif
#ifndef MOVE_5M_THRESHOLD_DEFAULT
#define MOVE_5M_THRESHOLD_DEFAULT 0.40f
#endif
#ifndef MOVE_5M_ALERT_THRESHOLD_DEFAULT
#define MOVE_5M_ALERT_THRESHOLD_DEFAULT 0.8f
#endif
#ifndef NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT
#define NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT 120000UL
#endif
#ifndef NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT
#define NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT 900000UL
#endif
#ifndef NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT
#define NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT 420000UL
#endif
#ifndef MQTT_HOST_DEFAULT
#define MQTT_HOST_DEFAULT "192.168.68.3"
#endif
#ifndef MQTT_PORT_DEFAULT
#define MQTT_PORT_DEFAULT 1883
#endif
#ifndef MQTT_USER_DEFAULT
#define MQTT_USER_DEFAULT "mosquitto"
#endif
#ifndef MQTT_PASS_DEFAULT
#define MQTT_PASS_DEFAULT "mqtt_password"
#endif
#ifndef ANCHOR_TAKE_PROFIT_DEFAULT
#define ANCHOR_TAKE_PROFIT_DEFAULT 5.0f
#endif
#ifndef ANCHOR_MAX_LOSS_DEFAULT
#define ANCHOR_MAX_LOSS_DEFAULT -3.0f
#endif
#ifndef TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT
#define TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT false
#endif
#ifndef UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT
#define UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT 1.15f
#endif
#ifndef UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT
#define UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT 1.2f
#endif
#ifndef DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT
#define DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT 0.85f
#endif
#ifndef DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT
#define DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT 0.8f
#endif
#ifndef SMART_CONFLUENCE_ENABLED_DEFAULT
#define SMART_CONFLUENCE_ENABLED_DEFAULT false
#endif
#ifndef WARM_START_ENABLED_DEFAULT
#define WARM_START_ENABLED_DEFAULT true
#endif
#ifndef WARM_START_1M_EXTRA_CANDLES_DEFAULT
#define WARM_START_1M_EXTRA_CANDLES_DEFAULT 15
#endif
#ifndef WARM_START_5M_CANDLES_DEFAULT
#define WARM_START_5M_CANDLES_DEFAULT 12
#endif
#ifndef WARM_START_30M_CANDLES_DEFAULT
#define WARM_START_30M_CANDLES_DEFAULT 8
#endif
#ifndef WARM_START_2H_CANDLES_DEFAULT
#define WARM_START_2H_CANDLES_DEFAULT 6
#endif
#ifndef AUTO_VOLATILITY_ENABLED_DEFAULT
#define AUTO_VOLATILITY_ENABLED_DEFAULT false
#endif
#ifndef AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT
#define AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT 60
#endif
#ifndef AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT
#define AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT 0.15f
#endif
#ifndef AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT
#define AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT 0.7f
#endif
#ifndef AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT
#define AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT 1.6f
#endif
#ifndef TREND_THRESHOLD_DEFAULT
#define TREND_THRESHOLD_DEFAULT 1.30f
#endif
#ifndef VOLATILITY_LOW_THRESHOLD_DEFAULT
#define VOLATILITY_LOW_THRESHOLD_DEFAULT 0.05f
#endif
#ifndef VOLATILITY_HIGH_THRESHOLD_DEFAULT
#define VOLATILITY_HIGH_THRESHOLD_DEFAULT 0.15f
#endif
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0
#endif
#ifndef BITVAVO_SYMBOL_DEFAULT
#define BITVAVO_SYMBOL_DEFAULT "BTC-EUR"
#endif

// Preference namespace en keys
const char* SettingsStore::PREF_NAMESPACE = "crypto";
const char* SettingsStore::PREF_KEY_NTFY_TOPIC = "ntfyTopic";
const char* SettingsStore::PREF_KEY_BITVAVO_SYMBOL = "bitvavoSymbol";
const char* SettingsStore::PREF_KEY_LANGUAGE = "language";
const char* SettingsStore::PREF_KEY_DISPLAY_ROTATION = "displayRotation";
const char* SettingsStore::PREF_KEY_TH1_UP = "th1Up";
const char* SettingsStore::PREF_KEY_TH1_DOWN = "th1Down";
const char* SettingsStore::PREF_KEY_TH30_UP = "th30Up";
const char* SettingsStore::PREF_KEY_TH30_DOWN = "th30Down";
const char* SettingsStore::PREF_KEY_SPIKE1M = "spike1m";
const char* SettingsStore::PREF_KEY_SPIKE5M = "spike5m";
const char* SettingsStore::PREF_KEY_MOVE30M = "move30m";
const char* SettingsStore::PREF_KEY_MOVE5M = "move5m";
const char* SettingsStore::PREF_KEY_MOVE5M_ALERT = "move5mAlert";
const char* SettingsStore::PREF_KEY_CD1MIN = "cd1Min";
const char* SettingsStore::PREF_KEY_CD30MIN = "cd30Min";
const char* SettingsStore::PREF_KEY_CD5MIN = "cd5Min";
const char* SettingsStore::PREF_KEY_MQTT_HOST = "mqttHost";
const char* SettingsStore::PREF_KEY_MQTT_PORT = "mqttPort";
const char* SettingsStore::PREF_KEY_MQTT_USER = "mqttUser";
const char* SettingsStore::PREF_KEY_MQTT_PASS = "mqttPass";
const char* SettingsStore::PREF_KEY_ANCHOR_TP = "anchorTP";
const char* SettingsStore::PREF_KEY_ANCHOR_ML = "anchorML";
const char* SettingsStore::PREF_KEY_ANCHOR_STRATEGY = "anchorStrategy";
const char* SettingsStore::PREF_KEY_TREND_ADAPT = "trendAdapt";
const char* SettingsStore::PREF_KEY_UP_ML_MULT = "upMLMult";
const char* SettingsStore::PREF_KEY_UP_TP_MULT = "upTPMult";
const char* SettingsStore::PREF_KEY_DOWN_ML_MULT = "downMLMult";
const char* SettingsStore::PREF_KEY_DOWN_TP_MULT = "downTPMult";
const char* SettingsStore::PREF_KEY_SMART_CONF = "smartConf";
const char* SettingsStore::PREF_KEY_WARM_START = "warmStart";
const char* SettingsStore::PREF_KEY_WS1M_EXTRA = "ws1mExtra";
const char* SettingsStore::PREF_KEY_WS5M = "ws5m";
const char* SettingsStore::PREF_KEY_WS30M = "ws30m";
const char* SettingsStore::PREF_KEY_WS2H = "ws2h";
const char* SettingsStore::PREF_KEY_AUTO_VOL = "autoVol";
const char* SettingsStore::PREF_KEY_AUTO_VOL_WIN = "autoVolWin";
const char* SettingsStore::PREF_KEY_AUTO_VOL_BASE = "autoVolBase";
const char* SettingsStore::PREF_KEY_AUTO_VOL_MIN = "autoVolMin";
const char* SettingsStore::PREF_KEY_AUTO_VOL_MAX = "autoVolMax";
const char* SettingsStore::PREF_KEY_TREND_TH = "trendTh";
const char* SettingsStore::PREF_KEY_VOL_LOW = "volLow";
const char* SettingsStore::PREF_KEY_VOL_HIGH = "volHigh";

// 2-hour alert threshold keys
const char* SettingsStore::PREF_KEY_2H_BREAK_MARGIN = "2hBreakMargin";
const char* SettingsStore::PREF_KEY_2H_BREAK_RESET = "2hBreakReset";
const char* SettingsStore::PREF_KEY_2H_BREAK_CD = "2hBreakCD";
const char* SettingsStore::PREF_KEY_2H_MEAN_MIN_DIST = "2hMeanMinDist";
const char* SettingsStore::PREF_KEY_2H_MEAN_TOUCH = "2hMeanTouch";
const char* SettingsStore::PREF_KEY_2H_MEAN_CD = "2hMeanCD";
const char* SettingsStore::PREF_KEY_2H_COMPRESS_TH = "2hCompressTh";
const char* SettingsStore::PREF_KEY_2H_COMPRESS_RESET = "2hCompressReset";
const char* SettingsStore::PREF_KEY_2H_COMPRESS_CD = "2hCompressCD";
const char* SettingsStore::PREF_KEY_2H_ANCHOR_MARGIN = "2hAnchorMargin";
const char* SettingsStore::PREF_KEY_2H_ANCHOR_CD = "2hAnchorCD";
// FASE X.4: Trend hysteresis en throttling preference keys
const char* SettingsStore::PREF_KEY_2H_TREND_HYSTERESIS = "2hTrendHyst";
const char* SettingsStore::PREF_KEY_2H_THROTTLE_TREND_CHANGE = "2hThrottleTC";
const char* SettingsStore::PREF_KEY_2H_THROTTLE_TREND_MEAN = "2hThrottleTM";
const char* SettingsStore::PREF_KEY_2H_THROTTLE_MEAN_TOUCH = "2hThrottleMT";
const char* SettingsStore::PREF_KEY_2H_THROTTLE_COMPRESS = "2hThrottleComp";
// FASE X.5: Secondary global cooldown en coalescing preference keys
const char* SettingsStore::PREF_KEY_2H_SEC_GLOBAL_CD = "2hSecGlobalCD";
const char* SettingsStore::PREF_KEY_2H_SEC_COALESCE = "2hSecCoalesce";

// Auto Anchor preference keys
const char* SettingsStore::PREF_KEY_ANCHOR_SOURCE_MODE = "anchorSourceMode";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_LAST_VALUE = "autoAnchorLastValue";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_LAST_UPDATE_EPOCH = "autoAnchorLastUpdateEpoch";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_UPDATE_MINUTES = "autoAnchorUpdateMinutes";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_FORCE_UPDATE_MINUTES = "autoAnchorForceUpdateMinutes";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_4H_CANDLES = "autoAnchor4hCandles";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_1D_CANDLES = "autoAnchor1dCandles";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_MIN_UPDATE_PCT = "autoAnchorMinUpdatePct";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_TREND_PIVOT_PCT = "autoAnchorTrendPivotPct";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_W4H_BASE = "autoAnchorW4hBase";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_W4H_TREND_BOOST = "autoAnchorW4hTrendBoost";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_INTERVAL_MODE = "autoAnchorIntervalMode";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_NOTIFY_ENABLED = "autoAnchorNotifyEnabled";
const char* SettingsStore::PREF_KEY_AUTO_ANCHOR_BLOB = "autoAnchorBlob";  // Één key voor config-blob

// Helper: Generate unique ESP32 device ID using Crockford Base32 encoding
// Deze functie moet extern beschikbaar zijn voor generateDefaultNtfyTopic
static void getESP32DeviceId(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize < 9) return;
    
    uint64_t chipid = ESP.getEfuseMac(); // 48-bit unique MAC address
    
    // Extract 40 bits (8 characters * 5 bits each) from the MAC address
    // Use lower 40 bits of MAC address
    uint64_t id = chipid & 0xFFFFFFFFFF; // 40 bits
    
    // Crockford Base32 encoding table (safe characters, no ambiguous chars)
    const char* base32 = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    
    // Encode 8 characters (40 bits / 5 bits per char = 8 chars)
    for (int i = 7; i >= 0; i--) {
        buffer[i] = base32[id & 0x1F]; // 5 bits
        id >>= 5;
    }
    buffer[8] = '\0';
}

// Helper: Load string preference (geoptimaliseerd: elimineert duplicatie)
// Consolideert getString + toCharArray logica
void SettingsStore::loadStringPreference(const char* key, char* buffer, size_t bufferSize, const char* defaultValue) {
    if (buffer == nullptr || bufferSize == 0) return;
    
    String value = prefs.getString(key, defaultValue);
    value.toCharArray(buffer, bufferSize);
    buffer[bufferSize - 1] = '\0'; // Ensure null termination
}

// Helper: Check if topic needs migration (geoptimaliseerd: geconsolideerde logica)
// Geconsolideerde migration checks in één functie
bool SettingsStore::needsTopicMigration(const char* topic) {
    if (topic == nullptr) return false;
    
    // Check exact match first (sneller)
    if (strcmp(topic, "crypto-monitor-alerts") == 0) {
        return true;
    }
    
    // Check if ends with "-alert"
    size_t topicLen = strlen(topic);
    if (topicLen < 6) return false; // "-alert" is 6 chars
    
    const char* suffix = topic + topicLen - 6;
    if (strcmp(suffix, "-alert") != 0) return false;
    
    // Check if starts with "crypt-" and has correct length
    if (topicLen != 14) return false; // "crypt-" (6) + hex (8) = 14
    if (strncmp(topic, "crypt-", 6) != 0) return false;
    
    // Check if hex part is valid (8 chars after "crypt-")
    const char* hexPart = topic + 6;
    for (int i = 0; i < 8; i++) {
        char c = hexPart[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    
    return true;
}

SettingsStore::SettingsStore() {
}

bool SettingsStore::begin() {
    return true; // Preferences heeft geen expliciete begin() nodig
}

void SettingsStore::generateDefaultNtfyTopic(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize < 64) return;
    
    char deviceId[9];
    getESP32DeviceId(deviceId, sizeof(deviceId));
    snprintf(buffer, bufferSize, "%s-alert", deviceId);
}

CryptoMonitorSettings::CryptoMonitorSettings() {
    // Initialize met defaults
    ntfyTopic[0] = '\0';
    strncpy(bitvavoSymbol, BITVAVO_SYMBOL_DEFAULT, sizeof(bitvavoSymbol) - 1);
    bitvavoSymbol[sizeof(bitvavoSymbol) - 1] = '\0';
    language = DEFAULT_LANGUAGE;
    displayRotation = 0;  // Default: normaal (0 graden)
    
    // Alert thresholds defaults
    alertThresholds.spike1m = SPIKE_1M_THRESHOLD_DEFAULT;
    alertThresholds.spike5m = SPIKE_5M_THRESHOLD_DEFAULT;
    alertThresholds.move30m = MOVE_30M_THRESHOLD_DEFAULT;
    alertThresholds.move5m = MOVE_5M_THRESHOLD_DEFAULT;
    alertThresholds.move5mAlert = MOVE_5M_ALERT_THRESHOLD_DEFAULT;
    alertThresholds.threshold1MinUp = THRESHOLD_1MIN_UP_DEFAULT;
    alertThresholds.threshold1MinDown = THRESHOLD_1MIN_DOWN_DEFAULT;
    alertThresholds.threshold30MinUp = THRESHOLD_30MIN_UP_DEFAULT;
    alertThresholds.threshold30MinDown = THRESHOLD_30MIN_DOWN_DEFAULT;
    
    // Notification cooldowns defaults
    notificationCooldowns.cooldown1MinMs = NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT;
    notificationCooldowns.cooldown30MinMs = NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT;
    notificationCooldowns.cooldown5MinMs = NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT;
    
    // MQTT defaults
    strncpy(mqttHost, MQTT_HOST_DEFAULT, sizeof(mqttHost) - 1);
    mqttHost[sizeof(mqttHost) - 1] = '\0';
    mqttPort = MQTT_PORT_DEFAULT;
    strncpy(mqttUser, MQTT_USER_DEFAULT, sizeof(mqttUser) - 1);
    mqttUser[sizeof(mqttUser) - 1] = '\0';
    strncpy(mqttPass, MQTT_PASS_DEFAULT, sizeof(mqttPass) - 1);
    mqttPass[sizeof(mqttPass) - 1] = '\0';
    
    // Anchor defaults
    anchorTakeProfit = ANCHOR_TAKE_PROFIT_DEFAULT;
    anchorMaxLoss = ANCHOR_MAX_LOSS_DEFAULT;
    anchorStrategy = 0;  // Default: 0 = handmatig
    
    // Trend-adaptive anchor defaults
    trendAdaptiveAnchorsEnabled = TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT;
    uptrendMaxLossMultiplier = UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
    uptrendTakeProfitMultiplier = UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;
    downtrendMaxLossMultiplier = DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT;
    downtrendTakeProfitMultiplier = DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT;
    
    // Smart Confluence defaults
    smartConfluenceEnabled = SMART_CONFLUENCE_ENABLED_DEFAULT;
    
    // Warm-Start defaults
    warmStartEnabled = WARM_START_ENABLED_DEFAULT;
    warmStart1mExtraCandles = WARM_START_1M_EXTRA_CANDLES_DEFAULT;
    warmStart5mCandles = WARM_START_5M_CANDLES_DEFAULT;
    warmStart30mCandles = WARM_START_30M_CANDLES_DEFAULT;
    warmStart2hCandles = WARM_START_2H_CANDLES_DEFAULT;
    
    // Auto-Volatility defaults
    autoVolatilityEnabled = AUTO_VOLATILITY_ENABLED_DEFAULT;
    autoVolatilityWindowMinutes = AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT;
    autoVolatilityBaseline1mStdPct = AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT;
    autoVolatilityMinMultiplier = AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT;
    autoVolatilityMaxMultiplier = AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT;
    
    // Trend and volatility defaults
    trendThreshold = TREND_THRESHOLD_DEFAULT;
    volatilityLowThreshold = VOLATILITY_LOW_THRESHOLD_DEFAULT;
    volatilityHighThreshold = VOLATILITY_HIGH_THRESHOLD_DEFAULT;
    
    // 2-hour alert thresholds defaults (van Alert2HThresholds namespace)
    alert2HThresholds.breakMarginPct = 0.15f;
    alert2HThresholds.breakResetMarginPct = 0.10f;
    alert2HThresholds.breakCooldownMs = 30UL * 60UL * 1000UL; // 30 min
    alert2HThresholds.meanMinDistancePct = 0.60f;
    alert2HThresholds.meanTouchBandPct = 0.10f;
    alert2HThresholds.meanCooldownMs = 60UL * 60UL * 1000UL; // 60 min
    alert2HThresholds.compressThresholdPct = 0.80f;
    alert2HThresholds.compressResetPct = 1.10f;
    alert2HThresholds.compressCooldownMs = 2UL * 60UL * 60UL * 1000UL; // 2 uur
    alert2HThresholds.anchorOutsideMarginPct = 0.25f;
    alert2HThresholds.anchorCooldownMs = 3UL * 60UL * 60UL * 1000UL; // 3 uur
    // FASE X.4: Trend hysteresis en throttling defaults
    alert2HThresholds.trendHysteresisFactor = 0.65f;  // 65% van threshold voor trend exit
    alert2HThresholds.throttlingTrendChangeMs = 180UL * 60UL * 1000UL; // 180 min
    alert2HThresholds.throttlingTrendToMeanMs = 60UL * 60UL * 1000UL;   // 60 min
    alert2HThresholds.throttlingMeanTouchMs = 60UL * 60UL * 1000UL;    // 60 min
    alert2HThresholds.throttlingCompressMs = 120UL * 60UL * 1000UL;    // 120 min
    // FASE X.5: Secondary global cooldown en coalescing defaults
    alert2HThresholds.twoHSecondaryGlobalCooldownSec = 7200UL;  // 120 minuten (default)
    alert2HThresholds.twoHSecondaryCoalesceWindowSec = 90UL;      // 90 seconden (default)
    
    // Auto Anchor defaults (geoptimaliseerd: gebruik compacte representatie)
    // Note: hasPSRAM() wordt later aangeroepen in load() voor dynamische defaults
    alert2HThresholds.anchorSourceMode = 0;  // MANUAL
    alert2HThresholds.autoAnchorLastValue = 0.0f;
    alert2HThresholds.autoAnchorLastUpdateEpoch = 0;
    alert2HThresholds.autoAnchorUpdateMinutes = 120;
    alert2HThresholds.autoAnchorForceUpdateMinutes = 720;
    alert2HThresholds.autoAnchor4hCandles = 24;  // Default zonder PSRAM, wordt overschreven in load()
    alert2HThresholds.autoAnchor1dCandles = 14;  // Default zonder PSRAM, wordt overschreven in load()
    alert2HThresholds.autoAnchorMinUpdatePct_x100 = 15;  // 0.15% * 100
    alert2HThresholds.autoAnchorTrendPivotPct_x100 = 100;  // 1.00% * 100
    alert2HThresholds.autoAnchorW4hBase_x100 = 35;  // 0.35 * 100
    alert2HThresholds.autoAnchorW4hTrendBoost_x100 = 35;  // 0.35 * 100
    alert2HThresholds.autoAnchorFlags = 0;  // intervalMode=0, notifyEnabled=false
}

CryptoMonitorSettings SettingsStore::load() {
    CryptoMonitorSettings settings;
    prefs.begin(PREF_NAMESPACE, true); // read-only mode
    
    // Generate default NTFY topic
    char defaultTopic[64];
    generateDefaultNtfyTopic(defaultTopic, sizeof(defaultTopic));
    
    // Load NTFY topic with migration logic (geoptimaliseerd: gebruik helper)
    char topicBuffer[64];
    String topic = prefs.getString(PREF_KEY_NTFY_TOPIC, defaultTopic);
    topic.toCharArray(topicBuffer, sizeof(topicBuffer));
    
    // Geoptimaliseerd: gebruik helper functie voor migration check
    if (needsTopicMigration(topicBuffer)) {
        strncpy(topicBuffer, defaultTopic, sizeof(topicBuffer) - 1);
        topicBuffer[sizeof(topicBuffer) - 1] = '\0';
        prefs.end();
        prefs.begin(PREF_NAMESPACE, false);
        prefs.putString(PREF_KEY_NTFY_TOPIC, topicBuffer);
        prefs.end();
        prefs.begin(PREF_NAMESPACE, true);
    }
    
    strncpy(settings.ntfyTopic, topicBuffer, sizeof(settings.ntfyTopic) - 1);
    settings.ntfyTopic[sizeof(settings.ntfyTopic) - 1] = '\0';
    
    // Geoptimaliseerd: gebruik helper functie voor string loading
    loadStringPreference(PREF_KEY_BITVAVO_SYMBOL, settings.bitvavoSymbol, 
                        sizeof(settings.bitvavoSymbol), BITVAVO_SYMBOL_DEFAULT);
    
    // Load language
    settings.language = prefs.getUChar(PREF_KEY_LANGUAGE, DEFAULT_LANGUAGE);
    
    // Load display rotation
    settings.displayRotation = prefs.getUChar(PREF_KEY_DISPLAY_ROTATION, 0);
    
    // Load alert thresholds
    settings.alertThresholds.threshold1MinUp = prefs.getFloat(PREF_KEY_TH1_UP, THRESHOLD_1MIN_UP_DEFAULT);
    settings.alertThresholds.threshold1MinDown = prefs.getFloat(PREF_KEY_TH1_DOWN, THRESHOLD_1MIN_DOWN_DEFAULT);
    settings.alertThresholds.threshold30MinUp = prefs.getFloat(PREF_KEY_TH30_UP, THRESHOLD_30MIN_UP_DEFAULT);
    settings.alertThresholds.threshold30MinDown = prefs.getFloat(PREF_KEY_TH30_DOWN, THRESHOLD_30MIN_DOWN_DEFAULT);
    settings.alertThresholds.spike1m = prefs.getFloat(PREF_KEY_SPIKE1M, SPIKE_1M_THRESHOLD_DEFAULT);
    settings.alertThresholds.spike5m = prefs.getFloat(PREF_KEY_SPIKE5M, SPIKE_5M_THRESHOLD_DEFAULT);
    settings.alertThresholds.move30m = prefs.getFloat(PREF_KEY_MOVE30M, MOVE_30M_THRESHOLD_DEFAULT);
    settings.alertThresholds.move5m = prefs.getFloat(PREF_KEY_MOVE5M, MOVE_5M_THRESHOLD_DEFAULT);
    settings.alertThresholds.move5mAlert = prefs.getFloat(PREF_KEY_MOVE5M_ALERT, MOVE_5M_ALERT_THRESHOLD_DEFAULT);
    
    // Load notification cooldowns
    settings.notificationCooldowns.cooldown1MinMs = prefs.getULong(PREF_KEY_CD1MIN, NOTIFICATION_COOLDOWN_1MIN_MS_DEFAULT);
    settings.notificationCooldowns.cooldown30MinMs = prefs.getULong(PREF_KEY_CD30MIN, NOTIFICATION_COOLDOWN_30MIN_MS_DEFAULT);
    settings.notificationCooldowns.cooldown5MinMs = prefs.getULong(PREF_KEY_CD5MIN, NOTIFICATION_COOLDOWN_5MIN_MS_DEFAULT);
    
    // Load MQTT settings (geoptimaliseerd: gebruik helper functie)
    loadStringPreference(PREF_KEY_MQTT_HOST, settings.mqttHost, 
                        sizeof(settings.mqttHost), MQTT_HOST_DEFAULT);
    settings.mqttPort = prefs.getUInt(PREF_KEY_MQTT_PORT, MQTT_PORT_DEFAULT);
    loadStringPreference(PREF_KEY_MQTT_USER, settings.mqttUser, 
                        sizeof(settings.mqttUser), MQTT_USER_DEFAULT);
    loadStringPreference(PREF_KEY_MQTT_PASS, settings.mqttPass, 
                        sizeof(settings.mqttPass), MQTT_PASS_DEFAULT);
    
    // Load anchor settings
    settings.anchorTakeProfit = prefs.getFloat(PREF_KEY_ANCHOR_TP, ANCHOR_TAKE_PROFIT_DEFAULT);
    settings.anchorMaxLoss = prefs.getFloat(PREF_KEY_ANCHOR_ML, ANCHOR_MAX_LOSS_DEFAULT);
    settings.anchorStrategy = prefs.getUChar(PREF_KEY_ANCHOR_STRATEGY, 0);
    
    // Load trend-adaptive anchor settings
    settings.trendAdaptiveAnchorsEnabled = prefs.getBool(PREF_KEY_TREND_ADAPT, TREND_ADAPTIVE_ANCHORS_ENABLED_DEFAULT);
    settings.uptrendMaxLossMultiplier = prefs.getFloat(PREF_KEY_UP_ML_MULT, UPTREND_MAX_LOSS_MULTIPLIER_DEFAULT);
    settings.uptrendTakeProfitMultiplier = prefs.getFloat(PREF_KEY_UP_TP_MULT, UPTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT);
    settings.downtrendMaxLossMultiplier = prefs.getFloat(PREF_KEY_DOWN_ML_MULT, DOWNTREND_MAX_LOSS_MULTIPLIER_DEFAULT);
    settings.downtrendTakeProfitMultiplier = prefs.getFloat(PREF_KEY_DOWN_TP_MULT, DOWNTREND_TAKE_PROFIT_MULTIPLIER_DEFAULT);
    
    // Load Smart Confluence Mode
    settings.smartConfluenceEnabled = prefs.getBool(PREF_KEY_SMART_CONF, SMART_CONFLUENCE_ENABLED_DEFAULT);
    
    // Load Warm-Start settings
    settings.warmStartEnabled = prefs.getBool(PREF_KEY_WARM_START, WARM_START_ENABLED_DEFAULT);
    settings.warmStart1mExtraCandles = prefs.getUChar(PREF_KEY_WS1M_EXTRA, WARM_START_1M_EXTRA_CANDLES_DEFAULT);
    settings.warmStart5mCandles = prefs.getUChar(PREF_KEY_WS5M, WARM_START_5M_CANDLES_DEFAULT);
    settings.warmStart30mCandles = prefs.getUChar(PREF_KEY_WS30M, WARM_START_30M_CANDLES_DEFAULT);
    settings.warmStart2hCandles = prefs.getUChar(PREF_KEY_WS2H, WARM_START_2H_CANDLES_DEFAULT);
    
    // Load Auto-Volatility Mode settings
    settings.autoVolatilityEnabled = prefs.getBool(PREF_KEY_AUTO_VOL, AUTO_VOLATILITY_ENABLED_DEFAULT);
    settings.autoVolatilityWindowMinutes = prefs.getUChar(PREF_KEY_AUTO_VOL_WIN, AUTO_VOLATILITY_WINDOW_MINUTES_DEFAULT);
    settings.autoVolatilityBaseline1mStdPct = prefs.getFloat(PREF_KEY_AUTO_VOL_BASE, AUTO_VOLATILITY_BASELINE_1M_STD_PCT_DEFAULT);
    settings.autoVolatilityMinMultiplier = prefs.getFloat(PREF_KEY_AUTO_VOL_MIN, AUTO_VOLATILITY_MIN_MULTIPLIER_DEFAULT);
    settings.autoVolatilityMaxMultiplier = prefs.getFloat(PREF_KEY_AUTO_VOL_MAX, AUTO_VOLATILITY_MAX_MULTIPLIER_DEFAULT);
    
    // Load trend and volatility settings
    settings.trendThreshold = prefs.getFloat(PREF_KEY_TREND_TH, TREND_THRESHOLD_DEFAULT);
    settings.volatilityLowThreshold = prefs.getFloat(PREF_KEY_VOL_LOW, VOLATILITY_LOW_THRESHOLD_DEFAULT);
    settings.volatilityHighThreshold = prefs.getFloat(PREF_KEY_VOL_HIGH, VOLATILITY_HIGH_THRESHOLD_DEFAULT);
    
    // Load 2-hour alert thresholds
    settings.alert2HThresholds.breakMarginPct = prefs.getFloat(PREF_KEY_2H_BREAK_MARGIN, 0.15f);
    settings.alert2HThresholds.breakResetMarginPct = prefs.getFloat(PREF_KEY_2H_BREAK_RESET, 0.10f);
    settings.alert2HThresholds.breakCooldownMs = prefs.getULong(PREF_KEY_2H_BREAK_CD, 30UL * 60UL * 1000UL);
    settings.alert2HThresholds.meanMinDistancePct = prefs.getFloat(PREF_KEY_2H_MEAN_MIN_DIST, 0.60f);
    settings.alert2HThresholds.meanTouchBandPct = prefs.getFloat(PREF_KEY_2H_MEAN_TOUCH, 0.10f);
    settings.alert2HThresholds.meanCooldownMs = prefs.getULong(PREF_KEY_2H_MEAN_CD, 60UL * 60UL * 1000UL);
    settings.alert2HThresholds.compressThresholdPct = prefs.getFloat(PREF_KEY_2H_COMPRESS_TH, 0.80f);
    settings.alert2HThresholds.compressResetPct = prefs.getFloat(PREF_KEY_2H_COMPRESS_RESET, 1.10f);
    settings.alert2HThresholds.compressCooldownMs = prefs.getULong(PREF_KEY_2H_COMPRESS_CD, 2UL * 60UL * 60UL * 1000UL);
    settings.alert2HThresholds.anchorOutsideMarginPct = prefs.getFloat(PREF_KEY_2H_ANCHOR_MARGIN, 0.25f);
    settings.alert2HThresholds.anchorCooldownMs = prefs.getULong(PREF_KEY_2H_ANCHOR_CD, 3UL * 60UL * 60UL * 1000UL);
    // FASE X.4: Trend hysteresis en throttling settings
    settings.alert2HThresholds.trendHysteresisFactor = prefs.getFloat(PREF_KEY_2H_TREND_HYSTERESIS, 0.65f);
    settings.alert2HThresholds.throttlingTrendChangeMs = prefs.getULong(PREF_KEY_2H_THROTTLE_TREND_CHANGE, 180UL * 60UL * 1000UL);
    settings.alert2HThresholds.throttlingTrendToMeanMs = prefs.getULong(PREF_KEY_2H_THROTTLE_TREND_MEAN, 60UL * 60UL * 1000UL);
    settings.alert2HThresholds.throttlingMeanTouchMs = prefs.getULong(PREF_KEY_2H_THROTTLE_MEAN_TOUCH, 60UL * 60UL * 1000UL);
    settings.alert2HThresholds.throttlingCompressMs = prefs.getULong(PREF_KEY_2H_THROTTLE_COMPRESS, 120UL * 60UL * 1000UL);
    // FASE X.5: Secondary global cooldown en coalescing settings
    settings.alert2HThresholds.twoHSecondaryGlobalCooldownSec = prefs.getULong(PREF_KEY_2H_SEC_GLOBAL_CD, 7200UL);
    settings.alert2HThresholds.twoHSecondaryCoalesceWindowSec = prefs.getULong(PREF_KEY_2H_SEC_COALESCE, 90UL);
    
    // Load Auto Anchor settings (gebruik config-blob indien beschikbaar, anders individuele keys)
    bool psramAvailable = hasPSRAM();
    AutoAnchorPersist blob;
    size_t blobSize = prefs.getBytes(PREF_KEY_AUTO_ANCHOR_BLOB, &blob, sizeof(blob));
    
    if (blobSize == sizeof(AutoAnchorPersist) && blob.version == 1) {
        // Config-blob gevonden en valide
        settings.alert2HThresholds.anchorSourceMode = blob.anchorSourceMode;
        settings.alert2HThresholds.autoAnchorLastValue = blob.autoAnchorLastValue;
        settings.alert2HThresholds.autoAnchorLastUpdateEpoch = blob.autoAnchorLastUpdateEpoch;
        settings.alert2HThresholds.autoAnchorUpdateMinutes = blob.autoAnchorUpdateMinutes;
        settings.alert2HThresholds.autoAnchorForceUpdateMinutes = blob.autoAnchorForceUpdateMinutes;
        settings.alert2HThresholds.autoAnchor4hCandles = blob.autoAnchor4hCandles;
        settings.alert2HThresholds.autoAnchor1dCandles = blob.autoAnchor1dCandles;
        settings.alert2HThresholds.autoAnchorMinUpdatePct_x100 = blob.autoAnchorMinUpdatePct_x100;
        settings.alert2HThresholds.autoAnchorTrendPivotPct_x100 = blob.autoAnchorTrendPivotPct_x100;
        settings.alert2HThresholds.autoAnchorW4hBase_x100 = blob.autoAnchorW4hBase_x100;
        settings.alert2HThresholds.autoAnchorW4hTrendBoost_x100 = blob.autoAnchorW4hTrendBoost_x100;
        settings.alert2HThresholds.setAutoAnchorIntervalMode(blob.getAutoAnchorIntervalMode());
        settings.alert2HThresholds.setAutoAnchorNotifyEnabled(blob.getAutoAnchorNotifyEnabled());
    } else {
        // Fallback: laad individuele keys (backward compatibility)
        uint8_t loadedMode = prefs.getUChar(PREF_KEY_ANCHOR_SOURCE_MODE, 255);
        if (loadedMode == 255) {
            // Workaround: lees uit warmStart2hCandles bit 7
            uint8_t ws2hValue = prefs.getUChar(PREF_KEY_WS2H, WARM_START_2H_CANDLES_DEFAULT);
            loadedMode = (ws2hValue >> 7) & 0x01;
            uint8_t ws2hOriginal = ws2hValue & 0x7F;
            
            if ((ws2hValue & 0x80) != 0) {
                Serial.printf("[SettingsStore] Loaded anchorSourceMode=%d via workaround (warmStart2hCandles bit 7)\n", loadedMode);
                Serial.printf("[SettingsStore] NOTE: Restored warmStart2hCandles to %d (was %d with bit 7 set)\n", 
                            ws2hOriginal, ws2hValue);
                settings.warmStart2hCandles = ws2hOriginal;
            } else {
                loadedMode = 0;  // Default: MANUAL
            }
        }
        settings.alert2HThresholds.anchorSourceMode = loadedMode;
        
        settings.alert2HThresholds.autoAnchorLastValue = prefs.getFloat(PREF_KEY_AUTO_ANCHOR_LAST_VALUE, 0.0f);
        settings.alert2HThresholds.autoAnchorLastUpdateEpoch = prefs.getULong(PREF_KEY_AUTO_ANCHOR_LAST_UPDATE_EPOCH, 0);
        settings.alert2HThresholds.autoAnchorUpdateMinutes = prefs.getUShort(PREF_KEY_AUTO_ANCHOR_UPDATE_MINUTES, 120);
        settings.alert2HThresholds.autoAnchorForceUpdateMinutes = prefs.getUShort(PREF_KEY_AUTO_ANCHOR_FORCE_UPDATE_MINUTES, 720);
        settings.alert2HThresholds.autoAnchor4hCandles = prefs.getUChar(PREF_KEY_AUTO_ANCHOR_4H_CANDLES, psramAvailable ? 48 : 24);
        settings.alert2HThresholds.autoAnchor1dCandles = prefs.getUChar(PREF_KEY_AUTO_ANCHOR_1D_CANDLES, psramAvailable ? 30 : 14);
        // Load floats en converteer naar compacte types
        float minUpdatePct = prefs.getFloat(PREF_KEY_AUTO_ANCHOR_MIN_UPDATE_PCT, 0.15f);
        settings.alert2HThresholds.setAutoAnchorMinUpdatePct(minUpdatePct);
        float trendPivotPct = prefs.getFloat(PREF_KEY_AUTO_ANCHOR_TREND_PIVOT_PCT, 1.00f);
        settings.alert2HThresholds.setAutoAnchorTrendPivotPct(trendPivotPct);
        float w4hBase = prefs.getFloat(PREF_KEY_AUTO_ANCHOR_W4H_BASE, 0.35f);
        settings.alert2HThresholds.setAutoAnchorW4hBase(w4hBase);
        float w4hTrendBoost = prefs.getFloat(PREF_KEY_AUTO_ANCHOR_W4H_TREND_BOOST, 0.35f);
        settings.alert2HThresholds.setAutoAnchorW4hTrendBoost(w4hTrendBoost);
        uint8_t intervalMode = prefs.getUChar(PREF_KEY_AUTO_ANCHOR_INTERVAL_MODE, 0);
        settings.alert2HThresholds.setAutoAnchorIntervalMode(intervalMode);
        bool notifyEnabled = prefs.getBool(PREF_KEY_AUTO_ANCHOR_NOTIFY_ENABLED, false);
        settings.alert2HThresholds.setAutoAnchorNotifyEnabled(notifyEnabled);
    }
    
    // Clamp waarden (helper functies moeten nog worden toegevoegd)
    if (settings.alert2HThresholds.autoAnchorUpdateMinutes < 10) settings.alert2HThresholds.autoAnchorUpdateMinutes = 10;
    if (settings.alert2HThresholds.autoAnchorUpdateMinutes > 1440) settings.alert2HThresholds.autoAnchorUpdateMinutes = 1440;
    if (settings.alert2HThresholds.autoAnchorForceUpdateMinutes < 60) settings.alert2HThresholds.autoAnchorForceUpdateMinutes = 60;
    if (settings.alert2HThresholds.autoAnchorForceUpdateMinutes > 2880) settings.alert2HThresholds.autoAnchorForceUpdateMinutes = 2880;
    uint8_t max4h = psramAvailable ? 100 : 50;
    uint8_t max1d = psramAvailable ? 60 : 30;
    if (settings.alert2HThresholds.autoAnchor4hCandles < 2) settings.alert2HThresholds.autoAnchor4hCandles = 2;
    if (settings.alert2HThresholds.autoAnchor4hCandles > max4h) settings.alert2HThresholds.autoAnchor4hCandles = max4h;
    if (settings.alert2HThresholds.autoAnchor1dCandles < 2) settings.alert2HThresholds.autoAnchor1dCandles = 2;
    if (settings.alert2HThresholds.autoAnchor1dCandles > max1d) settings.alert2HThresholds.autoAnchor1dCandles = max1d;
    // Clamp compacte types
    if (settings.alert2HThresholds.autoAnchorMinUpdatePct_x100 < 1) settings.alert2HThresholds.autoAnchorMinUpdatePct_x100 = 1;  // 0.01%
    if (settings.alert2HThresholds.autoAnchorMinUpdatePct_x100 > 500) settings.alert2HThresholds.autoAnchorMinUpdatePct_x100 = 500;  // 5.0%
    if (settings.alert2HThresholds.autoAnchorTrendPivotPct_x100 < 10) settings.alert2HThresholds.autoAnchorTrendPivotPct_x100 = 10;  // 0.1%
    if (settings.alert2HThresholds.autoAnchorTrendPivotPct_x100 > 1000) settings.alert2HThresholds.autoAnchorTrendPivotPct_x100 = 1000;  // 10.0%
    if (settings.alert2HThresholds.autoAnchorW4hBase_x100 > 100) settings.alert2HThresholds.autoAnchorW4hBase_x100 = 100;  // 1.0
    if (settings.alert2HThresholds.autoAnchorW4hTrendBoost_x100 > 100) settings.alert2HThresholds.autoAnchorW4hTrendBoost_x100 = 100;  // 1.0
    
    prefs.end();
    return settings;
}

void SettingsStore::save(const CryptoMonitorSettings& settings) {
    prefs.begin(PREF_NAMESPACE, false); // read-write mode
    
    // Save basic settings
    prefs.putString(PREF_KEY_NTFY_TOPIC, settings.ntfyTopic);
    prefs.putString(PREF_KEY_BITVAVO_SYMBOL, settings.bitvavoSymbol);
    prefs.putUChar(PREF_KEY_LANGUAGE, settings.language);
    prefs.putUChar(PREF_KEY_DISPLAY_ROTATION, settings.displayRotation);
    
    // Save alert thresholds
    prefs.putFloat(PREF_KEY_TH1_UP, settings.alertThresholds.threshold1MinUp);
    prefs.putFloat(PREF_KEY_TH1_DOWN, settings.alertThresholds.threshold1MinDown);
    prefs.putFloat(PREF_KEY_TH30_UP, settings.alertThresholds.threshold30MinUp);
    prefs.putFloat(PREF_KEY_TH30_DOWN, settings.alertThresholds.threshold30MinDown);
    prefs.putFloat(PREF_KEY_SPIKE1M, settings.alertThresholds.spike1m);
    prefs.putFloat(PREF_KEY_SPIKE5M, settings.alertThresholds.spike5m);
    prefs.putFloat(PREF_KEY_MOVE30M, settings.alertThresholds.move30m);
    prefs.putFloat(PREF_KEY_MOVE5M, settings.alertThresholds.move5m);
    prefs.putFloat(PREF_KEY_MOVE5M_ALERT, settings.alertThresholds.move5mAlert);
    
    // Save notification cooldowns
    prefs.putULong(PREF_KEY_CD1MIN, settings.notificationCooldowns.cooldown1MinMs);
    prefs.putULong(PREF_KEY_CD30MIN, settings.notificationCooldowns.cooldown30MinMs);
    prefs.putULong(PREF_KEY_CD5MIN, settings.notificationCooldowns.cooldown5MinMs);
    
    // Save MQTT settings
    prefs.putString(PREF_KEY_MQTT_HOST, settings.mqttHost);
    prefs.putUInt(PREF_KEY_MQTT_PORT, settings.mqttPort);
    prefs.putString(PREF_KEY_MQTT_USER, settings.mqttUser);
    prefs.putString(PREF_KEY_MQTT_PASS, settings.mqttPass);
    
    // Save anchor settings
    prefs.putFloat(PREF_KEY_ANCHOR_TP, settings.anchorTakeProfit);
    prefs.putFloat(PREF_KEY_ANCHOR_ML, settings.anchorMaxLoss);
    prefs.putUChar(PREF_KEY_ANCHOR_STRATEGY, settings.anchorStrategy);
    
    // Save trend-adaptive anchor settings
    prefs.putBool(PREF_KEY_TREND_ADAPT, settings.trendAdaptiveAnchorsEnabled);
    prefs.putFloat(PREF_KEY_UP_ML_MULT, settings.uptrendMaxLossMultiplier);
    prefs.putFloat(PREF_KEY_UP_TP_MULT, settings.uptrendTakeProfitMultiplier);
    prefs.putFloat(PREF_KEY_DOWN_ML_MULT, settings.downtrendMaxLossMultiplier);
    prefs.putFloat(PREF_KEY_DOWN_TP_MULT, settings.downtrendTakeProfitMultiplier);
    
    // Save Smart Confluence Mode
    prefs.putBool(PREF_KEY_SMART_CONF, settings.smartConfluenceEnabled);
    
    // Save Warm-Start settings
    prefs.putBool(PREF_KEY_WARM_START, settings.warmStartEnabled);
    prefs.putUChar(PREF_KEY_WS1M_EXTRA, settings.warmStart1mExtraCandles);
    prefs.putUChar(PREF_KEY_WS5M, settings.warmStart5mCandles);
    prefs.putUChar(PREF_KEY_WS30M, settings.warmStart30mCandles);
    prefs.putUChar(PREF_KEY_WS2H, settings.warmStart2hCandles);
    
    // Save Auto-Volatility Mode settings
    prefs.putBool(PREF_KEY_AUTO_VOL, settings.autoVolatilityEnabled);
    prefs.putUChar(PREF_KEY_AUTO_VOL_WIN, settings.autoVolatilityWindowMinutes);
    prefs.putFloat(PREF_KEY_AUTO_VOL_BASE, settings.autoVolatilityBaseline1mStdPct);
    prefs.putFloat(PREF_KEY_AUTO_VOL_MIN, settings.autoVolatilityMinMultiplier);
    prefs.putFloat(PREF_KEY_AUTO_VOL_MAX, settings.autoVolatilityMaxMultiplier);
    
    // Save trend and volatility settings
    prefs.putFloat(PREF_KEY_TREND_TH, settings.trendThreshold);
    prefs.putFloat(PREF_KEY_VOL_LOW, settings.volatilityLowThreshold);
    prefs.putFloat(PREF_KEY_VOL_HIGH, settings.volatilityHighThreshold);
    
    // Save Auto Anchor settings (gebruik config-blob)
    AutoAnchorPersist blob;
    blob.version = 1;
    blob.anchorSourceMode = settings.alert2HThresholds.anchorSourceMode;
    blob.autoAnchorLastValue = settings.alert2HThresholds.autoAnchorLastValue;
    blob.autoAnchorLastUpdateEpoch = settings.alert2HThresholds.autoAnchorLastUpdateEpoch;
    blob.autoAnchorUpdateMinutes = settings.alert2HThresholds.autoAnchorUpdateMinutes;
    blob.autoAnchorForceUpdateMinutes = settings.alert2HThresholds.autoAnchorForceUpdateMinutes;
    blob.autoAnchor4hCandles = settings.alert2HThresholds.autoAnchor4hCandles;
    blob.autoAnchor1dCandles = settings.alert2HThresholds.autoAnchor1dCandles;
    blob.autoAnchorMinUpdatePct_x100 = settings.alert2HThresholds.autoAnchorMinUpdatePct_x100;
    blob.autoAnchorTrendPivotPct_x100 = settings.alert2HThresholds.autoAnchorTrendPivotPct_x100;
    blob.autoAnchorW4hBase_x100 = settings.alert2HThresholds.autoAnchorW4hBase_x100;
    blob.autoAnchorW4hTrendBoost_x100 = settings.alert2HThresholds.autoAnchorW4hTrendBoost_x100;
    blob.setAutoAnchorIntervalMode(settings.alert2HThresholds.getAutoAnchorIntervalMode());
    blob.setAutoAnchorNotifyEnabled(settings.alert2HThresholds.getAutoAnchorNotifyEnabled());
    blob.crc = blob.calculateCRC();
    
    size_t blobResult = prefs.putBytes(PREF_KEY_AUTO_ANCHOR_BLOB, &blob, sizeof(blob));
    if (blobResult == sizeof(blob)) {
        Serial.printf("[SettingsStore] Saved auto anchor config-blob (version 1, %d bytes)\n", sizeof(blob));
    } else {
        Serial.printf("[SettingsStore] WARNING: Failed to save auto anchor config-blob, falling back to individual keys\n");
        // Fallback: save individuele keys (backward compatibility)
        size_t result = prefs.putUChar(PREF_KEY_ANCHOR_SOURCE_MODE, settings.alert2HThresholds.anchorSourceMode);
        if (result == 0) {
            // Workaround: sla op in warmStart2hCandles bit 7
            uint8_t existingWs2h = prefs.getUChar(PREF_KEY_WS2H, WARM_START_2H_CANDLES_DEFAULT);
            uint8_t combinedValue = (existingWs2h & 0x7F) | ((settings.alert2HThresholds.anchorSourceMode & 0x01) << 7);
            size_t combinedResult = prefs.putUChar(PREF_KEY_WS2H, combinedValue);
            if (combinedResult > 0) {
                Serial.printf("[SettingsStore] Workaround: stored anchorSourceMode in warmStart2hCandles (bit 7)\n");
            }
        }
        prefs.putFloat(PREF_KEY_AUTO_ANCHOR_LAST_VALUE, settings.alert2HThresholds.autoAnchorLastValue);
        prefs.putULong(PREF_KEY_AUTO_ANCHOR_LAST_UPDATE_EPOCH, settings.alert2HThresholds.autoAnchorLastUpdateEpoch);
        prefs.putUShort(PREF_KEY_AUTO_ANCHOR_UPDATE_MINUTES, settings.alert2HThresholds.autoAnchorUpdateMinutes);
        prefs.putUShort(PREF_KEY_AUTO_ANCHOR_FORCE_UPDATE_MINUTES, settings.alert2HThresholds.autoAnchorForceUpdateMinutes);
        prefs.putUChar(PREF_KEY_AUTO_ANCHOR_4H_CANDLES, settings.alert2HThresholds.autoAnchor4hCandles);
        prefs.putUChar(PREF_KEY_AUTO_ANCHOR_1D_CANDLES, settings.alert2HThresholds.autoAnchor1dCandles);
        // Save als floats voor backward compatibility
        prefs.putFloat(PREF_KEY_AUTO_ANCHOR_MIN_UPDATE_PCT, settings.alert2HThresholds.getAutoAnchorMinUpdatePct());
        prefs.putFloat(PREF_KEY_AUTO_ANCHOR_TREND_PIVOT_PCT, settings.alert2HThresholds.getAutoAnchorTrendPivotPct());
        prefs.putFloat(PREF_KEY_AUTO_ANCHOR_W4H_BASE, settings.alert2HThresholds.getAutoAnchorW4hBase());
        prefs.putFloat(PREF_KEY_AUTO_ANCHOR_W4H_TREND_BOOST, settings.alert2HThresholds.getAutoAnchorW4hTrendBoost());
        prefs.putUChar(PREF_KEY_AUTO_ANCHOR_INTERVAL_MODE, settings.alert2HThresholds.getAutoAnchorIntervalMode());
        prefs.putBool(PREF_KEY_AUTO_ANCHOR_NOTIFY_ENABLED, settings.alert2HThresholds.getAutoAnchorNotifyEnabled());
    }
    
    // Save 2-hour alert thresholds
    prefs.putFloat(PREF_KEY_2H_BREAK_MARGIN, settings.alert2HThresholds.breakMarginPct);
    prefs.putFloat(PREF_KEY_2H_BREAK_RESET, settings.alert2HThresholds.breakResetMarginPct);
    prefs.putULong(PREF_KEY_2H_BREAK_CD, settings.alert2HThresholds.breakCooldownMs);
    prefs.putFloat(PREF_KEY_2H_MEAN_MIN_DIST, settings.alert2HThresholds.meanMinDistancePct);
    prefs.putFloat(PREF_KEY_2H_MEAN_TOUCH, settings.alert2HThresholds.meanTouchBandPct);
    prefs.putULong(PREF_KEY_2H_MEAN_CD, settings.alert2HThresholds.meanCooldownMs);
    prefs.putFloat(PREF_KEY_2H_COMPRESS_TH, settings.alert2HThresholds.compressThresholdPct);
    prefs.putFloat(PREF_KEY_2H_COMPRESS_RESET, settings.alert2HThresholds.compressResetPct);
    prefs.putULong(PREF_KEY_2H_COMPRESS_CD, settings.alert2HThresholds.compressCooldownMs);
    prefs.putFloat(PREF_KEY_2H_ANCHOR_MARGIN, settings.alert2HThresholds.anchorOutsideMarginPct);
    prefs.putULong(PREF_KEY_2H_ANCHOR_CD, settings.alert2HThresholds.anchorCooldownMs);
    // FASE X.4: Trend hysteresis en throttling settings
    prefs.putFloat(PREF_KEY_2H_TREND_HYSTERESIS, settings.alert2HThresholds.trendHysteresisFactor);
    prefs.putULong(PREF_KEY_2H_THROTTLE_TREND_CHANGE, settings.alert2HThresholds.throttlingTrendChangeMs);
    prefs.putULong(PREF_KEY_2H_THROTTLE_TREND_MEAN, settings.alert2HThresholds.throttlingTrendToMeanMs);
    prefs.putULong(PREF_KEY_2H_THROTTLE_MEAN_TOUCH, settings.alert2HThresholds.throttlingMeanTouchMs);
    prefs.putULong(PREF_KEY_2H_THROTTLE_COMPRESS, settings.alert2HThresholds.throttlingCompressMs);
    // FASE X.5: Secondary global cooldown en coalescing settings
    prefs.putULong(PREF_KEY_2H_SEC_GLOBAL_CD, settings.alert2HThresholds.twoHSecondaryGlobalCooldownSec);
    prefs.putULong(PREF_KEY_2H_SEC_COALESCE, settings.alert2HThresholds.twoHSecondaryCoalesceWindowSec);
    
    prefs.end();
}
