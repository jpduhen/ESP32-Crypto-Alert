// PINS_ESP32S3_GEEK_ST7789_114.h
// Pin definitions for Spotpear ESP32-S3R2 GEEK Development Board (1.14" LCD, ST7789, 240x135)
// Source: Spotpear/Waveshare schematic and official documentation
// Reference: https://spotpear.com/wiki/ESP32-S3R2-GEEK-Development-Board-1.14inch-LCD-ST7789-USB-Dongle.html
//
// Notes
// - Display is SPI (4-wire) with ST7789 controller
// - 1.14 inch 240×135 pixels 65K color IPS LCD
// - MISO is not required for ST7789 reads in most Arduino_GFX setups; set to -1 unless you need it.
// - Backlight is controlled via a GPIO (GFX_BL). Set HIGH to enable.
//
// Pin mapping according to official documentation:
// - CS (Chip Select): GPIO10 (some sources say GPIO11, verify with schematic)
// - SDA/MOSI: GPIO11 (some sources say GPIO7, verify with schematic)
// - SCL/SCLK: GPIO12
// - DC (Data/Command): GPIO8
// - Reset: GPIO9
// - Backlight: GPIO7 (some sources say GPIO10, verify with schematic)
//
// NOTE: If display doesn't work, verify pin numbers against actual board schematic
// Different board revisions may have different pin assignments

#pragma once

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// Pin definitions - Spotpear ESP32-S3R2 GEEK Development Board
// Based on official schematic (section D5-D6)
// LCD Interface GPIO mappings from schematic:
//   LCD DC: GPIO8, LCD CS: GPIO10, LCD CLK: GPIO12, LCD MOSI: GPIO11, LCD RST: GPIO9, LCD BL: GPIO7
#define TFT_CS 10              // Chip Select (LCD_CS) - GPIO10 per schematic
#define TFT_DC 8               // Data/Command (LCD_DC) - GPIO8 per schematic
#define TFT_RST 9              // Reset (LCD_RST) - GPIO9 per schematic
#define GFX_BL 7               // Backlight pin (LCD_BL) - GPIO7 per schematic

// SPI pins
#define TFT_MOSI 11            // MOSI (LCD_MOSI) - GPIO11 per schematic
#define TFT_MISO -1            // MISO (not used for ST7789)
#define TFT_SCLK 12            // Clock (LCD_CLK) - GPIO12 per schematic

// Display dimensions for 1.14 inch 240x135 TFT (portrait: 135x240)
#define GFX_WIDTH 135
#define GFX_HEIGHT 240

// SPI frequency for ST7789 display (27 MHz - safe for ST7789)
#define GFX_SPEED 27000000UL

// Create SPI bus instance
// ESP32-S3 uses FSPI (SPI2) = 0 or HSPI (SPI3) = 1
// We use FSPI (SPI2) = 0 for ESP32-S3 (default SPI bus)
#if defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
// ESP32-S3: Use FSPI (SPI2) - numeric value 0 (default SPI bus for ESP32-S3)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, 0 /* spi_num = FSPI/SPI2 */);
#else
// ESP32: Use VSPI (fallback, maar GEEK is ESP32-S3)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, VSPI /* spi_num */);
#endif

// Create display instance (ST7789)
// Rotation 0 = portrait, true = IPS
// ST7789 voor GEEK: 135x240, col offset 52, row offset 40 (vergelijkbaar met TTGO)
// Pas COL_OFFSET_2 aan om ruis aan rechterkant bij rotatie 2 weg te werken
#ifndef GEEK_COL_OFFSET_2
#define GEEK_COL_OFFSET_2 53
#endif
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0 /* rotation */, true /* IPS */, 
                                       135 /* width */, 240 /* height */, 
                                       52 /* col offset 1 */, 40 /* row offset 1 */, 
                                       GEEK_COL_OFFSET_2 /* col offset 2 */, 40 /* row offset 2 */);

// Device initialization function
// Sets up backlight pin
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

// -------------------------
// Optional microSD (SPI) pins
// -------------------------
// The board exposes a microSD footprint/connector on some variants.
// If you do not use SD, you can ignore these.
#define SD_SCK   36   // SD_SCK
#define SD_MOSI  35   // SD_MOSI
#define SD_MISO  37   // SD_MISO
#define SD_CS    33   // SD_CS

// Extra SDIO pins present on the schematic (only relevant for SDIO-mode designs)
#define SDIO_D0  38
#define SDIO_D3  34

#else
#error "This pin configuration is for ESP32 / ESP32-S3 targets only"
#endif
