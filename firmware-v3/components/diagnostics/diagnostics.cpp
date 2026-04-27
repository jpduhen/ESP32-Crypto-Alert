#include "diagnostics/diagnostics.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

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

static diagnostics::SoakRuntimeCounters s_soak_rt{};

namespace {

const char *TAG_SOAK = "SOAK";

constexpr uint32_t kWsStaleRxMs = 120000;
constexpr uint32_t kCandleProgressStaleSec = 150;
constexpr uint32_t kContextLagMs = 240000;

bool s_started = false;
esp_timer_handle_t s_soak_timer = nullptr;

/** Monotonic: laatste keer dat closed_1m_count steeg. */
int64_t s_last_c1m_progress_us = 0;
size_t s_last_c1m_count = 0;

wifi_manager::WifiState s_prev_wifi = wifi_manager::WifiState::kUninitialized;
market_ws::MarketWsState s_prev_ws = market_ws::MarketWsState::kUninitialized;
uint64_t s_prev_reconnect = 0;

regime_engine::RegimeClass s_prev_reg = regime_engine::RegimeClass::kUnknown;
bool s_prev_reg_valid = false;

struct LevelFp {
    bool v;
    bool s1_ok;
    bool r1_ok;
    int32_t s1_centi;
    int32_t r1_centi;
};

LevelFp level_fingerprint(const level_engine::LevelSnapshot &ls) {
    LevelFp fp{};
    fp.v = ls.valid;
    fp.s1_ok = ls.nearest_support_1.valid;
    fp.r1_ok = ls.nearest_resistance_1.valid;
    fp.s1_centi = fp.s1_ok ? static_cast<int32_t>(ls.nearest_support_1.price * 100.0) : 0;
    fp.r1_centi = fp.r1_ok ? static_cast<int32_t>(ls.nearest_resistance_1.price * 100.0) : 0;
    return fp;
}

LevelFp s_prev_lvl_fp{};

struct SetupFp {
    bool valid;
    setup_engine::SetupSide side;
    setup_engine::SetupClass cls;
    uint8_t q;
    char level[32];
    bool regime_ok;
};

SetupFp setup_fingerprint(const setup_engine::SetupSnapshot &su) {
    SetupFp fp{};
    fp.valid = su.valid;
    fp.side = su.side;
    fp.cls = su.setup_class;
    fp.q = su.quality_score;
    fp.regime_ok = su.regime_ok;
    std::snprintf(fp.level, sizeof fp.level, "%s", su.level_name);
    return fp;
}

SetupFp s_prev_su_fp{};
bool s_have_su_fp = false;

trigger_engine::TriggerState s_prev_tr_st = trigger_engine::TriggerState::kNone;
trigger_engine::TriggerSide s_prev_tr_side = trigger_engine::TriggerSide::kNone;
bool s_prev_tr_valid = false;

bool s_first_ret_1m = false;
bool s_first_ret_5m = false;
bool s_first_ret_30m = false;
bool s_first_lvl = false;
bool s_first_su = false;
bool s_first_tr = false;

bool s_latch_ws_stale = false;
bool s_latch_candle_stale = false;
bool s_latch_ctx_stale = false;

uint32_t s_tick = 0;

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

void note_c1m_progress(size_t n) {
    const int64_t now = esp_timer_get_time();
    if (n != s_last_c1m_count) {
        s_last_c1m_count = n;
        s_last_c1m_progress_us = now;
    }
}

void poll_first_valid_analytics(const candle_engine::MarketAnalyticsSnapshot &a) {
    if (a.returns.valid_1m && !s_first_ret_1m) {
        s_first_ret_1m = true;
        ESP_LOGI(TAG_SOAK, "first_valid ret_1m");
    }
    if (a.returns.valid_5m && !s_first_ret_5m) {
        s_first_ret_5m = true;
        ESP_LOGI(TAG_SOAK, "first_valid ret_5m");
    }
    if (a.returns.valid_30m && !s_first_ret_30m) {
        s_first_ret_30m = true;
        ESP_LOGI(TAG_SOAK, "first_valid ret_30m");
    }
}

void poll_wifi() {
    const wifi_manager::WifiState w = wifi_manager::get_state();
    if (w == s_prev_wifi) {
        return;
    }
    s_prev_wifi = w;
    ESP_LOGI(TAG_SOAK, "wifi state=%s", wifi_manager::state_label(w));
}

void poll_ws_transport() {
    const market_ws::MarketWsState st = market_ws::get_state();
    if (st != s_prev_ws) {
        s_prev_ws = st;
        const uint64_t rc = market_ws::reconnect_count();
        ESP_LOGI(TAG_SOAK, "ws state=%s rx=%" PRIu64 " reconn=%" PRIu64 " err=%" PRIu32 " last_rx=%" PRIu32 "ms",
                 market_ws::state_to_string(st), market_ws::rx_count(), rc, market_ws::error_count(),
                 market_ws::idle_since_last_rx_ms());
    }
    const uint64_t rc = market_ws::reconnect_count();
    if (rc > s_prev_reconnect) {
        ESP_LOGI(TAG_SOAK, "ws reconnect cycle=%" PRIu64, rc);
        s_prev_reconnect = rc;
    }
}

void poll_regime(const regime_engine::RegimeSnapshot &r) {
    if (r.valid && !s_prev_reg_valid) {
        ESP_LOGI(TAG_SOAK, "first_valid regime");
    }
    if (r.valid && s_prev_reg_valid && r.regime != s_prev_reg) {
        ++s_soak_rt.regime_changes;
        ESP_LOGI(TAG_SOAK, "regime %s -> %s", regime_label(s_prev_reg), regime_label(r.regime));
    }
    s_prev_reg = r.regime;
    s_prev_reg_valid = r.valid;
}

void poll_levels(const level_engine::LevelSnapshot &ls) {
    const LevelFp fp = level_fingerprint(ls);
    if (ls.valid && !s_first_lvl) {
        s_first_lvl = true;
        ESP_LOGI(TAG_SOAK, "first_valid levels");
    }
    if (fp.v != s_prev_lvl_fp.v || fp.s1_ok != s_prev_lvl_fp.s1_ok || fp.r1_ok != s_prev_lvl_fp.r1_ok ||
        fp.s1_centi != s_prev_lvl_fp.s1_centi || fp.r1_centi != s_prev_lvl_fp.r1_centi) {
        s_prev_lvl_fp = fp;
        if (ls.valid) {
            ESP_LOGI(TAG_SOAK, "levels s1=%s r1=%s", ls.nearest_support_1.valid ? "ok" : "-",
                     ls.nearest_resistance_1.valid ? "ok" : "-");
        } else {
            ESP_LOGI(TAG_SOAK, "levels valid=0");
        }
    }
}

void poll_setup(const setup_engine::SetupSnapshot &su) {
    const SetupFp fp = setup_fingerprint(su);
    if (!s_have_su_fp) {
        if (su.valid && su.setup_class != setup_engine::SetupClass::kNone) {
            s_first_su = true;
            ESP_LOGI(TAG_SOAK, "first_valid setup");
        }
        s_prev_su_fp = fp;
        s_have_su_fp = true;
        return;
    }

    const bool cand_now = su.setup_class == setup_engine::SetupClass::kCandidate;
    const bool hq_now = su.setup_class == setup_engine::SetupClass::kHighQualityCandidate;
    const bool cand_prev = s_prev_su_fp.cls == setup_engine::SetupClass::kCandidate;
    const bool hq_prev = s_prev_su_fp.cls == setup_engine::SetupClass::kHighQualityCandidate;

    if (cand_now && !cand_prev) {
        ++s_soak_rt.setup_candidate_entries;
    }
    if (hq_now && !hq_prev) {
        ++s_soak_rt.setup_hq_entries;
    }

    if (su.valid && su.setup_class != setup_engine::SetupClass::kNone && !s_first_su) {
        s_first_su = true;
        ESP_LOGI(TAG_SOAK, "first_valid setup");
    }

    const bool changed = fp.valid != s_prev_su_fp.valid || fp.side != s_prev_su_fp.side || fp.cls != s_prev_su_fp.cls ||
                         fp.q != s_prev_su_fp.q || fp.regime_ok != s_prev_su_fp.regime_ok ||
                         std::strncmp(fp.level, s_prev_su_fp.level, sizeof fp.level) != 0;
    if (changed) {
        s_prev_su_fp = fp;
        const char *sd = "NONE";
        if (su.side == setup_engine::SetupSide::kLong) {
            sd = "LONG";
        } else if (su.side == setup_engine::SetupSide::kShort) {
            sd = "SHORT";
        }
        const char *cs = "NONE";
        if (su.setup_class == setup_engine::SetupClass::kCandidate) {
            cs = "CAND";
        } else if (su.setup_class == setup_engine::SetupClass::kHighQualityCandidate) {
            cs = "HQ";
        }
        ESP_LOGI(TAG_SOAK, "setup side=%s class=%s q=%u reg_ok=%d lvl=%s", sd, cs,
                 static_cast<unsigned>(su.quality_score), su.regime_ok ? 1 : 0, su.level_name);
    }
}

void poll_trigger(const trigger_engine::TriggerSnapshot &t) {
    if (t.valid && t.state != trigger_engine::TriggerState::kNone && !s_first_tr) {
        s_first_tr = true;
        ESP_LOGI(TAG_SOAK, "first_valid trigger");
    }

    if (t.state == trigger_engine::TriggerState::kTriggered && s_prev_tr_st != trigger_engine::TriggerState::kTriggered) {
        ++s_soak_rt.triggers;
    }
    if (t.state == trigger_engine::TriggerState::kInvalidated &&
        s_prev_tr_st != trigger_engine::TriggerState::kInvalidated) {
        ++s_soak_rt.invalidations;
    }

    if (t.state != s_prev_tr_st || t.side != s_prev_tr_side || t.valid != s_prev_tr_valid) {
        s_prev_tr_st = t.state;
        s_prev_tr_side = t.side;
        s_prev_tr_valid = t.valid;
    }
}

void check_stale(const candle_engine::MarketAnalyticsSnapshot &a, const regime_engine::RegimeSnapshot &rg,
                 const level_engine::LevelSnapshot &ls, const setup_engine::SetupSnapshot &su,
                 const trigger_engine::TriggerSnapshot &tr) {
    const bool ws_live = market_ws::is_live();
    const uint32_t idle = market_ws::idle_since_last_rx_ms();

    if (ws_live && idle > kWsStaleRxMs) {
        if (!s_latch_ws_stale) {
            s_latch_ws_stale = true;
            ESP_LOGW(TAG_SOAK, "ws_stale_detected last_rx=%" PRIu32 "ms while state=LIVE", idle);
        }
    } else {
        s_latch_ws_stale = false;
    }

    const int64_t now_us = esp_timer_get_time();
    if (ws_live && a.closed_1m_count >= 2 && s_last_c1m_progress_us > 0 &&
        (now_us - s_last_c1m_progress_us) > static_cast<int64_t>(kCandleProgressStaleSec) * 1000000LL) {
        if (!s_latch_candle_stale) {
            s_latch_candle_stale = true;
            const int stale_s = static_cast<int>((now_us - s_last_c1m_progress_us) / 1000000LL);
            ESP_LOGW(TAG_SOAK, "candle_progress_stale no_new_closed_1m_for=%ds", stale_s);
        }
    } else {
        s_latch_candle_stale = false;
    }

    if (!ws_live || a.closed_1m_count < 2) {
        s_latch_ctx_stale = false;
        return;
    }

    const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    const auto lag = [&](uint64_t layer_ts) -> uint64_t {
        if (layer_ts == 0 || now_ms <= layer_ts) {
            return 0;
        }
        return now_ms - layer_ts;
    };

    bool bad = false;
    if (rg.valid && lag(rg.ts_ms) > kContextLagMs) {
        bad = true;
    }
    if (ls.valid && lag(ls.ts_ms) > kContextLagMs) {
        bad = true;
    }
    if (su.valid && su.setup_class != setup_engine::SetupClass::kNone && lag(su.ts_ms) > kContextLagMs) {
        bad = true;
    }
    if (tr.valid && tr.state != trigger_engine::TriggerState::kNone && lag(tr.ts_ms) > kContextLagMs) {
        bad = true;
    }

    if (bad) {
        if (!s_latch_ctx_stale) {
            s_latch_ctx_stale = true;
            ESP_LOGW(TAG_SOAK,
                     "snapshot_stale ctx lag>%" PRIu32 "ms (rg=%llu lv=%llu su=%llu tr=%llu a=%llu)", kContextLagMs,
                     static_cast<unsigned long long>(lag(rg.ts_ms)), static_cast<unsigned long long>(lag(ls.ts_ms)),
                     static_cast<unsigned long long>(lag(su.ts_ms)), static_cast<unsigned long long>(lag(tr.ts_ms)),
                     static_cast<unsigned long long>(lag(a.ts_ms)));
        }
    } else {
        s_latch_ctx_stale = false;
    }
}

void soak_periodic_summary() {
    diagnostics::SoakHealthSnapshot h{};
    diagnostics::fill_soak_health_snapshot(&h);
    const market_store::IngestStats ing = market_store::get_ingest_stats();

    ESP_LOGI(TAG_SOAK,
             "up=%" PRIu64 "s heap=%" PRIu64 " minheap=%" PRIu64 " rx=%" PRIu64 " reconn=%" PRIu64 " err=%" PRIu32
             " last_rx=%" PRIu32 "ms c1m=%zu a1=%d a5=%d a30=%d reg=%d lvl=%d set=%d trg=%d "
             "rg_ch=%" PRIu32 " scand=%" PRIu32 " shq=%" PRIu32 " fire=%" PRIu32 " inv=%" PRIu32 " parse_fail=%" PRIu64,
             h.uptime_s, h.free_heap, h.min_free_heap, h.ws_rx_count, h.ws_reconnect_count, h.ws_error_count,
             h.ws_last_rx_age_ms, h.closed_1m_count, h.analytics_valid_1m ? 1 : 0, h.analytics_valid_5m ? 1 : 0,
             h.analytics_valid_30m ? 1 : 0, h.regime_valid ? 1 : 0, h.levels_valid ? 1 : 0, h.setup_valid ? 1 : 0,
             h.trigger_valid ? 1 : 0, s_soak_rt.regime_changes, s_soak_rt.setup_candidate_entries,
             s_soak_rt.setup_hq_entries, s_soak_rt.triggers, s_soak_rt.invalidations,
             static_cast<unsigned long long>(ing.failed));
}

void soak_timer_cb(void *arg) {
    (void)arg;
    ++s_tick;

    poll_wifi();
    poll_ws_transport();

    candle_engine::MarketAnalyticsSnapshot a{};
    if (!candle_engine::get_analytics_snapshot(&a)) {
        return;
    }
    note_c1m_progress(a.closed_1m_count);
    poll_first_valid_analytics(a);

    regime_engine::RegimeSnapshot rg{};
    level_engine::LevelSnapshot ls{};
    setup_engine::SetupSnapshot su{};
    trigger_engine::TriggerSnapshot tr{};
    (void)regime_engine::get_snapshot(&rg);
    (void)level_engine::get_snapshot(&ls);
    (void)setup_engine::get_snapshot(&su);
    (void)trigger_engine::get_snapshot(&tr);

    poll_regime(rg);
    poll_levels(ls);
    poll_setup(su);
    poll_trigger(tr);

    if (s_tick % 30 == 0) {
        check_stale(a, rg, ls, su, tr);
    }
    if (s_tick % 60 == 0) {
        soak_periodic_summary();
    }
}

}  // namespace

namespace diagnostics {

esp_err_t init() {
    ESP_LOGI(TAG_SOAK, "diagnostics init (soak observability)");
    return ESP_OK;
}

esp_err_t start() {
    if (s_started) {
        return ESP_OK;
    }
    s_started = true;

    const esp_timer_create_args_t args = {
        .callback = &soak_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "soak1s",
        .skip_unhandled_events = true,
    };
    esp_err_t err = esp_timer_create(&args, &s_soak_timer);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_timer_start_periodic(s_soak_timer, 1000000);
    if (err != ESP_OK) {
        esp_timer_delete(s_soak_timer);
        s_soak_timer = nullptr;
        return err;
    }

    ESP_LOGI(TAG_SOAK, "start 1s soak poll + 60s summary + 30s stale check");
    return ESP_OK;
}

void log_health_snapshot(const char *reason) {
    ESP_LOGI(TAG_SOAK, "health snapshot (%s)", reason ? reason : "?");
    const int64_t us = esp_timer_get_time();
    ESP_LOGI(TAG_SOAK, "uptime: %lld ms", static_cast<long long>(us / 1000));
    ESP_LOGI(TAG_SOAK, "heap: free=%zu, min_free=%zu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
}

void log_compact_status(const char *context) {
    ESP_LOGI(TAG_SOAK, "[%s] heap_free=%zu min_free=%zu", context ? context : "?",
             static_cast<size_t>(esp_get_free_heap_size()),
             static_cast<size_t>(esp_get_minimum_free_heap_size()));
}

void log_mws_transport(const char *ws_state, uint64_t rx_total, uint64_t data_events, uint64_t reconnect_cycles,
                       uint32_t error_count, uint32_t last_payload_len, uint32_t idle_since_rx_ms) {
    ESP_LOGD(TAG_SOAK,
             "legacy mws state=%s rx=%" PRIu64 " data_ev=%" PRIu64 " reconnect=%" PRIu64 " err=%" PRIu32
             " last_len=%" PRIu32 " idle_rx_ms=%" PRIu32,
             ws_state ? ws_state : "?", rx_total, data_events, reconnect_cycles, error_count, last_payload_len,
             idle_since_rx_ms);
}

void fill_soak_health_snapshot(SoakHealthSnapshot *out) {
    if (out == nullptr) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    out->uptime_s = static_cast<uint64_t>(esp_timer_get_time() / 1000000ULL);
    out->free_heap = esp_get_free_heap_size();
    out->min_free_heap = esp_get_minimum_free_heap_size();
    out->ws_rx_count = market_ws::rx_count();
    out->ws_reconnect_count = market_ws::reconnect_count();
    out->ws_error_count = market_ws::error_count();
    out->ws_last_rx_age_ms = market_ws::idle_since_last_rx_ms();
    out->ws_state_label = market_ws::state_to_string();

    candle_engine::MarketAnalyticsSnapshot a{};
    if (candle_engine::get_analytics_snapshot(&a)) {
        out->closed_1m_count = a.closed_1m_count;
        out->analytics_valid_1m = a.returns.valid_1m;
        out->analytics_valid_5m = a.returns.valid_5m;
        out->analytics_valid_30m = a.returns.valid_30m;
    }

    regime_engine::RegimeSnapshot rg{};
    if (regime_engine::get_snapshot(&rg)) {
        out->regime_valid = rg.valid;
    }
    level_engine::LevelSnapshot ls{};
    if (level_engine::get_snapshot(&ls)) {
        out->levels_valid = ls.valid;
    }
    setup_engine::SetupSnapshot su{};
    if (setup_engine::get_snapshot(&su)) {
        out->setup_valid = su.valid && su.setup_class != setup_engine::SetupClass::kNone;
    }
    trigger_engine::TriggerSnapshot tr{};
    if (trigger_engine::get_snapshot(&tr)) {
        out->trigger_valid = tr.valid && tr.state != trigger_engine::TriggerState::kNone;
    }

    const market_store::IngestStats ing = market_store::get_ingest_stats();
    out->market_parse_ok = ing.ticker_ok;
    out->market_parse_fail = ing.failed;
}

void get_soak_runtime_counters(SoakRuntimeCounters *out) {
    if (out == nullptr) {
        return;
    }
    *out = s_soak_rt;
}

size_t format_ws_health(char *buf, size_t buf_sz) {
    if (buf == nullptr || buf_sz == 0) {
        return 0;
    }
    const uint64_t rx = market_ws::rx_count();
    const uint32_t idle = market_ws::idle_since_last_rx_ms();
    const int n = std::snprintf(buf, buf_sz, "ws=%s rx=%" PRIu64 " idle_ms=%" PRIu32 " reconn=%" PRIu64,
                                market_ws::state_to_string(), rx, idle, market_ws::reconnect_count());
    if (n < 0) {
        buf[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

size_t format_analytics_health(char *buf, size_t buf_sz) {
    if (buf == nullptr || buf_sz == 0) {
        return 0;
    }
    candle_engine::MarketAnalyticsSnapshot a{};
    if (!candle_engine::get_analytics_snapshot(&a)) {
        buf[0] = '\0';
        return 0;
    }
    const int n =
        std::snprintf(buf, buf_sz, "c1m=%zu a1=%d a5=%d a30=%d", a.closed_1m_count, a.returns.valid_1m ? 1 : 0,
                      a.returns.valid_5m ? 1 : 0, a.returns.valid_30m ? 1 : 0);
    if (n < 0) {
        buf[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

size_t format_setup_health(char *buf, size_t buf_sz) {
    if (buf == nullptr || buf_sz == 0) {
        return 0;
    }
    setup_engine::SetupSnapshot su{};
    if (!setup_engine::get_snapshot(&su)) {
        buf[0] = '\0';
        return 0;
    }
    const char *sd = "NONE";
    if (su.side == setup_engine::SetupSide::kLong) {
        sd = "LONG";
    } else if (su.side == setup_engine::SetupSide::kShort) {
        sd = "SHORT";
    }
    const char *cs = "NONE";
    if (su.setup_class == setup_engine::SetupClass::kCandidate) {
        cs = "CAND";
    } else if (su.setup_class == setup_engine::SetupClass::kHighQualityCandidate) {
        cs = "HQ";
    }
    const int n =
        std::snprintf(buf, buf_sz, "setup v=%d side=%s cls=%s q=%u reg_ok=%d %s", su.valid ? 1 : 0, sd, cs,
                      static_cast<unsigned>(su.quality_score), su.regime_ok ? 1 : 0, su.level_name);
    if (n < 0) {
        buf[0] = '\0';
        return 0;
    }
    return static_cast<size_t>(n);
}

}  // namespace diagnostics
