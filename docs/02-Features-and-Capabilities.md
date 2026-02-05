# Chapter 2: Features and Capabilities

## 2.1 Overview of Core Features
ESP32-Crypto-Alert provides a range of advanced features to monitor cryptocurrency prices without overwhelming notifications. The system retrieves real-time data from Bitvavo and focuses on **contextual alerts** through multi-timeframe analysis and smart filters.

Key features:
- Real-time price monitoring of a selected Bitvavo market (e.g., BTC-EUR).
- Multi-timeframe analysis across 1m, 5m, 30m, 2h, 1d, and 7d.
- Contextual alert generation based on price changes, trend, volatility, and **anchor price**.
- Free notifications to your phone via NTFY.sh, plus local web interface, display, and MQTT.
- Fully configurable via web UI and MQTT (no recompilation required).
- Warm-start: retrieves historical data on startup.

## 2.2 Multi-Timeframe Analysis
The system analyzes price action across multiple timeframes simultaneously to filter out noise:

- **1m and 5m**: Detect fast spikes and short moves.
- **30m**: Confirms direction and filters micro-noise.
- **2h**: Provides broader context (trend, range, volatility).
- **1d and 7d**: Context for long-term trend (labels/indicators).

![Multi-timeframe analysis example 1](img/multi-timeframe-1.jpg)  
*Example of multi-timeframe analysis: higher timeframe shows the trend, lower timeframes indicate entry signals.*

## 2.3 Anchor Price and Risk Management
The **anchor price** is your personal reference price (e.g., entry price). The system evaluates alerts relative to this:

- **Take Profit zone**: Percentage above the anchor.
- **Max Loss zone**: Percentage below the anchor.
- Trend-adaptive adjustment.

![Anchor zones example 1](img/anchor-zones-1.png)  
*Example of anchor zones with profit and loss areas.*

## 2.4 Alert Types

### Short-Term Alerts (1m / 5m / 30m)
- **Spike**: Sudden price movement.
- **Move**: Confirmed directional shift.

### 2-Hour Contextual Alerts
- **Breakout / Breakdown**: Price breaks out of the 2h range.  
  ![Breakout pattern](img/breakout-pattern.jpg)  
  *Example of a breakout in crypto.*

- **Compression**: Volatility drops sharply (often a precursor to a big move).  
  ![Volatility compression](img/volatility-compression.jpg)  
  *Volatility Contraction Pattern (VCP) example.*

- **Mean Reversion**: Price returns toward the average.  
  ![Mean reversion example](img/mean-reversion.jpg)  
  *Price pulls back to the average after an extreme move.*

- **Trend Change**: Change in trend direction.
- **Anchor Outside Range**: Anchor no longer within current context.

## 2.5 Smart Filters and Adjustments
- Cooldown periods per timeframe.
- Confluence Mode: alerts only when multiple conditions align.
- Auto-Volatility Mode: thresholds adjust automatically.
- Night mode with time window and extra filters to reduce noise.

## 2.6 Integrations and Output
- NTFY.sh for mobile push notifications.
- Local web interface for monitoring.
- MQTT for Home Assistant.
- Display for direct visual feedback.

## 2.7 Summary
This system serves as a **decision support tool**: it filters noise and provides context, helping you make better decisions — without giving trading advice.

---

*Go to [Chapter 1: Introduction](01-Introduction.md) | [Chapter 3: Hardware Requirements](03-Hardware-Requirements.md)*