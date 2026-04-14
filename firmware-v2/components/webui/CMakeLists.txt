idf_component_register(
    SRCS "webui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES config_store esp_app_format esp_http_server esp_netif lwip market_data net_runtime esp_common
)
