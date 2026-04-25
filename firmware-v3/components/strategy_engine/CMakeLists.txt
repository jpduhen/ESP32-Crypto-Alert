idf_component_register(
    SRCS "strategy_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        level_engine
        regime_engine
        signal_engine
        esp_common
)
