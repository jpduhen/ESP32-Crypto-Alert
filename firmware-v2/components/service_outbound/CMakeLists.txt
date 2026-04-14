idf_component_register(
    SRCS "service_outbound.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common freertos mqtt_bridge ntfy_client
)
