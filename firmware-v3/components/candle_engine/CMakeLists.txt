idf_component_register(
    SRCS "candle_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common freertos esp_event esp_timer event_bus market_store
)
