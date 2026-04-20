# ESP32 Crypto Alert — V2 firmware (ESP-IDF)
cmake_minimum_required(VERSION 3.16)

# SemVer voor `esp_app_desc` / GET /api/status.json (`version`). 3.1.0: RWS-05 ticker24h heartbeat/observability; domain_metrics RWS-04/04b; docs PR02/PR03/PR04.
set(PROJECT_VER "3.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_crypto_alert_v2)
