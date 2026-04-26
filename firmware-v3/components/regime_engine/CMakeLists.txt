idf_component_register(
    SRCS "regime_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_event candle_engine event_bus freertos
)
