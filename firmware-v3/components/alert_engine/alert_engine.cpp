#include "alert_engine/alert_engine.hpp"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include "level_engine/level_engine.hpp"
#include "market_store/market_store.hpp"
#include "setup_engine/setup_engine.hpp"
#include "trigger_engine/trigger_engine.hpp"

namespace {

const char *TAG = "ALERT";
constexpr uint64_t kPollPeriodUs = 1000000ULL;

esp_timer_handle_t s_timer = nullptr;
bool s_started = false;

alert_engine::AlertSnapshot s_snap{};
alert_engine::AlertCounters s_cnt{};
portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;

const char *side_label_from_setup(setup_engine::SetupSide side) {
    if (side == setup_engine::SetupSide::kLong) {
        return "LONG";
    }
    if (side == setup_engine::SetupSide::kShort) {
        return "SHORT";
    }
    return "NONE";
}

const char *side_label_from_trigger(trigger_engine::TriggerSide side) {
    if (side == trigger_engine::TriggerSide::kLong) {
        return "LONG";
    }
    if (side == trigger_engine::TriggerSide::kShort) {
        return "SHORT";
    }
    return "NONE";
}

void copy_side(char *dst, size_t dst_sz, const char *src) {
    if (dst == nullptr || dst_sz == 0) {
        return;
    }
    std::snprintf(dst, dst_sz, "%s", src ? src : "NONE");
}

void set_level_name(char *dst, size_t dst_sz, const char *src) {
    if (dst == nullptr || dst_sz == 0) {
        return;
    }
    std::snprintf(dst, dst_sz, "%s", (src && src[0] != '\0') ? src : "none");
}

bool is_setup_candidate(const setup_engine::SetupSnapshot &su) {
    if (!su.valid) {
        return false;
    }
    return su.setup_class == setup_engine::SetupClass::kCandidate ||
           su.setup_class == setup_engine::SetupClass::kHighQualityCandidate;
}

bool is_trigger_candidate(const trigger_engine::TriggerSnapshot &tr) {
    return tr.valid && tr.state == trigger_engine::TriggerState::kCandidate;
}

bool is_trigger_triggered(const trigger_engine::TriggerSnapshot &tr) {
    return tr.valid && tr.state == trigger_engine::TriggerState::kTriggered;
}

bool is_trigger_invalidated(const trigger_engine::TriggerSnapshot &tr) {
    return tr.valid && tr.state == trigger_engine::TriggerState::kInvalidated;
}

struct EvalResult {
    alert_engine::AlertLifecycleState state;
    const char *source;
    const char *side;
    uint8_t quality;
    const char *level;
    double ref_price;
    const char *reason;
};

EvalResult evaluate_next(const setup_engine::SetupSnapshot &su, const trigger_engine::TriggerSnapshot &tr,
                         const level_engine::LevelSnapshot &ls, const market_store::MarketSnapshot &m,
                         const alert_engine::AlertSnapshot &prev) {
    (void)ls;
    EvalResult r{};
    r.state = alert_engine::AlertLifecycleState::kNone;
    r.source = "none";
    r.side = "NONE";
    r.quality = 0;
    r.level = "none";
    r.ref_price = (m.last_price > 0.0) ? m.last_price : 0.0;
    r.reason = nullptr;

    if (is_trigger_triggered(tr)) {
        r.state = alert_engine::AlertLifecycleState::kTriggered;
        r.source = "trigger_engine";
        r.side = side_label_from_trigger(tr.side);
        r.quality = tr.inherited_quality_score;
        r.level = tr.source_level_name;
        r.ref_price = tr.close_price > 0.0 ? tr.close_price : tr.trigger_level_price;
        return r;
    }

    if (is_trigger_invalidated(tr)) {
        r.state = alert_engine::AlertLifecycleState::kInvalidated;
        r.source = "trigger_engine";
        r.side = side_label_from_trigger(tr.side);
        r.quality = tr.inherited_quality_score;
        r.level = tr.source_level_name;
        r.ref_price = tr.close_price > 0.0 ? tr.close_price : tr.trigger_level_price;
        r.reason = "trigger_lost";
        return r;
    }

    if (is_setup_candidate(su) || is_trigger_candidate(tr)) {
        r.state = alert_engine::AlertLifecycleState::kCandidate;
        r.source = is_trigger_candidate(tr) ? "trigger_engine" : "setup_engine";
        if (is_trigger_candidate(tr)) {
            r.side = side_label_from_trigger(tr.side);
            r.quality = tr.inherited_quality_score;
            r.level = tr.source_level_name;
            r.ref_price = tr.trigger_level_price > 0.0 ? tr.trigger_level_price : r.ref_price;
        } else {
            r.side = side_label_from_setup(su.side);
            r.quality = su.quality_score;
            r.level = su.level_name;
            r.ref_price = m.last_price > 0.0 ? m.last_price : 0.0;
        }
        return r;
    }

    if (m.ws_live && m.last_price > 0.0) {
        r.state = alert_engine::AlertLifecycleState::kWatch;
        r.source = "market_store";
        r.ref_price = m.last_price;
        return r;
    }

    if (prev.state == alert_engine::AlertLifecycleState::kTriggered ||
        prev.state == alert_engine::AlertLifecycleState::kInvalidated ||
        prev.state == alert_engine::AlertLifecycleState::kCandidate) {
        r.state = alert_engine::AlertLifecycleState::kResolved;
        r.source = "alert_engine";
        r.side = prev.side;
        r.quality = prev.quality_score;
        r.level = prev.level_name;
        r.ref_price = m.last_price > 0.0 ? m.last_price : prev.ref_price;
        return r;
    }

    return r;
}

void log_state_change(const alert_engine::AlertSnapshot &prev, const alert_engine::AlertSnapshot &next,
                      const char *reason) {
    if (prev.state == next.state) {
        return;
    }
    const char *st = alert_engine::state_label(next.state);
    if (next.state == alert_engine::AlertLifecycleState::kInvalidated) {
        ESP_LOGI(TAG, "state=%s side=%s reason=%s", st, next.side, reason ? reason : "unknown");
    } else if (next.state == alert_engine::AlertLifecycleState::kResolved) {
        ESP_LOGI(TAG, "state=%s side=%s", st, next.side);
    } else if (next.state == alert_engine::AlertLifecycleState::kCandidate ||
               next.state == alert_engine::AlertLifecycleState::kTriggered) {
        ESP_LOGI(TAG, "state=%s side=%s q=%u level=%s price=%.1f", st, next.side,
                 static_cast<unsigned>(next.quality_score), next.level_name, next.ref_price);
    } else {
        ESP_LOGI(TAG, "state=%s side=%s price=%.1f", st, next.side, next.ref_price);
    }
}

void bump_counters(alert_engine::AlertLifecycleState st) {
    switch (st) {
        case alert_engine::AlertLifecycleState::kCandidate:
            ++s_cnt.candidate_count;
            break;
        case alert_engine::AlertLifecycleState::kTriggered:
            ++s_cnt.triggered_count;
            break;
        case alert_engine::AlertLifecycleState::kInvalidated:
            ++s_cnt.invalidated_count;
            break;
        case alert_engine::AlertLifecycleState::kResolved:
            ++s_cnt.resolved_count;
            break;
        default:
            break;
    }
}

void poll_once() {
    setup_engine::SetupSnapshot su{};
    trigger_engine::TriggerSnapshot tr{};
    level_engine::LevelSnapshot ls{};
    const market_store::MarketSnapshot m = market_store::get_snapshot();
    (void)setup_engine::get_snapshot(&su);
    (void)trigger_engine::get_snapshot(&tr);
    (void)level_engine::get_snapshot(&ls);

    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);

    alert_engine::AlertSnapshot prev{};
    portENTER_CRITICAL(&s_snap_mux);
    prev = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);

    const EvalResult ev = evaluate_next(su, tr, ls, m, prev);

    alert_engine::AlertSnapshot next = prev;
    next.ts_ms = now_ms;
    next.valid = ev.state != alert_engine::AlertLifecycleState::kNone;
    next.state = ev.state;
    std::snprintf(next.source, sizeof next.source, "%s", ev.source);
    copy_side(next.side, sizeof next.side, ev.side);
    next.quality_score = ev.quality;
    set_level_name(next.level_name, sizeof next.level_name, ev.level);
    next.ref_price = ev.ref_price;
    if (prev.state != next.state) {
        next.state_age_ms = 0;
    } else if (prev.ts_ms > 0 && now_ms >= prev.ts_ms) {
        next.state_age_ms = prev.state_age_ms + (now_ms - prev.ts_ms);
    } else {
        next.state_age_ms = prev.state_age_ms;
    }

    log_state_change(prev, next, ev.reason);
    if (prev.state != next.state) {
        bump_counters(next.state);
    }

    portENTER_CRITICAL(&s_snap_mux);
    s_snap = next;
    portEXIT_CRITICAL(&s_snap_mux);
}

void alert_timer_cb(void *arg) {
    (void)arg;
    poll_once();
}

}  // namespace

namespace alert_engine {

const char *state_label(AlertLifecycleState s) {
    switch (s) {
        case AlertLifecycleState::kWatch:
            return "WATCH";
        case AlertLifecycleState::kCandidate:
            return "CANDIDATE";
        case AlertLifecycleState::kTriggered:
            return "TRIGGERED";
        case AlertLifecycleState::kInvalidated:
            return "INVALIDATED";
        case AlertLifecycleState::kResolved:
            return "RESOLVED";
        default:
            return "NONE";
    }
}

esp_err_t init() {
    portENTER_CRITICAL(&s_snap_mux);
    std::memset(&s_snap, 0, sizeof(s_snap));
    s_snap.valid = false;
    s_snap.state = AlertLifecycleState::kNone;
    std::snprintf(s_snap.source, sizeof s_snap.source, "%s", "none");
    std::snprintf(s_snap.side, sizeof s_snap.side, "%s", "NONE");
    std::snprintf(s_snap.level_name, sizeof s_snap.level_name, "%s", "none");
    s_snap.ref_price = 0.0;
    s_snap.state_age_ms = 0;
    std::memset(&s_cnt, 0, sizeof(s_cnt));
    portEXIT_CRITICAL(&s_snap_mux);

    const esp_timer_create_args_t args = {
        .callback = &alert_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alertpoll",
        .skip_unhandled_events = true,
    };
    const esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "init (light lifecycle, log-only)");
    return ESP_OK;
}

esp_err_t start() {
    if (s_started) {
        return ESP_OK;
    }
    if (s_timer == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t err = esp_timer_start_periodic(s_timer, kPollPeriodUs);
    if (err != ESP_OK) {
        return err;
    }
    s_started = true;
    ESP_LOGI(TAG, "start (log-only lifecycle actief)");
    return ESP_OK;
}

bool get_snapshot(AlertSnapshot *out) {
    if (out == nullptr) {
        return false;
    }
    portENTER_CRITICAL(&s_snap_mux);
    *out = s_snap;
    portEXIT_CRITICAL(&s_snap_mux);
    return true;
}

void get_counters(AlertCounters *out) {
    if (out == nullptr) {
        return;
    }
    portENTER_CRITICAL(&s_snap_mux);
    *out = s_cnt;
    portEXIT_CRITICAL(&s_snap_mux);
}

}  // namespace alert_engine
