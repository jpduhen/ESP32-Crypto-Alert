#include "market_data/types.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"
#include <cstring>

namespace market_data {

static MarketSnapshot s_snap;
static uint32_t s_gen = 0;

esp_err_t mock_init()
{
    s_snap.connection = ConnectionState::Connected;
    s_snap.valid = true;
    s_snap.last_error = FeedErrorCode::None;
    s_snap.rest_bootstrap_ok = 0;
    s_snap.ws_reconnect_count = 0;
    s_snap.last_error_detail[0] = '\0';
    strncpy(s_snap.market_label, "MOCK", sizeof(s_snap.market_label) - 1);
    s_snap.last_tick_source = TickSource::None;
    s_snap.last_tick = {42000.0, 0};
    ESP_LOGI(DIAG_TAG_MARKET, "mock_init: dummy price %.2f EUR", s_snap.last_tick.price_eur);
    return ESP_OK;
}

void mock_tick()
{
    ++s_gen;
    s_snap.last_tick.ts_ms = static_cast<int64_t>(s_gen) * 1000;
    /* Minimale variatie om “feed” zichtbaar in logs. */
    s_snap.last_tick.price_eur = 42000.0 + (s_gen % 7) * 0.25;
}

MarketSnapshot mock_snapshot()
{
    return s_snap;
}

} // namespace market_data
