idf_component_register(
    SRCS "ui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES display_port diagnostics esp_common market_data esp_lvgl_port
)
