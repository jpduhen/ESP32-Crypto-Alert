idf_component_register(
    SRCS "app_core.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        alert_engine
        board_support
        candle_engine
        diagnostics
        display_ui
        event_bus
        level_engine
        market_store
        market_ws
        ntfy_client
        regime_engine
        settings_store
        strategy_engine
        ui_model
        wifi_manager
        esp_common
        freertos
)
