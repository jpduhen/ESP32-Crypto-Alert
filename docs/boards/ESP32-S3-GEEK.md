# ESP32-S3 GEEK — referentieboard V2

**Rol:** eerste doelhardware voor de V2-herbouw (afspraak in voorbereidingsfase).  
**V1-referentie in repo:** `PINS_ESP32S3_GEEK_ST7789_114.h`, selectie via `#define PLATFORM_ESP32S3_GEEK` in `platform_config.h` (standaard staat in upstream vaak een ander board actief).

## Hardware (kort)

- **MCU:** ESP32-S3 (typisch ESP32-S3R2 op dit board).
- **Display:** 1.14", ST7789, 240×135 (UI vaak in portrait 135×240), SPI.
- **Backlight:** GPIO (in V1: `GFX_BL` = GPIO7).
- **Pinout:** zie commentaar in `PINS_ESP32S3_GEEK_ST7789_114.h` (o.a. CS/DC/RST/MOSI/SCLK); fabrikantdocumentatie noemt soms afwijkende labels — **altijd schematic raadplegen**.

## Softwarestack (V1)

- Arduino + **Arduino_GFX** + ST7789-init (`Arduino_ST7789`), zie pin-header.
- LVGL v9 (projectbreed via `lv_conf.h`).
- Zelfde applicatielogica als andere boards; `UI_HAS_TF_MINMAX_STATUS_UI` is voor deze boardgroep ingeschakeld samen met JC3248 en LCDWIKI (`platform_config.h`).

## V2-notities

- Eerste port richt zich op **stabiele display + netwerk + minimale UI-shell** voordat zeldzamere boards worden toegevoegd.
- Overweging: ESP-IDF `esp_lcd` + ST7789-driver i.p.v. Arduino_GFX — **besluit volgt** tijdens implementatie (herschrijven HAL, niet blind kopiëren).

## Onzekerheden

- Exacte revisie van het Spotpear/Waveshare-board en eventuele pinvarianten — V1-header vermeldt expliciet dat bronnen kunnen afwijken.
