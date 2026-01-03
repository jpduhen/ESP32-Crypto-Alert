# ESP32-Crypto-Alert

[![GitHub stars](https://img.shields.io/github/stars/jpduhen/ESP32-Crypto-Alert?style=social)](https://github.com/jpduhen/ESP32-Crypto-Alert/stargazers)
[![GitHub license](https://img.shields.io/github/license/jpduhen/ESP32-Crypto-Alert)](https://github.com/jpduhen/ESP32-Crypto-Alert/blob/main/LICENSE)

**🇳🇱 Deze handleiding is ook beschikbaar in het Nederlands** → [README_NL.md](README_NL.md)

A standalone ESP32 device that monitors cryptocurrency prices in real-time via Binance and generates **contextual alerts** based on multi-timeframe analysis.  
No constant notifications — only meaningful signals like spikes, breakouts, compression, and trend changes, with smart filters and your personal **anchor price** as reference.

### Key Features
- Multi-timeframe analysis (1m, 5m, 30m, 2h)
- Contextual alerts with anchor price and risk management zones
- Notifications via NTFY.sh (push to phone)
- Local web interface for configuration and monitoring
- MQTT integration (e.g., Home Assistant)
- Support for popular ESP32 boards with TFT displays
- Display rotation and color inversion settings
- Fully configurable without recompiling

## Detailed User Guide

The complete English user guide is split into separate chapters for better readability:

1. [Chapter 1: Introduction](docs/en/01-Introduction.md)  
   Project overview, target audience, and unique features

2. [Chapter 2: Features and Capabilities](docs/en/02-Features-and-Capabilities.md)  
   Core functionality, multi-timeframe analysis, and alert types

3. [Chapter 3: Hardware Requirements](docs/en/03-Hardware-Requirements.md)  
   Recommended boards, pinouts, and compatibility

4. [Chapter 4: Installation](docs/en/04-Installation.md)  
   Arduino IDE setup, flashing, and initial WiFi configuration

5. [Chapter 5: Configuration via Web Interface](docs/en/05-Web-Interface-Configuration.md)  
   Dashboard, basic and advanced settings, NTFY setup

6. [Chapter 6: Understanding Core Concepts](docs/en/06-Core-Concepts.md)  
   Multi-timeframe analysis, anchor price, 2h context, and confluence

7. [Chapter 7: Alert Types and Examples](docs/en/07-Alert-Types-and-Examples.md)  
   All alert types with sample messages and charts

8. [Chapter 8: Integration with External Systems](docs/en/08-External-Systems-Integration.md)  
   MQTT, Home Assistant dashboards, and automations

9. [Chapter 9: Advanced Usage and Customization](docs/en/09-Advanced-Usage-and-Customization.md)  
   Modifying code, custom thresholds, OTA updates, and more

10. [Chapter 10: Troubleshooting and FAQ](docs/en/10-Troubleshooting-and-FAQ.md)  
    Common issues and solutions

## Quick Start
1. Choose a compatible board (e.g., Cheap Yellow Display).
2. Follow [Chapter 4: Installation](docs/en/04-Installation.md).
3. Configure via the web interface ([Chapter 5](docs/en/05-Web-Interface-Configuration.md)).
4. Start receiving your first alerts!

## Contributing
Suggestions, bug reports, and pull requests are very welcome!  
Open an issue or PR on [GitHub](https://github.com/jpduhen/ESP32-Crypto-Alert).

## Disclaimer
This project provides **no financial advice**. Cryptocurrency markets are highly volatile. Use at your own risk.

---

**Last guide update: January 2026**

Enjoy your ESP32-Crypto-Alert! 🚀