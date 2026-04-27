#include "regime_engine/regime_engine.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "event_bus/event_bus.hpp"
#include "freertos/portmacro.h"

static const char *TAG = "REGIME";

static regime_engine::RegimeSnapshot s_snap{};
static portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;

namespace {

/** Lokale drempels (later evt. naar settings). */
constexpr double kEps = 0.015;
/** Min. 5m range-% om "range/chop"-omgeving te vertrouwen. */
constexpr double kRangeMinPct = 0.04;
/** Trend: |ret_5m| minstens dit % (close-to-close over 5 gesloten minuten). */
constexpr double kTrendAbsRet5Pct = 0.06;
/** Trend: trend_strength minstens. */
constexpr double kTrendStrengthMin = 0.42;
/** Range: trend_strength hoogstens dit. */
constexpr double kRangeStrengthMax = 0.38;
/** Range: som |ret_5|+|ret_1| kleiner dan dit deel van 5m-range-%. */
constexpr double kRangeRetVsRange = 0.55;
/** Extra 30m-trend trigger. */
constexpr double kTrendAbsRet30Pct = 0.12;
/** Range: choppiness minstens (range vs |returns|). */
constexpr double kRangeChopMin = 0.28;

double abs_d(double x) {
    return std::fabs(x);
}

/** Laatste gesloten 1m close voor range-noemer. */
bool last_closed_close(double *close) {
    candle_engine::Candle1m c{};
    if (!candle_engine::get_closed_candle_from_latest(0, &c) || !c.valid || c.close <= 0.0) {
        return false;
    }
    *close = c.close;
    return true;
}

double range_pct_of_window(bool window_valid, double win_min, double win_max, double close) {
    if (!window_valid || close <= 0.0 || !std::isfinite(win_min) || !std::isfinite(win_max)) {
        return 0.0;
    }
    const double span = win_max - win_min;
    if (span < 0.0 || !std::isfinite(span)) {
        return 0.0;
    }
    return (span / close) * 100.0;
}

void compute_scores(const candle_engine::MarketAnalyticsSnapshot &a, double close0, double *range5, double *range30,
                    double *trend_s, double *chop_s) {
    *range5 = range_pct_of_window(a.ranges.valid_5m, a.ranges.min_5m, a.ranges.max_5m, close0);
    *range30 = range_pct_of_window(a.ranges.valid_30m, a.ranges.min_30m, a.ranges.max_30m, close0);

    const double ar1 = a.returns.valid_1m ? abs_d(a.returns.ret_1m_pct) : 0.0;
    const double ar5 = a.returns.valid_5m ? abs_d(a.returns.ret_5m_pct) : 0.0;
    const double ar30 = a.returns.valid_30m ? abs_d(a.returns.ret_30m_pct) : 0.0;

    if (a.ranges.valid_5m && *range5 > kEps) {
        *trend_s = std::min(1.0, ar5 / std::max(*range5 * 0.28, kEps));
        *chop_s = std::min(1.0, *range5 / std::max(ar5 + ar1 + kEps, kEps));
    } else if (a.ranges.valid_1m) {
        const double r1 =
            range_pct_of_window(true, a.ranges.min_1m, a.ranges.max_1m, close0);
        *trend_s = (a.returns.valid_1m && r1 > kEps) ? std::min(1.0, ar1 / std::max(r1 * 0.35, kEps)) : 0.0;
        *chop_s = (a.returns.valid_1m && r1 > kEps) ? std::min(1.0, r1 / std::max(ar1 + kEps, kEps)) : 0.0;
    } else {
        *trend_s = 0.0;
        *chop_s = 0.0;
    }

    if (a.ranges.valid_30m && *range30 > kEps && a.returns.valid_30m) {
        const double t30 = std::min(1.0, ar30 / std::max(*range30 * 0.28, kEps));
        *trend_s = std::max(*trend_s, t30 * 0.85);
    }
}

regime_engine::RegimeClass classify(const candle_engine::MarketAnalyticsSnapshot &a, double range5, double range30,
                                     double trend_s, double chop_s) {
    const double ar1 = a.returns.valid_1m ? abs_d(a.returns.ret_1m_pct) : 0.0;
    const double ar5 = a.returns.valid_5m ? abs_d(a.returns.ret_5m_pct) : 0.0;
    const double ar30 = a.returns.valid_30m ? abs_d(a.returns.ret_30m_pct) : 0.0;

    if (a.closed_1m_count < 2) {
        return regime_engine::RegimeClass::kUnknown;
    }

    const bool trend5 =
        a.returns.valid_5m && ar5 >= kTrendAbsRet5Pct && trend_s >= kTrendStrengthMin && a.ranges.valid_5m;
    const bool trend30 =
        a.returns.valid_30m && ar30 >= kTrendAbsRet30Pct && a.ranges.valid_30m && range30 > kEps &&
        (ar30 / std::max(range30 * 0.28, kEps)) >= kTrendStrengthMin;

    if (trend5 || trend30) {
        return regime_engine::RegimeClass::kTrend;
    }

    const bool range_like = a.ranges.valid_5m && range5 >= kRangeMinPct && trend_s <= kRangeStrengthMax &&
                            chop_s >= kRangeChopMin &&
                            (ar5 + ar1) < kRangeRetVsRange * std::max(range5, kRangeMinPct);

    if (range_like) {
        return regime_engine::RegimeClass::kRange;
    }

    return regime_engine::RegimeClass::kNeutral;
}

const char *class_label(regime_engine::RegimeClass c) {
    switch (c) {
        case regime_engine::RegimeClass::kRange:
            return "RANGE";
        case regime_engine::RegimeClass::kNeutral:
            return "NEUTRAL";
        case regime_engine::RegimeClass::kTrend:
            return "TREND";
        default:
            return "UNKNOWN";
    }
}

}  // namespace

namespace regime_engine {

void update_from_analytics(const candle_engine::MarketAnalyticsSnapshot &a) {
    RegimeSnapshot out{};
    out.ts_ms = a.ts_ms;
    out.abs_ret_1m = a.returns.valid_1m ? abs_d(a.returns.ret_1m_pct) : 0.0;
    out.abs_ret_5m = a.returns.valid_5m ? abs_d(a.returns.ret_5m_pct) : 0.0;
    out.abs_ret_30m = a.returns.valid_30m ? abs_d(a.returns.ret_30m_pct) : 0.0;

    double close0 = 0.0;
    if (!last_closed_close(&close0)) {
        out.valid = false;
        out.regime = RegimeClass::kUnknown;
        portENTER_CRITICAL(&s_snap_mux);
        s_snap = out;
        portEXIT_CRITICAL(&s_snap_mux);
        ESP_LOGD(TAG, "ret1m=NA ret5m=NA ret30m=NA range5m=NA range30m=NA trend=0.00 chop=0.00 class=UNKNOWN (no close)");
        return;
    }

    compute_scores(a, close0, &out.range_5m_pct, &out.range_30m_pct, &out.trend_strength_score, &out.choppiness_score);
    out.valid = (a.closed_1m_count >= 2);
    out.regime = out.valid ? classify(a, out.range_5m_pct, out.range_30m_pct, out.trend_strength_score, out.choppiness_score) : RegimeClass::kUnknown;

    portENTER_CRITICAL(&s_snap_mux);
    s_snap = out;
    portEXIT_CRITICAL(&s_snap_mux);

    char s1[16], s5[16], s30[16], g5[16], g30[16];
    if (a.returns.valid_1m) {
        snprintf(s1, sizeof s1, "%.2f", a.returns.ret_1m_pct);
    } else {
        snprintf(s1, sizeof s1, "NA");
    }
    if (a.returns.valid_5m) {
        snprintf(s5, sizeof s5, "%.2f", a.returns.ret_5m_pct);
    } else {
        snprintf(s5, sizeof s5, "NA");
    }
    if (a.returns.valid_30m) {
        snprintf(s30, sizeof s30, "%.2f", a.returns.ret_30m_pct);
    } else {
        snprintf(s30, sizeof s30, "NA");
    }
    if (a.ranges.valid_5m) {
        snprintf(g5, sizeof g5, "%.2f", out.range_5m_pct);
    } else {
        snprintf(g5, sizeof g5, "NA");
    }
    if (a.ranges.valid_30m) {
        snprintf(g30, sizeof g30, "%.2f", out.range_30m_pct);
    } else {
        snprintf(g30, sizeof g30, "NA");
    }

    ESP_LOGD(TAG, "ret1m=%s ret5m=%s ret30m=%s range5m=%s range30m=%s trend=%.2f chop=%.2f class=%s", s1, s5, s30, g5,
             g30, out.trend_strength_score, out.choppiness_score, class_label(out.regime));
}

extern "C" void regime_on_1m_closed(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    (void)arg;
    (void)base;
    (void)event_data;
    if (id != event_bus::EV_1M_CLOSED) {
        return;
    }
    candle_engine::MarketAnalyticsSnapshot snap{};
    if (!candle_engine::get_analytics_snapshot(&snap)) {
        return;
    }
    update_from_analytics(snap);
}

esp_err_t init() {
    std::memset(&s_snap, 0, sizeof(s_snap));
    const esp_err_t err =
        esp_event_handler_register(CRYPTO_V3_EVENTS, event_bus::EV_1M_CLOSED, &regime_on_1m_closed, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register EV_1M_CLOSED failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init (light regime op analytics, EV_1M_CLOSED)");
    return ESP_OK;
}

bool get_snapshot(RegimeSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_snap_mux);
    *out = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);
    return true;
}

}  // namespace regime_engine
