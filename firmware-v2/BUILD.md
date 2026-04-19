# Bouwen — V2 firmware (`firmware-v2/`)

## Vaste ESP-IDF-versie

| Veld | Waarde |
|------|--------|
| **Release** | **ESP-IDF v5.4.2** |
| **Git-tag (upstream)** | `v5.4.2` |
| **CI Docker-image** | `espressif/idf:v5.4.2` |

Waarom **5.4.x**: past bij ESP32-S3 (GEEK), actieve ondersteuning door Espressif, Docker image beschikbaar voor reproduceerbare builds. Patch **5.4.2** is de expliciet vastgelegde baseline; nieuwere patches (bijv. 5.4.3) alleen na expliciete beslissing (werkdocument + `ESP_IDF_VERSION` + CI).

Machineleesbare pin: [`ESP_IDF_VERSION`](ESP_IDF_VERSION).

## Lokale build (aanbevolen)

1. **Repo clonen** (of bestaande tree gebruiken).

2. **ESP-IDF v5.4.2** neerzetten:

   ```bash
   mkdir -p ~/esp && cd ~/esp
   git clone -b v5.4.2 --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   ```

3. **Omgeving laden** (elke nieuwe shell):

   ```bash
   . ~/esp/esp-idf/export.sh
   ```

4. **Bouwen:**

   ```bash
   cd /pad/naar/ESP32-Crypto-Alert/firmware-v2
   idf.py set-target esp32s3
   idf.py build
   ```

De eerste build downloadt **managed components** (o.a. `esp_websocket_client`, **`esp_lvgl_port`** + LVGL via `main/idf_component.yml`) — **internet** nodig. Daarna staat `managed_components/` lokaal (zie `.gitignore`).

**Display A/B (GEEK):** in `menuconfig` → *ESP32 Crypto Alert V2* kies **Display: A/B diagnoseprofiel** (0–3). Wissel je profiel, verwijder dan **`sdkconfig`** (en bij twijfel `build/`) zodat de nieuwe waarden echt in de build komen — zie [docs/display/GEEK_DISPLAY_DIAG.md](docs/display/GEEK_DISPLAY_DIAG.md).

## WiFi — onboarding (standaard)

Credentials komen uit **NVS** (ingevoerd via browser op het **SoftAP `CryptoAlert`**), niet meer alleen uit menuconfig.

**Eerste boot / lege WiFi in NVS:**

1. Device start SoftAP **`CryptoAlert`** (open).
2. Verbind je telefoon/laptop met `CryptoAlert`.
3. Open **http://192.168.4.1/** — vul thuis-SSID en wachtwoord in, sla op.
4. Apparaat schrijft naar NVS en **herstart**.
5. Daarna normale **STA**-verbinding; daarna rest van de app (display, market_data, …).

**Ontwikkelaars-fallback:** als NVS nog leeg is, kan `net_runtime::start_sta` nog steeds **menuconfig** `NET_WIFI_STA_*` gebruiken (zie `menuconfig`).

**Geforceerd opnieuw provisionen:** `menuconfig` → **WiFi: wis NVS-credentials en start altijd provisioning** (`WIFI_ONBOARDING_FORCE`), of in code `wifi_onboarding::clear_credentials_for_reprovision()` (zie ADR-003).

## T-103 — Bitvavo (menuconfig)

```bash
idf.py menuconfig
```

- **Market data: Bitvavo exchange** — uit (`n`) voor alleen mock-feed (CI/offline).

TLS gebruikt de **certificate bundle** (`sdkconfig.defaults`).

## Veelvoorkomende fout: `xtensa-esp32s3-elf-gcc` niet gevonden

- **`./install.sh esp32s3`** is niet uitgevoerd of mislukt → toolchain ontbreekt.
- **`export.sh` niet gesourced** in de shell waar je `idf.py build` draait.
- **Verkeerde IDF-branch** (andere major/minor) → gebruik exact tag `v5.4.2` voor reproduceerbaarheid met CI.

## Verificatie

```bash
which xtensa-esp32s3-elf-gcc
idf.py --version
```

Beide moeten slagen vóór `idf.py build`.

## Flash-budget (aanbevolen bij grotere wijzigingen)

Na een build:

```bash
idf.py size
idf.py size-components
# optioneel: idf.py size-files
```

- **`size`**: totaal `.text/.data/.bss` en resterende ruimte in het **huidige** app-slot (na wissel naar dual-OTA: zie `docs/flash/PARTITIONS_V2.md`).
- **`size-components`**: grootste bijdragers (mbedTLS, `esp_http_client`, `esp_websocket_client`, `esp_lcd`, …).

Bij **> ~2 MiB** firmware: controleer of je nog binnen **3 MiB** per OTA-slot past (`partitions_v2_16mb_ota.csv`).

## Partitietabel (V2)

- Standaard: **`partitions_v2_16mb_ota.csv`** — dual **OTA**, **3 MiB** per `ota_0` / `ota_1`, **16 MiB** flash (`sdkconfig` / `sdkconfig.defaults`).
- Eerste flash na wissel: volledige image of `erase-flash` — oude 1 MiB-layout op chip wissen.
- Details: [`docs/flash/PARTITIONS_V2.md`](docs/flash/PARTITIONS_V2.md).

## Flash (T-102 — GEEK)

Na succesvolle build: `idf.py -p <poort> flash monitor`.

**Verwacht gedrag op S3-GEEK**

1. Kort **zwart** volledig scherm (GRAM-clear; wist oude UI van eerdere firmware).
2. Daarna **effen groen** over het **hele** 135×240-gebied (geen “venster” midden/beneden).

**Optionele kleurdiagnose (fabriekstest):** in `components/display_port/display_port.cpp` kun je `DISPLAY_PORT_RGBK_DIAG` op `1` zetten: korte cyclus rood → groen → blauw → zwart, daarna opnieuw groen met kleine **witte hoekmarkers**. Standaard `0` voor snellere boot.

Bij **onvolledige vulling** of **resten oude beeld**: zie **ADR-001** (o.a. `swap_xy` moet uit voor deze route; gap/mirror/pin-revisie).
