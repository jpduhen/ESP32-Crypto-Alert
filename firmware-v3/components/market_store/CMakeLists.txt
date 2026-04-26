idf_component_register(
    SRCS "market_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common freertos esp_event esp_timer event_bus
)
