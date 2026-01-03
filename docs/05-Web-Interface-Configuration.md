# Chapter 5: Configuration via the Web Interface

## 5.1 Overview
After installation and the initial WiFi setup, you configure the ESP32-Crypto-Alert device entirely through a local **web interface**. This runs directly on the ESP32 (using AsyncWebServer) and is accessible from any device on the same network. No recompilation or reflashing is required — all settings are permanently saved in flash memory.

Reasonably optimized default values are already set, but feel free to experiment!

The web interface provides:
- Basic settings (pair, anchor price, notifications)
- Advanced alert parameters and presets
- Real-time monitoring of price, status, and logs
- MQTT configuration for Home Assistant

![Web interface dashboard](img/web-dashboard.jpg)  
*Example of the main dashboard showing real-time price and the latest alert.*

## 5.2 Accessing the Web Interface

1. After the initial WiFi setup, the board connects to your network.
2. Find the IP address:
   - Check the display (shown briefly on startup and in the footer of the main screen).
   - Or look it up in your router's device list.
3. Open your browser and go to: `http://<IP-address>` (e.g., http://192.168.1.123)

If you lose the IP: hold the reset button for 5 seconds during startup → the board restarts in AP mode ("no-net", 192.168.4.1).

![Captive portal WiFi setup](img/captive-portal.jpg)  
*WiFi setup portal on first startup (captive portal).*

## 5.3 Main Dashboard
Upon opening, you’ll see a clear dashboard with:
- Input field for your anchor price.
- Current price and the most important current settings.
- Various collapsible sections where you can adjust all settings in detail.

![Realtime monitoring](img/web-monitoring.jpg)  
*Real-time monitoring section with small price chart and status.*

## 5.4 Basic Configuration

### 5.4.1 Selecting a Trading Pair
Choose a Binance spot pair, e.g., `BTCEUR`, `ETHUSDT`, `SOLBTC`.  
All pairs supported by Binance will work.

![Pair selection](img/web-pair-selection.jpg)  
*Dropdown with trading pairs.*

### 5.4.2 Setting the Anchor Price
- Enter your personal reference price (e.g., average entry price).
- Set **Take Profit %** (above anchor) and **Max Loss %** (below anchor).

![Anchor settings](img/web-anchor-price.jpg)  
*Settings for anchor price and profit/loss zones.*

### 5.4.3 Display Settings
- **Display Rotation**: Rotate the display 180 degrees (0 = normal, 2 = rotated)
- **Display Color Inversion**: Invert display colors (useful for different CYD board variants)
  - On = inversion enabled
  - Off = no inversion (default for CYD 2.8 with 1 USB-C port)

### 5.4.4 NTFY.sh Notifications
1. Create a free topic on https://ntfy.sh (e.g., `mijn-crypto-alerts`).
2. Download the NTFY app on your phone and subscribe to your topic (**NOTE**: the topic shown at the top left of your device screen has '-alert' appended. Do not forget to include this suffix in the NTFY app when subscribing, otherwise notifications won't arrive!).
3. Enter the full topic URL (e.g., `https://ntfy.sh/mijn-crypto-alerts`).

![NTFY configuration](img/web-ntfy-setup.jpg)  
*NTFY settings in the web interface.*

![NTFY app example](img/ntfy-app-notification.jpg)  
*Example of an alert in the NTFY app on a phone.*

## 5.5 Advanced Settings

- **Preset selection**: Conservative, Balanced (default), or Aggressive.
- **Custom thresholds**: Adjust percentages for spike, breakout, compression, etc.
- **Cooldowns**: Minimum time between alerts per timeframe.
- **Confluence Mode**: Alerts only when multiple conditions align.
- **Auto-Volatility Mode**: Thresholds automatically adapt to market conditions.
- **MQTT**: Broker IP, port, username/password for Home Assistant integration.

![Advanced settings](img/web-advanced-settings.jpg)  
*Advanced section with presets and custom options.*

## 5.6 Saving and Testing
- Click **Save** at the bottom of each section.
- The device briefly restarts and applies the settings immediately.
- Test by waiting for market movements or use the debug option (if available).

## 5.7 Tips
- The interface is mobile-friendly — feel free to configure from your phone.
- Need to change WiFi? Use the AP-mode reset.
- All settings are persistently saved; after a power loss, the device starts with your last configuration.

Your device is now fully operational and ready for use!

---

*Go to [Chapter 4: Installation](04-Installation.md) | [Chapter 6: Understanding Core Concepts](06-Core-Concepts.md)*