idf_component_register(
    SRCS "ota_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_http_server esp_partition spi_flash freertos esp_common
)
