# ESP32 Crypto Alert — V2 firmware (ESP-IDF)
cmake_minimum_required(VERSION 3.16)

# SemVer voor `esp_app_desc` / GET /api/status.json (`version`). Minor 2.2.0: alert-cadans, domain_metrics wandklok, vol/regime, WS-inbound teller.
set(PROJECT_VER "2.2.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v2)
