# ADR-001 — ESP32-S3 GEEK display: ST7789 over SPI met `esp_lcd`

**Status:** besloten (T-102)  
**Datum:** 2026-04-11  
**Context:** `firmware-v2`, referentieboard ESP32-S3 GEEK (Spotpear 1.14" IPS).

## Besluit

De eerste displayroute voor V2 op de GEEK is:

- **Controller:** Sitronix **ST7789**
- **Bus:** **4-wire SPI** (MOSI/SCLK/CS/DC/RST), geen MISO voor pixels
- **Softwarestack:** ESP-IDF **`esp_lcd`** — `esp_lcd_new_panel_io_spi` + **`esp_lcd_new_panel_st7789`**
- **LVGL:** **niet** opgenomen in T-102; volledig schermvulling via `esp_lcd_panel_draw_bitmap` volstaat voor bring-up

## Alternatieven (afgewezen voor deze stap)

| Alternatief | Waarom niet nu |
|-------------|----------------|
| Arduino_GFX (V1-stijl) | Niet ESP-IDF-native; meer coupling, slechtere CI/reproduceerbaarheid |
| Direct bitbang SPI zonder `esp_lcd` | Meer onderhoud, geen officiële panel-abstractie |
| LVGL meteen | Te zwaar voor “kleinste zichtbare output”; kan later op `display_port` landen |
| `esp_lcd` I8080 | GEEK gebruikt SPI volgens schematic |

## Argumenten

1. Espressif ondersteunt ST7789 + SPI officieel; zelfde patroon als upstream voorbeelden.
2. `esp_lcd_panel_set_gap` / `invert_color` dekken paneeloffsets (52, 40, gelijk V1 `Arduino_ST7789`).
3. Geen extra graphics library nodig voor testbeeld.

## Geometrie (belangrijk) — `swap_xy` uit voor GEEK

V1 gebruikt **`Arduino_ST7789` met rotatie 0** (geen MV-bit in MADCTL). Een eerdere V2-probeerde **`esp_lcd_panel_swap_xy(true)`** te combineren met `draw_bitmap(0,0,135,240)`; dat leverde **geen volledige schermvulling** (ca. 135×135 zichtbaar gebied) en **oude GRAM-inhoud** bleef rondom zichtbaar.

**Vastgestelde aanpak:** `esp_lcd_panel_swap_xy(panel, false)` — gelijk aan V1 portrait zonder assenwissel. Daarna `esp_lcd_panel_draw_bitmap` over **135×240** (logische `WIDTH`×`HEIGHT` uit `geek_pins.hpp`) plus **eerst een volledige zwarte fill** om restanten van eerdere firmware te wissen.

Bij verkeerde spiegeling/draai: `esp_lcd_panel_mirror` (niet blind `swap_xy` combineren met V1-offsets).

## Risico’s / onbekenden

- **Pinout-revisies:** commentaar in V1-header waarschuwt voor revisies — bij geen beeld: pins en gap/spiegel eerst tunen.
- **Offsets/orientatie:** `set_gap` is startwaarde uit V1; bij randverschuiving alleen gap/mirror tunen (niet tegelijk een andere `swap_xy`-aanname).
- **Hardwarevalidatie:** CI bewijst alleen compile/link; **volledig scherm op echt paneel** blijft een gebruikerscheck.

## Gevolgen

- Toekomstige LVGL-laag kan op hetzelfde `esp_lcd_panel_handle_t` bouwen (aparte taak) — uitgewerkt in **[ADR-004 — LVGL op esp_lcd-route](ADR-004-lvgl-esp-lcd-ui-layer.md)** (`esp_lvgl_port`, `ui` ← `market_data`).
- Andere boards krijgen eigen BSP + `display_port`-achtige laag; geen GEEK-details buiten `bsp_s3_geek`/`geek_pins.hpp`.
