// PINS_ESP32S3_SuperMini_ST7789_154.h
// Pin definitions for ESP32-S3 Super Mini HW-747 v0.0.2i + 1.54" ST7789 SPI TFT
// 1.54 inch 240x240 TFT display with ST7789 controller
// Based on Setup701_ESP32S3_SuperMini_ST7789_154.h pin definitions

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// Pin definitions - based on Setup701_ESP32S3_SuperMini_ST7789_154.h
#define TFT_CS 9                // Chip Select (TFT_CS)
#define TFT_DC 10               // Data/Command (TFT_DC)
#define TFT_RST 11              // Reset (TFT_RST) - or -1 if RST is tied to EN/3V3
#define GFX_BL 8                // Backlight pin (TFT_BL)

// SPI pins (SCL/SDA labels on display = SCK/MOSI)
#define TFT_MOSI 12             // MOSI (SDA on display)
#define TFT_MISO -1             // MISO (not used for ST7789)
#define TFT_SCLK 13             // Clock (SCL on display)

// Display dimensions for 1.54 inch 240x240 TFT
#define GFX_WIDTH 240
#define GFX_HEIGHT 240

// SPI frequency for ST7789 display (27 MHz - verlaagd voor stabiliteit op ESP32-S3)
// 40 MHz kan problemen veroorzaken, 27 MHz is veiliger
#define GFX_SPEED 27000000UL

// SPI read frequency (usually not used for ST7789 without MISO)
#define SPI_READ_FREQUENCY 20000000UL

// Create SPI bus instance
// ESP32 uses VSPI, ESP32-S3 uses FSPI (SPI2) or HSPI (SPI3)
// For ESP32-S3: FSPI (SPI2) = 0, HSPI (SPI3) = 1 (NOT 2 and 3!)
// We use FSPI (SPI2) = 0 for ESP32-S3 (default SPI bus)
// Check for ESP32-S3 using ARDUINO_ESP32S3_DEV or CONFIG_IDF_TARGET_ESP32S3
#if defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
// ESP32-S3: Use FSPI (SPI2) - numeric value 0 (default SPI bus for ESP32-S3)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, 0 /* spi_num = FSPI/SPI2 */);
#else
// ESP32: Use VSPI
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO, VSPI /* spi_num */);
#endif

// Create display instance (ST7789)
// Rotation 0 = portrait, true = IPS
// ST7789 voor 1.54" 240x240: typically no offset needed, but may vary by display
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0 /* rotation */, true /* IPS */, 
                                       240 /* width */, 240 /* height */, 
                                       0 /* col offset 1 */, 0 /* row offset 1 */, 
                                       0 /* col offset 2 */, 0 /* row offset 2 */);

// Device initialization function
// Sets up backlight pin
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

// Optional: If colors are swapped (blue/red), uncomment the following:
// #define TFT_RGB_ORDER TFT_BGR

#else
#error "This pin configuration is for ESP32 or ESP32-S3 only"
#endif
