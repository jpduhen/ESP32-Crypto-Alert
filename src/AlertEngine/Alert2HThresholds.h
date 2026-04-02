#ifndef ALERT2HTHRESHOLDS_H
#define ALERT2HTHRESHOLDS_H

#include <Arduino.h>

// Debug flag voor 2h alerts logging (compile-time conditional)
// Zet op 1 om debug logging te activeren, 0 om uit te schakelen (geen performance impact)
#ifndef DEBUG_2H_ALERTS
#define DEBUG_2H_ALERTS 0  // Standaard uitgeschakeld
#endif

// Centrale namespace voor 2-uur alert threshold defaults
// Defaults zijn conservatief gekozen: weinig alert-spam en stabiel gedrag op typische ESP32-S3 setups
// Note: De instelbare thresholds staan in Alert2HThresholds struct in SettingsStore.h
// Deze namespace wordt alleen gebruikt voor defaults, niet voor runtime thresholds
namespace Alert2HDefaults {
    // --- Breakout / Breakdown ---
    // BREAK_MARGIN 0.15%: voorkomt "net erover" jitter op 2h high/low.
    // Breakout boven 2h high als we daar minimaal 0.15% boven zitten (hysteresis tegen jitter)
    static constexpr float BREAK_MARGIN_PCT = 0.15f;

    // Na een breakout pas opnieuw notificeren als prijs eerst weer onder (high2h - 0.10%) komt
    static constexpr float BREAK_RESET_MARGIN_PCT = 0.10f;

    // Cooldown per richting (om spam te voorkomen); default rustiger dan vroeger (3 uur)
    static constexpr uint32_t BREAK_COOLDOWN_MS = 10800000UL; // 3 uur — profiel 5F

    // --- Mean reversion touch (2h average) — profiel 5F ---
    static constexpr float MEAN_MIN_DISTANCE_PCT = 0.80f;

    static constexpr float MEAN_TOUCH_BAND_PCT = 0.10f;

    static constexpr uint32_t MEAN_COOLDOWN_MS = 10800000UL;

    // --- Range compression — profiel 5F ---
    static constexpr float COMPRESS_THRESHOLD_PCT = 0.70f;

    static constexpr float COMPRESS_RESET_PCT = 1.10f;

    static constexpr uint32_t COMPRESS_COOLDOWN_MS = 18000000UL;

    // --- Anchor context ---
    static constexpr float ANCHOR_OUTSIDE_MARGIN_PCT = 0.25f;

    static constexpr uint32_t ANCHOR_COOLDOWN_MS = 10800000UL;
}

// Helper functies (zonder heap allocaties)
// Bereken percentage verschil: (a - b) / b * 100
static inline float pctDiff(float a, float b) {
    if (b == 0.0f) return 0.0f; // Voorkom deling door nul
    return ((a - b) / b) * 100.0f;
}

// Absolute waarde van float (wrapper voor fabsf)
static inline float absf(float x) {
    return fabsf(x);
}

// Check of waarde binnen band ligt: |value - center| <= center * bandPct / 100
// bandPct is percentage (bijv. 0.10 = 0.10%)
static inline bool inBand(float value, float center, float bandPct) {
    if (center == 0.0f) return false; // Voorkom deling door nul
    float diff = absf(value - center);
    float threshold = center * (bandPct / 100.0f);
    return diff <= threshold;
}

#endif // ALERT2HTHRESHOLDS_H

