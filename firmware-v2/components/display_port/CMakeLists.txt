idf_component_register(
    SRCS "display_port.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bsp_s3_geek diagnostics esp_lcd
)

# Optioneel fabriekstest: RGBK-flash + hoekmarkers — zet aan met:
# target_compile_definitions(${COMPONENT_LIB} PRIVATE DISPLAY_PORT_RGBK_DIAG=1)
