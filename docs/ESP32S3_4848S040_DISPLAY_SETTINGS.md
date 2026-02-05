## ESP32-S3 4848S040 (ST7701) instellingen

Deze instellingen werken stabiel (kleur + sync) op de GUITION ESP32-S3-4848S040 met 480x480 IPS ST7701.

### Board/PSRAM (Arduino IDE)
- Board: ESP32S3 Dev Module (of equivalent S3 board)
- PSRAM: OPI PSRAM (octal), juiste grootte (bijv. 8MB)
- Flash mode: QIO (standaard is ok)

### Platform selectie
In `platform_config.h`:
- `#define PLATFORM_ESP32S3_4848S040`

### LVGL render grootte
In `platform_config.h` (binnen het 4848S040 blok):
- `LVGL_SCREEN_WIDTH  = 480`
- `LVGL_SCREEN_HEIGHT = 480`

### Display init (ST7701)
Bestand: `PINS_ESP32S3_4848S040_ST7701_480.h`

Belangrijke wijzigingen voor juiste kleuren:
- MDT uit: `0xCD = 0x00`
- Normal mode: `0x20` (niet 0x21)
- Geen expliciete `0x3A` pixel format (default werkt correct)
- Geen invert
- Geen R/B swap, geen big-endian

### RGB timing (anti-jitter)
RGB panel setup:
- `hsync_front_porch = 20`
- `hsync_pulse_width = 10`
- `hsync_back_porch = 60`
- `vsync_front_porch = 16`
- `vsync_pulse_width = 10`
- `vsync_back_porch = 30`
- `pclk_active_neg = 0`
- `prefer_speed = 10000000UL` (10 MHz)

Deze timing lost sporadisch bibberen op.

### Kleur instellingen (werkend)
In `PINS_ESP32S3_4848S040_ST7701_480.h`:
```
#define RGB_PANEL_SWAP_RB false
#define RGB_PANEL_USE_BIG_ENDIAN false
```

### Notities
- Deze panel driver vereist PSRAM; zonder PSRAM faalt de framebuffer allocatie.
- Als je later jitter ziet, probeer eerst `pclk_active_neg = 1` of `prefer_speed = 8000000UL`.
