idf_component_register(
    SRCS "level_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_event event_bus candle_engine freertos
)
