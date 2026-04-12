# V2 — flash-partitionering (ESP32-S3 GEEK)

## Probleem (voorheen)

- Standaard **single-app** (`partitions_singleapp.csv` uit IDF): vaak **1 MiB** `factory` — te krap voor mbedTLS + `esp_websocket_client` + `esp_lcd` + toekomstige WebUI/MQTT/LVGL.
- **Header** (`esptool` image header) kon **2 MiB** tonen terwijl de chip **16 MiB** fysiek heeft → mismatch tot `sdkconfig` + partition table zijn gealigneerd.

## Keuze productie / OTA: `partitions_v2_16mb_ota.csv`

| Partitie | Type   | Offset   | Grootte   | Doel |
|----------|--------|----------|-----------|------|
| nvs      | data   | 0x9000   | 24 KiB    | WiFi / config |
| otadata  | data   | 0xf000   | 8 KiB     | OTA bookkeeping |
| phy_init | data   | 0x11000  | 4 KiB     | RF |
| ota_0    | app    | 0x20000  | **3 MiB** | actief image |
| ota_1    | app    | (auto)   | **3 MiB** | OTA update |
| storage  | data   | (auto)   | ~10.4 MiB | SPIFFS / assets (later) |

- **OTA** blijft mogelijk (`esp_ota_*` tussen `ota_0` / `ota_1`).
- Twee gelijke slots → voorspelbare upgrade-grootte (max. **3 MiB** per image).
- `CONFIG_ESPTOOLPY_FLASHSIZE` moet **16MB** zijn op hardware met 16 MiB flash.

## Alternatief dev (zonder OTA-switch): `partitions_v2_16mb_factory.csv`

- Eén **factory**-slot (~15 MiB app-ruimte) + kleine `storage` — handig voor snelle iteratie als OTA niet nodig is.
- **Niet** gebruiken voor release als je OTA wilt testen; zet in `sdkconfig` dan `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` dienovereenkomstig.

## Workflow

1. Eerste keer na wisselen van tabel: **full flash** (`idf.py erase-flash flash`) of minimaal bootloader + partitietabel + app — anders oude layout op flash.
2. Controleer bootlog: `Partition Table` moet `ota_0` / `ota_1` tonen met 0x300000 per slot.

## Zie ook

- `BUILD.md` — `idf.py size` / `size-components` (flash-budget).
- Werkdocument § T-103b — TLS-spam + footprint.
