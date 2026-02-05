# Hoofdstuk 5: Configuratie via de Web Interface

## 5.1 Overzicht
Na de installatie en eerste WiFi-setup configureer je het ESP32-Crypto-Alert-apparaat volledig via een lokale **web-interface**. Deze draait rechtstreeks op de ESP32 (WebServer) en is toegankelijk via elk apparaat in hetzelfde netwerk. Geen hercompilatie of opnieuw flashen nodig — alle instellingen worden permanent opgeslagen in het flash-geheugen.

Standaard zijn al redelijk geoptimaliseerde waarden ingesteld, maar experimenteer gerust!

De web-interface biedt:
- Basisinstellingen (market, anchor, notificaties)
- Geavanceerde alert-parameters (thresholds, cooldowns, filters)
- Realtime monitoring van prijs en status
- MQTT-configuratie voor Home Assistant

![Web interface dashboard](img/web-dashboard.jpg)  
*Voorbeeld van het hoofd-dashboard met realtime prijs en laatste alert.*

## 5.2 Toegang tot de Web Interface

1. Na de eerste WiFi-setup verbindt het board zich met je netwerk.
2. Vind het IP-adres:
   - Kijk op het display (wordt kort getoond bij opstarten en in de voetregel van het hoofdscherm).
   - Of zoek in je router naar het apparaat.
3. Open in je browser: `http://<IP-adres>` (bijv. http://192.168.1.123)

Als je het IP kwijt bent: houd de reset-knop 5 seconden ingedrukt bij opstarten → het board start opnieuw in AP-mode ("no-net, 192.168.4.1").

![Captive portal WiFi setup](img/captive-portal.jpg)  
*WiFi-setup portal bij eerste opstarten (captive portal).*

## 5.3 Hoofddashboard
Bij openen zie je een overzichtelijk dashboard met:
- Instelveld voor je anchor-prijs.
- Huidige prijs en de belangrijkste huidige instellingen
- Diverse secties die je kunt openklikken om in detail alle instellingen aan te passen.

![Realtime monitoring](img/web-monitoring.jpg)  
*Realtime monitoring-sectie met prijsgrafiekje en status.*

## 5.4 Basisconfiguratie

### 5.4.1 Market Kiezen
Selecteer een Bitvavo market, bijv. `BTC-EUR` of `ETH-EUR`.

![Pair selectie](img/web-pair-selection.jpg)  
*Dropdown met trading pairs.*

### 5.4.2 Anchor Price Instellen
- Voer je persoonlijke referentieprijs in (bijv. gemiddelde aankoopprijs).
- Stel **Take Profit %** (boven anchor) en **Max Loss %** (onder anchor).

![Anchor instellingen](img/web-anchor-price.jpg)  
*Instellingen voor anchor price en profit/loss-zones.*

### 5.4.3 Display Instellingen
- **Display Rotatie**: Draai het display 180 graden (0 = normaal, 2 = gedraaid)

### 5.4.4 NTFY.sh Notificaties
1. Maak een gratis topic aan op https://ntfy.sh (bijv. `mijn-crypto-alerts`).
2. Download de NTFY-app op je telefoon en subscribe op je topic (LET OP: je topic staat ook linksbovenin je scherm MAAR krijgt wel '-alert' als toevoeging. Vergeet die toevoeging niet in je NTFY-app ook op te geven als je de topic invult waarop je je abonneert, anders komen de meldingen niet binnen!).
3. Vul de volledige topic-URL in (bijv. `https://ntfy.sh/mijn-crypto-alerts`).

![NTFY configuratie](img/web-ntfy-setup.jpg)  
*NTFY-instellingen in de web-interface.*

![NTFY app voorbeeld](img/ntfy-app-notification.jpg)  
*Voorbeeld van een alert in de NTFY-app op telefoon.*

## 5.5 Geavanceerde Instellingen

- **Custom thresholds**: Pas percentages aan voor spike, move en 2h‑alerts.
- **Cooldowns**: Minimale tijd tussen alerts per timeframe.
- **Confluence Mode**: Alleen alerts bij meerdere bevestigingen.
- **Auto-Volatility Mode**: Thresholds automatisch aanpassen aan marktcondities.
- **Nachtstand**: Tijdvenster en extra filters (incl. 5m/30m bevestiging en cooldown).
- **MQTT**: Broker IP, poort, user/password voor Home Assistant integratie.

![Geavanceerde instellingen](img/web-advanced-settings.jpg)  
*Geavanceerde sectie met presets en custom opties.*

## 5.6 Opslaan en Testen
- Klik op **Opslaan** onderaan de pagina.
- Het apparaat past de instellingen direct toe (zonder volledige herstart).
- Test door te wachten op marktbewegingen of via Home Assistant/MQTT.

## 5.7 Tips
- De interface is mobiel-vriendelijk — configureer gerust vanaf je telefoon.
- Wijzigingen in WiFi? Gebruik de AP-mode reset.
- Alles wordt persistent opgeslagen; na stroomuitval start het met jouw laatste instellingen.

Je apparaat is nu volledig operationeel en klaar voor gebruik!

---

*Ga naar [Hoofdstuk 4: Installatie](04-Installatie.md) | [Hoofdstuk 6: Begrip van Kernconcepten](06-Kernconcepten.md)*