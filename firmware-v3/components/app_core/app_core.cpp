#include "app_core/app_core.hpp"

#include "alert_engine/alert_engine.hpp"
#include "board_support/board_support.hpp"
#include "candle_engine/candle_engine.hpp"
#include "diagnostics/diagnostics.hpp"
#include "display_ui/display_ui.hpp"
#include "event_bus/event_bus.hpp"
#include "level_engine/level_engine.hpp"
#include "market_store/market_store.hpp"
#include "market_ws/market_ws.hpp"
#include "ntfy_client/ntfy_client.hpp"
#include "regime_engine/regime_engine.hpp"
#include "setup_engine/setup_engine.hpp"
#include "settings_store/settings_store.hpp"
#include "trigger_engine/trigger_engine.hpp"
#include "strategy_engine/strategy_engine.hpp"
#include "ui_model/ui_model.hpp"
#include "wifi_manager/wifi_manager.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP";

namespace app_core {

esp_err_t start() {
    ESP_LOGI(TAG, "boot: subsystemen starten");

    ESP_RETURN_ON_ERROR(board_support::init(), TAG, "board_support");
    ESP_RETURN_ON_ERROR(diagnostics::init(), TAG, "diagnostics");
    ESP_RETURN_ON_ERROR(settings_store::init(), TAG, "settings_store");
    ESP_RETURN_ON_ERROR(event_bus::init(), TAG, "event_bus");
    ESP_RETURN_ON_ERROR(wifi_manager::init(), TAG, "wifi_manager");
    ESP_RETURN_ON_ERROR(market_store::init(), TAG, "market_store");
    ESP_RETURN_ON_ERROR(candle_engine::init(), TAG, "candle_engine");
    ESP_RETURN_ON_ERROR(level_engine::init(), TAG, "level_engine");
    ESP_RETURN_ON_ERROR(regime_engine::init(), TAG, "regime_engine");
    ESP_RETURN_ON_ERROR(setup_engine::init(), TAG, "setup_engine");
    ESP_RETURN_ON_ERROR(trigger_engine::init(), TAG, "trigger_engine");
    ESP_RETURN_ON_ERROR(market_ws::init(), TAG, "market_ws");
    ESP_RETURN_ON_ERROR(ui_model::init(), TAG, "ui_model");
    ESP_RETURN_ON_ERROR(display_ui::init(), TAG, "display_ui");
    ESP_RETURN_ON_ERROR(strategy_engine::init(), TAG, "strategy_engine");
    ESP_RETURN_ON_ERROR(ntfy_client::init(), TAG, "ntfy_client");
    ESP_RETURN_ON_ERROR(alert_engine::init(), TAG, "alert_engine");

    ESP_RETURN_ON_ERROR(diagnostics::start(), TAG, "diagnostics start");

    {
        settings_store::WifiSettingsResult wr{};
        const esp_err_t wr_err = settings_store::resolve_wifi_settings(&wr);
        if (wr_err != ESP_OK) {
            ESP_LOGE(TAG, "resolve_wifi_settings failed: %s", esp_err_to_name(wr_err));
            return wr_err;
        }
        ESP_LOGI(TAG, "WiFi config: source=%s valid=%d", settings_store::wifi_settings_source_label(wr.source),
                 wr.settings.valid ? 1 : 0);
        if (!wr.settings.valid) {
            ESP_LOGW(TAG, "WiFi STA wordt niet gestart (geen geldige instellingen)");
        }
    }

    ESP_RETURN_ON_ERROR(wifi_manager::start(), TAG, "wifi_manager start");
    if (wifi_manager::get_state() == wifi_manager::WifiState::kNotConfigured) {
        ESP_LOGW(TAG, "WiFi overgeslagen: NotConfigured (zie SETTINGS/WIFI logs hierboven)");
    } else {
        ESP_LOGI(TAG, "WiFi driver state na start: %s", wifi_manager::state_label(wifi_manager::get_state()));
    }

    // Korte poll: na wifi_manager::start() is GotIp vaak iets later; geen WS starten zonder IP.
    if (wifi_manager::get_state() != wifi_manager::WifiState::kNotConfigured &&
        wifi_manager::get_state() != wifi_manager::WifiState::kError) {
        int waits = 0;
        constexpr int kMaxWaits = 80;  // 80 * 250 ms = 20 s max
        while (waits < kMaxWaits && wifi_manager::get_state() != wifi_manager::WifiState::kGotIp) {
            const auto st = wifi_manager::get_state();
            if (st == wifi_manager::WifiState::kError || st == wifi_manager::WifiState::kNotConfigured) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
            ++waits;
        }
        if (wifi_manager::get_state() == wifi_manager::WifiState::kGotIp) {
            ESP_LOGI(TAG, "WiFi GotIp na wachten (%d * 250 ms)", waits);
        }
    }

    if (wifi_manager::get_state() == wifi_manager::WifiState::kGotIp) {
        ESP_RETURN_ON_ERROR(market_ws::start(), TAG, "market_ws start");
        ESP_LOGI(TAG, "market_ws gestart (na GotIp), is_live volgt na eerste ticker");
    } else if (wifi_manager::get_state() == wifi_manager::WifiState::kNotConfigured) {
        ESP_LOGW(TAG, "market_ws: overgeslagen (geen WiFi-config)");
    } else {
        ESP_LOGW(TAG, "market_ws: overgeslagen tot IP (nu %s)", wifi_manager::state_label(wifi_manager::get_state()));
    }
    ESP_RETURN_ON_ERROR(strategy_engine::start(), TAG, "strategy_engine start (stub)");
    ESP_RETURN_ON_ERROR(alert_engine::start(), TAG, "alert_engine start (stub)");

    ESP_LOGI(TAG, "app_core klaar");
    return ESP_OK;
}

}  // namespace app_core
