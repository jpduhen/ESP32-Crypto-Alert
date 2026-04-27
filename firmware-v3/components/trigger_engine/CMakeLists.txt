idf_component_register(
    SRCS "trigger_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_event freertos event_bus candle_engine setup_engine level_engine regime_engine market_store
)
