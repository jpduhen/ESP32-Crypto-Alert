# Unified LVGL9 Crypto Monitor

A unified Crypto Monitor for different ESP32 display platforms: TTGO T-Display, CYD 2.4" and CYD 2.8".

## Supported Platforms

- **TTGO T-Display**: 1.14" 135x240 TFT display (ST7789)
- **CYD 2.4"**: 2.4" 240x320 TFT display with touchscreen (XPT2046)
- **CYD 2.8"**: 2.8" 240x320 TFT display with touchscreen (XPT2046)

## Features

- Real-time Bitcoin (BTCEUR) price monitoring via Binance API
- Live chart with 60 data points (1 minute history)
- Trend detection (2-hour trend analysis)
- Volatility monitoring (low/medium/high)
- Anchor price tracking with take profit and stop loss alerts
- 1 minute and 30 minute average price tracking
- Min/Max/Diff display for 1m and 30m periods
- MQTT integration for home automation
- NTFY.sh notifications for alerts
- Web interface for configuration
- WiFi Manager for easy WiFi setup

## Hardware Requirements

### TTGO T-Display
- ESP32 with TTGO T-Display module
- 1.14" 135x240 TFT display (ST7789)
- Physical reset button (GPIO 0)

### CYD 2.4" / 2.8"
- ESP32 with CYD display module
- 2.4" or 2.8" 240x320 TFT display
- Touchscreen (XPT2046)

## Software Requirements

- Arduino IDE 1.8.x or 2.x
- ESP32 Board Support Package
- LVGL library v9.2.2 or higher
- Arduino_GFX library
- WiFiManager library
- PubSubClient3 library
- XPT2046_Touchscreen library (only for CYD variants)

## Installation

> **⚠️ IMPORTANT**: Read the entire installation section before starting!

### Step 1: Clone the Repository
```bash
git clone https://github.com/<your-username>/<repository-name>.git
cd UNIFIED-LVGL9-Crypto_Monitor
```

**Or download as ZIP**: Click the green "Code" button on GitHub and select "Download ZIP"

### Step 2: Select Your Platform ⚠️ IMPORTANT - DO THIS FIRST!

**You MUST specify which board you're using before compiling the code!**

**Location**: Open the file **`platform_config.h`** in the root of the project (next to `UNIFIED-LVGL9-Crypto_Monitor.ino`)

**What to do**:
1. Open `platform_config.h` in a text editor or Arduino IDE
2. Find the following lines (approximately lines 5-7):
```cpp
//#define PLATFORM_CYD28
#define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

3. **Activate the correct platform** by removing the `//` (comment) and commenting out the other lines:

**For TTGO T-Display:**
```cpp
//#define PLATFORM_CYD28
#define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

**For CYD 2.4":**
```cpp
//#define PLATFORM_CYD28
// #define PLATFORM_TTGO
#define PLATFORM_CYD24
```

**For CYD 2.8":**
```cpp
#define PLATFORM_CYD28
// #define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

**⚠️ Note**: Only ONE platform can be active! Make sure the other two lines are commented out with `//`.

#### Optional: Set Default Language

You can optionally set the default language in `platform_config.h`. This will be used as a fallback if no language has been saved in Preferences yet (e.g., on first boot).

**Location**: In `platform_config.h`, find the language setting (around line 10-12):

```cpp
// Standaard taal instelling (0 = Nederlands, 1 = English)
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // 0 = Nederlands, 1 = English
#endif
```

**Options**:
- `0` = Dutch (Nederlands) - Default
- `1` = English

**Note**: You can always change the language later via the web interface. This setting is only used as a fallback on first boot.

### Step 3: Install Libraries

Install the required libraries via Arduino Library Manager:
   - **LVGL** (v9.2.2 or higher) - Required for all platforms
   - **WiFiManager** - Required for all platforms
   - **PubSubClient3** - Required for all platforms
   - **Arduino_GFX** - Required for all platforms
   - **XPT2046_Touchscreen** - Only required for CYD 2.4" and 2.8" (not needed for TTGO)

### Step 4: Upload to ESP32

1. Open `UNIFIED-LVGL9-Crypto_Monitor.ino` in Arduino IDE
2. Select your ESP32 board in Tools → Board
3. Select the correct port in Tools → Port
4. Click Upload

### Step 5: First Boot

On first boot:
   - Connect to the WiFi Access Point that is created
   - Configure your WiFi credentials via the web interface
   - Configure MQTT and NTFY settings (optional)

## Screenshots

The following screenshots show the different screens of the application:

### Startup Screen
![Startup Screen](images/startup.png)

### WiFi Configuration Screen
![WiFi Configuration Screen](images/wifi_config.png)

### WiFi Connected Screen
![WiFi Connected Screen](images/wifi_connected.png)

### Main Screen
![Main Screen](images/main_screen.png)

**Note**: To add screenshots to your repository:
1. Create an `images` folder in the root of the project
2. Place your screenshot images in this folder with the names shown above
3. Supported formats: PNG, JPG, or GIF
4. Recommended size: 800-1200px width for best display on GitHub

## Configuration

### Web Interface

After the first WiFi setup, you can access the web interface at the IP address shown on the display.

**Access**: Open your browser and go to `http://<IP-address>` (e.g. `http://192.168.1.50`)

The web interface provides a clear, dark interface with all settings grouped in sections:

#### Language Selection
- **Language**: Choose between Dutch (Nederlands) or English
  - This setting affects all texts on the display and in the web interface
  - The language is saved in Preferences and persists after reboot
  - You can also set a default language in `platform_config.h` (see Installation section)

#### Basic Settings
- **NTFY Topic**: Your NTFY.sh topic name for notifications
- **Binance Symbol**: The trading pair you want to monitor (e.g. BTCEUR, BTCUSDT, ETHUSDT)

#### Spike & Move Alerts
Configure when you want to receive alerts for rapid price movements:

- **1m Spike - ret_1m threshold (%)**: Threshold for 1-minute spike alerts (default: 0.30%)
- **1m Spike - ret_5m filter (%)**: Filter to prevent false alerts (default: 0.60%)
  - *Explanation*: Both conditions must be true for an alert
- **30m Move - ret_30m threshold (%)**: Threshold for 30-minute move alerts (default: 2.0%)
- **30m Move - ret_5m filter (%)**: Filter for 30-minute moves (default: 0.5%)
- **5m Move Alert - threshold (%)**: Threshold for 5-minute move alerts (default: 1.0%)

#### Cooldowns
Time between notifications to prevent spam:

- **1-minute spike cooldown (seconds)**: Time between 1m spike alerts (default: 600 = 10 minutes)
- **30-minute move cooldown (seconds)**: Time between 30m move alerts (default: 600 = 10 minutes)
- **5-minute move cooldown (seconds)**: Time between 5m move alerts (default: 600 = 10 minutes)

#### MQTT Settings
Configure your MQTT broker for integration with Home Assistant or other systems:

- **MQTT Host (IP)**: IP address of your MQTT broker (e.g. `192.168.1.100`)
- **MQTT Port**: Port of your MQTT broker (default: `1883` for unencrypted, `8883` for SSL)
- **MQTT User**: Username for MQTT authentication (optional)
- **MQTT Password**: Password for MQTT authentication (optional)

**Note**: Leave user and password empty if your MQTT broker doesn't require authentication.

#### Trend & Volatility Settings
- **Trend Threshold (%)**: Percentage difference for trend detection (default: 1.0%)
  - Above this value = UP, below = DOWN, otherwise = SIDEWAYS
- **Volatility Low Threshold (%)**: Below this value market is CALM (default: 0.06%)
- **Volatility High Threshold (%)**: Above this value market is VOLATILE (default: 0.12%)

#### Anchor Settings
- **Anchor Take Profit (%)**: Percentage above anchor for profit notification (default: 5.0%)
- **Anchor Max Loss (%)**: Percentage below anchor for loss notification (default: -3.0%)

**Save**: Click "Save" to save all settings. The device will automatically reconnect to MQTT if settings have changed.

### Language Settings

The device supports two languages: **Dutch (Nederlands)** and **English**. All texts are translated, including:

**Display Texts**:
- WiFi setup screens ("Connecting to WiFi", "Configure WiFi", etc.)
- Trend indicators (UP/DOWN/SIDEWAYS or OMHOOG/OMLAAG/ZIJWAARTS)
- Volatility indicators (CALM/MEDIUM/VOLATILE or RUSTIG/GEMIDDELD/VOLATIEL)
- "Wait Xm" messages

**Web Interface**:
- All labels and field names
- All help texts and explanations
- All section headers
- All buttons and messages

**How to Change Language**:
1. **Via Web Interface** (Recommended):
   - Go to the web interface
   - Select your preferred language from the dropdown at the top
   - Click "Save"
   - The language is saved and will persist after reboot

2. **Via platform_config.h** (Default only):
   - Edit `platform_config.h`
   - Set `DEFAULT_LANGUAGE` to `0` (Dutch) or `1` (English)
   - This only affects the initial language on first boot
   - After first boot, the language from Preferences takes precedence

**Note**: The language setting is stored in Preferences, so it persists across reboots. The `DEFAULT_LANGUAGE` in `platform_config.h` is only used as a fallback if no language has been saved yet.

### Settings Explanation

#### Binance Symbol
- **What it does**: Determines which cryptocurrency is monitored
- **Default**: `BTCEUR` (Bitcoin in Euro)
- **Examples**: `BTCUSDT`, `ETHUSDT`, `ADAUSDT`, etc.
- **Usage**: Enter the symbol as used on Binance

#### NTFY Topic
- **What it does**: The topic on which notifications are sent via NTFY.sh
- **Default**: Automatically generated as `[ESP32-ID]-alert` (e.g. `9MK28H3Q-alert`)
  - The ESP32-ID is unique per device (8 characters using Crockford Base32 encoding)
  - Uses safe character set without confusing characters (no 0/O, 1/I/L, U)
  - Character set: `0123456789ABCDEFGHJKMNPQRSTVWXYZ`
  - The ESP32-ID is displayed on the device screen for easy reference
- **Usage**: The default topic is already unique per device, but you can change it in the web interface if needed
- **Important**: 
  - Each device automatically gets a unique topic, preventing conflicts between multiple devices
  - **This is the topic you need to subscribe to in the NTFY app to receive notifications on your mobile**

#### MQTT Settings
- **MQTT Host**: IP address of your MQTT broker (e.g. `192.168.1.100` or `mqtt.example.com`)
- **MQTT Port**: Port of your MQTT broker (default: `1883` for unencrypted, `8883` for SSL)
- **MQTT User**: Username for MQTT authentication
- **MQTT Password**: Password for MQTT authentication
- **Usage**: Leave empty if you don't use MQTT (optional)

#### Thresholds
These determine when you receive notifications for rapid price movements:

- **1 Min Up**: Notification on rising trend > X% per minute (default: `0.5%`)
  - *Example*: At 0.5% you get a notification if the price rises more than 0.5% in 1 minute
  
- **1 Min Down**: Notification on falling trend < -X% per minute (default: `-0.5%`)
  - *Example*: At -0.5% you get a notification if the price falls more than 0.5% in 1 minute

- **30 Min Up**: Notification on rising trend > X% per 30 minutes (default: `2.0%`)
  - *Example*: At 2.0% you get a notification if the price rises more than 2% in 30 minutes

- **30 Min Down**: Notification on falling trend < -X% per 30 minutes (default: `-2.0%`)
  - *Example*: At -2.0% you get a notification if the price falls more than 2% in 30 minutes

#### Anchor Settings
Settings for the anchor price functionality:

- **Take Profit**: Percentage above anchor price for profit notification (default: `5.0%`)
  - *Example*: If you set anchor at €50,000 and take profit at 5%, you get a notification at €52,500
  
- **Max Loss**: Percentage below anchor price for loss notification (default: `-3.0%`)
  - *Example*: If you set anchor at €50,000 and max loss at -3%, you get a notification at €48,500

- **Trend Threshold**: Percentage difference for trend detection (default: `1.0%`)
  - Determines when a trend is considered "UP" or "DOWN" (vs "SIDEWAYS")

#### Volatility Thresholds
Determine when the market is considered calm, medium or volatile:

- **Low Threshold**: Below this value the market is "CALM" (default: `0.06%`)
- **High Threshold**: Above this value the market is "VOLATILE" (default: `0.12%`)
- Between these values the market is "MEDIUM"

## Display Overview

### What is displayed?

The display shows real-time cryptocurrency information in a clear layout:

#### Top Section (Header)
- **Date and Time**: Current date and time (right-aligned)
- **Version Number**: Software version (center)
- **Chart Title**: First letters of your NTFY topic (CYD) or first letters on line 2 (TTGO)

#### Chart Section
- **Live Price Chart**: 
  - 60 data points (1 minute history)
  - Blue line shows price movement
  - Automatic scale adjustment
- **Trend Indicator**: Top-left in the chart
  - 🟢 **UP** (green): Rising trend (>1% over 2 hours)
  - 🔴 **DOWN** (red): Falling trend (<-1% over 2 hours)
  - ⚪ **SIDEWAYS** (gray): No clear trend
  - Shows "Wait Xm" if there's not enough data yet (minimum 2 hours needed)
- **Volatility Indicator**: Bottom-left in the chart
  - 🟢 **CALM** (green): <0.06% average movement
  - 🟠 **MEDIUM** (orange): 0.06% - 0.12% average movement
  - 🔴 **VOLATILE** (red): >0.12% average movement
  - Available immediately from first minute

#### Price Cards (3 blocks)

**1. BTCEUR Card (Main Price)**
- **Title**: Cryptocurrency symbol (e.g. BTCEUR)
- **Current Price**: Real-time price (blue)
- **Anchor Price Info**:
  - **CYD**: Right-aligned, vertically centered:
    - Top (green): Take profit price with percentage (e.g. "+5.00% 52500.00")
    - Middle (orange): Anchor price with percentage difference (e.g. "+2.50% 51250.00")
    - Bottom (red): Stop loss price with percentage (e.g. "-3.00% 48500.00")
  - **TTGO**: Right-aligned, vertically centered (without percentages):
    - Top (green): Take profit price (e.g. "52500.00")
    - Middle (orange): Anchor price (e.g. "50000.00")
    - Bottom (red): Stop loss price (e.g. "48500.00")
- **Interaction**: 
  - **CYD**: Tap on the block to set anchor price
  - **TTGO**: Press reset button (GPIO 0) to set anchor price

**2. 1 Minute Card**
- **Title**: "1m" (TTGO) or "1 min" (CYD)
- **Percentage**: 1-minute return percentage (price change vs 1 minute ago)
- **Right-aligned**:
  - Top (green): Max price in last minute
  - Middle (gray): Difference between max and min
  - Bottom (red): Min price in last minute

**3. 30 Minutes Card**
- **Title**: "30m" (TTGO) or "30 min" (CYD)
- **Percentage**: 30-minute return percentage (price change vs 30 minutes ago)
- **Right-aligned**:
  - Top (green): Max price in last 30 minutes
  - Middle (gray): Difference between max and min
  - Bottom (red): Min price in last 30 minutes

#### Footer Section
- **CYD**: Shows IP address, WiFi signal strength (dBm) and available RAM (kB)
  - Example: `IP: 192.168.1.50   -45dBm   RAM: 125kB`
- **TTGO**: Shows only IP address (due to limited space)
  - Example: `192.168.1.50`

## Difference between CYD and TTGO Display

### CYD 2.4" / 2.8" Display (240x320 pixels)

**Layout Features:**
- **Spacious layout** with more details
- **Chart Title**: Large title above the chart with first letters of NTFY topic
- **Date/Time/Version**: All three on the same line right-aligned (version at 120px, date at 180px, time at 240px)
- **Chart**: 240px wide, 80px high
- **Font Sizes**: Larger for better readability
  - BTCEUR title: 18px
  - BTCEUR price: 16px
  - Anchor labels: 14px
- **Anchor Display**: With percentages (e.g. "+5.00% 52500.00")
- **Footer**: Extended with IP, WiFi signal strength and RAM usage
- **Touchscreen**: Interaction via touch (tap BTCEUR block for anchor)

### TTGO T-Display (135x240 pixels)

**Layout Features:**
- **Compact layout** optimized for small screen
- **No Chart Title**: First letters are on line 2 left (instead of above chart)
- **Date/Time/Version**: Compact on 2 lines
  - Line 1: Date right, Trend indicator left
  - Line 2: First letters left, Version center, Time right, Volatility left
- **Chart**: 135px wide, 60px high (smaller but still clear)
- **Font Sizes**: Smaller for compact display
  - BTCEUR title: 14px
  - BTCEUR price: 12px
  - Anchor labels: 10px
- **Anchor Display**: Prices only without percentages (e.g. "52500.00")
- **Footer**: Only IP address (no room for extra info)
- **Physical Button**: Reset button (GPIO 0) for anchor functionality

### Main Differences Summary

| Feature | CYD | TTGO |
|---------|-----|------|
| Screen size | 240x320 | 135x240 |
| Chart title | Above chart | On line 2 left |
| Date/time layout | 1 line, 3 items | 2 lines, compact |
| Font sizes | Larger (14-18px) | Smaller (10-14px) |
| Anchor display | With percentages | Prices only |
| Footer | IP + RSSI + RAM | IP only |
| Interaction | Touchscreen | Physical button |
| Chart size | 240x80 | 135x60 |

Both layouts show the same information, but the TTGO version is optimized for the smaller screen with a more compact display and smaller fonts.

## Platform-specific Features

### TTGO T-Display
- Compact layout adapted for small screen (135x240)
- Physical reset button for anchor price
- IP address only in footer

### CYD 2.4" / 2.8"
- Spacious layout with more details
- Touchscreen interaction via dedicated "Klik Vast" button
- Two-line footer: WiFi signal/RAM (line 1), IP/version (line 2)

## NTFY.sh Setup and Usage

### What is NTFY.sh?

NTFY.sh is a free, open-source push notification service. It allows you to receive notifications on your phone, tablet or computer without needing to run your own server.

### Install NTFY App

1. **Android/iOS**: Install the official NTFY app from the Play Store or App Store
2. **Desktop**: Download the desktop app from [ntfy.sh/apps](https://ntfy.sh/apps)

### Set NTFY Topic

**Automatic Unique Topic Generation**:
- By default, the device automatically generates a unique NTFY topic using your ESP32's unique ID
- Format: `[ESP32-ID]-alert` (e.g. `9MK28H3Q-alert`)
- The ESP32-ID is derived from the device's MAC address using Crockford Base32 encoding (8 characters)
- Uses safe character set: `0123456789ABCDEFGHJKMNPQRSTVWXYZ` (no confusing 0/O, 1/I/L, U)
- Provides 2^40 = 1.1 trillion possible combinations, ensuring uniqueness
- The ESP32-ID is displayed on the device screen (in the chart title area for CYD, or on line 2 for TTGO)

**Manual Configuration**:
1. **Via Web Interface** (Recommended):
   - Go to your device's web interface
   - The default topic is already set with your unique ESP32 ID
   - You can change it if needed in "NTFY Topic"
   - **Important**: This is the NTFY topic you need to subscribe to in the NTFY app to receive notifications on your mobile
   - Save the settings

2. **Subscribe to the topic in NTFY app**:
   - Open the NTFY app
   - Click "Subscribe to topic"
   - Enter your topic name (shown on the device display or in web interface)
   - Example: If your ESP32-ID is `9MK28H3Q`, subscribe to `9MK28H3Q-alert`
   - Click "Subscribe"

**Note**: The ESP32-ID is displayed on the device screen, making it easy to identify which topic to subscribe to in the NTFY app.

### Notification Types

The device sends the following types of notifications:

#### 1. Trend Change Notifications
- **When**: On trend change (UP ↔ DOWN ↔ SIDEWAYS)
- **Cooldown**: Maximum 1x per 10 minutes (to prevent spam)
- **Content**: 
  - Old and new trend
  - 2-hour return percentage
  - 30-minute return percentage
  - Current volatility status
- **Example**: 
  ```
  Title: "Trend Change: SIDEWAYS → UP"
  Message: "2h: +1.5% | 30m: +0.8% | Vol: MEDIUM"
  ```
- **Color**: Green for UP, red for DOWN, gray for SIDEWAYS

#### 2. 1-Minute Spike Notifications
- **When**: Rapid price movement in 1 minute
- **Conditions**:
  - 1-minute return > threshold (rise) OR < threshold (fall)
  - 5-minute return as filter (to prevent false alerts)
- **Cooldown**: Maximum 1x per 10 minutes
- **Limit**: Maximum 6 notifications per hour
- **Example**:
  ```
  Title: "1m Spike: +0.8%"
  Message: "Price: €52,450.00 (was €52,030.00)"
  ```
- **Color**: Green for rise, red for fall

#### 3. 30-Minute Move Notifications
- **When**: Significant price movement over 30 minutes
- **Conditions**:
  - 30-minute return > threshold (rise) OR < threshold (fall)
  - 5-minute return as filter
- **Cooldown**: Maximum 1x per 10 minutes
- **Limit**: Maximum 6 notifications per hour
- **Example**:
  ```
  Title: "30m Move: +2.5%"
  Message: "Price: €53,125.00 (was €51,840.00)"
  ```
- **Color**: Green for rise, red for fall

#### 4. 5-Minute Move Notifications
- **When**: Significant price movement over 5 minutes
- **Conditions**: 5-minute return > threshold
- **Cooldown**: Maximum 1x per 10 minutes
- **Limit**: Maximum 6 notifications per hour
- **Example**:
  ```
  Title: "5m Move: +1.2%"
  Message: "Price: €52,622.40"
  ```
- **Color**: Green for rise, red for fall

#### 5. Anchor Price Notifications
- **Take Profit**: 
  - **When**: Price reaches take profit percentage above anchor price
  - **Example**: Anchor €50,000, take profit 5% → notification at €52,500
  - **Content**: Anchor price, current price, profit percentage
  - **Color**: Green with 💰 emoji
  - **One-time**: Only sent once per anchor
  
- **Max Loss (Stop Loss)**: 
  - **When**: Price reaches max loss percentage below anchor price
  - **Example**: Anchor €50,000, max loss -3% → notification at €48,500
  - **Content**: Anchor price, current price, loss percentage
  - **Color**: Red with ⚠️ emoji
  - **One-time**: Only sent once per anchor

### Notification Settings Tips

- **Fewer notifications**: Increase threshold values (e.g. 1 Min Up from 0.5% to 1.0%)
- **More notifications**: Decrease threshold values (e.g. 1 Min Up from 0.5% to 0.3%)
- **Only important movements**: Use only 30-minute notifications
- **Fast alerts**: Use 1-minute notifications for quick reactions
- **Anchor tracking**: Set anchor price for important price levels you want to monitor

### NTFY Topic Security (Optional)

For extra security, you can secure your topic with a password:

1. Go to [ntfy.sh](https://ntfy.sh) and create an account
2. Create a secured topic with password
3. In NTFY app: Add the password when subscribing

**Note**: The default NTFY.sh service is public - anyone with your topic name can see your notifications. 
- **Good news**: Each device automatically gets a unique topic based on its ESP32 ID (e.g. `a1b2c3-alert`), making conflicts very unlikely
- For extra security, you can still secure your topic with a password (see above)

### Troubleshooting NTFY

- **Not receiving notifications?**
  - Check if you're correctly subscribed to the right topic
  - Check if the topic name matches exactly (case-sensitive)
  - Check your internet connection on the device
  
- **Notifications arriving late?**
  - NTFY.sh uses free servers that can sometimes have delays
  - For better performance you can run your own NTFY server

## MQTT Integration

### MQTT Topics

The device publishes to the following topics (prefix is platform-specific: `ttgo_crypto`, `cyd24_crypto`, or `cyd28_crypto`):

#### Data Topics (Read-only)
- `{prefix}/values/price` - Current price (float, e.g. `52345.67`)
- `{prefix}/values/return_1m` - 1 minute return percentage (float, e.g. `0.25`)
- `{prefix}/values/return_5m` - 5 minute return percentage (float, e.g. `0.50`)
- `{prefix}/values/return_30m` - 30 minute return percentage (float, e.g. `1.25`)
- `{prefix}/values/timestamp` - Unix timestamp in milliseconds

#### Status Topics
- `{prefix}/trend` - Trend state (string: "UP", "DOWN", or "SIDEWAYS")
- `{prefix}/volatility` - Volatility state (string: "LOW", "MEDIUM", or "HIGH")
- `{prefix}/anchor/event` - Anchor events (JSON with event type, price and timestamp)

#### Config Topics (Read/Write)
These topics can be read (current value) and written (to change):

- `{prefix}/config/spike1m` - 1m spike threshold (float)
- `{prefix}/config/spike5m` - 5m spike filter (float)
- `{prefix}/config/move30m` - 30m move threshold (float)
- `{prefix}/config/move5m` - 5m move filter (float)
- `{prefix}/config/move5mAlert` - 5m move alert threshold (float)
- `{prefix}/config/cooldown1min` - 1m cooldown in seconds (integer)
- `{prefix}/config/cooldown30min` - 30m cooldown in seconds (integer)
- `{prefix}/config/cooldown5min` - 5m cooldown in seconds (integer)
- `{prefix}/config/binanceSymbol` - Binance symbol (string)
- `{prefix}/config/ntfyTopic` - NTFY topic (string)
- `{prefix}/config/anchorTakeProfit` - Anchor take profit % (float)
- `{prefix}/config/anchorMaxLoss` - Anchor max loss % (float)
- `{prefix}/config/trendThreshold` - Trend threshold % (float)
- `{prefix}/config/volatilityLowThreshold` - Volatility low threshold % (float)
- `{prefix}/config/volatilityHighThreshold` - Volatility high threshold % (float)

**To change a setting**: Publish the new value to `{prefix}/config/{setting}/set`

**Example**: To change the 1m spike threshold to 0.5%:
```bash
mosquitto_pub -h 192.168.1.100 -t "ttgo_crypto/config/spike1m/set" -m "0.5"
```

#### Control Topics
- `{prefix}/button/reset/set` - Publish "PRESSED" to set anchor price (string)

### Home Assistant Integration

The device supports **MQTT Auto Discovery** for Home Assistant. This means your device is automatically detected and added to Home Assistant!

#### Automatic Detection

1. **Make sure MQTT is configured** in the web interface
2. **Make sure MQTT Broker is configured** in Home Assistant
3. **Start the device** - it automatically publishes discovery messages
4. **Go to Home Assistant** → Settings → Devices & Services → MQTT
5. **Click "Configure"** on your MQTT integration
6. Your device should automatically appear under "Discovered MQTT devices"

#### Available Entities in Home Assistant

After detection you get the following entities:

**Sensors (Read-only)**:
- `sensor.{device_id}_price` - Current cryptocurrency price
- `sensor.{device_id}_return_1m` - 1 minute return percentage
- `sensor.{device_id}_return_5m` - 5 minute return percentage
- `sensor.{device_id}_return_30m` - 30 minute return percentage
- `sensor.{device_id}_anchor_event` - Anchor events (JSON)

**Numbers (Read/Write)**:
- `number.{device_id}_spike1m` - 1m spike threshold
- `number.{device_id}_spike5m` - 5m spike filter
- `number.{device_id}_move30m` - 30m move threshold
- `number.{device_id}_move5m` - 5m move filter
- `number.{device_id}_move5mAlert` - 5m move alert threshold
- `number.{device_id}_cooldown1min` - 1m cooldown (seconds)
- `number.{device_id}_cooldown30min` - 30m cooldown (seconds)
- `number.{device_id}_cooldown5min` - 5m cooldown (seconds)
- `number.{device_id}_anchorTakeProfit` - Anchor take profit %
- `number.{device_id}_anchorMaxLoss` - Anchor max loss %
- `number.{device_id}_trendThreshold` - Trend threshold %
- `number.{device_id}_volatilityLowThreshold` - Volatility low threshold %
- `number.{device_id}_volatilityHighThreshold` - Volatility high threshold %

**Text (Read/Write)**:
- `text.{device_id}_binanceSymbol` - Binance symbol
- `text.{device_id}_ntfyTopic` - NTFY topic

**Button**:
- `button.{device_id}_reset` - Reset anchor price (click to set anchor)

#### Home Assistant Automations

You can create automations based on MQTT data:

**Example 1: Notification on high price**
```yaml
automation:
  - alias: "Crypto Price Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.ttgo_crypto_xxxxx_price
        above: 55000
    action:
      - service: notify.mobile_app
        data:
          message: "Bitcoin price is above €55,000!"
```

**Example 2: Notification on rapid fall**
```yaml
automation:
  - alias: "Crypto Crash Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.ttgo_crypto_xxxxx_return_1m
        below: -1.0
    action:
      - service: notify.mobile_app
        data:
          message: "Warning: Rapid fall detected!"
```

**Example 3: Dashboard Card**
Add a card to your dashboard:
```yaml
type: entities
entities:
  - entity: sensor.ttgo_crypto_xxxxx_price
    name: Bitcoin Price
  - entity: sensor.ttgo_crypto_xxxxx_return_30m
    name: 30 Min Return
  - entity: sensor.ttgo_crypto_xxxxx_trend
    name: Trend
```

#### Manual MQTT Configuration (without Auto Discovery)

If Auto Discovery doesn't work, you can manually add sensors in Home Assistant:

1. Go to Configuration → Integrations → MQTT → Configure
2. Click "Add Entry"
3. Add a sensor with:
   - **Topic**: `ttgo_crypto/values/price` (or your prefix)
   - **Name**: `Crypto Price`
   - **State Topic**: `ttgo_crypto/values/price`
   - **Unit of Measurement**: `EUR`

### MQTT Usage without Home Assistant

MQTT is optional and can also be used with other systems such as:
- **Node-RED**: For advanced automations
- **OpenHAB**: Home automation platform
- **Grafana**: For data visualization
- **Custom scripts**: Python, Node.js, etc.

If you don't use MQTT, you can leave the MQTT settings empty in the web interface.

## Version History

### Version 3.24
- **TTGO Partition Scheme Fix**: Fixed flash size detection issue for TTGO T-Display
  - TTGO now uses `huge_app` partition scheme with explicit `FlashSize=4M` setting
  - Resolves "Detected size(4096k) smaller than the size in the binary image header(16384k)" error
  - Upload script now correctly configures partition scheme per platform

### Version 3.23
- **SPI Frequency Configuration**: Explicitly set SPI frequencies in platform-specific header files
  - TTGO T-Display: 27 MHz (PINS_TTGO_T_Display.h)
  - CYD 2.8": 55 MHz (PINS_CYD-ESP32-2432S028-2USB.h)
  - CYD 2.4": 40 MHz (PINS_CYD-ESP32-2432S024.h)

### Version 3.22
- **CYD Footer Redesign**: Two-line footer layout
  - Line 1: WiFi signal strength (dBm) left, RAM (kB) right
  - Line 2: IP address left, version number right
- **Anchor Button**: Blue "Klik Vast" button below 30min box (80px wide, 0.66x of original)
- **BTCEUR Box**: Touch functionality removed (now handled by dedicated button)
- **Performance Improvements for CYD**:
  - Increased UI task mutex timeout (50ms → 100ms) for better chart updates
  - Increased LVGL handler frequency (5ms → 3ms) for smoother rendering
  - Decreased API task mutex timeout (300ms → 200ms) for faster UI updates
  - Reduces chart stuttering/hanging issues on CYD devices

### Version 3.21
- Touchscreen responsiveness improvements (5ms polling, PRESSED event support)
- Touchscreen notification format aligned with physical button
- LVGL deprecated define fix (LV_FS_DEFAULT_DRIVER_LETTER)

## License

MIT License - See `LICENSE` file for details.

## Author

Jan Pieter Duhen

## Credits

- **LVGL** - Graphics library for embedded systems
- **Binance** - Cryptocurrency API
- **Arduino_GFX** - Display drivers for ESP32
- **WiFiManager** - WiFi configuration library
- **PubSubClient3** - MQTT client library
# ESP32-Crypto-Alert
