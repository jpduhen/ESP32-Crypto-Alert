// Gedeelde dynamische EUR-quoteformatter voor UI, notificaties en teksten.
// Regels (absolute waarde |prijs|):
//   >= 10000     -> %.0f
//   >= 100       -> %.2f
//   >= 1         -> %.4f
//   < 1          -> %.5f
// Procentuele returns (ret_1m, enz.) horen hier niet thuis.

#pragma once

#include <Arduino.h>
#include <math.h>

inline const char* quotePricePrintfSpecFor(float price)
{
    const float p = fabsf(price);
    if (p >= 10000.0f) {
        return "%.0f";
    }
    if (p >= 100.0f) {
        return "%.2f";
    }
    if (p >= 1.0f) {
        return "%.4f";
    }
    return "%.5f";
}

// Intern: |mag| > 0; vaste formatstrings i.p.v. dynamische spec (ESP32 printf + %f is hier betrouwbaarder).
static inline void formatQuotePriceEurMag(char* buf, size_t bufLen, float mag)
{
    const float ap = fabsf(mag);
    const double d = (double)mag;
    if (ap >= 10000.0f) {
        snprintf(buf, bufLen, "%.0f", d);
    } else if (ap >= 100.0f) {
        snprintf(buf, bufLen, "%.2f", d);
    } else if (ap >= 1.0f) {
        snprintf(buf, bufLen, "%.4f", d);
    } else {
        snprintf(buf, bufLen, "%.5f", d);
    }
}

// Positieve koers (spot); ongeldig / <= 0 -> "-"
inline void formatQuotePriceEur(char* buf, size_t bufLen, float price)
{
    if (bufLen == 0) {
        return;
    }
    if (!isfinite(price) || price <= 0.0f) {
        snprintf(buf, bufLen, "-");
        return;
    }
    formatQuotePriceEurMag(buf, bufLen, price);
}

// Bedragen in EUR die negatief mogen zijn (verschil t.o.v. anker, enz.)
inline void formatQuotePriceEurSigned(char* buf, size_t bufLen, float price)
{
    if (bufLen == 0) {
        return;
    }
    if (!isfinite(price)) {
        snprintf(buf, bufLen, "-");
        return;
    }
    if (price == 0.0f) {
        snprintf(buf, bufLen, "0");
        return;
    }
    const float ap = fabsf(price);
    char tmp[32];
    formatQuotePriceEurMag(tmp, sizeof(tmp), ap);
    if (price < 0.0f) {
        snprintf(buf, bufLen, "-%s", tmp);
    } else {
        snprintf(buf, bufLen, "%s", tmp);
    }
}
