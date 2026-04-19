idf_component_register(
    SRCS "alert_observability.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common freertos cjson
)
