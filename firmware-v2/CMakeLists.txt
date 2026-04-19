# ESP32 Crypto Alert — V2 firmware (ESP-IDF)
cmake_minimum_required(VERSION 3.16)

# SemVer voor `esp_app_desc` / GET /api/status.json (`version`). 3.0.0: S2H-3 2h outbound + observability (30m+2h verticale slices compleet).
set(PROJECT_VER "3.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v2)
