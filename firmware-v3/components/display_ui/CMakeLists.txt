idf_component_register(
    SRCS "display_ui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ui_model board_support esp_common esp_lcd freertos heap
)
