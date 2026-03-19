// PINS_ESP32S3_JC3248W535_AXS15231B.h
// Pin definitions for JC3248W535CIY module (3.5" TFT, AXS15231B, QSPI, 320x480)
// Bron: JC3248W535 schema – ESP32-S3-WROOM-1, 3.5" TFT QSPI, touch I2C, I2S audio
//
// Referentie: Officiële demo (DEMO_LVGL, display.h + esp_bsp.c) gebruikt
//   BSP_LCD_BIGENDIAN = 1 (RGB565 big-endian) en een volledige AXS15231B-initreeks.
//   Bij kleurenruis: GFX_SPEED verlagen; bij verkeerde kleuren: byte-order in
//   Arduino_GFX AXS15231B-driver controleren (big vs little endian).
//
// Belangrijk: backlight is op IO1; zet GFX_BL HIGH om het scherm zichtbaar te maken.

#pragma once

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// --- Display (QSPI, AXS15231B) ---
#define TFT_CS   15   // LCD CS  (IO15)
#define TFT_SCK  14   // LCD SCK (IO14)
#define TFT_D0   13   // LCD D0 / data 0 (IO13)
#define TFT_D1   12   // LCD D1 / data 1 (IO12)
#define TFT_D2   11   // LCD D2 / data 2 (IO11)
#define TFT_D3   10   // LCD D3 / data 3 (IO10)
#define TFT_RST  39   // LCD RST (IO39)
#define GFX_BL    1   // Backlight (IO1) – HIGH = aan (via Q2, LED_K naar GND)
#define TFT_TE   38   // Tearing effect (optioneel, niet gebruikt door Arduino_GFX)

// Displayafmetingen: 3.5" 320×480
#define GFX_WIDTH  320
#define GFX_HEIGHT 480

// QSPI kloksnelheid (lagere snelheid vermindert kleurenruis; demo gebruikt esp_lcd met eigen timing)
// Bij stabiel beeld kan je proberen: 40000000UL of 60000000UL.
#define GFX_SPEED 20000000UL

// QSPI-bus (CS, SCK, D0, D1, D2, D3)
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);

// Displaydriver AXS15231B (320×480)
Arduino_GFX *gfx = new Arduino_AXS15231B(
    bus, TFT_RST, 0 /* rotation */, false /* IPS */, GFX_WIDTH, GFX_HEIGHT);

// --- Touch (I2C, optioneel voor toekomstig gebruik) ---
#define TOUCH_SDA  8
#define TOUCH_SCL 18
#define TOUCH_INT  4

// --- Audio I2S (NS4168/MAX98357A, optioneel) ---
#define I2S_DIN   41
#define I2S_BCLK  42
#define I2S_LRCLK  2

// Device-init: backlight aan (anders blijft scherm zwart)
#define DEV_DEVICE_INIT() \
    do { \
        pinMode(GFX_BL, OUTPUT); \
        digitalWrite(GFX_BL, HIGH); \
    } while (0)

#else
#error "This pin configuration is for ESP32 / ESP32-S3 only"
#endif
