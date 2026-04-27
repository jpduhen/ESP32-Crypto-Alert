#include "trigger_engine/trigger_engine.hpp"

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
#include "setup_engine/setup_engine.hpp"

static const char *TAG = "TRIGGER";

static trigger_engine::TriggerSnapshot s_snap{};
static portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;

/** Interne FSM (los van laatste gepubliceerde snapshot bij INVALIDATED cleanup). */
static trigger_engine::TriggerState s_fsm = trigger_engine::TriggerState::kNone;
static trigger_engine::TriggerSide s_side = trigger_engine::TriggerSide::kNone;
static double s_level_px = 0.0;
static uint8_t s_quality = 0;
static char s_level_name[32]{};

static trigger_engine::TriggerState s_prev_log_state = trigger_engine::TriggerState::kNone;
static trigger_engine::TriggerSide s_prev_log_side = trigger_engine::TriggerSide::kNone;

namespace {

/**
 * Trigger (gesloten 1m candle `C` op index back=0, net na minuutsluiting):
 *
 * Long:  C.low <= level && C.close > level
 * Short: C.high >= level && C.close < level
 *
 * Invalidatie vóór trigger (candidate): close breekt level met marge zonder trigger-setup:
 * Long:  C.close < level * (1 - kAwayFrac)
 * Short: C.close > level * (1 + kAwayFrac)
 *
 * kAwayFrac = 0.001 (~0,1%) — lokaal, later instelbaar.
 */
constexpr double kAwayFrac = 0.001;

bool resolve_level_price(const level_engine::LevelSnapshot &ls, const char *name, double *out) {
    if (out == nullptr || name == nullptr || name[0] == '\0') {
        return false;
    }
    if (strcmp(name, "support1") == 0 && ls.nearest_support_1.valid) {
        *out = ls.nearest_support_1.price;
        return true;
    }
    if (strcmp(name, "support2") == 0 && ls.nearest_support_2.valid) {
        *out = ls.nearest_support_2.price;
        return true;
    }
    if (strcmp(name, "resistance1") == 0 && ls.nearest_resistance_1.valid) {
        *out = ls.nearest_resistance_1.price;
        return true;
    }
    if (strcmp(name, "resistance2") == 0 && ls.nearest_resistance_2.valid) {
        *out = ls.nearest_resistance_2.price;
        return true;
    }
    return false;
}

bool setup_is_active_candidate(const setup_engine::SetupSnapshot &su) {
    if (!su.valid) {
        return false;
    }
    if (!su.regime_ok) {
        return false;
    }
    if (su.side == setup_engine::SetupSide::kNone) {
        return false;
    }
    return su.setup_class == setup_engine::SetupClass::kCandidate ||
           su.setup_class == setup_engine::SetupClass::kHighQualityCandidate;
}

void publish_snapshot(const trigger_engine::TriggerSnapshot &snap) {
    portENTER_CRITICAL(&s_snap_mux);
    s_snap = snap;
    portEXIT_CRITICAL(&s_snap_mux);
}

void log_if_changed(const trigger_engine::TriggerSnapshot &snap) {
    const bool changed = (snap.state != s_prev_log_state) || (snap.side != s_prev_log_side);
    if (!changed) {
        return;
    }

    s_prev_log_state = snap.state;
    s_prev_log_side = snap.side;

    const char *sd = "NONE";
    if (snap.side == trigger_engine::TriggerSide::kLong) {
        sd = "LONG";
    } else if (snap.side == trigger_engine::TriggerSide::kShort) {
        sd = "SHORT";
    }

    if (snap.state == trigger_engine::TriggerState::kNone) {
        ESP_LOGI(TAG, "state=NONE side=NONE level=none reason=%s", snap.reason);
    } else if (snap.state == trigger_engine::TriggerState::kTriggered) {
        ESP_LOGI(TAG, "state=TRIGGERED side=%s q=%u/3 level=%s touched=%d close_back=%d price=%.1f close=%.1f", sd,
                 static_cast<unsigned>(snap.inherited_quality_score), snap.source_level_name, snap.level_touched ? 1 : 0,
                 snap.close_back_confirmed ? 1 : 0, snap.trigger_level_price, snap.close_price);
    } else if (snap.state == trigger_engine::TriggerState::kCandidate) {
        ESP_LOGI(TAG, "state=CANDIDATE side=%s q=%u/3 level=%s price=%.1f", sd,
                 static_cast<unsigned>(snap.inherited_quality_score), snap.source_level_name, snap.trigger_level_price);
    } else if (snap.state == trigger_engine::TriggerState::kInvalidated) {
        ESP_LOGI(TAG, "state=INVALIDATED side=%s level=%s reason=%s q=%u/3", sd, snap.source_level_name, snap.reason,
                 static_cast<unsigned>(snap.inherited_quality_score));
    }
}

void arm_candidate(const setup_engine::SetupSnapshot &su, const level_engine::LevelSnapshot &ls, uint64_t ts_ms) {
    double px = 0.0;
    if (!resolve_level_price(ls, su.level_name, &px) || px <= 0.0) {
        return;
    }
    s_fsm = trigger_engine::TriggerState::kCandidate;
    if (su.side == setup_engine::SetupSide::kLong) {
        s_side = trigger_engine::TriggerSide::kLong;
    } else {
        s_side = trigger_engine::TriggerSide::kShort;
    }
    s_level_px = px;
    s_quality = su.quality_score;
    snprintf(s_level_name, sizeof s_level_name, "%s", su.level_name);

    trigger_engine::TriggerSnapshot out{};
    out.ts_ms = ts_ms;
    out.valid = true;
    out.state = trigger_engine::TriggerState::kCandidate;
    out.side = s_side;
    out.level_touched = false;
    out.close_back_confirmed = false;
    out.trigger_level_price = px;
    out.close_price = 0.0;
    out.inherited_quality_score = s_quality;
    snprintf(out.source_level_name, sizeof out.source_level_name, "%s", s_level_name);
    snprintf(out.reason, sizeof out.reason, "%s", "active_candidate");
    publish_snapshot(out);
    log_if_changed(out);
}

void fire_triggered(const candle_engine::Candle1m &c, uint64_t ts_ms, bool touched, bool close_back) {
    s_fsm = trigger_engine::TriggerState::kTriggered;
    trigger_engine::TriggerSnapshot out{};
    out.ts_ms = ts_ms;
    out.valid = true;
    out.state = trigger_engine::TriggerState::kTriggered;
    out.side = s_side;
    out.level_touched = touched;
    out.close_back_confirmed = close_back;
    out.trigger_level_price = s_level_px;
    out.close_price = c.close;
    out.inherited_quality_score = s_quality;
    snprintf(out.source_level_name, sizeof out.source_level_name, "%s", s_level_name);
    snprintf(out.reason, sizeof out.reason, "%s", "confirm_ok");
    publish_snapshot(out);
    log_if_changed(out);
}

void fire_invalidated(trigger_engine::TriggerSide side, uint64_t ts_ms, const char *reason) {
    s_fsm = trigger_engine::TriggerState::kInvalidated;
    trigger_engine::TriggerSnapshot out{};
    out.ts_ms = ts_ms;
    out.valid = true;
    out.state = trigger_engine::TriggerState::kInvalidated;
    out.side = side;
    out.level_touched = false;
    out.close_back_confirmed = false;
    out.trigger_level_price = s_level_px;
    out.close_price = 0.0;
    out.inherited_quality_score = s_quality;
    snprintf(out.source_level_name, sizeof out.source_level_name, "%s", s_level_name);
    snprintf(out.reason, sizeof out.reason, "%s", reason ? reason : "invalidated");
    publish_snapshot(out);
    log_if_changed(out);
}

void publish_none(uint64_t ts_ms) {
    s_fsm = trigger_engine::TriggerState::kNone;
    s_side = trigger_engine::TriggerSide::kNone;
    s_level_px = 0.0;
    s_quality = 0;
    s_level_name[0] = '\0';

    trigger_engine::TriggerSnapshot out{};
    out.ts_ms = ts_ms;
    out.valid = false;
    out.state = trigger_engine::TriggerState::kNone;
    out.side = trigger_engine::TriggerSide::kNone;
    out.level_touched = false;
    out.close_back_confirmed = false;
    out.trigger_level_price = 0.0;
    out.close_price = 0.0;
    out.inherited_quality_score = 0;
    out.source_level_name[0] = '\0';
    std::snprintf(out.reason, sizeof out.reason, "%s", "no_active_candidate");
    publish_snapshot(out);
    log_if_changed(out);
}

void on_1m_closed_impl() {
    const market_store::MarketSnapshot m = market_store::get_snapshot();
    const uint64_t ts_ms = m.ts_ms ? m.ts_ms : 0;

    if (s_fsm == trigger_engine::TriggerState::kInvalidated) {
        publish_none(ts_ms);
    }

    setup_engine::SetupSnapshot su{};
    regime_engine::RegimeSnapshot rg{};
    level_engine::LevelSnapshot ls{};
    candle_engine::Candle1m c{};

    const bool have_su = setup_engine::get_snapshot(&su);
    const bool have_rg = regime_engine::get_snapshot(&rg);
    const bool have_ls = level_engine::get_snapshot(&ls);
    const bool have_c = candle_engine::get_closed_candle_from_latest(0, &c) && c.valid;

    if (!have_c) {
        return;
    }

    if (s_fsm == trigger_engine::TriggerState::kTriggered) {
        const bool still = have_su && setup_is_active_candidate(su) &&
                             ((su.side == setup_engine::SetupSide::kLong && s_side == trigger_engine::TriggerSide::kLong) ||
                              (su.side == setup_engine::SetupSide::kShort && s_side == trigger_engine::TriggerSide::kShort));
        if (!still) {
            publish_none(ts_ms);
        } else {
            trigger_engine::TriggerSnapshot out{};
            out.ts_ms = ts_ms;
            out.valid = true;
            out.state = trigger_engine::TriggerState::kTriggered;
            out.side = s_side;
            out.level_touched = true;
            out.close_back_confirmed = true;
            out.trigger_level_price = s_level_px;
            out.close_price = c.close;
            out.inherited_quality_score = s_quality;
            snprintf(out.source_level_name, sizeof out.source_level_name, "%s", s_level_name);
            snprintf(out.reason, sizeof out.reason, "%s", "trigger_held");
            publish_snapshot(out);
        }
        return;
    }

    if (s_fsm == trigger_engine::TriggerState::kNone) {
        if (have_su && have_ls && have_rg && rg.valid && rg.regime != regime_engine::RegimeClass::kTrend &&
            rg.regime != regime_engine::RegimeClass::kUnknown && setup_is_active_candidate(su)) {
            double px = 0.0;
            if (resolve_level_price(ls, su.level_name, &px) && px > 0.0) {
                arm_candidate(su, ls, ts_ms);
            }
        }
        return;
    }

    if (s_fsm != trigger_engine::TriggerState::kCandidate) {
        return;
    }

    const bool regime_bad =
        !have_rg || !rg.valid || rg.regime == regime_engine::RegimeClass::kTrend || rg.regime == regime_engine::RegimeClass::kUnknown;
    const bool setup_lost = !have_su || !setup_is_active_candidate(su) || su.side == setup_engine::SetupSide::kNone ||
                            (su.side == setup_engine::SetupSide::kLong && s_side != trigger_engine::TriggerSide::kLong) ||
                            (su.side == setup_engine::SetupSide::kShort && s_side != trigger_engine::TriggerSide::kShort);
    const bool name_mismatch =
        have_su && (strncmp(su.level_name, s_level_name, sizeof s_level_name) != 0 || su.level_name[0] == '\0');

    if (regime_bad) {
        fire_invalidated(s_side, ts_ms, "regime_lost");
        return;
    }
    if (setup_lost || name_mismatch) {
        fire_invalidated(s_side, ts_ms, "setup_lost");
        return;
    }

    if (s_side == trigger_engine::TriggerSide::kLong) {
        if (c.close < s_level_px * (1.0 - kAwayFrac)) {
            fire_invalidated(s_side, ts_ms, "level_context_lost");
            return;
        }
        const bool touched = c.low <= s_level_px;
        const bool close_back = c.close > s_level_px;
        if (touched && close_back) {
            fire_triggered(c, ts_ms, true, true);
            return;
        }
        if (touched || close_back) {
            ESP_LOGD(TAG, "touch=%d close_back=%d -> no trigger side=LONG level=%.1f close=%.1f",
                     touched ? 1 : 0, close_back ? 1 : 0, s_level_px, c.close);
        }
    } else if (s_side == trigger_engine::TriggerSide::kShort) {
        if (c.close > s_level_px * (1.0 + kAwayFrac)) {
            fire_invalidated(s_side, ts_ms, "level_context_lost");
            return;
        }
        const bool touched = c.high >= s_level_px;
        const bool close_back = c.close < s_level_px;
        if (touched && close_back) {
            fire_triggered(c, ts_ms, true, true);
            return;
        }
        if (touched || close_back) {
            ESP_LOGD(TAG, "touch=%d close_back=%d -> no trigger side=SHORT level=%.1f close=%.1f",
                     touched ? 1 : 0, close_back ? 1 : 0, s_level_px, c.close);
        }
    }

    {
        trigger_engine::TriggerSnapshot out{};
        out.ts_ms = ts_ms;
        out.valid = true;
        out.state = trigger_engine::TriggerState::kCandidate;
        out.side = s_side;
        out.level_touched = false;
        out.close_back_confirmed = false;
        out.trigger_level_price = s_level_px;
        out.close_price = c.close;
        out.inherited_quality_score = s_quality;
        snprintf(out.source_level_name, sizeof out.source_level_name, "%s", s_level_name);
        snprintf(out.reason, sizeof out.reason, "%s", "waiting_confirm");
        publish_snapshot(out);
    }
}

extern "C" void trigger_on_1m_closed(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
    (void)arg;
    (void)base;
    (void)event_data;
    if (id != event_bus::EV_1M_CLOSED) {
        return;
    }
    on_1m_closed_impl();
}

}  // namespace

namespace trigger_engine {

esp_err_t init() {
    std::memset(&s_snap, 0, sizeof(s_snap));
    s_fsm = TriggerState::kNone;
    s_side = TriggerSide::kNone;
    s_level_px = 0.0;
    s_quality = 0;
    s_level_name[0] = '\0';
    s_prev_log_state = TriggerState::kNone;
    s_prev_log_side = TriggerSide::kNone;

    const esp_err_t err =
        esp_event_handler_register(CRYPTO_V3_EVENTS, event_bus::EV_1M_CLOSED, &trigger_on_1m_closed, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register EV_1M_CLOSED failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init (light triggers op gesloten 1m, na setup)");
    return ESP_OK;
}

bool get_snapshot(TriggerSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_snap_mux);
    *out = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);
    return true;
}

}  // namespace trigger_engine
