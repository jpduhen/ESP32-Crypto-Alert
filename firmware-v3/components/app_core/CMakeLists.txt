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
        market_store
        market_ws
        ntfy_client
        settings_store
        strategy_engine
        ui_model
        wifi_manager
        esp_common
        freertos
)
