# ADR-004 — LVGL op de bevestigde `esp_lcd`-route (eerste echte UI-laag)

**Status:** besloten (T-103d)  
**Datum:** 2026-04-10  
**Gerelateerd:** [ADR-001 — ST7789 + esp_lcd](ADR-001-geek-display-st7789-esp-lcd.md), [ADR-002 — Bitvavo / M-002](ADR-002-bitvavo-exchange-and-m002.md)

## Context

- T-102 heeft de **hardwareroute** vastgelegd: ST7789, SPI, **`esp_lcd`** (GEEK).
- T-103 heeft **live marktdata** bewezen via façade **`market_data`** (geen directe koppeling exchange → scherm).
- De volgende stap is bewust **klein**: van debug-/testpixels naar een **eerste echte product-UI**, zonder volledige schermmigratie of dashboard.

## Besluit

1. **LVGL is de gekozen UI-engine** voor V2 op deze route, via Espressif **`esp_lvgl_port`** (managed component), gekoppeld aan het **bestaande** `esp_lcd_panel_io` + `esp_lcd_panel_handle` uit `display_port`.
2. **`display_port`** blijft **laag-niveau**: SPI-bus, panel-init, backlight, optionele ruwe pixelhulp (zoals `draw_bitmap`); **geen** domeinlogica, **geen** Bitvavo.
3. **`ui`** is de **enige plek** voor widgets (labels, later schermen). Initialisatie: `lvgl_port_init` → `lvgl_port_add_disp` met handles van `display_port`.
4. **Live data** bereikt widgets **uitsluitend** via `market_data::snapshot()` en dunne updates (`ui::refresh_from_snapshot`). Geen imports van `exchange_bitvavo` in `ui`.

## Waarom LVGL nu wél (na T-102 “LVGL later”)

| Argument | Toelichting |
|----------|-------------|
| Zelfde stack als upstream | `esp_lcd` + `esp_lvgl_port` is het standaard Espressif-patroon; sluit aan op ADR-001. |
| Schaalbaar zonder V1-spaghetti | Widgets en schermen blijven in `ui`; transport blijft in `market_data` / netwerklaag. |
| Beheerbare eerste stap | Eén scherm, drie labels — geen kaarten/grafieken nodig om waarde te leveren. |

T-102 bewust **zonder** LVGL: alleen bewijs dat het paneel klopt. Nu is die basis er; de risico’s van “LVGL te vroeg” zijn daarmee ingeperkt.

## Dataflow (geen vervuiling)

```
exchange_bitvavo  →  market_data::snapshot()  →  app_core  →  ui::refresh_from_snapshot()
                     (facade)                    (tick + lock)   (LVGL labels)
```

- **`ui`** kent geen URLs, geen TLS, geen WS-events — alleen snapshot-velden.
- **`app_core`** roept `refresh_from_snapshot` na `market_data::tick()` en gebruikt **`lvgl_port_lock` / `unlock`** rond label-updates (via `ui`).

## Randvoorwaarden / bekende beperkingen

- **T-103c validatiestrip** (`display_port::ws_live_validation_strip_toggle`) schrijft **direct** naar het panel **buiten** LVGL om. Combinatie met actieve LVGL op dezelfde regio geeft **tear/overschrijf** — in de eerste LVGL-build is de strip-aanroep uit `app_core` gehaald; pixeltest blijft beschikbaar als API voor geïsoleerde tests.
- **Performance / buffer**: eerste configuratie gebruikt modeste lijn-buffer + double buffer; finetuning volgt bij grotere UI.
- **Meerdere schermen / thema**: bewust nog niet; volgt na deze fase.

## Gevolgen voor de codebase

- `main/idf_component.yml`: dependency **`espressif/esp_lvgl_port`**.
- `display_port`: getters voor IO/panel + afmetingen; init eindigt met **zwarte** full-screen clear vóór LVGL (geen groen testbeeld als vaste eindstaat).
- `ui`: LVGL-opzet + minimale labels; `market_data` als enige snapshot-bron.

## Volgende beslismomenten (niet in deze stap)

- Thema/fonts en tweede view (bv. instellingen) zonder dashboard-scope.
- Eventuele **eigen** LVGL-taak vs. bestaande main-loop (als tickfrequentie omhoog moet).
