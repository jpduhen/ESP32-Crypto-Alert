idf_component_register(
    SRCS "webui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_store esp_app_format esp_http_server esp_netif json lwip market_data net_runtime ota_service esp_common
)
