idf_component_register(
    SRCS "service_outbound.cpp"
    INCLUDE_DIRS "include"
    REQUIRES alert_observability esp_common freertos mqtt_bridge ntfy_client
)
