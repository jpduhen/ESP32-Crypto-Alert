idf_component_register(
    SRCS "settings_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_common
)
