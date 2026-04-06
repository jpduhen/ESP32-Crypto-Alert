// LVGL-chart Y-as: integer schaling op basis van spotprijs (EUR), los van label-formattering.
// Oude gedrag: y = round(price * 100) = centen — te grof voor ADA e.d.
//
// Regime (schaalfactor S = eenheden per EUR):
//   prijs >= 10000  -> S = 1        (BTC-achtig)
//   prijs >= 100    -> S = 100
//   prijs >= 1      -> S = 10000
//   prijs < 1       -> S = 100000   (sub-cent bewegingen zichtbaar)
//
// Verticale half-range in EUR (dynamisch; vroeger vast €2 enkelzijdig => halfRangeY = 2*S):
//   prijs >= 10000  -> max(0.20% van prijs, 50 EUR)
//   prijs >= 100    -> max(0.40% van prijs, 0.50 EUR)
//   prijs >= 1      -> max(0.60% van prijs, 0.01 EUR)
//   prijs < 1       -> max(1.00% van prijs, 0.002 EUR)
// Chart-eenheden: halfRangeY = round(halfRangeEur * S) — zelfde S als chartPriceEurToY.

#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

inline float getChartPriceScale(float priceEur)
{
    if (!isfinite(priceEur) || priceEur <= 0.0f) {
        return 10000.0f;
    }
    const float p = fabsf(priceEur);
    if (p >= 10000.0f) {
        return 1.0f;
    }
    if (p >= 100.0f) {
        return 100.0f;
    }
    if (p >= 1.0f) {
        return 10000.0f;
    }
    return 100000.0f;
}

// Halve verticale span rond het midden, in EUR (enkelzijdig; totale Y-span ≈ 2 * halfRangeEur * S).
inline float chartHalfRangeEur(float priceEur)
{
    if (!isfinite(priceEur) || priceEur <= 0.0f) {
        return 2.0f;
    }
    const float p = fabsf(priceEur);
    if (p >= 10000.0f) {
        return fmaxf(0.002f * p, 50.0f);
    }
    if (p >= 100.0f) {
        return fmaxf(0.004f * p, 0.5f);
    }
    if (p >= 1.0f) {
        return fmaxf(0.006f * p, 0.01f);
    }
    return fmaxf(0.01f * p, 0.002f);
}

inline int32_t chartPriceEurToY(float priceEur)
{
    if (!isfinite(priceEur) || priceEur <= 0.0f) {
        return 0;
    }
    const float s = getChartPriceScale(priceEur);
    const double y = (double)priceEur * (double)s;
    if (y >= (double)INT32_MAX) {
        return INT32_MAX;
    }
    if (y <= (double)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)lroundf((float)y);
}

inline int32_t chartHalfRangeY(float priceEur, float scale)
{
    if (!isfinite(scale) || scale <= 0.0f) {
        return 200;
    }
    const float hrEur = chartHalfRangeEur(priceEur);
    const double y = (double)hrEur * (double)scale;
    if (y >= (double)INT32_MAX) {
        return INT32_MAX;
    }
    if (y <= (double)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)lroundf((float)y);
}
