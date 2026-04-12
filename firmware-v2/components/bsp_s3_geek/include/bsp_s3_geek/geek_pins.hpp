#pragma once

/**
 * Pin- en timing-constanten ESP32-S3 GEEK 1.14" ST7789 (SPI).
 * Bron: `PINS_ESP32S3_GEEK_ST7789_114.h` (V1) / Spotpear schematic — bij afwijkende revisie aanpassen.
 */
namespace bsp_s3_geek::pins {

inline constexpr int MOSI = 11;
inline constexpr int SCLK = 12;
inline constexpr int CS = 10;
inline constexpr int DC = 8;
inline constexpr int RST = 9;
inline constexpr int BACKLIGHT = 7;

/**
 * Panel logische resolutie (portrait) — gelijk V1 `GFX_WIDTH`/`GFX_HEIGHT`.
 * `esp_lcd_panel_draw_bitmap`: volledig venster (0,0)–(WIDTH,HEIGHT) met **swap_xy uit** (zoals V1 rotatie 0).
 */
inline constexpr int WIDTH = 135;
inline constexpr int HEIGHT = 240;

/** SPI klok (Hz) — conservatief t.o.v. ST7789; gelijk aan V1 GFX_SPEED. */
inline constexpr int SPI_PIXEL_CLOCK_HZ = 27 * 1000 * 1000;

/**
 * ST7789 1.14" vensteroffsets in RAM (zelfde als V1 Arduino_ST7789 init).
 * Fijnregeling kan nodig zijn (randeffect / rotatie).
 */
inline constexpr int ST7789_X_GAP = 52;
inline constexpr int ST7789_Y_GAP = 40;

} // namespace bsp_s3_geek::pins
