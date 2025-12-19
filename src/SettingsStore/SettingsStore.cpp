#include "SettingsStore.h"
#include <Arduino.h>
#include <esp_system.h>

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
#ifndef BINANCE_SYMBOL_DEFAULT
#define BINANCE_SYMBOL_DEFAULT "BTCEUR"
#endif

// Preference namespace en keys
const char* SettingsStore::PREF_NAMESPACE = "crypto";
const char* SettingsStore::PREF_KEY_NTFY_TOPIC = "ntfyTopic";
const char* SettingsStore::PREF_KEY_BINANCE_SYMBOL = "binanceSymbol";
const char* SettingsStore::PREF_KEY_LANGUAGE = "language";
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
    strncpy(binanceSymbol, BINANCE_SYMBOL_DEFAULT, sizeof(binanceSymbol) - 1);
    binanceSymbol[sizeof(binanceSymbol) - 1] = '\0';
    language = DEFAULT_LANGUAGE;
    
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
}

CryptoMonitorSettings SettingsStore::load() {
    CryptoMonitorSettings settings;
    prefs.begin(PREF_NAMESPACE, true); // read-only mode
    
    // Generate default NTFY topic
    char defaultTopic[64];
    generateDefaultNtfyTopic(defaultTopic, sizeof(defaultTopic));
    
    // Load NTFY topic with migration logic
    String topic = prefs.getString(PREF_KEY_NTFY_TOPIC, defaultTopic);
    
    // Migrate old formats (same logic as original)
    bool needsMigration = false;
    if (topic == "crypto-monitor-alerts") {
        needsMigration = true;
    } else if (topic.endsWith("-alert")) {
        int alertPos = topic.indexOf("-alert");
        if (alertPos > 0) {
            String beforeAlert = topic.substring(0, alertPos);
            if (beforeAlert.startsWith("crypt-") && beforeAlert.length() == 14) {
                String hexPart = beforeAlert.substring(6);
                bool isHex = true;
                for (int i = 0; i < hexPart.length(); i++) {
                    char c = hexPart.charAt(i);
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        isHex = false;
                        break;
                    }
                }
                if (isHex && hexPart.length() == 8) {
                    needsMigration = true;
                }
            }
        }
    }
    
    if (needsMigration) {
        topic = defaultTopic;
        prefs.end();
        prefs.begin(PREF_NAMESPACE, false);
        prefs.putString(PREF_KEY_NTFY_TOPIC, topic);
        prefs.end();
        prefs.begin(PREF_NAMESPACE, true);
    }
    
    topic.toCharArray(settings.ntfyTopic, sizeof(settings.ntfyTopic));
    
    // Load binance symbol
    String symbol = prefs.getString(PREF_KEY_BINANCE_SYMBOL, BINANCE_SYMBOL_DEFAULT);
    symbol.toCharArray(settings.binanceSymbol, sizeof(settings.binanceSymbol));
    
    // Load language
    settings.language = prefs.getUChar(PREF_KEY_LANGUAGE, DEFAULT_LANGUAGE);
    
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
    
    // Load MQTT settings
    String host = prefs.getString(PREF_KEY_MQTT_HOST, MQTT_HOST_DEFAULT);
    host.toCharArray(settings.mqttHost, sizeof(settings.mqttHost));
    settings.mqttPort = prefs.getUInt(PREF_KEY_MQTT_PORT, MQTT_PORT_DEFAULT);
    String user = prefs.getString(PREF_KEY_MQTT_USER, MQTT_USER_DEFAULT);
    user.toCharArray(settings.mqttUser, sizeof(settings.mqttUser));
    String pass = prefs.getString(PREF_KEY_MQTT_PASS, MQTT_PASS_DEFAULT);
    pass.toCharArray(settings.mqttPass, sizeof(settings.mqttPass));
    
    // Load anchor settings
    settings.anchorTakeProfit = prefs.getFloat(PREF_KEY_ANCHOR_TP, ANCHOR_TAKE_PROFIT_DEFAULT);
    settings.anchorMaxLoss = prefs.getFloat(PREF_KEY_ANCHOR_ML, ANCHOR_MAX_LOSS_DEFAULT);
    
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
    
    prefs.end();
    return settings;
}

void SettingsStore::save(const CryptoMonitorSettings& settings) {
    prefs.begin(PREF_NAMESPACE, false); // read-write mode
    
    // Save basic settings
    prefs.putString(PREF_KEY_NTFY_TOPIC, settings.ntfyTopic);
    prefs.putString(PREF_KEY_BINANCE_SYMBOL, settings.binanceSymbol);
    prefs.putUChar(PREF_KEY_LANGUAGE, settings.language);
    
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
    
    prefs.end();
}



