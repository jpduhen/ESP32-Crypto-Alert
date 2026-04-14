/**
 * app_core — lifecycle-orchestratie alleen (M-002): geen exchange-URLs; marktdata via market_data::.
 * Outbound service-events: service_outbound:: (M-002c) — geen protocol in UI/display.
 */
#include "app_core/app_core.hpp"
#include "config_store/config_store.hpp"
#include "diagnostics/diagnostics.hpp"
#include "display_port/display_port.hpp"
#include "bsp_s3_geek/bsp_s3_geek.hpp"
#include "market_data/market_data.hpp"
#include "market_types/types.hpp"
#include "net_runtime/net_runtime.hpp"
#include "service_outbound/service_outbound.hpp"
#include "webui/webui.hpp"
#include "wifi_onboarding/wifi_onboarding.hpp"
#include "ui/ui.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <cmath>

namespace app_core {

static const char TAG[] = "app_core";

#if CONFIG_MD_USE_EXCHANGE_BITVAVO
/** T-103 field-test: bewijs dat WS-prijs via exchange → market_data::snapshot loopt. Begrensd: prijsverandering of ≥30 s. */
static void t103_field_log_ws_via_market_data()
{
    const market_data::MarketSnapshot snap = market_data::snapshot();
    if (!snap.valid || snap.last_tick_source != market_types::TickSource::Ws) {
        return;
    }
    const uint64_t now_ms = esp_timer_get_time() / 1000ULL;
    static double s_last_price = 0;
    static uint64_t s_last_log_ms = 0;
    const double p = snap.last_tick.price_eur;
    const bool changed = std::fabs(p - s_last_price) > 1e-6;
    const bool due = (now_ms - s_last_log_ms) >= 30000ULL;
    if (!changed && !due) {
        return;
    }
    s_last_price = p;
    s_last_log_ms = now_ms;
    const char *sym = snap.market_label[0] ? snap.market_label : "?";
    ESP_LOGI(DIAG_TAG_MARKET,
             "T-103 field: sym=%s price=%.2f EUR bron=WS | market_data::snapshot ok (ts_ms=%lld)",
             sym, p, (long long)snap.last_tick.ts_ms);
}
#endif

static esp_err_t lifecycle_startup(config_store::RuntimeConfig &cfg)
{
    ESP_RETURN_ON_ERROR(diagnostics::init_early(), TAG, "init_early");
    ESP_RETURN_ON_ERROR(config_store::init(), TAG, "config_store::init");
    ESP_RETURN_ON_ERROR(config_store::load_or_defaults(cfg), TAG, "load_or_defaults");
    ESP_RETURN_ON_ERROR(net_runtime::early_init(), TAG, "net_runtime::early_init");
    ESP_RETURN_ON_ERROR(wifi_onboarding::run(cfg), TAG, "wifi_onboarding::run");
    ESP_RETURN_ON_ERROR(bsp_s3_geek::init(), TAG, "bsp_s3_geek::init");
    ESP_RETURN_ON_ERROR(display_port::init(), TAG, "display_port::init");
    ESP_RETURN_ON_ERROR(ui::init(), TAG, "ui::init");
    {
        const esp_err_t w = net_runtime::start_sta(cfg.wifi_sta_ssid, cfg.wifi_sta_pass);
        if (w != ESP_OK) {
            ESP_LOGW(TAG, "WiFi STA niet gestart (%s) — exchange zonder IP; mock mogelijk",
                     esp_err_to_name(w));
        }
    }
    ESP_RETURN_ON_ERROR(market_data::init(cfg), TAG, "market_data::init");
    ESP_RETURN_ON_ERROR(service_outbound::init(), TAG, "service_outbound::init");
    ESP_RETURN_ON_ERROR(webui::init(), TAG, "webui::init");
    return ESP_OK;
}

esp_err_t run()
{
    config_store::RuntimeConfig cfg{};
    esp_err_t err = lifecycle_startup(cfg);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "config: default_symbol=%s", cfg.default_symbol);
    ESP_LOGI(TAG, "runtime: idle (market_data tick)");
    service_outbound::emit(service_outbound::Event::ApplicationReady);
    service_outbound::poll();
    for (;;) {
        market_data::tick();
#if CONFIG_MD_USE_EXCHANGE_BITVAVO
        t103_field_log_ws_via_market_data();
#endif
        ui::refresh_from_snapshot(market_data::snapshot());
        diagnostics::tick_heartbeat();
        service_outbound::poll();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

} // namespace app_core
