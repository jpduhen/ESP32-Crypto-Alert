# Crypto-Alert V3 — ESP-IDF (schone basis, WS-first roadmap)
cmake_minimum_required(VERSION 3.16)

# Standaard chip: ESP32-S3-GEEK. Overschrijf met idf.py set-target of -DIDF_TARGET=...
if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32s3)
endif()

set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v3)
