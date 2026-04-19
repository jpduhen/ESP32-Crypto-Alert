# S3-GEEK — display-diagnose (LVGL `swap_bytes` / SPI-klok)

**Doel:** gerichte A/B op **V2** wanneer kale `esp_lcd`-output (T-102) goed oogt maar de **LVGL**-view wazig of verkeerd gekleurd is.

## V1 (`PINS_ESP32S3_GEEK_ST7789_114.h`) vs V2

| Item | V1 | V2 (`geek_pins.hpp` + `display_port` + `ui`) |
|------|----|-----------------------------------------------|
| MOSI / SCLK / CS / DC / RST / BL | 11 / 12 / 10 / 8 / 9 / 7 | Zelfde |
| Logische resolutie | `GFX_WIDTH` 135 × `GFX_HEIGHT` 240 | `WIDTH` 135 × `HEIGHT` 240 |
| SPI-snelheid | `GFX_SPEED` 27 MHz | `CONFIG_DISPLAY_SPI_PCLK_HZ` (default **27 000 000**) |
| Paneeloffsets | Arduino_ST7789: 52, 40, **53**, 40 (twee col-offsets) | `esp_lcd` **set_gap(52, 40)** — één paar; klein verschil t.o.v. tweede V1-col-offset |
| Rotatie / assen | rotatie 0, IPS | `swap_xy(false)` — gelijk uitgangspunt |
| Kleurpad UI | Arduino_GFX | **LVGL 9** + `esp_lvgl_port`: **`swap_bytes`** (configureerbaar), RGB565, DMA-buffer |

**Alleen in V2 / LVGL:** `lvgl_port_display_cfg_t.flags.swap_bytes`, LVGL flush, `double_buffer`, `buffer_size` — geen equivalent in kale V1-fullscreen fill.

### Externe tips (o.a. LVGL 9 + ST7789) — mapping naar deze firmware

| Tip (algemeen) | In deze repo |
|----------------|--------------|
| **`LV_COLOR_16_SWAP` in `lv_conf.h`** | In de praktijk overlappend met **`CONFIG_UI_LVGL_SWAP_BYTES`** → `disp_cfg.flags.swap_bytes` in `ui` (esp_lvgl_port). Geen eigen `lv_conf` in deze repo bewerken; dat zit in de managed LVGL-stack. |
| **`rgb_ele_order` RGB vs BGR** | **`CONFIG_DISPLAY_ST7789_BGR_ORDER`** (0=RGB, 1=BGR) in `display_port` → `panel_config.rgb_ele_order`. |
| **Handmatige byte-swap in flush** | Niet nodig zolang esp_lvgl_port + `swap_bytes`/`swap_bytes` A/B volstaat; anders laatste redmiddel. |
| **SPI zeer hoog (40–60 MHz)** | Wij default **27 MHz** (`CONFIG_DISPLAY_SPI_PCLK_HZ`); verlagen mogelijk bij ruis, verhogen vooral als experiment. |
| **`LV_DRAW_SW_DRAW_UNIT_CNT`** | Zit in LVGL-build van `esp_lvgl_port`; alleen via upstream/lv_conf/Kconfig van die component — buiten scope tenzij je fork/patch. |

## A/B-procedure (profielen 0–3 — altijd andere compile)

In **menuconfig** → *ESP32 Crypto Alert V2* staat nu één keuze: **Display: A/B diagnoseprofiel**.

| Profiel | `swap_bytes` | `rgb_order` | SPI-pclk | Wanneer |
|---------|----------------|-------------|----------|---------|
| **0** | 1 | RGB | 27 MHz | Alternatief / diagnose |
| **1** | 0 | RGB | 27 MHz | **Productdefault ESP32-S3 GEEK** (ST7789 + LVGL 9 + esp_lvgl_port) |
| **2** | 1 | BGR | 27 MHz | `esp_lcd` kleurvolgorde (BGR) |
| **3** | 1 | RGB | 20 MHz | lagere klok / timing |

Kconfig- en `sdkconfig.defaults`-**default** is **profiel 1** (`CONFIG_DISPLAY_DIAG_PROFILE_SWAP_OFF`).

**Belangrijk:** een oude **`sdkconfig`** in `firmware-v2/` bevat nog losse `CONFIG_UI_*` / eerdere waarden en **negeert** dan het profiel. Na wisselen van profiel:

```bash
cd firmware-v2
rm -f sdkconfig sdkconfig.old
idf.py set-target esp32s3
idf.py build flash monitor
```

(optioneel: `idf.py fullclean` i.p.v. alleen `sdkconfig` verwijderen)

Daarna moet de bootlog **wél** verschillen (`pclk=…`, `rgb_order=…`, en `ui`: `swap_bytes=…`).

**Vroeger:** losse int-opties; die zijn vervangen door dit profiel zodat elke build consistent is.

**Technisch:** een oude `CONFIG_UI_LVGL_SWAP_BYTES=…` in `sdkconfig` kan naast een profiel blijven staan. De code leidt `swap_bytes`, `pclk` en `rgb_order` nu af van **`CONFIG_DISPLAY_DIAG_PROFILE_*`** (compile-time), niet van die losse regel.

## Interpretatie (hypotheses)

| Observatie | Waarschijnlijke oorzaak |
|------------|-------------------------|
| T-102 goed, LVGL wazig/verschuifd; **swap_bytes=0** herstelt scherpte/kleuren | **(B)** byte order / LVGL-flush t.o.v. ST7789 |
| Alleen bij hoge klok wazig; lagere **pclk** helpt | **(C)** SPI-timing / signaalintegriteit |
| Verschil links/rechts rand i.p.v. blur; offsets alleen | **(A)** gap/offset (evt. V1 52/53) — gecombineerd met LVGL mogelijk zichtbaarder dan bij effen fill |

**Invullen na hardwaretest**

| Profiel | Verwacht in log (kort) | Scherpte | Kleuren | Notities |
|---------|-------------------------|----------|---------|----------|
| 0 | swap 1, RGB, 27M | | | |
| 1 | swap 0, RGB, 27M | | | |
| 2 | swap 1, BGR, 27M | | | |
| 3 | swap 1, RGB, 20M | | | |

## Conclusie (na meten)

Op **ESP32-S3 GEEK** (135×240 ST7789) is de **vastgelegde productie-instelling**: **profiel 1** — `swap_bytes=0`, **RGB**, **27 MHz** SPI. Menuconfig-default en `sdkconfig.defaults` volgen dit; profielen 0/2/3 blijven beschikbaar voor diagnose.
