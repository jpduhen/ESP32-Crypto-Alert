# Firmware V2 (ESP-IDF)

Nieuwe firmwarebasis voor **ESP32 Crypto Alert V2**. V1 blijft in de repo-root de functionele referentie (`ESP32-Crypto-Alert.ino`, `src/`).

**Primair — status, besluiten, prioriteiten, migratierichting:** [v_2_herbouw_werkdocument_esp_32_crypto_alert.md](v_2_herbouw_werkdocument_esp_32_crypto_alert.md)  
**Aanvullend — governance-overzicht:** [docs/architecture/V2_WORKDOCUMENT_MASTER.md](../docs/architecture/V2_WORKDOCUMENT_MASTER.md)  
**Technische skeleton-notities:** [docs/architecture/V2_SKELETON_NOTES.md](../docs/architecture/V2_SKELETON_NOTES.md)

## ESP-IDF-versie (vastgelegd)

**V2 gebruikt ESP-IDF v5.4.2** — zie [`ESP_IDF_VERSION`](ESP_IDF_VERSION) en [BUILD.md](BUILD.md) (clone, `install.sh esp32s3`, `export.sh`, troubleshooting).

**CI:** GitHub Actions smoke build met Docker `espressif/idf:v5.4.2` (workflow: `.github/workflows/firmware-v2-smoke.yml`).

## Skeletonfase (huidig)

- **Buildsysteem:** ESP-IDF (`CMakeLists.txt` in deze map = projectroot).
- **Eerste board:** ESP32-S3 GEEK (`components/bsp_s3_geek/`).
- **Geen** feature-pariteit met V1; **T-103** levert Bitvavo REST/WS achter `market_data` (geen WebUI/MQTT/NTFY in deze stap).
- **UI:** eerste LVGL-scherm op `esp_lcd` (**[ADR-004](../docs/architecture/ADR-004-lvgl-esp-lcd-ui-layer.md)**); live data alleen via `market_data::snapshot()`.

### Structuur

```
firmware-v2/
├── CMakeLists.txt
├── ESP_IDF_VERSION
├── sdkconfig.defaults
├── BUILD.md
├── main/
│   └── idf_component.yml      # esp_lvgl_port (ADR-004)
├── components/
│   ├── app_core/
│   ├── config_store/
│   ├── diagnostics/
│   ├── bsp_common/
│   ├── bsp_s3_geek/
│   ├── display_port/
│   ├── ui/
│   ├── market_types/
│   ├── net_runtime/
│   ├── wifi_onboarding/
│   ├── exchange_bitvavo/
│   └── market_data/
└── README.md
```

### Bouwen (kort)

```bash
cd firmware-v2
idf.py set-target esp32s3
idf.py build
```

Vereist werkende ESP-IDF **v5.4.2**-omgeving — zie **[BUILD.md](BUILD.md)**.

`build/`, gegenereerde `sdkconfig` en `managed_components/` staan in `.gitignore`.

## Relatie met V1

- **V1:** Arduino-firmware in de **repo-root** op **`main`** — referentie en onderhoud; formeel vastgelegd onder **[B-001](../docs/V1_REFERENCE_FREEZE_B001.md)** (optionele tag `v1-reference-frozen`).
- **V2:** deze map op branch **`v2/foundation`** — actieve ontwikkeling.
- Geen automatische migratie van broncode: domeinlogica wordt gefaseerd gemapt (zie `docs/migration/MIGRATION_MATRIX_V2_DRAFT.md`).
- **Netwerkgrenzen (M-002):** [docs/architecture/M002_NETWORK_BOUNDARIES.md](../docs/architecture/M002_NETWORK_BOUNDARIES.md).
