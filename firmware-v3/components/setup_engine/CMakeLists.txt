idf_component_register(
    SRCS "setup_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_event freertos event_bus candle_engine regime_engine level_engine market_store
)
