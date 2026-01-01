# Hoofdstuk 9: Geavanceerde Gebruik en Aanpassingen

## 9.1 Inleiding
Dit hoofdstuk is bedoeld voor gebruikers die de standaard web-configuratie willen overstijgen. Hier leer je hoe je de broncode aanpast voor eigen gedrag, nieuwe hardware ondersteunt of de alert-logica verder finetunet. Alle wijzigingen vereisen het bewerken van de code en opnieuw flashen via Arduino IDE.

**Belangrijke tip**: Maak altijd een backup van je werkende versie en experimenteer bij voorkeur op een aparte branch in GitHub.

![Arduino IDE code editor](img/arduino-code-editor.jpg)  
*Arduino IDE met geopende ESP32-Crypto-Alert sketch.*

## 9.2 Benodigde Libraries Installeren en Updaten
Het project gebruikt externe libraries die je via de Library Manager moet installeren:

- **TFT_eSPI** (Bodmer) – voor display-ondersteuning
- **ESPAsyncWebServer** + **AsyncTCP** – voor de web-interface
- **ArduinoJson** – voor JSON-handling
- **PubSubClient** – voor MQTT (indien gebruikt)

Ga naar **Sketch → Include Library → Manage Libraries** en zoek op de namen.

![Library manager](img/library-manager.jpg)  
*Library Manager in Arduino IDE met TFT_eSPI geïnstalleerd.*

## 9.3 Display Configuratie Aanpassen (TFT_eSPI)
Voor optimale prestaties en juiste weergave moet je TFT_eSPI configureren voor jouw board.

1. Na installatie vind je de map `TFT_eSPI` in je Arduino libraries-folder.
2. Open `User_Setup.h` of maak een custom setup-bestand.
3. Selecteer de juiste driver (bijv. ILI9341 voor CYD, ST7789 voor TTGO/Waveshare) en pin-mapping.

![TFT_eSPI User Setup](img/tft-espi-setup.jpg)  
*Voorbeeld van User_Setup.h met ILI9341-configuratie voor Cheap Yellow Display.*

In de projectcode activeer je het juiste board met defines bovenaan:

```cpp
#define CYD_2432S028R      1  // Voor Cheap Yellow Display
// #define LILYGO_TDISPLAY  0  // etc.
```

## 9.4 Custom Thresholds en Alert-Logica Wijzigen
De meeste parameters staan als constante variabelen in de code en kunnen eenvoudig aangepast worden.
Voorbeelden van veelgebruikte variabelen:
const float SPIKE_THRESHOLD       = 1.8;   // % voor spike-alert
const float BREAKOUT_PERCENT      = 2.5;   // % buiten 2h-range
const int   COOLDOWN_SHORT_SEC    = 300;   // 5 minuten cooldown korte alerts
const int   COOLDOWN_LONG_SEC     = 1800;  // 30 minuten cooldown 2h alerts
Pas deze aan naar jouw trading-stijl (lagere waarden = meer alerts).

## 9.5 toekomstige Aanpassingen
Enkele toekomstige modificaties:
- Meerdere trading pairs monitoren (bijv. automatisch switchen elke 10 minuten).
- SD-kaart logging activeren (voor boards met SD-slot).
- OTA (Over-The-Air) updates inschakelen voor draadloos flashen.
- Extra MQTT-topics toevoegen (bijv. raw kandledata).
- Custom UI-elementen in de web-interface (bijv. grafiekje van 2h-range).

Risico-Management Uitbreiden
- Dynamische take profit / max loss gebaseerd op volatiliteit.
- Automatische anchor-reset bij bevestigde trend change.
- Waarschuwingen bij extreme drawdown vanaf anchor.

## 9.6 Veilig Experimenteren
Gebruik GitHub branches voor verschillende versies.
Test wijzigingen eerst met de Serial Monitor (baud 115200) voor debug-output.
Begin met kleine aanpassingen en bouw geleidelijk op.