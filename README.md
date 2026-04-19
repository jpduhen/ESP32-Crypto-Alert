# ESP32-Crypto-Alert

[![GitHub stars](https://img.shields.io/github/stars/jpduhen/ESP32-Crypto-Alert?style=social)](https://github.com/jpduhen/ESP32-Crypto-Alert/stargazers)
[![GitHub license](https://img.shields.io/github/license/jpduhen/ESP32-Crypto-Alert)](https://github.com/jpduhen/ESP32-Crypto-Alert/blob/main/LICENSE)

**🇳🇱 Deze handleiding is ook beschikbaar in het Nederlands** → [README_NL.md](README_NL.md)

A standalone ESP32 device that monitors cryptocurrency prices in real-time via Bitvavo and generates **contextual alerts** based on multi-timeframe analysis.  
No constant notifications — only meaningful signals like spikes, breakouts, compression, and trend changes, with smart filters and your personal **anchor price** as reference.

### Key Features
- Multi-timeframe analysis (1m, 5m, 30m, 2h, 1d, 7d)
- Contextual alerts with anchor price and risk management zones
- Notifications via NTFY.sh (push to phone)
- Local web interface for configuration and monitoring
- MQTT integration (e.g., Home Assistant)
- Night mode with time window and extra filters (configurable via Web UI/MQTT)
- Support for popular ESP32 boards with TFT displays
- Display rotation settings
- Fully configurable without recompiling

## Detailed User Guide

The complete English user guide is split into separate chapters for better readability:

[Release notes](RELEASE_NOTES.md)

1. [Chapter 1: Introduction](docs/01-Introduction.md)  
   Project overview, target audience, and unique features

2. [Chapter 2: Features and Capabilities](docs/02-Features-and-Capabilities.md)  
   Core functionality, multi-timeframe analysis, and alert types

3. [Chapter 3: Hardware Requirements](docs/03-Hardware-Requirements.md)  
   Recommended boards, pinouts, and compatibility

4. [Chapter 4: Installation](docs/04-Installation.md)  
   Arduino IDE setup, flashing, and initial WiFi configuration

5. [Chapter 5: Configuration via Web Interface](docs/05-Web-Interface-Configuration.md)  
   Dashboard, basic and advanced settings, NTFY setup

6. [Chapter 6: Understanding Core Concepts](docs/06-Core-Concepts.md)  
   Multi-timeframe analysis, anchor price, 2h context, and confluence

7. [Chapter 7: Alert Types and Examples](docs/07-Alert-Types-and-Examples.md)  
   All alert types with sample messages and charts

8. [Chapter 8: Web UI Settings](docs/08-WebUI-Settings.md)  
   Explanation of all Web UI components and their effects

9. [Chapter 9: Integration with External Systems](docs/09-External-Systems-Integration.md)  
   MQTT, Home Assistant dashboards, and automations

10. [Chapter 10: Advanced Usage and Customization](docs/10-Advanced-Usage-and-Customization.md)  
    Modifying code, custom thresholds, OTA updates, and more

## Developer notes

- [Metric contract](docs/METRICS_CONTRACT.md) — firmware vs JSON vs MQTT names, regression/trend % vs classic returns, warm/mixed/live semantics, 2h trend vs 2h price stats.
- [NTFY / WS cleanup tracker](docs/ntfy_ws_cleanup_tracker.md) — productie delivery path, diagnostics flags, phased cleanup (Fase 1–4 closed as of 2026-03-27).
- **Regime Engine (v1):** `src/RegimeEngine/` — regime snapshot for UI/settings; see [RELEASE_NOTES.md](RELEASE_NOTES.md).

### Firmware V2 (preparation)

A controlled rebuild (ESP-IDF skeleton under `firmware-v2/`, docs) is tracked on branch [**`v2/foundation`**](https://github.com/jpduhen/ESP32-Crypto-Alert/tree/v2/foundation). Production-ready firmware remains on **`main`** until V2 matures.

**Primary V2 status (decisions, priorities, migration):** [firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md](firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md).  
**Governance summary:** [docs/architecture/V2_WORKDOCUMENT_MASTER.md](docs/architecture/V2_WORKDOCUMENT_MASTER.md).  
**Build V2 firmware (ESP-IDF v5.4.2):** [firmware-v2/BUILD.md](firmware-v2/BUILD.md) · [firmware-v2/README.md](firmware-v2/README.md).

### V1 vs V2 (B-001)

- **V1 (Arduino, repo root)** is de **formele referentie-/onderhoudslijn** op branch **`main`**; optionele tag **`v1-reference-frozen`** — zie **[docs/V1_REFERENCE_FREEZE_B001.md](docs/V1_REFERENCE_FREEZE_B001.md)**.
- **V2** (`firmware-v2/`, branch `v2/foundation`) is de **actieve herbouw**; geen verplichting tot feature-pariteit tot die beslissing expliciet is.

## Quick Start
1. Choose a compatible board (e.g., Cheap Yellow Display).
2. Follow [Chapter 4: Installation](docs/04-Installation.md).
3. Configure via the web interface ([Chapter 5](docs/05-Web-Interface-Configuration.md)).
4. Start receiving your first alerts!

## Contributing
Suggestions, bug reports, and pull requests are very welcome!  
Open an issue or PR on [GitHub](https://github.com/jpduhen/ESP32-Crypto-Alert).

## Disclaimer
This project provides **no financial advice**. Cryptocurrency markets are highly volatile. Use at your own risk.

---

**Last guide update: March 27, 2026**

Enjoy your ESP32-Crypto-Alert! 🚀