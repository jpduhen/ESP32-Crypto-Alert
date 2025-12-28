// PINS_TTGO_T_Display.h
// Pin definitions for TTGO T-Display ESP32
// 1.14 inch 135x240 TFT display with ST7789 controller
// Based on Setup900_TTGO_T_Display.h pin definitions

#ifndef PINS_TTGO_T_DISPLAY_H
#define PINS_TTGO_T_DISPLAY_H

#if defined(ESP32)

#include <Arduino_GFX_Library.h>

// Pin definitions - based on Setup900_TTGO_T_Display.h
#define TFT_CS 5               // Chip Select (TFT_CS)
#define TFT_DC 16              // Data/Command (TFT_DC)
#define TFT_RST 23             // Reset (TFT_RST)
#define GFX_BL 4               // Backlight pin (TFT_BL)

// SPI pins
#define TFT_MOSI 19            // MOSI (TFT_MOSI)
#define TFT_MISO -1            // MISO (not used for ST7789)
#define TFT_SCLK 18            // Clock (TFT_SCLK)

// Display dimensions for 1.14 inch 135x240 TFT (portrait)
#define GFX_WIDTH 135
#define GFX_HEIGHT 240

// SPI frequency for ST7789 display (27 MHz - safe for ST7789, based on Setup900_TTGO_T_Display.h)
#define GFX_SPEED 27000000UL

// Create SPI bus instance using VSPI
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, VSPI /* spi_num */);

// Create display instance (ST7789)
// Rotation 0 = portrait, true = IPS
// ST7789 voor TTGO T-Display: 135x240, col offset 52, row offset 40
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0 /* rotation */, true /* IPS */, 
                                       135 /* width */, 240 /* height */, 
                                       52 /* col offset 1 */, 40 /* row offset 1 */, 
                                       52 /* col offset 2 */, 40 /* row offset 2 */);

// Device initialization function
// Sets up backlight pin
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

#else
#error "This pin configuration is for ESP32 only"
#endif

#endif // PINS_TTGO_T_DISPLAY_H

