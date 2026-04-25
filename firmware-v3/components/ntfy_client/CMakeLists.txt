idf_component_register(
    SRCS "ntfy_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_common
)
