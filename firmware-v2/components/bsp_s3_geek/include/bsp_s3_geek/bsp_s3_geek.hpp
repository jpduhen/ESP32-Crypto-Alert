#pragma once

#include "bsp_common/board_types.hpp"
#include "esp_err.h"

namespace bsp_s3_geek {

/**
 * Spotpear ESP32-S3 GEEK — 1.14" ST7789, 135×240 (portrait gebruik in V1).
 * Pinmapping: `geek_pins.hpp` (consistent met `PINS_ESP32S3_GEEK_ST7789_114.h`).
 */
esp_err_t init();

/** Active-high backlight (GPIO7). Alleen na display-init aanzetten. */
esp_err_t backlight_set(bool on);

const bsp_common::BoardDescriptor &board_descriptor();

} // namespace bsp_s3_geek
