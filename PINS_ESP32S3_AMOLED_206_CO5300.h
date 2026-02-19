#pragma once

// PINS_ESP32S3_AMOLED_206_CO5300.h
// ESP32-S3 AMOLED 2.06" (CO5300, QSPI) pin mapping
// Source: Spotpear schematic (AMOLED connector mapping)

#if defined(ESP32) || defined(ESP32S3) || defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Arduino_GFX_Library.h>

// QSPI display pins (CO5300)
#define LCD_SDIO0    4
#define LCD_SDIO1    5
#define LCD_SDIO2    6
#define LCD_SDIO3    7
#define LCD_SCLK    11
#define LCD_CS      12
#define LCD_RESET    8
#define LCD_TE      13

// Touch (FT3168) pins (not used in current firmware)
#define TP_SCL       14
#define TP_SDA       15
#define TP_INT       38
#define TP_RESET      9

// Shared I2C bus (touch/IMU/RTC/PMIC)
#define IIC_SDA      15
#define IIC_SCL      14

// IMU (QMI8658) interrupt
#define IMU_INT      21

// RTC (PCF85063) interrupt
#define RTC_INT      39

// Buttons
#define BOOT_BTN     0
#define PWR_BTN      10

// Display dimensions
#define GFX_WIDTH   410
#define GFX_HEIGHT  502

// SDMMC pins (optioneel)
static const int SDMMC_CLK = 2;
static const int SDMMC_CMD = 1;
static const int SDMMC_DATA = 3;
static const int SDMMC_CS = 17;

// QSPI data bus
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

// CO5300 AMOLED display
Arduino_GFX *gfx = new Arduino_CO5300(
    bus, LCD_RESET, 0 /* rotation */, false /* IPS */, GFX_WIDTH, GFX_HEIGHT);

// Device init (no backlight pin on AMOLED)
#define DEV_DEVICE_INIT() do { } while (0)

#else
#error "This pin configuration is for ESP32 / ESP32-S3 targets only"
#endif
