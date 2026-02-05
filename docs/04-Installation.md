# Chapter 4: Installation

## 4.1 Overview
This chapter guides you step by step through installing the firmware on your ESP32 board. We use the **Arduino IDE** (recommended for beginners) as the primary method.

The installation consists of:
1. Preparing Arduino IDE with ESP32 support
2. Installing USB drivers
3. Downloading and configuring the code for your board
4. Flashing the firmware via USB
5. Initial WiFi setup

**Estimated time**: 15-30 minutes.

![Arduino IDE overview](img/arduino-ide-overview.jpg)  
*Arduino IDE 2.x with ESP32 board support.*

## 4.2 Installing and Preparing Arduino IDE

1. Download and install the latest Arduino IDE from https://www.arduino.cc/en/software (version 2.x recommended).

2. **Add ESP32 board support**:
   - Go to **File → Preferences**.
   - Paste the following URL into "Additional Boards Manager URLs":  
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Go to **Tools → Board → Boards Manager**.
   - Search for "esp32" and install **esp32 by Espressif Systems** (latest version).

![Boards Manager ESP32](img/boards-manager-esp32.jpg)  
*Installation of the ESP32 package in Boards Manager.*

## 4.3 Installing USB Drivers
Most ESP32 boards use a CH340, CH341, or CP210x USB chip. ESP32-S3 boards (such as Waveshare) often use native USB or CP210x.

- **Windows**: Download and install:
  - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH340/CH341: https://www.wch.cn/downloads/CH341SER_EXE.html
- **macOS / Linux**: Usually recognized automatically.

![USB driver installation](img/usb-driver-install.jpg)  
*Example of CP210x driver installation on Windows.*

Check in Device Manager (Windows) whether the COM port appears when you connect the board.

## 4.4 Downloading and Configuring the Code

1. Go to https://github.com/jpduhen/ESP32-Crypto-Alert
2. Click **Code → Download ZIP** or clone with Git.
3. Open the folder and double-click `ESP32-Crypto-Alert.ino` to load it in Arduino IDE.

4. **Select board** (Tools → Board):
   - Cheap Yellow Display: **ESP32 Dev Module**
   - TTGO T-Display: **TTGO T-Display**
   - Waveshare ESP32-S3-GEEK: **ESP32S3 Dev Module**

![Board selection](img/board-selection.jpg)  
*Selecting ESP32S3 Dev Module for S3 boards.*

5. **Important settings** (Tools menu):
   - Upload Speed: 921600 (lower to 115200 if issues occur)
   - Flash Mode: QIO
   - Partition Scheme: "Default 4MB with spiffs" or "Huge APP"
   - PSRAM: **Disabled** for CYD24/CYD28 and TTGO, **Enabled** for S3 boards with PSRAM

6. **Board-specific define**:
   - In `platform_config.h` select the right board (`PLATFORM_CYD24`, `PLATFORM_CYD28`, `PLATFORM_TTGO`, `PLATFORM_ESP32S3_GEEK`, `PLATFORM_ESP32S3_SUPERMINI`, `PLATFORM_ESP32S3_4848S040`).
   - Keep exactly one platform enabled.

![Code defines](img/code-defines.jpg)  
*Example of board defines at the top of the sketch.*

## 4.5 Flashing the Firmware

1. Connect the board via USB.
2. **Enter boot mode** (required on many boards):
   - Hold down the **BOOT** button.
   - Briefly press **EN/RESET**.
   - Release BOOT.

![Boot buttons](img/boot-buttons.jpg)  
*BOOT and EN buttons on a typical ESP32 board.*

3. Select the correct port under Tools → Port.
4. Click the **Upload** button (arrow on the right).

![Upload process](img/upload-process.jpg)  
*Successful upload process in Arduino IDE.*

On success, you will see "Done uploading" and the board will restart.

## 4.6 First Startup and WiFi Setup
- After uploading, the board starts in **Access Point (AP) mode**.
- Look in your WiFi list for "no-net" and connect to it.
- Open http://192.168.4.1 in your browser.
- Enter your home WiFi details and save.
- The board restarts and connects to your network.

![WiFi captive portal](img/wifi-captive-portal.jpg)  
*Example of the WiFi setup portal (captive portal).*

You are now ready for configuration via the web interface (Chapter 5).

## 4.7 Common Issues
- **No COM port**: Install drivers or try a different cable.
- **Upload failed**: Manually enter boot mode.
- **Brownout**: Use a better power supply (min. 1A-2A).

---

*Go to [Chapter 3: Hardware Requirements](03-Hardware-Requirements.md) | [Chapter 5: Configuration via Web Interface](05-Web-Interface-Configuration.md)*