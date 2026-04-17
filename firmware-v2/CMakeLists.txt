# ESP32 Crypto Alert — V2 firmware (ESP-IDF)
cmake_minimum_required(VERSION 3.16)

# SemVer voor `esp_app_desc` / GET /api/status.json (`version`). Patch bump: M-013k confluence-policy POST.
set(PROJECT_VER "2.1.1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v2)
