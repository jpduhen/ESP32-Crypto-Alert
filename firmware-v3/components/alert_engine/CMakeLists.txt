idf_component_register(
    SRCS "alert_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common esp_timer freertos setup_engine trigger_engine level_engine market_store regime_engine ntfy_client
)
