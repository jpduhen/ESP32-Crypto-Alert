# V2 skeletonfase — ontwerpkeuzes (S-001 … S-007)

**Primaire projectstatus:** [firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) (§9, besluit D-005). Dit bestand bevat alleen technische detailkeuzes ter ondersteuning.

**Datum:** 2026-04-11  
**Scope:** eerste **buildbare ESP-IDF**-basis onder `firmware-v2/`, referentieboard **ESP32-S3 GEEK**. Geen V1-featureport, geen CYD/TTGO.

## Tickets → implementatie

| Ticket | Inhoud |
|--------|--------|
| **S-001** | Projectroot `CMakeLists.txt`, `sdkconfig.defaults`, `main/`, componentenboom zoals gevraagd. |
| **S-002** | `app_core`: lifecycle `init_early` → NVS/config → BSP → display → ui → **net_runtime** → **market_data** → idle loop. |
| **S-003** | `config_store`: `RuntimeConfig`, schema `kSchemaVersion`, NVS namespace `v2cfg`, load/save stubs + mismatch → defaults. |
| **S-004** | `diagnostics`: vaste logtags (`DIAG_TAG_*`), `init_early`, periodieke `tick_heartbeat` (debug). |
| **S-005** | `bsp_s3_geek`: descriptor + GPIO backlight (GPIO7, conform V1-header), geen volledige panel-driver. |
| **S-006** | `display_port` + `ui`: placeholderteksten / logging; geen LVGL in deze fase. |
| **S-007** | `market_data`: façade + mock; na **T-103** ook Bitvavo via `exchange_bitvavo`. |

## Migratiematrix (koppeling)

| Matrix (concept) | Skeleton-component |
|------------------|-------------------|
| Entry / orchestratie | `main/main.cpp` + `app_core` |
| Platform / board | `bsp_common`, `bsp_s3_geek` |
| Settings / NVS | `config_store` |
| Heap/diagnose (vereenvoudigd) | `diagnostics` |
| Display / UI (later LVGL) | `display_port`, `ui` |
| Bitvavo / koers | `market_data` (façade) ← `exchange_bitvavo`; `net_runtime` (WiFi + mutex) |

## Gemaakte keuzes

- **Taal:** C++ in components en `main` — `extern "C" void app_main` als enige IDF-entry.
- **BSP:** alleen **S3 GEEK** als concrete BSP; `bsp_common` voor gedeelde types (`BoardDescriptor`).
- **Config:** eigen NVS-namespace en schemaversie; **geen** V1-keymapping (bewust open punt).
- **Display (T-102):** `esp_lcd` + ST7789; **geen LVGL** in bring-up — zie [ADR-001](ADR-001-geek-display-st7789-esp-lcd.md).
- **Market (T-103):** `exchange_bitvavo` (REST + WS, TLS bundle); `market_types` voor snapshot; mock via Kconfig — zie [ADR-002](ADR-002-bitvavo-exchange-and-m002.md).

## T-101 — toolchain & CI (basisverharding)

- **ESP-IDF v5.4.2** vastgelegd (`firmware-v2/ESP_IDF_VERSION`, besluit D-006 in werkdocument).
- **Documentatie:** [firmware-v2/BUILD.md](../../firmware-v2/BUILD.md) (lokale stappen, troubleshooting).
- **CI-smoke:** `.github/workflows/firmware-v2-smoke.yml` — `espressif/idf:v5.4.2`, `idf.py set-target esp32s3` + `build`.

## T-102 — display GEEK

- **ADR:** [ADR-001-geek-display-st7789-esp-lcd.md](ADR-001-geek-display-st7789-esp-lcd.md).
- **Implementatie:** `firmware-v2/components/display_port/`, pins `geek_pins.hpp`, backlight `bsp_s3_geek::backlight_set`.
- **Geometrie:** `swap_xy` **uit** (zoals V1 rotatie 0); zwarte full-screen clear vóór groen; optioneel `DISPLAY_PORT_RGBK_DIAG` (zie `BUILD.md`).

## T-103 — Bitvavo exchange

- **ADR:** [ADR-002-bitvavo-exchange-and-m002.md](ADR-002-bitvavo-exchange-and-m002.md).
- **Componenten:** `exchange_bitvavo` (REST `/v2/ticker/price`, WS `wss://ws.bitvavo.com/v2/`), `net_runtime` (WiFi + `net_mutex`), `market_types` (snapshot), façade `market_data`.
- **Registry:** `firmware-v2/idf_component.yml` → `esp_websocket_client`.

## Open punten (expliciet)

- PSRAM / clock / flash voor GEEK finetunen in `sdkconfig`.
- LVGL: optioneel later; niet vereist voor T-102.
- ~~`exchange` / `net` (T-103)~~ — basis aanwezig; M-002-verfijning volgt.

## Build

Zie [firmware-v2/BUILD.md](../../firmware-v2/BUILD.md) en [firmware-v2/README.md](../../firmware-v2/README.md).
