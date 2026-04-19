# ESP32 Crypto Alert — V2 firmware (ESP-IDF)
cmake_minimum_required(VERSION 3.16)

# SemVer voor `esp_app_desc` / GET /api/status.json (`version`). Minor 2.3.0: C3 mini-regime (‰ raw/clamp observability, werkdocument).
set(PROJECT_VER "2.3.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v2)
