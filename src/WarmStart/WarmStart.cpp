#include "WarmStart.h"
#include "../SettingsStore/SettingsStore.h"

WarmStartWrapper::WarmStartWrapper()
    : m_settings(nullptr)
    , m_logger(nullptr)
    , m_stats{0, 0, 0, 0, false, false, false, false, WS_MODE_DISABLED, 0}
    , m_status(LIVE)
    , m_startTimeMs(0)
{
}

void WarmStartWrapper::bindSettings(CryptoMonitorSettings* settings) {
    m_settings = settings;
}

void WarmStartWrapper::bindLogger(Stream* logger) {
    m_logger = logger;
}

void WarmStartWrapper::resetStats() {
    m_stats = {0, 0, 0, 0, false, false, false, false, WS_MODE_DISABLED, 0};
}

void WarmStartWrapper::beginRun() {
    m_startTimeMs = millis();
    resetStats();
    m_status = WARMING_UP;
    logStart();
}

void WarmStartWrapper::endRun(WarmStartMode mode, WarmStartStats& stats, WarmStartStatus& status,
                              float ret2h, float ret30m, bool hasRet2h, bool hasRet30m,
                              uint16_t req1m, uint16_t req5m, uint16_t req30m, uint16_t req2h) {
    // Update internal state
    m_stats = stats;
    m_status = status;
    
    // Calculate duration
    unsigned long durationMs = millis() - m_startTimeMs;
    m_stats.mode = mode;
    
    // Log result
    logResult(mode, stats, status, ret2h, ret30m, hasRet2h, hasRet30m, req1m, req5m, req30m, req2h);
    
    // Log duration
    if (m_logger) {
        m_logger->print(F("[WarmStart] Duration: "));
        m_logger->print(durationMs);
        m_logger->println(F(" ms"));
    }
}

bool WarmStartWrapper::isEnabled() const {
    return getSettingValue(&CryptoMonitorSettings::warmStartEnabled, false);
}

uint8_t WarmStartWrapper::get1mExtraCandles() const {
    return getSettingValue(&CryptoMonitorSettings::warmStart1mExtraCandles, static_cast<uint8_t>(15));
}

uint8_t WarmStartWrapper::get5mCandles() const {
    return getSettingValue(&CryptoMonitorSettings::warmStart5mCandles, static_cast<uint8_t>(12));
}

uint8_t WarmStartWrapper::get30mCandles() const {
    return getSettingValue(&CryptoMonitorSettings::warmStart30mCandles, static_cast<uint8_t>(8));
}

uint8_t WarmStartWrapper::get2hCandles() const {
    return getSettingValue(&CryptoMonitorSettings::warmStart2hCandles, static_cast<uint8_t>(6));
}

void WarmStartWrapper::logStart() {
    if (!m_logger) return;
    
    m_logger->println(F("[WarmStart] ========================================"));
    m_logger->println(F("[WarmStart] Starting warm-start sequence"));
    
    if (m_settings) {
        m_logger->print(F("[WarmStart] Enabled: "));
        m_logger->println(m_settings->warmStartEnabled ? F("YES") : F("NO"));
        
        if (m_settings->warmStartEnabled) {
            m_logger->print(F("[WarmStart] 1m extra candles: "));
            m_logger->println(m_settings->warmStart1mExtraCandles);
            m_logger->print(F("[WarmStart] 5m candles: "));
            m_logger->println(m_settings->warmStart5mCandles);
            m_logger->print(F("[WarmStart] 30m candles: "));
            m_logger->println(m_settings->warmStart30mCandles);
            m_logger->print(F("[WarmStart] 2h candles: "));
            m_logger->println(m_settings->warmStart2hCandles);
        }
    } else {
        m_logger->println(F("[WarmStart] WARNING: Settings not bound"));
    }
    
    m_logger->println(F("[WarmStart] ========================================"));
}

void WarmStartWrapper::logResult(WarmStartMode mode, const WarmStartStats& stats, WarmStartStatus status,
                                 float ret2h, float ret30m, bool hasRet2h, bool hasRet30m,
                                 uint16_t req1m, uint16_t req5m, uint16_t req30m, uint16_t req2h) {
    if (!m_logger) return;
    
    m_logger->println(F("[WarmStart] ========================================"));
    m_logger->print(F("[WarmStart] Result: "));
    m_logger->println(modeToString(mode));
    m_logger->print(F("[WarmStart] Status: "));
    m_logger->println(statusToString(status));
    
    // Requested counts
    m_logger->print(F("[WarmStart] Requested: 1m="));
    m_logger->print(req1m);
    m_logger->print(F(" 5m="));
    m_logger->print(req5m);
    m_logger->print(F(" 30m="));
    m_logger->print(req30m);
    m_logger->print(F(" 2h="));
    m_logger->println(req2h);
    
    // Actual used counts
    m_logger->print(F("[WarmStart] Used:      1m="));
    m_logger->print(stats.loaded1m);
    m_logger->print(F(" 5m="));
    m_logger->print(stats.loaded5m);
    m_logger->print(F(" 30m="));
    m_logger->print(stats.loaded30m);
    m_logger->print(F(" 2h="));
    m_logger->println(stats.loaded2h);
    
    // Returns and availability
    m_logger->print(F("[WarmStart] hasRet2h="));
    m_logger->print(hasRet2h ? 1 : 0);
    m_logger->print(F(" hasRet30m="));
    m_logger->print(hasRet30m ? 1 : 0);
    m_logger->print(F(" ret2h="));
    m_logger->print(ret2h, 3);
    m_logger->print(F(" ret30m="));
    m_logger->println(ret30m, 3);
    
    // Detailed status per timeframe
    m_logger->println(F("[WarmStart] Status per timeframe:"));
    
    // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
    // 1m: candles loaded (voor buffer)
    m_logger->print(F("  1m: "));
    m_logger->print(stats.loaded1m);
    m_logger->print(F(" candles loaded ("));
    m_logger->print(stats.warmStartOk1m ? F("OK") : F("FAIL"));
    m_logger->println(F(")"));
    
    // 5m/30m/2h: closes used (voor returns)
    logTimeframeStatus("5m", stats.loaded5m, stats.warmStartOk5m && stats.loaded5m >= 2);
    logTimeframeStatus("30m", stats.loaded30m, stats.warmStartOk30m && stats.loaded30m >= 2 && hasRet30m);
    logTimeframeStatus("2h", stats.loaded2h, stats.warmStartOk2h && stats.loaded2h >= 2 && hasRet2h);
    
    m_logger->print(F("[WarmStart] Warm-up progress: "));
    m_logger->print(stats.warmUpProgress);
    m_logger->println(F("%"));
    
    m_logger->println(F("[WarmStart] ========================================"));
}

const char* WarmStartWrapper::modeToString(WarmStartMode mode) const {
    switch (mode) {
        case WS_MODE_FULL: return "FULL";
        case WS_MODE_PARTIAL: return "PARTIAL";
        case WS_MODE_FAILED: return "FAILED";
        case WS_MODE_DISABLED: return "DISABLED";
        default: return "UNKNOWN";
    }
}

const char* WarmStartWrapper::statusToString(WarmStartStatus status) const {
    switch (status) {
        case WARMING_UP: return "WARMING_UP";
        case LIVE: return "LIVE";
        case LIVE_COLD: return "LIVE_COLD";
        default: return "UNKNOWN";
    }
}

// Helper: Log timeframe status (geoptimaliseerd: elimineert code duplicatie)
void WarmStartWrapper::logTimeframeStatus(const char* label, uint16_t loaded, bool ok, bool hasRet) const {
    if (!m_logger) return;
    
    m_logger->print(F("  "));
    m_logger->print(label);
    m_logger->print(F(": "));
    m_logger->print(loaded);
    m_logger->print(F(" candles fetched, 2 closes used ("));
    m_logger->print(ok ? F("OK") : F("FAIL"));
    m_logger->println(F(")"));
}
