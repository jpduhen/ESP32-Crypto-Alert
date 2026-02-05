# ESP32-Crypto-Alert

[![GitHub stars](https://img.shields.io/github/stars/jpduhen/ESP32-Crypto-Alert?style=social)](https://github.com/jpduhen/ESP32-Crypto-Alert/stargazers)
[![GitHub license](https://img.shields.io/github/license/jpduhen/ESP32-Crypto-Alert)](https://github.com/jpduhen/ESP32-Crypto-Alert/blob/main/LICENSE)

Een standalone ESP32-apparaat dat cryptocurrency-prijzen realtime monitort via Bitvavo en **contextuele alerts** genereert op basis van multi-timeframe analyse.  
Geen constante notificaties, maar alleen relevante signalen zoals spikes, breakouts, compression en trend changes – met slimme filters en jouw persoonlijke **anchor price** als referentie.

### Belangrijkste features
- Multi-timeframe analyse (1m, 5m, 30m, 2h, 1d, 7d)
- Contextuele alerts met anchor price en risicobeheer-zones
- Notificaties via NTFY.sh (push naar telefoon)
- Lokale web-interface voor configuratie en monitoring
- MQTT-integratie (o.a. Home Assistant)
- Nachtstand met tijdvenster en extra filters (instelbaar via WebUI/MQTT)
- Ondersteuning voor populaire ESP32-boards met TFT-display
- Display rotatie instellingen
- Volledig configureerbaar zonder hercompilatie

## 📚 Gedetailleerde Handleiding

De volledige Nederlandstalige handleiding is onderverdeeld in aparte hoofdstukken voor betere leesbaarheid:

[Release notes](RELEASE_NOTES.md)

1. [Hoofdstuk 1: Inleiding](docs/01-Inleiding.md)  
   Overzicht, doelgroep en unieke features

2. [Hoofdstuk 2: Functies en Mogelijkheden](docs/02-Functies-en-Mogelijkheden.md)  
   Kernfuncties, multi-timeframe analyse en alert-types

3. [Hoofdstuk 3: Hardwarevereisten](docs/03-Hardwarevereisten.md)  
   Aanbevolen boards, pinouts en compatibiliteit

4. [Hoofdstuk 4: Installatie](docs/04-Installatie.md)  
   Arduino IDE setup, flashing en eerste WiFi-configuratie

5. [Hoofdstuk 5: Configuratie via de Web Interface](docs/05-Configuratie-Web-Interface.md)  
   Dashboard, basis- en geavanceerde instellingen, NTFY setup

6. [Hoofdstuk 6: Begrip van Kernconcepten](docs/06-Kernconcepten.md)  
   Multi-timeframe, anchor price, 2h-context en confluence

7. [Hoofdstuk 7: Alert Types en Voorbeelden](docs/07-Alert-Types-en-Voorbeelden.md)  
   Alle alert-types met voorbeeldberichten en charts

8. [Hoofdstuk 8: Integratie met Externe Systemen](docs/08-Integratie-Externe-Systemen.md)  
   MQTT, Home Assistant dashboards en automations

9. [Hoofdstuk 9: Geavanceerd Gebruik en Aanpassingen](docs/09-Geavanceerd-Gebruik-en-Aanpassingen.md)  
   Code wijzigen, custom thresholds, OTA en meer

10. [Hoofdstuk 10: Troubleshooting en FAQ](docs/10-Troubleshooting-FAQ.md)  
    Veelvoorkomende problemen en oplossingen

## 🚀 Quick Start
1. Kies een compatibel board (bijv. Cheap Yellow Display).
2. Volg [Hoofdstuk 4: Installatie](docs/04-Installatie.md).
3. Configureer via de web-interface ([Hoofdstuk 5](docs/05-Configuratie-Web-Interface.md)).
4. Ontvang je eerste alerts!

## 🤝 Bijdragen
Suggesties, bugreports en pull requests zijn van harte welkom!  
Open een issue of PR op [GitHub](https://github.com/jpduhen/ESP32-Crypto-Alert).

## ⚠️ Disclaimer
Dit project biedt **geen financieel advies**. Cryptocurrency-markten zijn volatiel. Gebruik op eigen risico.

---

**Laatste update handleiding: Januari 2026**

Veel plezier met je ESP32-Crypto-Alert! 🚀