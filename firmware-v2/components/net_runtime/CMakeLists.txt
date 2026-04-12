idf_component_register(
    SRCS "net_runtime.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif esp_wifi esp_event
)
