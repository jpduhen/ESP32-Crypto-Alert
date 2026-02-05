# Chapter 8: Integration with External Systems

## 8.1 Overview
ESP32-Crypto-Alert is designed to work not only standalone but also to integrate seamlessly with other systems. You can forward alerts and price data to notification apps, smart home platforms, or your own scripts.

The main integrations are:
- **NTFY.sh** – direct push notifications to your phone
- **Local web interface** – real-time monitoring (covered in Chapter 5)
- **MQTT** – connection to Home Assistant, Node-RED, and other IoT platforms
- **Display** – local visual output

This chapter focuses primarily on **MQTT** and **Home Assistant**, the most powerful options for automations and dashboards.

![Integration overview](img/integration-overview.jpg)  
*Overview of possible integrations: NTFY, MQTT, and local display.*

## 8.2 NTFY.sh (Recap and Example)
NTFY.sh sends direct push notifications to the official iOS/Android app. Configuration is done in the web interface (see Chapter 5).

![NTFY notification example](img/ntfy-alert-example.jpg)  
*Example of an ESP32-Crypto-Alert notification in the NTFY app.*

## 8.3 MQTT Integration
The device continuously publishes price data and alerts via MQTT topics. This enables integration with virtually any platform that supports MQTT.

### 8.3.1 MQTT Configuration in the Web Interface
Fill in the following fields in the web interface:
- **Broker URL/IP** – e.g., the IP address of your Home Assistant or Mosquitto broker
- **Port** – default 1883
- **Username** and **password** – if authentication is required

The MQTT topic prefix is derived automatically from your NTFY topic and is unique per device.

![MQTT settings web](img/web-mqtt-config.jpg)  
*MQTT section in the web interface.*

### 8.3.2 Published MQTT Topics
The device publishes JSON values under `<prefix>/values/*` and configuration under `<prefix>/config/*`.

**Examples (values):**
- `<prefix>/values/price`
- `<prefix>/values/return_1m`
- `<prefix>/values/return_5m`
- `<prefix>/values/return_30m`
- `<prefix>/values/return_2h`
- `<prefix>/values/return_1d`
- `<prefix>/values/return_7d`
- `<prefix>/values/trend_2h`
- `<prefix>/values/trend_1d`
- `<prefix>/values/trend_7d`

**Configuration topics** (read/write):
- `<prefix>/config/displayRotation` and `<prefix>/config/displayRotation/set`
- `<prefix>/config/move5m` and `<prefix>/config/move5m/set`
- `<prefix>/config/nightMode` and `<prefix>/config/nightMode/set`
- `<prefix>/config/nightStartHour` / `<prefix>/config/nightEndHour`

When using Home Assistant, **MQTT Discovery** is published automatically so sensors and settings appear on their own.

![MQTT topics example](img/mqtt-topics.jpg)  
*Example of MQTT topics in a broker client (e.g., MQTT Explorer).*

### 8.3.3 Integration with Home Assistant
Home Assistant natively supports MQTT and can automatically discover sensors (MQTT Discovery is enabled by default).

**Manual sensor configuration (via UI or YAML):**

```yaml
mqtt:
  sensor:
    - name: "Crypto Price"
      state_topic: "<prefix>/values/price"
      unit_of_measurement: "€"
      value_template: "{{ value_json.price | round(2) }}"
    - name: "Return 1m"
      state_topic: "<prefix>/values/return_1m"
      unit_of_measurement: "%"
      value_template: "{{ value_json | round(2) }}"
```

### 8.3.4 Home Assistant Dashboard Examples
With the available sensors, you can build beautiful Lovelace dashboards.

![Home Assistant crypto dashboard 1](img/ha-dashboard-1.jpg)  
*Simple dashboard with price, change, and latest alert.*

![Home Assistant crypto dashboard 2](img/ha-dashboard-2.jpg)  
*Advanced dashboard with history graph, trend indicator, and alerts.*

### 8.3.5 Automation Examples
Some ideas for Home Assistant automations:
- Flash lights on a **BREAKOUT** or **SPIKE**.
- Play a TTS message on a Google Nest speaker on a **Trend Change**.
- Send a notification via Telegram or WhatsApp when approaching the **Max Loss zone**.

![Home Assistant automation example](img/ha-automation-example.jpg)  
*Example of an automation that activates a light on a breakout alert.*

## 8.4 Other Integration Options
- **Node-RED**: Subscribe to the topics and build complex flows.
- **OpenHAB / Domoticz**: MQTT support available.
- **Custom scripts**: Use an MQTT client (Python, Node.js) for custom logic.

With these integrations, your ESP32-Crypto-Alert becomes a full-fledged part of your smart home or monitoring setup.

---

*Go to [Chapter 7: Alert Types and Examples](07-Alert-Types-and-Examples.md) | [Chapter 9: Advanced Usage and Customization](09-Advanced-Usage-and-Customization.md)*