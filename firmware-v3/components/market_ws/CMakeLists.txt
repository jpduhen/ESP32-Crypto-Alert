idf_component_register(
    SRCS "market_ws.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common freertos esp_websocket_client esp_timer mbedtls wifi_manager market_store
)
