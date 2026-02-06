// PINS_ESP32S3_LCDWIKI_28_ILI9341.h
// Pin definitions for LCD wiki 2.8" ESP32-S3 Display (ILI9341, 240x320)
// Source: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display

#pragma once

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// Pin definitions from LCD wiki
#define TFT_CS 10              // LCD CS (IO10)
#define TFT_DC 46              // LCD DC (IO46)
#define TFT_RST -1             // LCD RST shared with ESP32-S3 reset (no dedicated pin)
#define GFX_BL 45              // LCD backlight (IO45)

// SPI pins
#define TFT_MOSI 11            // LCD MOSI (IO11)
#define TFT_MISO 13            // LCD MISO (IO13)
#define TFT_SCLK 12            // LCD SCLK (IO12)

// Display dimensions for 2.8 inch 240x320 TFT
#define GFX_WIDTH 240
#define GFX_HEIGHT 320

// SPI frequency for ILI9341 display
#define GFX_SPEED 40000000UL

// Create SPI bus instance
// ESP32-S3: Use FSPI (SPI2) = 0
#if defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, 0 /* spi_num = FSPI/SPI2 */);
#else
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, VSPI /* spi_num */);
#endif

// Create display instance (ILI9341)
// Rotation 0 = portrait
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 0 /* rotation */, false /* IPS */);

// LCDWIKI 2.8" kleureninversie (aanbevolen)
#define PLATFORM_LCDWIKI28_INVERT_COLORS

// Device initialization function
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

#else
#error "This pin configuration is for ESP32 / ESP32-S3 targets only"
#endif
