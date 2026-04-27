#include "ui_model/ui_model.hpp"

#include <cstdio>
#include <cstring>

#include "app_version/app_version.hpp"
#include "alert_engine/alert_engine.hpp"
#include "candle_engine/candle_engine.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "level_engine/level_engine.hpp"
#include "market_store/market_store.hpp"
#include "market_ws/market_ws.hpp"
#include "regime_engine/regime_engine.hpp"
#include "setup_engine/setup_engine.hpp"
#include "trigger_engine/trigger_engine.hpp"
#include "wifi_manager/wifi_manager.hpp"

static const char *TAG = "UI_MODEL";

namespace {

const char *regime_label(regime_engine::RegimeClass c) {
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

const char *setup_state_label(const setup_engine::SetupSnapshot &su, char *buf, size_t buf_sz) {
    if (!su.valid || su.setup_class == setup_engine::SetupClass::kNone) {
        std::snprintf(buf, buf_sz, "NONE");
        return buf;
    }
    const char *side = "N";
    if (su.side == setup_engine::SetupSide::kLong) {
        side = "L";
    } else if (su.side == setup_engine::SetupSide::kShort) {
        side = "S";
    }
    const char *cls = (su.setup_class == setup_engine::SetupClass::kHighQualityCandidate) ? "HQ" : "CAND";
    std::snprintf(buf, buf_sz, "%s %s q=%u", cls, side, static_cast<unsigned>(su.quality_score));
    return buf;
}

const char *trigger_state_label(const trigger_engine::TriggerSnapshot &tr, char *buf, size_t buf_sz) {
    if (!tr.valid || tr.state == trigger_engine::TriggerState::kNone) {
        std::snprintf(buf, buf_sz, "NONE");
        return buf;
    }
    const char *st = "NONE";
    if (tr.state == trigger_engine::TriggerState::kCandidate) {
        st = "CANDIDATE";
    } else if (tr.state == trigger_engine::TriggerState::kTriggered) {
        st = "TRIGGERED";
    } else if (tr.state == trigger_engine::TriggerState::kInvalidated) {
        st = "INVALIDATED";
    }
    std::snprintf(buf, buf_sz, "%s", st);
    return buf;
}

}  // namespace

namespace ui_model {

esp_err_t init() {
    ESP_LOGI(TAG, "init (status model snapshots)");
    return ESP_OK;
}

bool get_status_model(UiStatusModel *out) {
    if (out == nullptr) {
        return false;
    }
    std::memset(out, 0, sizeof(*out));

    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    out->ts_ms = now_ms;
    out->uptime_s = static_cast<uint64_t>(esp_timer_get_time() / 1000000ULL);
    out->free_heap = static_cast<uint32_t>(esp_get_free_heap_size());
    out->min_free_heap = static_cast<uint32_t>(esp_get_minimum_free_heap_size());
    std::snprintf(out->version, sizeof out->version, "%s", app_version::version_string());

    const auto wst = wifi_manager::get_state();
    out->wifi_ready = (wst == wifi_manager::WifiState::kGotIp);
    std::snprintf(out->wifi_state, sizeof out->wifi_state, "%s", wifi_manager::state_label(wst));

    out->ws_live = market_ws::is_live();
    std::snprintf(out->ws_state, sizeof out->ws_state, "%s", market_ws::state_to_string());
    out->ws_last_rx_age_ms = market_ws::idle_since_last_rx_ms();

    const market_store::MarketSnapshot m = market_store::get_snapshot();
    out->last_price = m.last_price;

    candle_engine::MarketAnalyticsSnapshot a{};
    if (candle_engine::get_analytics_snapshot(&a)) {
        out->ret_1m_valid = a.returns.valid_1m;
        out->ret_5m_valid = a.returns.valid_5m;
        out->ret_30m_valid = a.returns.valid_30m;
        out->ret_1m_pct = a.returns.ret_1m_pct;
        out->ret_5m_pct = a.returns.ret_5m_pct;
        out->ret_30m_pct = a.returns.ret_30m_pct;
    }

    regime_engine::RegimeSnapshot rg{};
    if (regime_engine::get_snapshot(&rg) && rg.valid) {
        std::snprintf(out->regime, sizeof out->regime, "%s", regime_label(rg.regime));
    } else {
        std::snprintf(out->regime, sizeof out->regime, "UNKNOWN");
    }

    level_engine::LevelSnapshot ls{};
    if (level_engine::get_snapshot(&ls) && ls.valid) {
        out->support_1_valid = ls.nearest_support_1.valid;
        out->support_1 = ls.nearest_support_1.price;
        out->resistance_1_valid = ls.nearest_resistance_1.valid;
        out->resistance_1 = ls.nearest_resistance_1.price;
    }

    setup_engine::SetupSnapshot su{};
    if (setup_engine::get_snapshot(&su)) {
        setup_state_label(su, out->setup_state, sizeof out->setup_state);
    } else {
        std::snprintf(out->setup_state, sizeof out->setup_state, "NONE");
    }

    trigger_engine::TriggerSnapshot tr{};
    if (trigger_engine::get_snapshot(&tr)) {
        trigger_state_label(tr, out->trigger_state, sizeof out->trigger_state);
    } else {
        std::snprintf(out->trigger_state, sizeof out->trigger_state, "NONE");
    }

    alert_engine::AlertSnapshot al{};
    if (alert_engine::get_snapshot(&al)) {
        std::snprintf(out->alert_state, sizeof out->alert_state, "%s", alert_engine::state_label(al.state));
    } else {
        std::snprintf(out->alert_state, sizeof out->alert_state, "NONE");
    }

    return true;
}

}  // namespace ui_model
