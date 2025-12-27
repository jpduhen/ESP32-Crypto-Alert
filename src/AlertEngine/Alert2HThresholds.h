#ifndef ALERT2HTHRESHOLDS_H
#define ALERT2HTHRESHOLDS_H

#include <Arduino.h>

// Debug flag voor 2h alerts logging (compile-time conditional)
// Zet op 1 om debug logging te activeren, 0 om uit te schakelen (geen performance impact)
#ifndef DEBUG_2H_ALERTS
#define DEBUG_2H_ALERTS 0  // Standaard uitgeschakeld
#endif

// Centrale namespace voor 2-uur alert threshold defaults
// Deze thresholds zijn gekozen om op CYD (geen PSRAM) weinig spam te geven en stabiel te zijn
// Note: De instelbare thresholds staan in Alert2HThresholds struct in SettingsStore.h
// Deze namespace wordt alleen gebruikt voor defaults, niet voor runtime thresholds
namespace Alert2HDefaults {
    // --- Breakout / Breakdown ---
    // BREAK_MARGIN 0.15%: voorkomt "net erover" jitter op 2h high/low.
    // Breakout boven 2h high als we daar minimaal 0.15% boven zitten (hysteresis tegen jitter)
    static constexpr float BREAK_MARGIN_PCT = 0.15f;

    // Na een breakout pas opnieuw notificeren als prijs eerst weer onder (high2h - 0.10%) komt
    static constexpr float BREAK_RESET_MARGIN_PCT = 0.10f;

    // Cooldown per richting (om spam te voorkomen)
    static constexpr uint32_t BREAK_COOLDOWN_MS = 30UL * 60UL * 1000UL; // 30 min

    // --- Mean reversion touch (2h average) ---
    // MEAN_MIN_DISTANCE 0.60% + TOUCH 0.10%: pas melden als het echt een return-to-mean event is.
    // Alleen melden als prijs vooraf minimaal 0.60% van avg2h af lag en daarna avg2h "raakt"
    static constexpr float MEAN_MIN_DISTANCE_PCT = 0.60f;

    // Definieer "touch" als binnen 0.10% van avg2h
    static constexpr float MEAN_TOUCH_BAND_PCT = 0.10f;

    // Cooldown om niet elke keer te spammen als hij rond avg2h blijft hangen
    static constexpr uint32_t MEAN_COOLDOWN_MS = 60UL * 60UL * 1000UL; // 60 min

    // --- Range compression ---
    // COMPRESS 0.80% / reset 1.10%: compressie is zeldzamer en relevant; reset hoger voorkomt flapper.
    // Range% = (high2h-low2h)/avg2h*100
    static constexpr float COMPRESS_THRESHOLD_PCT = 0.80f;

    // Reset compressie-notificatie pas als range weer boven 1.10% komt
    static constexpr float COMPRESS_RESET_PCT = 1.10f;

    static constexpr uint32_t COMPRESS_COOLDOWN_MS = 2UL * 60UL * 60UL * 1000UL; // 2 uur

    // --- Anchor context ---
    // ANCHOR outside 0.25%: anchor-meldingen moeten schaars zijn; dit is "duidelijk buiten range".
    // Anchor buiten 2h range als hij >0.25% buiten high/low ligt
    static constexpr float ANCHOR_OUTSIDE_MARGIN_PCT = 0.25f;

    // Cooldown voor anchor-context
    static constexpr uint32_t ANCHOR_COOLDOWN_MS = 3UL * 60UL * 60UL * 1000UL; // 3 uur
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

