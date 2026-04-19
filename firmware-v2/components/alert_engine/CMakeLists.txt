idf_component_register(
    SRCS "alert_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_store domain_metrics market_data service_outbound diagnostics esp_timer esp_common
)
