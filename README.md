# ESP32 Crypto Alert Monitor (CYD)

## What is this?
This project is a **standalone crypto price monitoring system** that runs on an ESP32
and analyzes real-time price data from Binance.

It provides **context-aware notifications**, not just raw price updates.

The device includes a display and sends notifications via **ntfy.sh**.

👉 The goal is **market awareness and context**, not automated trading.

---

## Who is this for?
This project is intended for people who:
- actively follow crypto prices
- want to know *when something meaningful happens*
- prefer insight over automated trading
- are interested in ESP32 or embedded projects

You **do not need to be a programmer** to use it.
All configuration is done through a web interface.

---

## What does the system do?
The system:
- periodically fetches price data from Binance
- analyzes price movement across multiple timeframes
- determines trend and volatility
- compares price to a user-defined reference (anchor)
- sends notifications when predefined situations occur

Everything runs **locally on the device**.

---

## Key concepts (important)

### Anchor (reference price)
The **anchor** is a price you define as a reference point.
For example:
- your entry price
- an important technical level
- a psychological threshold

Many alerts are evaluated **relative to this anchor**.

---

### Timeframes
The system does not look at a single moment, but at multiple windows:

| Timeframe | Purpose |
|---------|--------|
| 1 minute | fast spikes |
| 5 minutes | short moves |
| 30 minutes | medium-term moves |
| 2 hours | market structure & context |

This helps separate noise from meaningful movement.

---

### Trend
Based on **2-hour price change**, the system determines:
- UP trend
- DOWN trend
- FLAT

Trend information can influence risk settings and alert behavior.

---

### Volatility
Volatility describes how calm or aggressive the market is.
High volatility uses different thresholds than low volatility.

The system can automatically adapt thresholds to volatility.

---

## How the system works (high level)
1. ESP32 connects to WiFi
2. Price data is fetched from Binance
3. Data is stored in internal buffers
4. Indicators are calculated
5. Logic decides whether a situation is alert-worthy
6. Notification is sent (if applicable)
7. Current state is shown on the display

---

## Types of alerts
The system can generate alerts such as:

- ⚡ Fast price spikes (1m)
- 📈 Short-term moves (5m)
- 📊 Medium-term moves (30m)
- 🔄 Mean reversion to 2h average
- 📦 Volatility compression
- 🚀 Breakout / breakdown relative to 2h high/low
- 🎯 Price far outside anchor context
- 💰 Take profit / max loss signals

Cooldowns prevent alert spam.

---

## Configuration (conceptual)
All settings are configurable through the web interface.

You control:
- sensitivity of alerts
- percentage thresholds
- cooldown times
- risk boundaries
- whether thresholds adapt automatically to volatility

You **do not need to understand the internal formulas** to use the system effectively.

---

## What this project is NOT
- ❌ not a trading bot
- ❌ not an automated buy/sell system
- ❌ not financial advice
- ❌ not a high-frequency trading tool

It is designed as a **decision-support and awareness tool**.

---

## Hardware
Tested on:
- ESP32 (CYD / ESP32-2432S028)
- 240×320 TFT display
- No PSRAM required

The system is optimized for **limited-memory environments**.

---

## Installation (brief)
- Flash the firmware
- Connect to WiFi
- Open the web interface
- Configure your settings
- Done

See the installation section in this repository for details.

---

## Final note
Use this system as:
- an extra set of eyes
- a context generator
- a way to reduce emotional noise

Not as an automated truth machine.
