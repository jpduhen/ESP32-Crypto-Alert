idf_component_register(
    SRCS "domain_metrics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES market_data diagnostics esp_timer esp_common exchange_bitvavo
)
