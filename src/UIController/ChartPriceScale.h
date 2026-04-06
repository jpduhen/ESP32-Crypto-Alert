// LVGL-chart Y-as: integer schaling op basis van spotprijs (EUR), los van label-formattering.
// Oude gedrag: y = round(price * 100) = centen — te grof voor ADA e.d.
//
// Regime (schaalfactor S = eenheden per EUR):
//   prijs >= 10000  -> S = 1        (BTC-achtig)
//   prijs >= 100    -> S = 100
//   prijs >= 1      -> S = 10000
//   prijs < 1       -> S = 100000   (sub-cent bewegingen zichtbaar)
//
// Verticale "radius" in chart-eenheden: zelfde €2 als vroeger ±200 cent rond het midden:
//   halfRangeY = round(2 * S)  (was: 200 cent-eenheden = €2 enkelzijdig)

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

inline int32_t chartHalfRangeY(float scale)
{
    if (!isfinite(scale) || scale <= 0.0f) {
        return 200;
    }
    return (int32_t)lroundf(2.0f * scale);
}
