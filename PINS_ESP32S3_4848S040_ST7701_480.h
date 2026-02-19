// PINS_ESP32S3_4848S040_ST7701_480.h
// Pin definitions for GUITION ESP32-S3-4848S040 Development Board (4" LCD, ST7701, 480x480)
// Display: RGB parallel interface with ST7701 controller (SPI used for init/control)
//
// Notes:
// - No MISO required; set to GFX_NOT_DEFINED.
// - Backlight controlled via GPIO (GFX_BL). Set HIGH to enable.
// - No dedicated reset pin; use GFX_NOT_DEFINED for RST.
// - For correct colors, the ST7701 init sequence might need tweaks:
//   Change 0xCD -> 0x00 and use 0x20 instead of 0x21 in st7701_type1_init_operations.

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// Custom ST7701 init sequence for 4848S040 panel
// - Disable MDT flag: 0xCD -> 0x00
// - Use normal mode: 0x20 (not 0x21)
// - Skip 0x3A pixel format (default 18-bit works best on this panel)
static const uint8_t st7701_4848s040_init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02,
    WRITE_C8_D16, 0xC2, 0x31, 0x05,
    WRITE_C8_D8, 0xCD, 0x00,

    WRITE_COMMAND_8, 0xB0, // Positive Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x18, 0x0E,
    0x11, 0x06, 0x07, 0x08,
    0x07, 0x22, 0x04, 0x12,
    0x0F, 0xAA, 0x31, 0x18,

    WRITE_COMMAND_8, 0xB1, // Negative Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x19, 0x0E,
    0x12, 0x07, 0x08, 0x08,
    0x08, 0x22, 0x04, 0x11,
    0x11, 0xA9, 0x32, 0x18,

    // PAGE1
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE_C8_D8, 0xB0, 0x60, // Vop=4.7375v
    WRITE_C8_D8, 0xB1, 0x32, // VCOM=32
    WRITE_C8_D8, 0xB2, 0x07, // VGH=15v
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x49, // VGL=-10.17v
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21, // AVDD=6.6 & AVCL=-4.6
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,

    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x1B, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11,
    0x08, 0xA0, 0x00, 0x00,
    0x07, 0xA0, 0x00, 0x00,
    0x00, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 12,
    0x11, 0x11, 0x44, 0x44,
    0xED, 0xA0, 0x00, 0x00,
    0xEC, 0xA0, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE4, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16,
    0x0A, 0xE9, 0xD8, 0xA0,
    0x0C, 0xEB, 0xD8, 0xA0,
    0x0E, 0xED, 0xD8, 0xA0,
    0x10, 0xEF, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE7, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16,
    0x09, 0xE8, 0xD8, 0xA0,
    0x0B, 0xEA, 0xD8, 0xA0,
    0x0D, 0xEC, 0xD8, 0xA0,
    0x0F, 0xEE, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7,
    0x02, 0x00, 0xE4, 0xE4,
    0x88, 0x00, 0x40,

    WRITE_C8_D16, 0xEC, 0x3C, 0x00,

    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16,
    0xAB, 0x89, 0x76, 0x54,
    0x02, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x20,
    0x45, 0x67, 0x98, 0xBA,

    //-----------VAP & VAN---------------
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,

    WRITE_C8_D8, 0xE5, 0xE4,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0x20,   // 0x20 normal, 0x21 IPS

    WRITE_COMMAND_8, 0x11, // Sleep Out
    END_WRITE,

    DELAY, 120,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29, // Display On
    END_WRITE};

// Sync pins (RGB interface)
#define DE_PIN 18              // DE (Data Enable)
#define VSYNC_PIN 17           // VSYNC
#define HSYNC_PIN 16           // HSYNC
#define PCLK_PIN 21            // PCLK (Pixel Clock)

// Red bits (R0 LSB to R4 MSB)
#define R0_PIN 11
#define R1_PIN 12
#define R2_PIN 13
#define R3_PIN 14
#define R4_PIN 0

// Green bits (G0 LSB to G5 MSB)
#define G0_PIN 8
#define G1_PIN 20
#define G2_PIN 3
#define G3_PIN 46
#define G4_PIN 9
#define G5_PIN 10

// Blue bits (B0 LSB to B4 MSB)
#define B0_PIN 4
#define B1_PIN 5
#define B2_PIN 6
#define B3_PIN 7
#define B4_PIN 15

// Color order tuning (adjust if colors are swapped)
#define RGB_PANEL_SWAP_RB false
#define RGB_PANEL_USE_BIG_ENDIAN false

// SPI pins for display control/initialization
#define TFT_CS 39
#define TFT_SCLK 48
#define TFT_MOSI 47
#define TFT_MISO GFX_NOT_DEFINED

// Backlight pin
#define GFX_BL 38

// Display dimensions
#define GFX_WIDTH 480
#define GFX_HEIGHT 480

// Create SPI bus instance for control (DC not used for ST7701 RGB init)
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, TFT_CS /* CS */,
    TFT_SCLK /* SCK */, TFT_MOSI /* MOSI */, TFT_MISO /* MISO */);

// Create RGB panel instance
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    DE_PIN /* DE */, VSYNC_PIN /* VSYNC */, HSYNC_PIN /* HSYNC */, PCLK_PIN /* PCLK */,
#if RGB_PANEL_SWAP_RB
    B0_PIN /* R0 */, B1_PIN /* R1 */, B2_PIN /* R2 */, B3_PIN /* R3 */, B4_PIN /* R4 */,
#else
    R0_PIN /* R0 */, R1_PIN /* R1 */, R2_PIN /* R2 */, R3_PIN /* R3 */, R4_PIN /* R4 */,
#endif
    G0_PIN /* G0 */, G1_PIN /* G1 */, G2_PIN /* G2 */, G3_PIN /* G3 */, G4_PIN /* G4 */, G5_PIN /* G5 */,
#if RGB_PANEL_SWAP_RB
    R0_PIN /* B0 */, R1_PIN /* B1 */, R2_PIN /* B2 */, R3_PIN /* B3 */, R4_PIN /* B4 */,
#else
    B0_PIN /* B0 */, B1_PIN /* B1 */, B2_PIN /* B2 */, B3_PIN /* B3 */, B4_PIN /* B4 */,
#endif
    1 /* hsync_polarity */, 20 /* hsync_front_porch */, 20 /* hsync_pulse_width */, 80 /* hsync_back_porch */,
    1 /* vsync_polarity */, 16 /* vsync_front_porch */, 10 /* vsync_pulse_width */, 30 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 6000000UL /* prefer_speed */, RGB_PANEL_USE_BIG_ENDIAN);

// Create display instance (ST7701)
Arduino_GFX *gfx = new Arduino_RGB_Display(
    GFX_WIDTH /* width */, GFX_HEIGHT /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_4848s040_init_operations, sizeof(st7701_4848s040_init_operations));

// Device initialization function
// Sets up backlight pin
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

#else
#error "This pin configuration is for ESP32 / ESP32-S3 targets only"
#endif
