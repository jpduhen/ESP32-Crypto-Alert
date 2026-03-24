#ifndef REGIME_ENGINE_H
#define REGIME_ENGINE_H

#include <stdint.h>

// Drie marktregimes (Fase A: alleen berekening + snapshot, geen alert-gating).
enum RegimeKind : uint8_t {
    REGIME_SLAP = 0,
    REGIME_GELADEN = 1,
    REGIME_ENERGIEK = 2,
};

struct RegimeSnapshot {
    float n1 = 0.0f;
    float n5 = 0.0f;
    float n30 = 0.0f;
    float n2h = 0.0f;
    float directionScore = 0.0f;
    float compressionScore = 0.0f;
    float energyScore = 0.0f;
    float loadedScore = 0.0f;
    float twoHRangePct = 0.0f;
    RegimeKind proposedRegime = REGIME_GELADEN;
    RegimeKind committedRegime = REGIME_GELADEN;
    uint8_t pendingRegime = 0xFF;  // 0xFF = geen pending wissel
    uint32_t pendingSinceMs = 0;
    bool engineEnabledLastTick = false;
};

// Leest thresholds/regime-instellingen uit globale variabelen (gesynchroniseerd via SettingsStore).
void regimeEngineTick(uint32_t nowMs,
                      float ret_1m,
                      float ret_5m,
                      float ret_30m,
                      float ret_2h,
                      float twoHRangePct,
                      bool twoHMetricsValid);

const RegimeSnapshot& regimeEngineGetSnapshot();

#endif
