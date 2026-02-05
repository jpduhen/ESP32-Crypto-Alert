# Chapter 1: Introduction

## 1.1 Project Overview
ESP32-Crypto-Alert is a standalone device based on the ESP32 microcontroller, designed to monitor cryptocurrency prices in real-time and generate contextual alerts. It fetches live price data from Bitvavo and analyzes price movements across multiple timeframes (1 minute, 5 minutes, 30 minutes, 2 hours, 1 day, and 7 days). Instead of constant notifications, it focuses on relevant changes such as spikes, momentum shifts, or structural breaks.

The device operates locally with external services for data and notifications: price data via Bitvavo and alerts via NTFY.sh. Alerts are displayed on a built-in screen, accessible through a web interface, and optionally forwarded via MQTT to Home Assistant.

![Cheap Yellow Display front](img/cyd-front.webp)  
*ESP32-2432S028R "Cheap Yellow Display" – a popular choice for this project.*

![TTGO T-Display](img/ttgo-t-display.avif)  
*LilyGO TTGO T-Display – compact board with 1.14" screen.*

![ESP32-S3 Supermini](img/esp32-s3-supermini.webp)  
*ESP32-S3 Supermini – mini variant with ESP32-S3.*

![Waveshare ESP32-S3-GEEK](img/waveshare-s3-geek.jpg)  
*Waveshare ESP32-S3-GEEK – powerful S3 board with 1.14" IPS display.*

## 1.2 Target Audience and Use Cases
This tool is intended for:
- Individual crypto traders who want to track price changes without constantly checking apps.
- Smart home users (e.g., Home Assistant enthusiasts).
- Hobbyists interested in IoT and cryptocurrency.

Examples:
- Place the device on your desk for visual updates.
- Integrate with NTFY for mobile notifications.
- Connect to Home Assistant for automations.

## 1.3 Unique Features
- **Anchor Price Concept**: User-defined reference price to contextualize profit/loss.
- **Multi-Timeframe Analysis**: Combines short- and long-term movements to filter noise.
- **Night Mode**: Optional night window with additional filters to reduce noise.
- **Fully Local Configuration**: Via web UI – no recompilation required.
- **Integrations**: NTFY.sh and optional MQTT (e.g., Home Assistant).
- **Broad Hardware Support**: Various ESP32 boards with TFT displays.

## 1.4 Warnings and Disclaimer
- This project provides **no financial advice**.
- Cryptocurrency markets are volatile; use at your own risk.
- Ensure a stable WiFi connection (data is sourced from the Bitvavo API).

## 1.5 Next Steps
In the following chapters, we will dive deeper into the features, hardware, installation, and configuration.

---

*Go to [Chapter 2: Features and Capabilities](02-Features-and-Capabilities.md)*