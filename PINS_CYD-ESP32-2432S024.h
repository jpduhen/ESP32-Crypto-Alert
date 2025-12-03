// PINS_CYD-ESP32-2432S024.h
// Pin definitions for Cheap Yellow Display (CYD) ESP32-2432S024
// 2.4 inch 240x320 TFT display with ILI9341 controller
// Based on Setup901_CYD_2432S024R.h pin definitions

#if defined(ESP32)

#include <Arduino_GFX_Library.h>

// Pin definitions - based on Setup901_CYD_2432S024R.h
#define TFT_CS 15              // Chip Select (TFT_CS)
#define TFT_DC 2               // Data/Command (TFT_DC)
#define TFT_RST -1             // Reset (TFT_RST = -1 means no reset pin)
#define GFX_BL 27              // Backlight pin (TFT_BL) - differs from 2.8 inch version (was 21)


// SPI pins
#define TFT_MOSI 13            // MOSI (TFT_MOSI)
#define TFT_MISO 12            // MISO (TFT_MISO)
#define TFT_SCLK 14            // Clock (TFT_SCLK)

// Display dimensions for 2.4 inch 240x320 TFT
#define GFX_WIDTH 240
#define GFX_HEIGHT 320

// SPI frequency for ILI9341 display (40 MHz - based on Setup901_CYD_2432S024R.h)
#define GFX_SPEED 40000000UL

// Create SPI bus instance using HSPI (as defined in Setup902_CYD28R_2USB.h)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, HSPI /* spi_num */);

// Create display instance (ILI9341)
// Rotation 0 = portrait, false = not IPS
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 0 /* rotation */, false /* IPS */);

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

