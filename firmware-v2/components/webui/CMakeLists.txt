idf_component_register(
    SRCS "webui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES alert_engine alert_observability config_store domain_metrics esp_app_format esp_http_server esp_netif cjson freertos lwip market_data net_runtime ota_service service_outbound esp_common
)
