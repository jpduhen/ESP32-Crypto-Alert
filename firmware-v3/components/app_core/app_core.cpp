#include "app_core/app_core.hpp"

#include "alert_engine/alert_engine.hpp"
#include "board_support/board_support.hpp"
#include "candle_engine/candle_engine.hpp"
#include "diagnostics/diagnostics.hpp"
#include "display_ui/display_ui.hpp"
#include "event_bus/event_bus.hpp"
#include "market_store/market_store.hpp"
#include "market_ws/market_ws.hpp"
#include "ntfy_client/ntfy_client.hpp"
#include "settings_store/settings_store.hpp"
#include "strategy_engine/strategy_engine.hpp"
#include "ui_model/ui_model.hpp"
#include "wifi_manager/wifi_manager.hpp"

#include "esp_log.h"

static const char *TAG = "app_core";

namespace app_core {

esp_err_t start() {
    ESP_LOGI(TAG, "app_core: subsystemen starten");

    ESP_RETURN_ON_ERROR(board_support::init(), TAG, "board_support");
    ESP_RETURN_ON_ERROR(diagnostics::init(), TAG, "diagnostics");
    ESP_RETURN_ON_ERROR(settings_store::init(), TAG, "settings_store");
    ESP_RETURN_ON_ERROR(event_bus::init(), TAG, "event_bus");
    ESP_RETURN_ON_ERROR(wifi_manager::init(), TAG, "wifi_manager");
    ESP_RETURN_ON_ERROR(market_store::init(), TAG, "market_store");
    ESP_RETURN_ON_ERROR(candle_engine::init(), TAG, "candle_engine");
    ESP_RETURN_ON_ERROR(market_ws::init(), TAG, "market_ws");
    ESP_RETURN_ON_ERROR(ui_model::init(), TAG, "ui_model");
    ESP_RETURN_ON_ERROR(display_ui::init(), TAG, "display_ui");
    ESP_RETURN_ON_ERROR(strategy_engine::init(), TAG, "strategy_engine");
    ESP_RETURN_ON_ERROR(ntfy_client::init(), TAG, "ntfy_client");
    ESP_RETURN_ON_ERROR(alert_engine::init(), TAG, "alert_engine");

    ESP_RETURN_ON_ERROR(diagnostics::start(), TAG, "diagnostics start");
    ESP_RETURN_ON_ERROR(wifi_manager::start(), TAG, "wifi_manager start");
    ESP_RETURN_ON_ERROR(market_ws::start(), TAG, "market_ws start (stub)");
    ESP_RETURN_ON_ERROR(strategy_engine::start(), TAG, "strategy_engine start (stub)");
    ESP_RETURN_ON_ERROR(alert_engine::start(), TAG, "alert_engine start (stub)");

    ESP_LOGI(TAG, "app_core: klaar");
    return ESP_OK;
}

}  // namespace app_core
