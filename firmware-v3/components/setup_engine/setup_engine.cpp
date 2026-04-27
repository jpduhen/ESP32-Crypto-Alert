#include "setup_engine/setup_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "candle_engine/candle_engine.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "event_bus/event_bus.hpp"
#include "freertos/portmacro.h"
#include "level_engine/level_engine.hpp"
#include "market_store/market_store.hpp"
#include "regime_engine/regime_engine.hpp"

static const char *TAG = "SETUP";

static setup_engine::SetupSnapshot s_snap{};
static portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;

namespace {

/**
 * Drempels (lokaal, later evt. settings):
 * - Proximity: abs(distance_pct) <= kProximityAbsPct telt als "dicht bij level".
 * - Fast approach: |ret_1m| >= kFast1mAbsPct of |ret_5m| >= kFast5mAbsPct (alleen als analytics valid),
 *   én level proximity moet waar zijn (anders geen spike-naar-level).
 * - fast_approach_score: min(1, max(|ret_1m|,|ret_5m|) / kFastScoreNormalizer) met alleen geldige returns meegeteld.
 */
constexpr double kProximityAbsPct = 0.10;
constexpr double kFast1mAbsPct = 0.034;
constexpr double kFast5mAbsPct = 0.055;
constexpr double kFastScoreNormalizer = 0.10;

const level_engine::PriceLevel *pick_support(const level_engine::LevelSnapshot &ls) {
    if (ls.nearest_support_1.valid) {
        return &ls.nearest_support_1;
    }
    if (ls.nearest_support_2.valid) {
        return &ls.nearest_support_2;
    }
    return nullptr;
}

const level_engine::PriceLevel *pick_resistance(const level_engine::LevelSnapshot &ls) {
    if (ls.nearest_resistance_1.valid) {
        return &ls.nearest_resistance_1;
    }
    if (ls.nearest_resistance_2.valid) {
        return &ls.nearest_resistance_2;
    }
    return nullptr;
}

double max_abs_returns(const candle_engine::MarketAnalyticsSnapshot &a) {
    double m = 0.0;
    if (a.returns.valid_1m) {
        m = std::max(m, std::fabs(a.returns.ret_1m_pct));
    }
    if (a.returns.valid_5m) {
        m = std::max(m, std::fabs(a.returns.ret_5m_pct));
    }
    return m;
}

double fast_approach_score_from(const candle_engine::MarketAnalyticsSnapshot &a) {
    const double m = max_abs_returns(a);
    if (m <= 0.0 || !std::isfinite(m)) {
        return 0.0;
    }
    return std::min(1.0, m / kFastScoreNormalizer);
}

bool approach_ok_for_side(bool level_ok, const candle_engine::MarketAnalyticsSnapshot &a) {
    if (!level_ok) {
        return false;
    }
    const bool hit1 = a.returns.valid_1m && (std::fabs(a.returns.ret_1m_pct) >= kFast1mAbsPct);
    const bool hit5 = a.returns.valid_5m && (std::fabs(a.returns.ret_5m_pct) >= kFast5mAbsPct);
    return hit1 || hit5;
}

bool level_proximity_ok(const level_engine::PriceLevel *lv) {
    if (lv == nullptr || !lv->valid) {
        return false;
    }
    return std::fabs(lv->distance_pct) <= kProximityAbsPct;
}

int regime_quality_point(const regime_engine::RegimeSnapshot &r) {
    if (!r.valid) {
        return 0;
    }
    if (r.regime == regime_engine::RegimeClass::kRange) {
        return 1;
    }
    return 0;
}

bool regime_allows_setup(const regime_engine::RegimeSnapshot &r) {
    if (!r.valid) {
        return false;
    }
    return r.regime == regime_engine::RegimeClass::kRange || r.regime == regime_engine::RegimeClass::kNeutral;
}

void evaluate_long(const candle_engine::MarketAnalyticsSnapshot &analytics, const regime_engine::RegimeSnapshot &regime,
                   const level_engine::LevelSnapshot &levels, bool regime_ok, uint8_t *q, bool *lvl_ok, bool *app_ok,
                   double *dist_pct, double *fast_sc, char *lvl_name, size_t lvl_name_sz) {
    *q = 0;
    *lvl_ok = false;
    *app_ok = false;
    *dist_pct = 0.0;
    *fast_sc = fast_approach_score_from(analytics);
    if (lvl_name_sz > 0) {
        lvl_name[0] = '\0';
    }

    const level_engine::PriceLevel *sup = pick_support(levels);
    *lvl_ok = level_proximity_ok(sup);
    if (sup != nullptr) {
        *dist_pct = sup->distance_pct;
        if (sup == &levels.nearest_support_1) {
            snprintf(lvl_name, lvl_name_sz, "support1");
        } else {
            snprintf(lvl_name, lvl_name_sz, "support2");
        }
    }

    *app_ok = approach_ok_for_side(*lvl_ok, analytics);

    const int rq = regime_quality_point(regime);
    const int lq = *lvl_ok ? 1 : 0;
    const int aq = *app_ok ? 1 : 0;
    *q = static_cast<uint8_t>(rq + lq + aq);

    if (!regime_ok) {
        *q = 0;
        *app_ok = false;
        *lvl_ok = false;
    }
}

void evaluate_short(const candle_engine::MarketAnalyticsSnapshot &analytics, const regime_engine::RegimeSnapshot &regime,
                    const level_engine::LevelSnapshot &levels, bool regime_ok, uint8_t *q, bool *lvl_ok, bool *app_ok,
                    double *dist_pct, double *fast_sc, char *lvl_name, size_t lvl_name_sz) {
    *q = 0;
    *lvl_ok = false;
    *app_ok = false;
    *dist_pct = 0.0;
    *fast_sc = fast_approach_score_from(analytics);
    if (lvl_name_sz > 0) {
        lvl_name[0] = '\0';
    }

    const level_engine::PriceLevel *res = pick_resistance(levels);
    *lvl_ok = level_proximity_ok(res);
    if (res != nullptr) {
        *dist_pct = res->distance_pct;
        if (res == &levels.nearest_resistance_1) {
            snprintf(lvl_name, lvl_name_sz, "resistance1");
        } else {
            snprintf(lvl_name, lvl_name_sz, "resistance2");
        }
    }

    *app_ok = approach_ok_for_side(*lvl_ok, analytics);

    const int rq = regime_quality_point(regime);
    const int lq = *lvl_ok ? 1 : 0;
    const int aq = *app_ok ? 1 : 0;
    *q = static_cast<uint8_t>(rq + lq + aq);

    if (!regime_ok) {
        *q = 0;
        *app_ok = false;
        *lvl_ok = false;
    }
}

setup_engine::SetupClass class_from_quality(uint8_t q) {
    if (q >= 3) {
        return setup_engine::SetupClass::kHighQualityCandidate;
    }
    if (q == 2) {
        return setup_engine::SetupClass::kCandidate;
    }
    return setup_engine::SetupClass::kNone;
}

void rebuild_and_publish() {
    setup_engine::SetupSnapshot out{};
    const market_store::MarketSnapshot m = market_store::get_snapshot();
    out.ts_ms = m.ts_ms ? m.ts_ms : 0;

    candle_engine::MarketAnalyticsSnapshot analytics{};
    regime_engine::RegimeSnapshot regime{};
    level_engine::LevelSnapshot levels{};

    const bool have_a = candle_engine::get_analytics_snapshot(&analytics);
    const bool have_r = regime_engine::get_snapshot(&regime);
    const bool have_l = level_engine::get_snapshot(&levels);

    if (!have_a || !have_r || !have_l || analytics.closed_1m_count < 2 || m.last_price <= 0.0) {
        out.valid = false;
        out.side = setup_engine::SetupSide::kNone;
        out.setup_class = setup_engine::SetupClass::kNone;
        portENTER_CRITICAL(&s_snap_mux);
        s_snap = out;
        portEXIT_CRITICAL(&s_snap_mux);
        return;
    }

    out.valid = true;
    const bool regime_ok = regime_allows_setup(regime);
    out.regime_ok = regime_ok;

    if (!regime_ok) {
        out.side = setup_engine::SetupSide::kNone;
        out.setup_class = setup_engine::SetupClass::kNone;
        out.level_ok = false;
        out.approach_ok = false;
        out.distance_to_level_pct = 0.0;
        out.fast_approach_score = fast_approach_score_from(analytics);
        out.quality_score = 0;
        snprintf(out.level_name, sizeof out.level_name, "none");

        portENTER_CRITICAL(&s_snap_mux);
        s_snap = out;
        portEXIT_CRITICAL(&s_snap_mux);

        (void)have_r;
        return;
    }

    uint8_t qL = 0;
    uint8_t qS = 0;
    bool lvlL = false, appL = false;
    bool lvlS = false, appS = false;
    double dL = 0.0, dS = 0.0;
    double fL = 0.0, fS = 0.0;
    char nameL[32]{};
    char nameS[32]{};

    evaluate_long(analytics, regime, levels, regime_ok, &qL, &lvlL, &appL, &dL, &fL, nameL, sizeof nameL);
    evaluate_short(analytics, regime, levels, regime_ok, &qS, &lvlS, &appS, &dS, &fS, nameS, sizeof nameS);

    setup_engine::SetupSide side = setup_engine::SetupSide::kNone;
    setup_engine::SetupClass cls = setup_engine::SetupClass::kNone;
    bool pickL = false;
    bool pickS = false;
    if (qL >= 2 && qS >= 2 && qL == qS) {
        if (fL > fS) {
            pickL = true;
        } else if (fS > fL) {
            pickS = true;
        }
    } else {
        if (qL >= 2 && qL > qS) {
            pickL = true;
        } else if (qS >= 2 && qS > qL) {
            pickS = true;
        } else if (qL >= 2) {
            pickL = true;
        } else if (qS >= 2) {
            pickS = true;
        }
    }

    if (pickL && !pickS) {
        side = setup_engine::SetupSide::kLong;
        cls = class_from_quality(qL);
        out.level_ok = lvlL;
        out.approach_ok = appL;
        out.distance_to_level_pct = dL;
        out.fast_approach_score = fL;
        out.quality_score = qL;
        snprintf(out.level_name, sizeof out.level_name, "%s", nameL);
    } else if (pickS && !pickL) {
        side = setup_engine::SetupSide::kShort;
        cls = class_from_quality(qS);
        out.level_ok = lvlS;
        out.approach_ok = appS;
        out.distance_to_level_pct = dS;
        out.fast_approach_score = fS;
        out.quality_score = qS;
        snprintf(out.level_name, sizeof out.level_name, "%s", nameS);
    } else {
        side = setup_engine::SetupSide::kNone;
        cls = setup_engine::SetupClass::kNone;
        out.level_ok = false;
        out.approach_ok = false;
        out.distance_to_level_pct = 0.0;
        out.fast_approach_score = fast_approach_score_from(analytics);
        out.quality_score = static_cast<uint8_t>(std::max<uint8_t>(qL, qS));
        snprintf(out.level_name, sizeof out.level_name, "none");
    }

    out.side = side;
    out.setup_class = cls;

    portENTER_CRITICAL(&s_snap_mux);
    s_snap = out;
    portEXIT_CRITICAL(&s_snap_mux);

}

extern "C" void setup_on_1m_closed(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    (void)arg;
    (void)base;
    (void)event_data;
    if (id != event_bus::EV_1M_CLOSED) {
        return;
    }
    rebuild_and_publish();
}

}  // namespace

namespace setup_engine {

esp_err_t init() {
    std::memset(&s_snap, 0, sizeof(s_snap));
    const esp_err_t err =
        esp_event_handler_register(CRYPTO_V3_EVENTS, event_bus::EV_1M_CLOSED, &setup_on_1m_closed, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register EV_1M_CLOSED failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init (mean-reversion setup light, EV_1M_CLOSED)");
    return ESP_OK;
}

bool get_snapshot(SetupSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_snap_mux);
    *out = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);
    return true;
}

}  // namespace setup_engine
