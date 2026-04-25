idf_component_register(
    SRCS "diagnostics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos esp_common
)
