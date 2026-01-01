# Chapter 9: Advanced Usage and Customization

## 9.1 Introduction
This chapter is intended for users who want to go beyond the standard web configuration. Here you will learn how to modify the source code to implement custom behavior, support new hardware, or fine-tune the alert logic. All changes require editing the code and reflashing via the Arduino IDE.

**Important tip**: Always make a backup of your working version and preferably experiment on a separate branch in GitHub.

![Arduino IDE code editor](img/arduino-code-editor.jpg)  
*Arduino IDE with the open ESP32-Crypto-Alert sketch.*

## 9.2 Installing and Updating Required Libraries
The project uses external libraries that you must install via the Library Manager:

- **TFT_eSPI** (Bodmer) – for display support
- **ESPAsyncWebServer** + **AsyncTCP** – for the web interface
- **ArduinoJson** – for JSON handling
- **PubSubClient** – for MQTT (if used)

Go to **Sketch → Include Library → Manage Libraries** and search for the names.

![Library manager](img/library-manager.jpg)  
*Library Manager in Arduino IDE with TFT_eSPI installed.*

## 9.3 Customizing Display Configuration (TFT_eSPI)
For optimal performance and correct rendering, you need to configure TFT_eSPI for your board.

1. After installation, locate the `TFT_eSPI` folder in your Arduino libraries directory.
2. Open `User_Setup.h` or create a custom setup file.
3. Select the correct driver (e.g., ILI9341 for CYD, ST7789 for TTGO/Waveshare) and pin mapping.

![TFT_eSPI User Setup](img/tft-espi-setup.jpg)  
*Example of User_Setup.h with ILI9341 configuration for Cheap Yellow Display.*

In the project code, activate the correct board using defines at the top:

```cpp
#define CYD_2432S028R      1  // For Cheap Yellow Display
// #define LILYGO_TDISPLAY  0  // etc.
```

## 9.4 Custom Thresholds en Alert-Logica Wijzigen
De meeste parameters staan als constante variabelen in de code en kunnen eenvoudig aangepast worden.

Voorbeelden van veelgebruikte variabelen:

```cpp
const float SPIKE_THRESHOLD       = 1.8;   // % voor spike-alert
const float BREAKOUT_PERCENT      = 2.5;   // % buiten 2h-range
const int   COOLDOWN_SHORT_SEC    = 300;   // 5 minuten cooldown korte alerts
const int   COOLDOWN_LONG_SEC     = 1800;  // 30 minuten cooldown 2h alerts
```
Pas deze aan naar jouw trading-stijl (lagere waarden = meer alerts).
## 9.5 Toekomstige Aanpassingen
Enkele toekomstige modificaties:
- Meerdere trading pairs monitoren (bijv. automatisch switchen elke 10 minuten).
- SD-kaart logging activeren (voor boards met SD-slot).
- OTA (Over-The-Air) updates inschakelen voor draadloos flashen.
- Extra MQTT-topics toevoegen (bijv. raw kandledata).
- Custom UI-elementen in de web-interface (bijv. grafiekje van 2h-range).

### Risico-Management Uitbreiden
- Dynamische take profit / max loss gebaseerd op volatiliteit.
- Automatische anchor-reset bij bevestigde trend change.
- Waarschuwingen bij extreme drawdown vanaf anchor.

![OTA update voorbeeld](img/ota-update.jpg)  
*OTA-update interface na activering in de code.*

![Serial Monitor debugging](img/serial-monitor-debug.jpg)  
*Serial Monitor met debug-output tijdens testen van custom logica.*

## 9.6 Veilig Experimenteren
- Gebruik GitHub branches voor verschillende versies.
- Test wijzigingen eerst met de Serial Monitor (baud 115200) voor debug-output.
- Begin met kleine aanpassingen en bouw geleidelijk op.

---

*Ga naar [Hoofdstuk 8: Integratie met Externe Systemen](08-Integratie-Externe-Systemen.md)