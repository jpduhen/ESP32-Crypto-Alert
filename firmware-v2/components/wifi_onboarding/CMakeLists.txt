idf_component_register(
    SRCS "wifi_onboarding.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_netif esp_wifi esp_event config_store net_runtime nvs_flash freertos esp_common
)
