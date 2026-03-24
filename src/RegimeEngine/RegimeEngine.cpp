#include "RegimeEngine.h"

#include <Arduino.h>
#include "../SettingsStore/SettingsStore.h"
#include <math.h>

// Globale instellingen (ESP32-Crypto-Alert.ino + loadSettings/saveSettings)
extern AlertThresholds alertThresholds;
extern float trendThreshold;
extern bool regimeEngineEnabled;
extern uint32_t regimeMinDwellSec;
extern float regimeEnergeticEnter;
extern float regimeEnergeticExit;
extern float regimeSlapEnter;
extern float regimeSlapExit;
extern float regimeLoadedFloor;
extern float regimeLoadedDrop;
extern float regimeDirDeadband1mPct;
extern float regimeDirDeadband5mPct;
extern float regimeDirDeadband30mPct;
extern float regimeDirDeadband2hPct;
extern float regime2hCompressMinPct;
extern float regime2hCompressMaxPct;

namespace {

static RegimeSnapshot g_snap;

static RegimeKind g_committed = REGIME_GELADEN;
static uint8_t g_pendingRegime = 0xFF;
static uint32_t g_pendingSinceMs = 0;

static float clampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float normAbs(float x, float ref) {
    const float r = fmaxf(ref, 0.0001f);
    return clampF(fabsf(x) / r, 0.0f, 1.5f);
}

static int8_t signDb(float x, float db) {
    if (fabsf(x) <= db) {
        return 0;
    }
    return (x > 0.0f) ? 1 : -1;
}

struct RegimeConfig {
    bool enabled;
    uint32_t minDwellSec;
    float energeticEnter;
    float energeticExit;
    float slapEnter;
    float slapExit;
    float loadedFloor;
    float loadedDrop;
    float dd1m;
    float dd5m;
    float dd30m;
    float dd2h;
    float compressMinPct;
    float compressMaxPct;
};

static RegimeConfig readConfigFromGlobals() {
    RegimeConfig c;
    c.enabled = regimeEngineEnabled;
    c.minDwellSec = regimeMinDwellSec;
    c.energeticEnter = regimeEnergeticEnter;
    c.energeticExit = regimeEnergeticExit;
    c.slapEnter = regimeSlapEnter;
    c.slapExit = regimeSlapExit;
    c.loadedFloor = regimeLoadedFloor;
    c.loadedDrop = regimeLoadedDrop;
    c.dd1m = regimeDirDeadband1mPct;
    c.dd5m = regimeDirDeadband5mPct;
    c.dd30m = regimeDirDeadband30mPct;
    c.dd2h = regimeDirDeadband2hPct;
    c.compressMinPct = regime2hCompressMinPct;
    c.compressMaxPct = regime2hCompressMaxPct;
    return c;
}

static RegimeKind classifyProposed(RegimeKind current,
                                   float energyScore,
                                   float loadedScore,
                                   const RegimeConfig& cfg) {
    switch (current) {
        case REGIME_ENERGIEK:
            if (energyScore >= cfg.energeticExit) {
                return REGIME_ENERGIEK;
            }
            return (loadedScore >= cfg.loadedDrop) ? REGIME_GELADEN : REGIME_SLAP;

        case REGIME_SLAP:
            if (energyScore <= cfg.slapExit && loadedScore < cfg.loadedFloor) {
                return REGIME_SLAP;
            }
            return (energyScore >= cfg.energeticEnter) ? REGIME_ENERGIEK : REGIME_GELADEN;

        case REGIME_GELADEN:
        default:
            if (energyScore >= cfg.energeticEnter) {
                return REGIME_ENERGIEK;
            }
            if (energyScore <= cfg.slapEnter && loadedScore <= cfg.loadedDrop) {
                return REGIME_SLAP;
            }
            return REGIME_GELADEN;
    }
}

static void applyDwellAndCommit(uint32_t nowMs,
                                RegimeKind proposed,
                                const RegimeConfig& cfg) {
    if (proposed == g_committed) {
        g_pendingRegime = 0xFF;
        g_pendingSinceMs = 0;
        return;
    }

    const uint32_t dwellMs = cfg.minDwellSec * 1000UL;

    if (g_pendingRegime != static_cast<uint8_t>(proposed)) {
        g_pendingRegime = static_cast<uint8_t>(proposed);
        g_pendingSinceMs = nowMs;
        return;
    }

    if ((nowMs - g_pendingSinceMs) >= dwellMs) {
        const RegimeKind prev = g_committed;
        g_committed = proposed;
        g_pendingRegime = 0xFF;
        g_pendingSinceMs = 0;
        Serial.printf(
            "[Regime] commit %u -> %u (dwell ok)\n",
            static_cast<unsigned>(prev),
            static_cast<unsigned>(g_committed));
    }
}

}  // namespace

void regimeEngineTick(uint32_t nowMs,
                      float ret_1m,
                      float ret_5m,
                      float ret_30m,
                      float ret_2h,
                      float twoHRangePct,
                      bool twoHMetricsValid) {
    const RegimeConfig cfg = readConfigFromGlobals();
    g_snap.engineEnabledLastTick = cfg.enabled;

    if (!cfg.enabled) {
        g_snap.committedRegime = g_committed;
        g_snap.proposedRegime = g_committed;
        return;
    }

    const float n1 = normAbs(ret_1m, alertThresholds.spike1m);
    const float n5 = normAbs(ret_5m, alertThresholds.move5mAlert);
    const float n30 = normAbs(ret_30m, alertThresholds.move30m);
    const float n2h = normAbs(ret_2h, trendThreshold);

    const int8_t d1 = signDb(ret_1m, cfg.dd1m);
    const int8_t d5 = signDb(ret_5m, cfg.dd5m);
    const int8_t d30 = signDb(ret_30m, cfg.dd30m);
    const int8_t d2h = signDb(ret_2h, cfg.dd2h);

    const float align15 = (d1 != 0 && d1 == d5) ? 1.0f : 0.0f;
    const float align1530 = (align15 > 0.0f && d30 == d1) ? 1.0f : 0.0f;
    float ctx2h = 0.0f;
    if (d1 != 0 && d2h == d1) {
        ctx2h = 0.5f;
    } else if (d1 != 0 && d2h != 0 && d2h != d1) {
        ctx2h = -0.25f;
    }
    const float directionScore =
        clampF((align15 + align1530 + ctx2h) / 2.5f, -1.0f, 1.0f);

    float rangeUse = twoHRangePct;
    if (!twoHMetricsValid) {
        rangeUse = cfg.compressMaxPct;
    }

    const float span = fmaxf(cfg.compressMaxPct - cfg.compressMinPct, 0.01f);
    const float compressionScore =
        clampF((cfg.compressMaxPct - rangeUse) / span, 0.0f, 1.0f);

    const float energyScore =
        clampF(0.45f * n1 + 0.35f * n5 + 0.15f * n30 + 0.05f * n2h, 0.0f, 1.5f);

    const float n30Clamped = clampF(n30, 0.0f, 1.0f);
    const float loadedScore = clampF(
        0.55f * compressionScore + 0.30f * n30Clamped + 0.15f * fabsf(directionScore),
        0.0f,
        1.0f);

    const RegimeKind proposed = classifyProposed(g_committed, energyScore, loadedScore, cfg);

    g_snap.n1 = n1;
    g_snap.n5 = n5;
    g_snap.n30 = n30;
    g_snap.n2h = n2h;
    g_snap.directionScore = directionScore;
    g_snap.compressionScore = compressionScore;
    g_snap.energyScore = energyScore;
    g_snap.loadedScore = loadedScore;
    g_snap.twoHRangePct = rangeUse;
    g_snap.proposedRegime = proposed;

    applyDwellAndCommit(nowMs, proposed, cfg);

    g_snap.committedRegime = g_committed;
    g_snap.pendingRegime = g_pendingRegime;
    g_snap.pendingSinceMs = g_pendingSinceMs;

    Serial.printf(
        "[Regime] prop=%u comm=%u E=%.3f L=%.3f dir=%.3f cmp=%.3f n1=%.3f n5=%.3f n30=%.3f n2h=%.3f "
        "2hR=%.3f\n",
        static_cast<unsigned>(proposed),
        static_cast<unsigned>(g_committed),
        energyScore,
        loadedScore,
        directionScore,
        compressionScore,
        n1,
        n5,
        n30,
        n2h,
        rangeUse);
}

const RegimeSnapshot& regimeEngineGetSnapshot() {
    return g_snap;
}
