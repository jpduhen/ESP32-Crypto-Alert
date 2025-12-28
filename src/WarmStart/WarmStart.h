#ifndef WARMSTART_H
#define WARMSTART_H

#include <Arduino.h>
#include <Stream.h>

// Forward declarations
struct CryptoMonitorSettings;

// Warm-Start: System status (gedefinieerd in .ino, maar hier ook nodig voor class definitie)
enum WarmStartStatus {
    WARMING_UP,  // Buffers bevatten nog Binance data
    LIVE,        // Volledig op live data
    LIVE_COLD    // Live data maar warm-start gefaald (cold start)
};

// Warm-Start: Mode (gedefinieerd in .ino, maar hier ook nodig voor class definitie)
enum WarmStartMode {
    WS_MODE_FULL,     // Alle timeframes succesvol geladen
    WS_MODE_PARTIAL,  // Gedeeltelijk geladen (sommige timeframes gefaald)
    WS_MODE_FAILED,   // Alle timeframes gefaald
    WS_MODE_DISABLED  // Warm-start uitgeschakeld
};

// Warm-Start: Statistics struct (gedefinieerd in .ino, maar hier ook nodig voor class definitie)
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

/**
 * WarmStartWrapper: Wrapper module voor warm-start status/logging/settings
 * 
 * Deze wrapper bundelt alleen settings, stats en logging.
 * De warm-start functionaliteit blijft volledig in de .ino.
 */
class WarmStartWrapper {
public:
    WarmStartWrapper();
    
    // Bind dependencies
    void bindSettings(CryptoMonitorSettings* settings);
    void bindLogger(Stream* logger);
    
    // Stats management
    void resetStats();
    
    // Run lifecycle
    void beginRun();
    void endRun(WarmStartMode mode, WarmStartStats& stats, WarmStartStatus& status, 
                float ret2h, float ret30m, bool hasRet2h, bool hasRet30m,
                uint16_t req1m, uint16_t req5m, uint16_t req30m, uint16_t req2h);
    
    // Getters
    const WarmStartStats& stats() const { return m_stats; }
    WarmStartStatus status() const { return m_status; }
    
    // Settings getters (via pointer)
    bool isEnabled() const;
    uint8_t get1mExtraCandles() const;
    uint8_t get5mCandles() const;
    uint8_t get30mCandles() const;
    uint8_t get2hCandles() const;

private:
    CryptoMonitorSettings* m_settings;
    Stream* m_logger;
    WarmStartStats m_stats;
    WarmStartStatus m_status;
    unsigned long m_startTimeMs;
    
    void logStart();
    void logResult(WarmStartMode mode, const WarmStartStats& stats, WarmStartStatus status,
                   float ret2h, float ret30m, bool hasRet2h, bool hasRet30m,
                   uint16_t req1m, uint16_t req5m, uint16_t req30m, uint16_t req2h);
    const char* modeToString(WarmStartMode mode) const;
    const char* statusToString(WarmStartStatus status) const;
    
    // Helper: Log timeframe status (geoptimaliseerd: elimineert code duplicatie)
    void logTimeframeStatus(const char* label, uint16_t loaded, bool ok, bool hasRet = false) const;
    
    // Helper: Get setting value with default (geoptimaliseerd: elimineert code duplicatie)
    // Template moet inline in header staan
    template<typename T>
    T getSettingValue(T CryptoMonitorSettings::*field, T defaultValue) const {
        return m_settings ? (m_settings->*field) : defaultValue;
    }
};

#endif // WARMSTART_H
