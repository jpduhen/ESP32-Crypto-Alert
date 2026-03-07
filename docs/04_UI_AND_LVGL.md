# 04 – UI en LVGL

## Rol van de UI

De **UIController**-module (src/UIController) is verantwoordelijk voor:

- Initialisatie van LVGL 9 (display, driver, buffers, callbacks).
- Bouwen van het scherm: chart, kaarten (prijzen 1m/30m/2h/…), header/footer, anchor/trend/volatiliteit-labels.
- Periodiek updaten van alle zichtbare waarden (prijzen, returns, trend, volatiliteit, anchor, warm-start status, datum/tijd).

De hoofdschets levert de **data** (globale arrays, `prices[]`, `ret_*`, trend/vol state, anchor); de UIController **leest** die en schrijft naar LVGL-objecten. Geen geheimen in de UI; alleen weergave.

---

## LVGL-configuratie (lv_conf.h)

- **Kleurdiepte**: 16 bit (RGB565) voor weinig RAM.
- **Geheugen**: LV_MEM_SIZE (bijv. 64 KB); draw buffer voor rendering.
- **OS**: FreeRTOS (`LV_USE_OS LV_OS_FREERTOS`), eventueel task notify.
- **Rendering**: LV_DRAW_SW, stride/align, layer buffer size.
- **Display refresh**: LV_DEF_REFR_PERIOD (bijv. 33 ms).

Platform-specifieke dingen (resolutie, fonts, chartgrootte) komen uit **platform_config.h** (CHART_WIDTH, CHART_HEIGHT, FONT_SIZE_*, SYMBOL_COUNT, etc.).

---

## Display en PINS (PINS_*.h)

- Per board wordt een **Arduino_GFX**-display gebruikt (ST7789, ILI9341, ST7701, …) met een **Arduino_*SPI** (of I2C) bus.
- PINS_*.h definieert: TFT_CS, TFT_DC, TFT_RST, TFT_MOSI/SCLK, GFX_BL, GFX_WIDTH/HEIGHT, `bus`, `gfx`, `DEV_DEVICE_INIT()`.
- De .ino roept `DEV_DEVICE_INIT()` aan (backlight, pinnen); daarna wordt het display aan LVGL gekoppeld via `lv_display_*` en een flush-callback die `gfx->drawBitmap()` aanroept.

---

## UIController: belangrijkste functies

| Functie | Rol |
|--------|-----|
| **setupLVGL()** | Display aanmaken, draw buffer(s) alloceren, `lv_disp_draw_buf_init`, `lv_display_set_flush_cb`, `lv_display_set_driver`, init callback (millis, log). |
| **buildUI()** | Alle widgets aanmaken: chart + serie, header (titel, datum, tijd, versie), price boxes (per symbool), footer (RSSI, RAM, IP), anchor/trend/vol labels, min/max/diff labels voor 1m/30m/2h/1d/7d (platformafhankelijk). |
| **updateUI()** | Centrale update: roept `updateHeaderSection()`, `updateChartSection()`, `updatePriceCardsSection()` aan, plus `updateDateTimeLabels()`, `updateTrendLabel()`, `updateVolatilityLabel()`, `updateWarmStartStatusLabel()`, e.d. |
| **updateChartSection()** | Chart range (min/max) aanpassen aan huidige prijs; nieuwe punt toevoegen aan `dataSeries` bij nieuwe prijsdata; chart titel/versie. |
| **updatePriceCardsSection()** | Per symbool: kaartkleur (groen/rood/grijs op basis van return), titel (bijv. "1m +0.12%"), gemiddelde prijs-label; gebruikt buffers (priceTitleBuffer, priceLblBufferArray) en caches (lastPriceLblValueArray) om flikkeren te beperken. |
| **update*Label()** | Anchor (prijs, take-profit/max-loss), trend, volatiliteit, medium/long-term trend, warm-start status, volume confirmatie; alleen bij wijziging van waarde updaten (cache). |
| **checkButton()** | Fysieke knop (GPIO 0) uitlezen; bij druk: cyclen symbool-op-chart, of andere actie (platformafhankelijk). |

---

## Widgets en globals

- **Chart**: `chart`, `dataSeries`; punten uit recente prijs (bijv. laatste 60 uit secondPrices of een eigen buffer); range dynamisch.
- **Price cards**: `priceBox[i]`, `priceTitle[i]`, `priceLbl[i]` voor i = 0..SYMBOL_COUNT-1 (0 = BTC-EUR, 1 = 1m, 2 = 30m, 3 = 2h, …).
- **Labels**: o.a. `anchorLabel`, `anchorMaxLabel`, `anchorMinLabel`, `trendLabel`, `volatilityLabel`, `warmStartStatusLabel`, `chartDateLabel`, `chartTimeLabel`, `lblFooterLine1`, `lblFooterLine2`.
- **Buffers**: statische char-buffers (priceLblBuffer, anchorLabelBuffer, price1MinMaxLabelBuffer, …) om geen `String` te alloceren in de hot path.
- **Caches**: lastPriceLblValue, lastAnchorValue, lastPrice1MinMaxValue, … zodat alleen bij verandering de labeltekst wordt gezet.

---

## Tasks en threading

- **uiTask** (Core 0): roept met vaste periode (UPDATE_UI_INTERVAL, 1 s) `uiController.updateUI()` aan. Neemt `dataMutex` zeer kort (timeout 0), geeft direct weer en roept dan `updateUI()` aan. **Alle** leesacties van globals (prices, ret_*, trend, volatiliteit, anchor, enz.) gebeuren **zonder** mutex (zie .ino rond regel 8800–8807).
- **Risico:** Tijdens `updateUI()` kunnen apiTask of priceRepeatTask dezelfde globals wijzigen → mogelijk inconsistent frame (mix van oude/nieuwe waarden). Geen crash.
- **Aanbevolen patroon (niet in code):** Snapshot under mutex: mutex nemen → minimaal benodigde velden (prices[], ret_*, trend/vol state, anchor, warm-start status, connectivity) naar een lokale struct kopiëren → mutex geven → alleen op basis van die snapshot renderen. Huidige code doet dit niet; ze leest globals direct tijdens updateUI().
- **LVGL handler**: in dezelfde task regelmatig `lv_task_handler()` of (CYD 2.4) `lv_refr_now(disp)` voor animaties en input.
- Geen UI-updates vanuit apiTask of webTask; alleen uiTask schrijft naar LVGL.

---

## Failure modes

- **Display/null**: Controles op `disp != NULL` en widget-pointers voordat er wordt getekend of geüpdatet.
- **Mutex timeout**: Als uiTask de dataMutex niet krijgt, wordt de update overgeslagen; volgende tick probeert opnieuw.
- **Geheugen**: Geen grote allocaties in update-pad; buffers en caches zijn vast. Bij weinig heap kan LVGL zelf falen (draw buffer); dat is in lv_conf en board-geheugen te tunen.

Dit document beperkt zich tot werking van de UI en LVGL; voor display-specifieke instellingen (rotatie, helderheid) zie configuratie- en installatiedocs.

---
**[← 03 Alertregels](03_ALERTING_RULES.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking) | **[05 Configuratie →](05_CONFIGURATION.md)**
