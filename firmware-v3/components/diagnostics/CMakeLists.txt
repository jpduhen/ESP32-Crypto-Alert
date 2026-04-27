idf_component_register(
    SRCS "diagnostics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        freertos
        esp_common
        wifi_manager
        market_ws
        market_store
        candle_engine
        regime_engine
        level_engine
        setup_engine
        trigger_engine
        alert_engine
)
