idf_component_register(
    SRCS "market_data.cpp" "mock_provider.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        market_types
        diagnostics
        config_store
        esp_common
        exchange_bitvavo
        net_runtime
)
