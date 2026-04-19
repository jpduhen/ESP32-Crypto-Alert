idf_component_register(
    SRCS "bsp_s3_geek.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bsp_common diagnostics driver esp_common
)
