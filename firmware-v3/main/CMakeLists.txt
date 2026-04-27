idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        app_core
        app_version
        nvs_flash
        esp_netif
        esp_event
)
