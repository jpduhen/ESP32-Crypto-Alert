// LCDWIKI28_ILI9341V_init.h
// ILI9341V init voor LCDwiki 2.8" — zelfde register/volgorde als docs/ILI9341V_Init.txt
// (Arduino_GFX default type1 mist o.a. volledige E0/E1 en wijkt af op C0/C1/C5/C7/B6/F6/B1).
// Gebruikt Arduino_GFX batchOperation-opcodes (compatibel met GFX_Library_for_Arduino).

#pragma once

#include <Arduino_GFX_Library.h>

static const uint8_t lcdwiki28_ili9341v_init_ops[] = {
    BEGIN_WRITE,

    WRITE_C8_BYTES, 0xCF, 3,
    0x00, 0xC1, 0x30,
    WRITE_C8_BYTES, 0xED, 4,
    0x64, 0x03, 0x12, 0x81,
    WRITE_C8_BYTES, 0xE8, 3,
    0x85, 0x00, 0x78,
    WRITE_C8_BYTES, 0xCB, 5,
    0x39, 0x2C, 0x00, 0x34, 0x02,
    WRITE_C8_D8, 0xF7, 0x20,
    WRITE_C8_D16, 0xEA, 0x00, 0x00,

    WRITE_C8_D8, ILI9341_PWCTR1, 0x13,
    WRITE_C8_D8, ILI9341_PWCTR2, 0x13,
    WRITE_C8_D16, ILI9341_VMCTR1, 0x22, 0x35,
    WRITE_C8_D8, ILI9341_VMCTR2, 0xBD,

    // Display inversion (vendor zet 0x21 vóór sleep out; tftInit() zet daarna invertDisplay(false),
    // setupDisplay() past PLATFORM_LCDWIKI28_INVERT_COLORS toe — volgorde blijft consistent.)
    WRITE_COMMAND_8, ILI9341_INVON,

    WRITE_C8_D8, ILI9341_MADCTL, 0x08,
    WRITE_C8_BYTES, ILI9341_DFUNCTR, 2,
    0x0A, 0xA2,

    WRITE_C8_D8, ILI9341_PIXFMT, 0x55,

    WRITE_C8_BYTES, 0xF6, 2,
    0x01, 0x30,

    WRITE_C8_BYTES, ILI9341_FRMCTR1, 2,
    0x00, 0x1B,

    WRITE_C8_D8, 0xF2, 0x00,
    WRITE_C8_D8, ILI9341_GAMMASET, 0x01,

    WRITE_C8_BYTES, ILI9341_GMCTRP1, 15,
    0x0F, 0x35, 0x31, 0x0B, 0x0E, 0x06, 0x49, 0xA7, 0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00,
    WRITE_C8_BYTES, ILI9341_GMCTRN1, 15,
    0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58, 0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F,

    WRITE_COMMAND_8, ILI9341_SLPOUT,
    DELAY, 120,
    WRITE_COMMAND_8, ILI9341_DISPON,
    END_WRITE,
};
