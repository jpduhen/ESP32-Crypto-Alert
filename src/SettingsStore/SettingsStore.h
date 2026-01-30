#ifndef SETTINGSSTORE_H
#define SETTINGSSTORE_H

#include <Preferences.h>
#include <Arduino.h>
#include <stddef.h>  // Voor offsetof

// Compacte struct voor auto anchor persistent state (één NVS key)
// Versie 1: huidige implementatie
struct AutoAnchorPersist {
    uint8_t version;              // Struct versie (voor migraties)
    uint8_t anchorSourceMode;     // 0=MANUAL, 1=AUTO, 2=AUTO_FALLBACK, 3=OFF
    float autoAnchorLastValue;    // Laatste berekende waarde
    uint32_t autoAnchorLastUpdateEpoch;  // Timestamp laatste update
    uint16_t autoAnchorUpdateMinutes;    // Update interval
    uint16_t autoAnchorForceUpdateMinutes; // Force update interval
    uint8_t autoAnchor4hCandles;  // Aantal 4h candles
    uint8_t autoAnchor1dCandles;  // Aantal 1d candles
    uint16_t autoAnchorMinUpdatePct_x100;  // Min update percentage * 100
    uint16_t autoAnchorTrendPivotPct_x100; // Trend pivot percentage * 100
    uint8_t autoAnchorW4hBase_x100;       // 4h base weight * 100
    uint8_t autoAnchorW4hTrendBoost_x100; // 4h trend boost * 100
    uint8_t autoAnchorFlags;      // Bitfield: intervalMode(0-1), notifyEnabled(2)
    uint16_t crc;                  // CRC16 voor validatie (optioneel)
    
    // Helper methods voor bitfield access
    uint8_t getAutoAnchorIntervalMode() const { return (autoAnchorFlags & 0x03); }
    void setAutoAnchorIntervalMode(uint8_t mode) { autoAnchorFlags = (autoAnchorFlags & ~0x03) | (mode & 0x03); }
    bool getAutoAnchorNotifyEnabled() const { return (autoAnchorFlags & 0x04) != 0; }
    void setAutoAnchorNotifyEnabled(bool enabled) { autoAnchorFlags = enabled ? (autoAnchorFlags | 0x04) : (autoAnchorFlags & ~0x04); }
    
    // Helper: bereken CRC (optioneel, voor extra validatie)
    uint16_t calculateCRC() const {
        // Eenvoudige CRC16 berekening (optioneel)
        uint16_t crc = 0xFFFF;
        const uint8_t* data = (const uint8_t*)this;
        size_t len = offsetof(AutoAnchorPersist, crc);  // Tot CRC veld
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 1) crc = (crc >> 1) ^ 0xA001;
                else crc >>= 1;
            }
        }
        return crc;
    }
};
static_assert(sizeof(AutoAnchorPersist) <= 64, "AutoAnchorPersist te groot voor NVS");  // NVS entry max ~4000 bytes, maar blijf compact

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
    // FASE X.4: Trend hysteresis en throttling instellingen
    float trendHysteresisFactor;        // Hysterese factor voor trend exit (default 0.65)
    uint32_t throttlingTrendChangeMs;   // Trend Change → Trend Change cooldown (default 180 min)
    uint32_t throttlingTrendToMeanMs;   // Trend Change → Mean Touch cooldown (default 60 min)
    uint32_t throttlingMeanTouchMs;     // Mean Touch → Mean Touch cooldown (default 60 min)
    uint32_t throttlingCompressMs;      // Compress → Compress cooldown (default 120 min)
    // FASE X.5: Secondary global cooldown en coalescing
    uint32_t twoHSecondaryGlobalCooldownSec;  // Global cooldown voor SECONDARY alerts (default 7200 = 120 min)
    uint32_t twoHSecondaryCoalesceWindowSec;  // Coalescing window voor burst-demping (default 90 sec)
    
    // Auto Anchor settings (geoptimaliseerd: floats vervangen door compacte types)
    uint8_t anchorSourceMode;              // 0=MANUAL, 1=AUTO, 2=AUTO_FALLBACK, 3=OFF
    float autoAnchorLastValue;              // Laatste berekende auto anchor waarde (moet float blijven voor precisie)
    uint32_t autoAnchorLastUpdateEpoch;     // Epoch tijd van laatste update
    uint16_t autoAnchorUpdateMinutes;       // Update interval in minuten (default 120, max 1440)
    uint16_t autoAnchorForceUpdateMinutes; // Force update interval (default 720 = 12h, max 2880)
    uint8_t autoAnchor4hCandles;           // Aantal 4h candles voor EMA (default: 24 zonder PSRAM, 48 met PSRAM, max 100)
    uint8_t autoAnchor1dCandles;           // Aantal 1d candles voor EMA (default: 14 zonder PSRAM, 30 met PSRAM, max 60)
    uint16_t autoAnchorMinUpdatePct_x100;   // Minimale wijziging % * 100 (default 15 = 0.15%, max 500 = 5.0%)
    uint16_t autoAnchorTrendPivotPct_x100; // Trend pivot percentage * 100 (default 100 = 1.00%, max 1000 = 10.0%)
    uint8_t autoAnchorW4hBase_x100;        // Basis gewicht voor 4h EMA * 100 (default 35 = 0.35, max 100)
    uint8_t autoAnchorW4hTrendBoost_x100;  // Extra gewicht bij trend * 100 (default 35 = 0.35, max 100)
    uint8_t autoAnchorFlags;               // Bitfield: intervalMode(0-1), notifyEnabled(2), reserved(3-7)
    
    // Helper methods voor bitfield access
    uint8_t getAutoAnchorIntervalMode() const { return (autoAnchorFlags & 0x03); }
    void setAutoAnchorIntervalMode(uint8_t mode) { autoAnchorFlags = (autoAnchorFlags & ~0x03) | (mode & 0x03); }
    bool getAutoAnchorNotifyEnabled() const { return (autoAnchorFlags & 0x04) != 0; }
    void setAutoAnchorNotifyEnabled(bool enabled) { autoAnchorFlags = enabled ? (autoAnchorFlags | 0x04) : (autoAnchorFlags & ~0x04); }
    
    // Helper methods voor float conversie (voor gebruik in berekeningen)
    float getAutoAnchorMinUpdatePct() const { return autoAnchorMinUpdatePct_x100 / 100.0f; }
    void setAutoAnchorMinUpdatePct(float pct) { autoAnchorMinUpdatePct_x100 = (uint16_t)(pct * 100.0f + 0.5f); }
    float getAutoAnchorTrendPivotPct() const { return autoAnchorTrendPivotPct_x100 / 100.0f; }
    void setAutoAnchorTrendPivotPct(float pct) { autoAnchorTrendPivotPct_x100 = (uint16_t)(pct * 100.0f + 0.5f); }
    float getAutoAnchorW4hBase() const { return autoAnchorW4hBase_x100 / 100.0f; }
    void setAutoAnchorW4hBase(float w) { autoAnchorW4hBase_x100 = (uint8_t)(w * 100.0f + 0.5f); }
    float getAutoAnchorW4hTrendBoost() const { return autoAnchorW4hTrendBoost_x100 / 100.0f; }
    void setAutoAnchorW4hTrendBoost(float w) { autoAnchorW4hTrendBoost_x100 = (uint8_t)(w * 100.0f + 0.5f); }
};

// Settings struct - bevat alle instelbare waarden
struct CryptoMonitorSettings {
    // Basic settings
    char ntfyTopic[64];
    char bitvavoSymbol[16];  // Bitvavo market (bijv. "BTC-EUR")
    uint8_t language;
    uint8_t displayRotation;  // Display rotatie: 0 = normaal, 2 = 180 graden gedraaid
    
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
    uint8_t anchorStrategy;  // 0 = handmatig, 1 = conservatief (TP +1.8%, SL -1.2%), 2 = actief (TP +1.2%, SL -0.9%)
    
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
    bool warmStartSkip1m;
    bool warmStartSkip5m;
    
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
    
    // Helper: Load string preference (geoptimaliseerd: elimineert duplicatie)
    void loadStringPreference(const char* key, char* buffer, size_t bufferSize, const char* defaultValue);
    
    // Helper: Check if topic needs migration (geoptimaliseerd: geconsolideerde logica)
    static bool needsTopicMigration(const char* topic);
    
    // Preference keys
    static const char* PREF_KEY_NTFY_TOPIC;
    static const char* PREF_KEY_BITVAVO_SYMBOL;
    static const char* PREF_KEY_LANGUAGE;
    static const char* PREF_KEY_DISPLAY_ROTATION;
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
    static const char* PREF_KEY_ANCHOR_STRATEGY;
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
    static const char* PREF_KEY_WS_SKIP_1M;
    static const char* PREF_KEY_WS_SKIP_5M;
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
    // FASE X.4: Trend hysteresis en throttling preference keys
    static const char* PREF_KEY_2H_TREND_HYSTERESIS;
    static const char* PREF_KEY_2H_THROTTLE_TREND_CHANGE;
    static const char* PREF_KEY_2H_THROTTLE_TREND_MEAN;
    static const char* PREF_KEY_2H_THROTTLE_MEAN_TOUCH;
    static const char* PREF_KEY_2H_THROTTLE_COMPRESS;
    // FASE X.5: Secondary global cooldown en coalescing preference keys
    static const char* PREF_KEY_2H_SEC_GLOBAL_CD;
    static const char* PREF_KEY_2H_SEC_COALESCE;
    
    // Auto Anchor preference keys (voor backward compatibility, maar gebruik config-blob)
    static const char* PREF_KEY_ANCHOR_SOURCE_MODE;
    static const char* PREF_KEY_AUTO_ANCHOR_LAST_VALUE;
    static const char* PREF_KEY_AUTO_ANCHOR_LAST_UPDATE_EPOCH;
    static const char* PREF_KEY_AUTO_ANCHOR_UPDATE_MINUTES;
    static const char* PREF_KEY_AUTO_ANCHOR_FORCE_UPDATE_MINUTES;
    static const char* PREF_KEY_AUTO_ANCHOR_4H_CANDLES;
    static const char* PREF_KEY_AUTO_ANCHOR_1D_CANDLES;
    static const char* PREF_KEY_AUTO_ANCHOR_MIN_UPDATE_PCT;
    static const char* PREF_KEY_AUTO_ANCHOR_TREND_PIVOT_PCT;
    static const char* PREF_KEY_AUTO_ANCHOR_W4H_BASE;
    static const char* PREF_KEY_AUTO_ANCHOR_W4H_TREND_BOOST;
    static const char* PREF_KEY_AUTO_ANCHOR_INTERVAL_MODE;
    static const char* PREF_KEY_AUTO_ANCHOR_NOTIFY_ENABLED;
    static const char* PREF_KEY_AUTO_ANCHOR_BLOB;  // Één key voor config-blob
};

#endif // SETTINGSSTORE_H

