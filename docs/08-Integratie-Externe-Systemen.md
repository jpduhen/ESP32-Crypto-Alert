# Hoofdstuk 8: Integratie met Externe Systemen

## 8.1 Overzicht
ESP32-Crypto-Alert is ontworpen om niet alleen standalone te werken, maar ook naadloos te integreren met andere systemen. Je kunt alerts en prijsdata doorsturen naar notificatie-apps, smarthome-platforms of eigen scripts.

De belangrijkste integraties zijn:
- **NTFY.sh** – directe push-notificaties naar je telefoon
- **Lokale web-interface** – realtime monitoring (al behandeld in Hoofdstuk 5)
- **MQTT** – koppeling met Home Assistant, Node-RED en andere IoT-platforms
- **Display** – lokale visuele output

Dit hoofdstuk richt zich vooral op **MQTT** en **Home Assistant**, de krachtigste opties voor automations en dashboards.

![Integratie overzicht](img/integration-overview.jpg)  
*Overzicht van mogelijke integraties: NTFY, MQTT en lokale display.*

## 8.2 NTFY.sh (Herhaling en Voorbeeld)
NTFY.sh stuurt directe push-notificaties naar de officiële iOS/Android-app. Configuratie gebeurt in de web-interface (zie Hoofdstuk 5).

![NTFY notificatie voorbeeld](img/ntfy-alert-example.jpg)  
*Voorbeeld van een ESP32-Crypto-Alert melding in de NTFY-app.*

## 8.3 MQTT Integratie
Het apparaat publiceert continu prijsdata en alerts via MQTT-topics. Dit maakt integratie met vrijwel elk platform mogelijk dat MQTT ondersteunt.

### 8.3.1 MQTT Configuratie in de Web Interface
Vul in de web-interface de volgende velden in:
- **Broker URL/IP** – bijv. IP-adres van je Home Assistant of Mosquitto broker
- **Poort** – standaard 1883
- **Gebruiker** en **wachtwoord** – indien authenticatie vereist
- **Base topic** – standaard `cryptoalert` (aanpasbaar)

![MQTT instellingen web](img/web-mqtt-config.jpg)  
*MQTT-sectie in de web-interface.*

### 8.3.2 Gepubliceerde MQTT Topics
Het apparaat stuurt periodiek (elke paar seconden) en bij alerts de volgende topics (onder het gekozen base topic):

- `cryptoalert/price` → huidige prijs (JSON of plain)
- `cryptoalert/change_pct` → percentage verandering t.o.v. vorige update
- `cryptoalert/alert` → volledige tekst van de laatste alert
- `cryptoalert/trend` → huidige 2h-trend (up/down/sideways)
- `cryptoalert/volatility` → classificatie (low/normal/high)
- `cryptoalert/status` → verbindingsstatus en laatste update-tijd

![MQTT topics voorbeeld](img/mqtt-topics.jpg)  
*Voorbeeld van MQTT-topics in een broker-client (bijv. MQTT Explorer).*

### 8.3.3 Integratie met Home Assistant
Home Assistant ondersteunt MQTT native en kan automatisch sensors ontdekken (MQTT Discovery, indien ingeschakeld in de code).

**Handmatige sensor-configuratie (via UI of YAML):**

```yaml
mqtt:
  sensor:
    - name: "Crypto Alert Prijs"
      state_topic: "cryptoalert/price"
      unit_of_measurement: "€"
      value_template: "{{ value_json.price | round(2) }}"
    - name: "Crypto Alert Laatste Alert"
      state_topic: "cryptoalert/alert"
    - name: "Crypto Alert Verandering %"
      state_topic: "cryptoalert/change_pct"
      unit_of_measurement: "%"
```

### 8.3.4 Home Assistant Dashboard Voorbeelden
Met de beschikbare sensors kun je prachtige Lovelace-dashboards bouwen.

![Home Assistant crypto dashboard 1](img/ha-dashboard-1.jpg)  
*Eenvoudig dashboard met prijs, verandering en laatste alert.*

![Home Assistant crypto dashboard 2](img/ha-dashboard-2.jpg)  
*Geavanceerd dashboard met history graph, trend-indicator en alerts.*

### 8.3.5 Voorbeelden van Automations
Enkele ideeën voor Home Assistant automations:
- Lampen laten knipperen bij een **BREAKOUT** of **SPIKE**.
- Een TTS-melding op een Google Nest speaker bij een **Trend Change**.
- Notificatie sturen via Telegram of WhatsApp bij benadering van de **Max Loss-zone**.

![Home Assistant automation voorbeeld](img/ha-automation-example.jpg)  
*Voorbeeld van een automation die een lamp activeert bij een breakout-alert.*

## 8.4 Andere Integratiemogelijkheden
- **Node-RED**: Subscribe op de topics en bouw complexe flows.
- **OpenHAB / Domoticz**: MQTT-ondersteuning aanwezig.
- **Custom scripts**: Gebruik een MQTT-client (Python, Node.js) voor eigen logica.

Met deze integraties wordt jouw ESP32-Crypto-Alert een volwaardig onderdeel van je smarthome of monitoring-setup.

---

*Ga naar [Hoofdstuk 7: Alert Types en Voorbeelden](07-Alert-Types-en-Voorbeelden.md) | [Hoofdstuk 9: Geavanceerde Gebruik en Aanpassingen](09-Geavanceerd-Gebruik-en-Aanpassingen.md)*
