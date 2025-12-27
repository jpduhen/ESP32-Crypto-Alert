#ifndef SETTINGSSTORE_H
#define SETTINGSSTORE_H

#include <Preferences.h>
#include <Arduino.h>

// Alert thresholds struct
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

// Notification cooldowns struct
struct NotificationCooldowns {
    unsigned long cooldown1MinMs;
    unsigned long cooldown30MinMs;
    unsigned long cooldown5MinMs;
};

// 2-hour alert thresholds struct
// Geoptimaliseerd: gebruik uint32_t i.p.v. unsigned long (bespaart geen bytes op ESP32, maar consistenter)
struct Alert2HThresholds {
    float breakMarginPct;
    float breakResetMarginPct;
    uint32_t breakCooldownMs;
    float meanMinDistancePct;
    float meanTouchBandPct;
    uint32_t meanCooldownMs;
    float compressThresholdPct;
    float compressResetPct;
    uint32_t compressCooldownMs;
    float anchorOutsideMarginPct;
    uint32_t anchorCooldownMs;
};

// Settings struct - bevat alle instelbare waarden
struct CryptoMonitorSettings {
    // Basic settings
    char ntfyTopic[64];
    char binanceSymbol[16];
    uint8_t language;
    
    // Alert thresholds
    AlertThresholds alertThresholds;
    
    // 2-hour alert thresholds
    Alert2HThresholds alert2HThresholds;
    
    // Notification cooldowns
    NotificationCooldowns notificationCooldowns;
    
    // MQTT settings
    char mqttHost[64];
    uint16_t mqttPort;
    char mqttUser[64];
    char mqttPass[64];
    
    // Anchor settings
    float anchorTakeProfit;
    float anchorMaxLoss;
    
    // Trend-adaptive anchor settings
    bool trendAdaptiveAnchorsEnabled;
    float uptrendMaxLossMultiplier;
    float uptrendTakeProfitMultiplier;
    float downtrendMaxLossMultiplier;
    float downtrendTakeProfitMultiplier;
    
    // Smart Confluence Mode
    bool smartConfluenceEnabled;
    
    // Warm-Start settings
    bool warmStartEnabled;
    uint8_t warmStart1mExtraCandles;
    uint8_t warmStart5mCandles;
    uint8_t warmStart30mCandles;
    uint8_t warmStart2hCandles;
    
    // Auto-Volatility Mode settings
    bool autoVolatilityEnabled;
    uint8_t autoVolatilityWindowMinutes;
    float autoVolatilityBaseline1mStdPct;
    float autoVolatilityMinMultiplier;
    float autoVolatilityMaxMultiplier;
    
    // Trend and volatility settings
    float trendThreshold;
    float volatilityLowThreshold;
    float volatilityHighThreshold;
    
    // Constructor met defaults
    CryptoMonitorSettings();
};

class SettingsStore {
public:
    SettingsStore();
    bool begin();
    
    // Load en save alle settings
    CryptoMonitorSettings load();
    void save(const CryptoMonitorSettings& settings);
    
    // Helper: Generate default NTFY topic (moet extern beschikbaar zijn)
    static void generateDefaultNtfyTopic(char* buffer, size_t bufferSize);

private:
    Preferences prefs;
    static const char* PREF_NAMESPACE;
    
    // Preference keys
    static const char* PREF_KEY_NTFY_TOPIC;
    static const char* PREF_KEY_BINANCE_SYMBOL;
    static const char* PREF_KEY_LANGUAGE;
    static const char* PREF_KEY_TH1_UP;
    static const char* PREF_KEY_TH1_DOWN;
    static const char* PREF_KEY_TH30_UP;
    static const char* PREF_KEY_TH30_DOWN;
    static const char* PREF_KEY_SPIKE1M;
    static const char* PREF_KEY_SPIKE5M;
    static const char* PREF_KEY_MOVE30M;
    static const char* PREF_KEY_MOVE5M;
    static const char* PREF_KEY_MOVE5M_ALERT;
    static const char* PREF_KEY_CD1MIN;
    static const char* PREF_KEY_CD30MIN;
    static const char* PREF_KEY_CD5MIN;
    static const char* PREF_KEY_MQTT_HOST;
    static const char* PREF_KEY_MQTT_PORT;
    static const char* PREF_KEY_MQTT_USER;
    static const char* PREF_KEY_MQTT_PASS;
    static const char* PREF_KEY_ANCHOR_TP;
    static const char* PREF_KEY_ANCHOR_ML;
    static const char* PREF_KEY_TREND_ADAPT;
    static const char* PREF_KEY_UP_ML_MULT;
    static const char* PREF_KEY_UP_TP_MULT;
    static const char* PREF_KEY_DOWN_ML_MULT;
    static const char* PREF_KEY_DOWN_TP_MULT;
    static const char* PREF_KEY_SMART_CONF;
    static const char* PREF_KEY_WARM_START;
    static const char* PREF_KEY_WS1M_EXTRA;
    static const char* PREF_KEY_WS5M;
    static const char* PREF_KEY_WS30M;
    static const char* PREF_KEY_WS2H;
    static const char* PREF_KEY_AUTO_VOL;
    static const char* PREF_KEY_AUTO_VOL_WIN;
    static const char* PREF_KEY_AUTO_VOL_BASE;
    static const char* PREF_KEY_AUTO_VOL_MIN;
    static const char* PREF_KEY_AUTO_VOL_MAX;
    static const char* PREF_KEY_TREND_TH;
    static const char* PREF_KEY_VOL_LOW;
    static const char* PREF_KEY_VOL_HIGH;
    
    // 2-hour alert threshold keys
    static const char* PREF_KEY_2H_BREAK_MARGIN;
    static const char* PREF_KEY_2H_BREAK_RESET;
    static const char* PREF_KEY_2H_BREAK_CD;
    static const char* PREF_KEY_2H_MEAN_MIN_DIST;
    static const char* PREF_KEY_2H_MEAN_TOUCH;
    static const char* PREF_KEY_2H_MEAN_CD;
    static const char* PREF_KEY_2H_COMPRESS_TH;
    static const char* PREF_KEY_2H_COMPRESS_RESET;
    static const char* PREF_KEY_2H_COMPRESS_CD;
    static const char* PREF_KEY_2H_ANCHOR_MARGIN;
    static const char* PREF_KEY_2H_ANCHOR_CD;
};

#endif // SETTINGSSTORE_H



