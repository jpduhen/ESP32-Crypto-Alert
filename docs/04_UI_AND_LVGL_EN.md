# 04 – UI and LVGL

## Role of the UI

The **UIController** module (src/UIController) is responsible for:

- Initialising LVGL 9 (display, driver, buffers, callbacks).
- Building the screen: chart, cards (prices 1m/30m/2h/…), header/footer, anchor/trend/volatility labels.
- Periodically updating all visible values (prices, returns, trend, volatility, anchor, warm-start status, date/time).

The main sketch provides the **data** (global arrays, `prices[]`, `ret_*`, trend/vol state, anchor); UIController **reads** it and writes to LVGL objects. No secrets in the UI; display only.

---

## LVGL configuration (lv_conf.h)

- **Colour depth**: 16 bit (RGB565) for low RAM.
- **Memory**: LV_MEM_SIZE (e.g. 64 KB); draw buffer for rendering.
- **OS**: FreeRTOS (`LV_USE_OS LV_OS_FREERTOS`), optionally task notify.
- **Rendering**: LV_DRAW_SW, stride/align, layer buffer size.
- **Display refresh**: LV_DEF_REFR_PERIOD (e.g. 33 ms).

Platform-specific items (resolution, fonts, chart size) come from **platform_config.h** (CHART_WIDTH, CHART_HEIGHT, FONT_SIZE_*, SYMBOL_COUNT, etc.).

---

## Display and PINS (PINS_*.h)

- Per board an **Arduino_GFX** display is used (ST7789, ILI9341, ST7701, …) with an **Arduino_*SPI** (or I2C) bus.
- PINS_*.h defines: TFT_CS, TFT_DC, TFT_RST, TFT_MOSI/SCLK, GFX_BL, GFX_WIDTH/HEIGHT, `bus`, `gfx`, `DEV_DEVICE_INIT()`.
- The .ino calls `DEV_DEVICE_INIT()` (backlight, pins); then the display is attached to LVGL via `lv_display_*` and a flush callback that calls `gfx->drawBitmap()`.

---

## UIController: main functions

| Function | Role |
|----------|------|
| **setupLVGL()** | Create display, allocate draw buffer(s), `lv_disp_draw_buf_init`, `lv_display_set_flush_cb`, `lv_display_set_driver`, init callback (millis, log). |
| **buildUI()** | Create all widgets: chart + series, header (title, date, time, version), price boxes (per symbol), footer (RSSI, RAM, IP), anchor/trend/vol labels, min/max/diff labels for 1m/30m/2h/1d/7d (platform-dependent). |
| **updateUI()** | Central update: calls `updateHeaderSection()`, `updateChartSection()`, `updatePriceCardsSection()`, plus `updateDateTimeLabels()`, `updateTrendLabel()`, `updateVolatilityLabel()`, `updateWarmStartStatusLabel()`, etc. |
| **updateChartSection()** | Adjust chart range (min/max) to current price; add new point to `dataSeries` on new price data; chart title/version. |
| **updatePriceCardsSection()** | Per symbol: card colour (green/red/grey from return), title (e.g. "1m +0.12%"), average price label; uses buffers (priceTitleBuffer, priceLblBufferArray) and caches (lastPriceLblValueArray) to limit flicker. |
| **update*Label()** | Anchor (price, take-profit/max-loss), trend, volatility, medium/long-term trend, warm-start status, volume confirmation; only update on value change (cache). |
| **checkButton()** | Read physical button (GPIO 0); on press: cycle symbol on chart, or other action (platform-dependent). |

---

## Widgets and globals

- **Chart**: `chart`, `dataSeries`; points from recent price (e.g. last 60 from secondPrices or own buffer); range dynamic.
- **Price cards**: `priceBox[i]`, `priceTitle[i]`, `priceLbl[i]` for i = 0..SYMBOL_COUNT-1 (0 = BTC-EUR, 1 = 1m, 2 = 30m, 3 = 2h, …).
- **Labels**: e.g. `anchorLabel`, `anchorMaxLabel`, `anchorMinLabel`, `trendLabel`, `volatilityLabel`, `warmStartStatusLabel`, `chartDateLabel`, `chartTimeLabel`, `lblFooterLine1`, `lblFooterLine2`.
- **Buffers**: static char buffers (priceLblBuffer, anchorLabelBuffer, price1MinMaxLabelBuffer, …) to avoid `String` allocation in the hot path.
- **Caches**: lastPriceLblValue, lastAnchorValue, lastPrice1MinMaxValue, … so label text is set only when the value changes.

---

## Tasks and threading

- **uiTask** (Core 0): calls `uiController.updateUI()` at a fixed interval (UPDATE_UI_INTERVAL, 1 s). Takes `dataMutex` very briefly (timeout 0), releases immediately and then calls `updateUI()`. **All** reads of globals (prices, ret_*, trend, volatility, anchor, etc.) happen **without** the mutex (see .ino around lines 8800–8807).
- **Risk:** During `updateUI()`, apiTask or priceRepeatTask can modify the same globals → possible inconsistent frame (mix of old/new values). No crash.
- **Recommended pattern (not in code):** Snapshot under mutex: take mutex → copy minimum required fields (prices[], ret_*, trend/vol state, anchor, warm-start status, connectivity) to a local struct → release mutex → render only from that snapshot. Current code does not do this; it reads globals directly during updateUI().
- **LVGL handler**: in the same task call `lv_task_handler()` or (CYD 2.4) `lv_refr_now(disp)` regularly for animations and input.
- No UI updates from apiTask or webTask; only uiTask writes to LVGL.

---

## Failure modes

- **Display/null**: Checks on `disp != NULL` and widget pointers before drawing or updating.
- **Mutex timeout**: If uiTask does not get dataMutex, the update is skipped; next tick tries again.
- **Memory**: No large allocations in the update path; buffers and caches are fixed. With low heap LVGL itself can fail (draw buffer); that is tuned in lv_conf and board memory.

This document is limited to how the UI and LVGL work; for display-specific settings (rotation, brightness) see configuration and installation docs.

---
**[← 03 Alerting rules](03_ALERTING_RULES_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture) | **[05 Configuration →](05_CONFIGURATION_EN.md)**
