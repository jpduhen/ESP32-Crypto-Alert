idf_component_register(
    SRCS "alert_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ntfy_client esp_common
)
