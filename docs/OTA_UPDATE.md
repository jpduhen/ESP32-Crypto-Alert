## OTA firmware update (web-based, chunked)

Deze firmware ondersteunt een optionele OTA-firmware update via de webinterface. Hiermee kun je een nieuw `.bin`-bestand uploaden zonder USB-kabel.

### 1. Vereisten

- **OTA-partitieschema op de ESP32**  
  Gebruik een partitieschema met **twee app-partities** (OTA-slots), bijvoorbeeld:
  - `Minimal SPIFFS (1.9MB APP with OTA)` of een vergelijkbaar schema met `ota_0` en `ota_1`.
- **Voldoende vrije flash** voor de firmware (`~1.9MB` maximum in deze build; zie `OTA_MAX_SIZE`).
- Stabiele WiFi-verbinding (zelfde netwerk als je browser).

> Let op: Deze code **wijzigt het partitieschema niet automatisch**. Je moet zelf in de Arduino IDE een geschikt schema kiezen voordat je OTA gebruikt.

### 2. OTA inschakelen

Standaard staat OTA in deze v5.09 rebuild **uit**:

- In `platform_config.h`:

```cpp
// --- OTA (Over-The-Air) updates via web UI ---
// Standaard uitgeschakeld; zet OTA_ENABLED op 1 alleen als je een geschikt schema gebruikt.
#ifndef OTA_ENABLED
#define OTA_ENABLED 0
#endif
```

Om OTA via de webinterface te gebruiken:

1. Kies een board/partitieschema met OTA-ondersteuning in de Arduino IDE.
2. Zet `OTA_ENABLED` op `1` in `platform_config.h` voor jouw platform.
3. Compileer en flash de firmware via USB.

### 3. /update openen

1. Zorg dat het device met WiFi verbonden is.
2. Open de webinterface in je browser (IP-adres staat op het scherm).
3. Ga naar:
   - `http://<device-ip>/update`

Je ziet nu een eenvoudige OTA-pagina met:

- Een bestands-selector (`.bin` bestand)
- Een `Upload` knop
- Een voortgangsbalk en percentage
- Een statusveld voor OK/foutmeldingen

### 4. `.bin` bestand voorbereiden

In de Arduino IDE:

1. Open dit project.
2. Kies het juiste board en partitieschema met OTA-support.
3. Gebruik **Sketch → Export compiled Binary**.
4. Zoek het bestand `ESP32-Crypto-Alert-v509.ino.bin` in de build-/output-map.

Dit `.bin`-bestand gebruik je voor de OTA-upload.

### 5. OTA upload-flow (/update)

1. Open `/update` in de browser.
2. Klik op **Browse** / **Bestand kiezen** en selecteer het `.bin`-bestand.
3. Klik op **Upload**.
4. De pagina zal:
   - Een `POST /update/start` doen met de totale bestandsgrootte.
   - Het bestand in chunks (`~32KB`) uploaden naar `/update/chunk`.
   - Na de laatste chunk `POST /update/end` aanroepen.
5. Tijdens het uploaden:
   - De voortgangsbalk wordt gevuld.
   - Het percentage (`0–100%`) wordt geüpdatet op basis van `written/total`.

### 6. Succes- en foutmeldingen

- **Bij succes**:
  - De voortgang gaat naar `100%`.
  - De status toont een groene melding: `OK. Herstart...`.
  - Na enkele seconden wordt de pagina automatisch teruggestuurd naar `/` (device herstart).

- **Bij fouten**:
  - De status toont een rode foutmelding (HTTP-status of interne fout).
  - De `Upload`-knop wordt weer ingeschakeld.
  - Geen reboot; je blijft op de `/update`-pagina en kunt opnieuw proberen.

Interne checks:

- `Update.begin(total, U_FLASH)` moet slagen.
- Elke chunk mag de `OTA_MAX_SIZE` niet overschrijden.
- Fouten in `Update.write(...)` of `Update.end(true)` worden als fout gerapporteerd.

### 7. Wat er na een succesvolle OTA gebeurt

1. `Update.end(true)` markeert de nieuwe firmware als de volgende boot-slot.
2. De webserver stuurt `{"done":true}` terug naar de browser.
3. Er wordt kort gewacht, `Serial.flush()` wordt aangeroepen.
4. Het device voert `ESP.restart()` uit.
5. Na reboot start de nieuwe firmware, met behoud van bestaande instellingen (NVS).

### 8. Beperkingen en veiligheid

- OTA wijzigt **geen** instellingen, MQTT-config, notificatieinstellingen of thresholds.
- OTA schrijft alleen naar de firmware-partitie (`U_FLASH`), niet naar SPIFFS/NVS.
- Onderbreek OTA niet:
  - Geen voeding losnemen tijdens upload of vlak na `/update/end`.
  - Wacht tot de browser `OK. Herstart...` toont en het device opnieuw is opgestart.

