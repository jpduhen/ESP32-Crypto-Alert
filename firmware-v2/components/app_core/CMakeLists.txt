idf_component_register(
    SRCS "app_core.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        config_store
        diagnostics
        bsp_common
        bsp_s3_geek
        display_port
        ui
        market_data
        market_types
        net_runtime
        service_outbound
        webui
        wifi_onboarding
        freertos
        esp_common
)
