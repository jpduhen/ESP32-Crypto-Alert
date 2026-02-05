# Chapter 10: Advanced Usage and Customization

## 10.1 Introduction
This chapter is intended for users who want to go beyond the standard web configuration. Here you will learn how to modify the source code to implement custom behavior, support new hardware, or fine-tune the alert logic. All changes require editing the code and reflashing via the Arduino IDE.

**Important tip**: Always make a backup of your working version and preferably experiment on a separate branch in GitHub.

![Arduino IDE code editor](img/arduino-code-editor.jpg)  
*Arduino IDE with the open ESP32-Crypto-Alert sketch.*

## 10.2 Installing and Updating Required Libraries
The project uses external libraries that you must install via the Library Manager:

- **Arduino_GFX_Library** – for display support
- **WebServer** (ESP32 core) – for the web interface
- **PubSubClient** – for MQTT (if used)

Go to **Sketch → Include Library → Manage Libraries** and search for the names.

![Library manager](img/library-manager.jpg)  
*Library Manager in Arduino IDE with TFT_eSPI installed.*

## 10.3 Customizing Display Configuration (platform_config.h)
For optimal performance and correct rendering, select the correct board in `platform_config.h`.

1. Open `platform_config.h`.
2. Enable exactly one platform define (e.g., `PLATFORM_CYD24` or `PLATFORM_ESP32S3_4848S040`).
3. Recompile and flash the project.

## 10.4 Custom Thresholds and Alert Logic
Most thresholds are configurable via the web UI and MQTT. Code changes are mainly useful if you want to add new logic or extra data.

## 10.5 Future Adjustments
Some potential modifications:
- Meerdere trading pairs monitoren (bijv. automatisch switchen elke 10 minuten).
- SD-kaart logging activeren (voor boards met SD-slot).
- Enable OTA (Over-The-Air) updates for wireless flashing (optional).
- Extra MQTT-topics toevoegen (bijv. raw kandledata).
- Custom UI-elementen in de web-interface (bijv. grafiekje van 2h-range).

### Risk Management Extensions
- Dynamic take profit / max loss based on volatility.
- Automatic anchor reset after confirmed trend change.
- Warnings for extreme drawdown from the anchor.

![OTA update voorbeeld](img/ota-update.jpg)  
*OTA-update interface na activering in de code.*

![Serial Monitor debugging](img/serial-monitor-debug.jpg)  
*Serial Monitor met debug-output tijdens testen van custom logica.*

## 10.6 Veilig Experimenteren
- Gebruik GitHub branches voor verschillende versies.
- Test wijzigingen eerst met de Serial Monitor (baud 115200) voor debug-output.
- Begin met kleine aanpassingen en bouw geleidelijk op.

---

*Go to [Chapter 9: Integration with External Systems](09-External-Systems-Integration.md) | Next chapter: —*
