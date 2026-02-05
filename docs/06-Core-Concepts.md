# Chapter 6: Understanding Core Concepts

## 6.1 Introduction
This chapter explains the key technical and trading concepts that form the foundation of the ESP32-Crypto-Alert's alert logic. A solid understanding of these will help you interpret alerts correctly and tune thresholds/filters to your style.

All calculations are performed locally on the ESP32 using candle data from Bitvavo across multiple timeframes: 1 minute, 5 minutes, 30 minutes, and 2 hours (plus 1d and 7d as context labels).

![Multi-timeframe overview](img/multi-timeframe-overview.jpg)  
*Overview of the four timeframes used and their role in the analysis.*

## 6.2 Multi-Timeframe Analysis
The system combines information from multiple timeframes to reduce noise and signal only confirmed movements.

- **1 minute & 5 minutes**: Detect fast, short-term spikes and moves.
- **30 minutes**: Confirms direction and filters micro-noise.
- **2 hours**: Provides broader market context (trend direction, range, volatility).
- **1 day & 7 days**: Context for long-term trend (labels/indicators).

By issuing alerts only when multiple timeframes align (confluence), false signals are avoided in sideways or choppy markets.

![Multi-timeframe confluence](img/multi-timeframe-confluence.jpg)  
*Example of how a short-term spike only triggers an alert if it fits within the 2h trend.*

## 6.3 Anchor Price
The **anchor price** is your personal reference price (e.g., average entry price, important support/resistance level, or psychological round number).

Key functions:
- All alerts are contextualized relative to the anchor ("+4.2% above anchor").
- Defines two risk management zones:
  - **Take Profit zone**: Percentage above the anchor.
  - **Max Loss zone**: Percentage below the anchor.
- Trend-adaptive: interpretation takes the current 2h trend direction into account.

![Anchor price zones](img/anchor-price-zones.jpg)  
*Visual representation of anchor price with take profit and max loss zones on a crypto chart.*

## 6.4 2-Hour Context and Structural Alerts
The 2-hour timeframe serves as the basis for all longer-term alerts:

- **Range**: Highest and lowest price of the last 2 hours.
- **Average**: Weighted midpoint price.
- **Volatility**: Width of the range and average candle size.

Important structural alerts at this level:

- **Breakout / Breakdown**: Price closes outside the 2h range → potential trend acceleration.  
  ![Breakout example](img/breakout-2h.jpg)  
  *Breakout above the 2h high.*

- **Compression**: The 2h range shrinks sharply → often a precursor to an explosive move.  
  ![Compression example](img/compression-2h.jpg)  
  *Volatility compression in the 2h timeframe.*

- **Mean Reversion**: Price deviates strongly from the 2h average and starts returning.  
  ![Mean reversion example](img/mean-reversion-2h.jpg)  
  *Price pulls back toward the 2h average.*

- **Trend Change**: Shift from higher highs/higher lows to the opposite.
- **Anchor Outside Range**: The anchor is no longer within the current 2h range → warning of increased risk.

## 6.5 Short-Term Alerts
These alerts (Spike, Move) are only sent if they fit within the 2h context:

- **Spike**: Rapid price change on 1m/5m.
- **Move**: Confirmed directional shift on 5m/30m.

![Short-term spike](img/short-term-spike.jpg)  
*Example of a spike triggering an alert within an uptrend.*

## 6.6 Summary of the Philosophy
ESP32-Crypto-Alert is built as a **decision support tool**:
- It filters noise through multi-timeframe confluence.
- It provides context via the anchor price and 2h structural analysis.
- It offers no trading advice but helps you spot relevant market events faster.

With this understanding, you’ll be better equipped to explore the different alert types and their example messages in the next chapter.

---

*Go to [Chapter 5: Configuration via Web Interface](05-Web-Interface-Configuration.md) | [Chapter 7: Alert Types and Examples](07-Alert-Types-and-Examples.md)*