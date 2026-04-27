idf_component_register(
    SRCS "board_support.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_common driver esp_lcd esp_driver_spi
)
