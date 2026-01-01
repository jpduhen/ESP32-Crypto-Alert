# Hoofdstuk 4: Installatie

## 4.1 Overzicht
Dit hoofdstuk begeleidt je stap voor stap bij het installeren van de firmware op je ESP32-board. We gebruiken de **Arduino IDE** (aanbevolen voor beginners) als primaire methode.

De installatie bestaat uit:
1. Arduino IDE voorbereiden met ESP32-ondersteuning
2. USB-drivers installeren
3. Code downloaden en board-specifiek configureren
4. Firmware flashen via USB
5. Eerste WiFi-setup

**Geschatte tijd**: 15-30 minuten.

![Arduino IDE overzicht](img/arduino-ide-overview.jpg)  
*Arduino IDE 2.x met ESP32 board-ondersteuning.*

## 4.2 Arduino IDE Installeren en Voorbereiden

1. Download en installeer de nieuwste Arduino IDE van https://www.arduino.cc/en/software (versie 2.x aanbevolen).

2. **ESP32 board-ondersteuning toevoegen**:
   - Ga naar **Bestand → Voorkeuren** (File → Preferences).
   - Plak in "Extra bordbeheer-URL's" de volgende URL:  
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Ga naar **Extra → Bordbeheer** (Tools → Board → Boards Manager).
   - Zoek op "esp32" en installeer **esp32 by Espressif Systems** (laatste versie).

![Boards Manager ESP32](img/boards-manager-esp32.jpg)  
*Installatie van het ESP32-pakket in de Boards Manager.*

## 4.3 USB-Drivers Installeren
De meeste ESP32-boards gebruiken een CH340, CH341 of CP210x USB-chip. ESP32-S3 boards (zoals Waveshare en T-Display S3) gebruiken vaak native USB of CP210x.

- **Windows**: Download en installeer:
  - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH340/CH341: https://www.wch.cn/downloads/CH341SER_EXE.html
- **macOS / Linux**: Meestal automatisch herkend.

![USB driver installatie](img/usb-driver-install.jpg)  
*Voorbeeld van CP210x driver-installatie op Windows.*

Controleer in Apparaatbeheer (Windows) of de COM-poort verschijnt wanneer je het board aansluit.

## 4.4 Code Downloaden en Configureren

1. Ga naar https://github.com/jpduhen/ESP32-Crypto-Alert
2. Klik op **Code → Download ZIP** of kloon met Git.
3. Open de map en dubbelklik op `ESP32-Crypto-Alert.ino` om het in Arduino IDE te openen.

4. **Board selecteren** (Tools → Board):
   - Cheap Yellow Display: **ESP32 Dev Module**
   - TTGO T-Display: **TTGO T-Display**
   - T-Display S3 / Waveshare ESP32-S3-GEEK: **ESP32S3 Dev Module**

![Board selectie](img/board-selection.jpg)  
*Keuze van ESP32S3 Dev Module voor S3-boards.*

5. **Belangrijke instellingen** (Tools-menu):
   - Upload Speed: 921600 (verlaag naar 115200 bij problemen)
   - Flash Mode: QIO
   - Partition Scheme: "Default 4MB with spiffs" of "Huge APP"
   - PSRAM: **Enabled** (voor S3-boards en boards met PSRAM)

6. **Board-specifieke define**:
   - Bovenaan de code staan regels zoals `#define CYD_2432S028R` of `#define LILYGO_TDISPLAY_S3`.
   - Zet de juiste op `1` en de anderen op `0`.

![Code defines](img/code-defines.jpg)  
*Voorbeeld van board-defines bovenaan de sketch.*

## 4.5 Firmware Flashen

1. Sluit het board aan via USB.
2. **Boot mode activeren** (bij veel boards nodig):
   - Houd de **BOOT**-knop ingedrukt.
   - Druk kort op **EN/RESET**.
   - Laat BOOT los.

![Boot knoppen](img/boot-buttons.jpg)  
*BOOT- en EN-knoppen op een typisch ESP32-board.*

3. Selecteer de juiste poort onder Tools → Port.
4. Klik op de **Upload**-knop (pijl rechts).

![Upload proces](img/upload-process.jpg)  
*Succesvol upload-proces in Arduino IDE.*

Bij succes zie je "Done uploading" en start het board opnieuw.

## 4.6 Eerste Opstart en WiFi-Setup
- Na upload start het board in **Access Point (AP) mode**.
- Zoek in je WiFi-lijst naar "no-net" en verbind ermee.
- Open in je browser http://192.168.4.1
- Vul je thuis-WiFi-gegevens in en sla op.
- Het board herstart en verbindt met je netwerk.

![WiFi captive portal](img/wifi-captive-portal.jpg)  
*Voorbeeld van de WiFi-setup portal (captive portal).*

Je bent nu klaar voor configuratie via de web-interface (Hoofdstuk 5).

## 4.7 Veelvoorkomende Problemen
- **Geen COM-poort**: Driver installeren of andere kabel proberen.
- **Upload mislukt**: Boot mode handmatig activeren.
- **Brownout**: Betere voeding gebruiken (min. 1A-2A).

---

*Ga naar [Hoofdstuk 3: Hardwarevereisten](03-Hardwarevereisten.md) | [Hoofdstuk 5: Configuratie via de Web Interface](05-Configuratie-Web-Interface.md)*