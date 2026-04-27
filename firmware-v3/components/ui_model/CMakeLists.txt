idf_component_register(
    SRCS "ui_model.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        esp_timer
        wifi_manager
        market_ws
        market_store
        candle_engine
        regime_engine
        level_engine
        setup_engine
        trigger_engine
        alert_engine
        app_version
)
