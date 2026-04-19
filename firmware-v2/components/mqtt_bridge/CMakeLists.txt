idf_component_register(
    SRCS "mqtt_bridge.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_store mqtt mbedtls esp_event esp_netif net_runtime esp_common
)
