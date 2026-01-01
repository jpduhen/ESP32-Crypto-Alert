# ESP32 Crypto Alert System

**Smart crypto alerts on low-power hardware**

## 1. What is this project?

The ESP32 Crypto Alert System is a standalone crypto market alert device.
It continuously monitors cryptocurrency prices (via Binance) and sends intelligent notifications when relevant market events occur.

Unlike simple price trackers, this system:

- Works without a PC or cloud backend
- Runs fully on an ESP32
- Uses multi-timeframe analysis (1m, 5m, 30m, 2h)
- Understands context: trend, volatility, range, anchor price
- Filters noise to avoid alert spam

It is designed for:

- Traders who want context-aware alerts
- Hobbyists building a dedicated crypto display
- Anyone wanting low-power, always-on alerts

## 2. What does the system do?

At a high level, the system:

- Fetches live prices from Binance
- Builds short-term and medium-term price history
- Calculates:
  - Returns (1m / 5m / 30m / 2h)
  - Trend direction
  - Volatility regime
  - Ranges (high / low / average)
- Compares price behavior against configurable thresholds
- Sends alerts via NTFY.sh when conditions are met
- Shows status on a local display
- Provides a web interface for configuration

## 3. Hardware overview

**Supported boards:**
- ESP32-CYD (Cheap Yellow Display) - 2.4" and 2.8" variants
- ESP32-S3 SuperMini (1.54" display)
- ESP32-S3 GEEK (1.14" display, 2MB PSRAM)
- TTGO T-Display (1.14" display)

**Optional:**
- Touchscreen display
- WiFi connection
- NTFY.sh account (free)

No external servers or databases required.

## 4. Core concepts (important!)

### 4.1 Anchor price

The anchor price is your reference level.

Think of it as:
- "My important price"
- "Where I care about profit/loss"
- "My psychological baseline"

All major alerts are evaluated relative to the anchor.

**Examples:**
- +5% above anchor → Take profit zone
- −3% below anchor → Max loss warning
- Price oscillating near anchor → consolidation

### 4.2 Multi-timeframe logic

The system looks at different time horizons simultaneously:

| Timeframe | Purpose |
|-----------|---------|
| 1m | Detect sudden spikes |
| 5m | Confirm short-term moves |
| 30m | Identify meaningful momentum |
| 2h | Define trend, range, context |

This prevents reacting to noise.

### 4.3 Alert philosophy

Alerts are meant to answer:

**"Is something interesting happening that deserves my attention?"**

Not:
- Every tick
- Every candle
- Every small fluctuation

The system prefers:
- Fewer alerts
- Higher relevance
- Context-rich messages

## 5. Types of alerts

### 5.1 Short-term alerts (1m / 5m / 30m)

- **Spike alert** – sudden short-term movement
- **Move alert** – confirmed directional move
- **Momentum alert** – sustained movement

These are fast and reactive.

### 5.2 2-hour context alerts (important!)

These alerts describe market structure, not just movement:

- **Breakout** – price leaves the 2h range
- **Breakdown** – price drops below range
- **Compression** – volatility collapse (range tightening)
- **Mean reversion** – price far from 2h average, returning
- **Anchor outside range** – anchor no longer inside current market context
- **Trend change** – shift in 2h trend direction

These are slower, more strategic alerts.

## 6. Web interface overview

The device hosts a local web interface where you configure everything.

**You do not need to recompile code to change behavior.**

## 7. Settings explained (plain language)

### 7.1 Basic & connectivity

| Setting | Meaning |
|---------|---------|
| NTFY Topic | Where alerts are sent |
| Binance Symbol | Trading pair (e.g. BTCEUR) |
| Language | UI & alert language |

### 7.2 Anchor & risk management

| Setting | Meaning |
|---------|---------|
| Take Profit | % above anchor that is considered profit |
| Max Loss | % below anchor considered unacceptable loss |

Used for risk-aware alerts, not trading execution.

### 7.3 Signal generation thresholds

These define how sensitive the system is.

**Examples:**
- 1m Spike Threshold = minimum % change to trigger a spike
- 30m Move Threshold = minimum movement to be meaningful
- Trend Threshold = how strong a 2h move must be to count as a trend

**Higher values = fewer alerts**  
**Lower values = more alerts**

### 7.4 Volatility levels

The system classifies volatility as:
- Low
- Normal
- High

This affects:
- Alert sensitivity
- Trend confidence
- Filtering

### 7.5 2-hour alert thresholds

These control structural alerts:

| Setting | Purpose |
|---------|---------|
| Breakout Margin | How far beyond range = breakout |
| Cooldown | Minimum time between alerts |
| Compress Threshold | Defines "tight range" |
| Mean Reversion Distance | How far price must drift from average |

### 7.6 Smart logic & filters

Optional intelligence layers:

- **Trend-adaptive anchors**  
  → Risk thresholds adapt to trend direction

- **Confluence mode**  
  → Alerts only fire when multiple conditions agree

- **Auto-volatility mode**  
  → Thresholds adapt automatically to market behavior

### 7.7 Cooldowns

Prevent alert spam.

Each timeframe has its own cooldown.

### 7.8 Warm-start (advanced)

On boot, the device can fetch historical candles to avoid waiting hours for context.

This makes the system usable almost immediately after restart.

## 8. Recommended presets

### Conservative (few alerts)
- Higher thresholds
- Longer cooldowns
- Confluence ON

### Balanced (default)
- Moderate thresholds
- Mixed alerts
- Good signal/noise ratio

### Aggressive (many alerts)
- Lower thresholds
- Short cooldowns
- More suitable for scalpers

## 9. How to interpret alerts

**General guidance:**

- 1m alerts → attention
- 5m / 30m alerts → momentum
- 2h alerts → context change

**Multiple alerts close together usually indicate:**
- Transition between regimes
- Breakout or breakdown
- Volatility expansion

## 10. What this system is NOT

- ❌ Not a trading bot
- ❌ Not financial advice
- ❌ Not predictive AI
- ❌ Not a price chart replacement

**It is a decision support tool.**

## 11. Who is this for?

- Crypto traders who want alerts with context
- Makers building a dedicated crypto device
- People who want insight without staring at charts

---

## Quick Start

1. Flash the firmware
2. Connect to WiFi
3. Open the web interface
4. Configure your settings
5. Set your anchor price
6. Done!

See `README_QUICKSTART.md` for detailed installation steps.

---

## Installation

- Flash the firmware to your ESP32
- Connect the device to WiFi
- Open the web interface (IP address shown on display)
- Configure your settings
- Set your anchor price
- Start receiving alerts

No external servers or databases required.

---

## Final note

Use this system as:
- An extra set of eyes
- A context generator
- A way to reduce emotional noise

**Not as an automated truth machine.**
