# Hoofdstuk 10: Geavanceerd Gebruik en Aanpassingen

## 10.1 Inleiding
Dit hoofdstuk is bedoeld voor gebruikers die de standaard web-configuratie willen overstijgen. Hier leer je hoe je de broncode aanpast voor eigen gedrag, nieuwe hardware ondersteunt of de alert-logica verder finetunet. Alle wijzigingen vereisen het bewerken van de code en opnieuw flashen via Arduino IDE.

**Belangrijke tip**: Maak altijd een backup van je werkende versie en experimenteer bij voorkeur op een aparte branch in GitHub.

![Arduino IDE code editor](img/arduino-code-editor.jpg)  
*Arduino IDE met geopende ESP32-Crypto-Alert sketch.*

## 10.2 Benodigde Libraries Installeren en Updaten
Het project gebruikt externe libraries die je via de Library Manager moet installeren:

- **Arduino_GFX_Library** – voor display-ondersteuning
- **WebServer** (ESP32 core) – voor de web-interface
- **PubSubClient** – voor MQTT (indien gebruikt)

Ga naar **Sketch → Include Library → Manage Libraries** en zoek op de namen.

![Library manager](img/library-manager.jpg)  
*Library Manager in Arduino IDE met TFT_eSPI geïnstalleerd.*

## 10.3 Display Configuratie Aanpassen (platform_config.h)
Voor optimale prestaties en juiste weergave kies je het juiste board in `platform_config.h`.

1. Open `platform_config.h`.
2. Zet één platform‑define aan (bijv. `PLATFORM_CYD24` of `PLATFORM_ESP32S3_4848S040`).
3. Hercompileer en flash het project.

## 10.4 Custom Thresholds en Alert-Logica Wijzigen
De meeste thresholds zijn via de web‑UI en MQTT instelbaar. Code‑aanpassingen zijn vooral nuttig als je nieuwe logica of extra data wilt toevoegen.

## 10.5 Toekomstige Aanpassingen
Enkele toekomstige modificaties:
- Meerdere trading pairs monitoren (bijv. automatisch switchen elke 10 minuten).
- SD-kaart logging activeren (voor boards met SD-slot).
- OTA (Over-The-Air) updates inschakelen voor draadloos flashen (optioneel).
- Extra MQTT-topics toevoegen (bijv. raw kandledata).
- Custom UI-elementen in de web-interface (bijv. grafiekje van 2h-range).

Risico-Management Uitbreiden
- Dynamische take profit / max loss gebaseerd op volatiliteit.
- Automatische anchor-reset bij bevestigde trend change.
- Waarschuwingen bij extreme drawdown vanaf anchor.

## 10.6 Veilig Experimenteren
Gebruik GitHub branches voor verschillende versies.
Test wijzigingen eerst met de Serial Monitor (baud 115200) voor debug-output.
Begin met kleine aanpassingen en bouw geleidelijk op.

---

*Ga naar [Hoofdstuk 9: Integratie met Externe Systemen](09-Integratie-Externe-Systemen.md) | Volgende hoofdstuk: —*
