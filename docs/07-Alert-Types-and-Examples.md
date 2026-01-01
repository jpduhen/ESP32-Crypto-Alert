# Chapter 7: Alert Types and Examples

## 7.1 Introduction
This chapter describes all the alert types that ESP32-Crypto-Alert can generate, including their meaning, conditions, and concrete example messages. Every alert always includes rich context: direction (up/down), timeframe, percentage change, relation to the anchor price, and the current trend.

Alerts appear on:
- The local TFT display
- NTFY.sh push notifications
- The web interface
- Via MQTT (for Home Assistant)

![Alert on display](img/alert-on-display.jpg)  
*Example of an alert displayed on a Cheap Yellow Display.*

![NTFY alert example](img/ntfy-alert-spike.jpg)  
*The same alert as a push notification in the NTFY app.*

## 7.2 Short-Term Alerts (1m / 5m / 30m)
These alerts respond to rapid market developments and are only sent if they fit within the 2h context.

### Spike
A sudden, strong price movement on 1m or 5m.

**Example message:**  
`🔥 SPIKE UP +3.8% on 5m – Price breaks above anchor (+2.1%) in strong uptrend`

![Spike chart](img/alert-spike-chart.jpg)  
*Typical chart pattern of a spike that triggers this alert.*

### Move
Confirmed directional change on 5m or 30m.

**Example message:**  
`➡️ MOVE DOWN -2.4% on 30m – Momentum reverses below anchor`

### Momentum
Sustained directional strength on 30m.

**Example message:**  
`🚀 MOMENTUM UP – Strong continuation, +5.6% above 2h average`

![Momentum chart](img/alert-momentum-chart.jpg)  
*Momentum buildup within an existing trend.*

## 7.3 2-Hour Contextual Alerts
These alerts are more strategic and point to larger market changes.

### Breakout
Price closes above the highest price of the past 2 hours.

**Example message:**  
`📈 BREAKOUT UP +4.2% – Price above 2h high, strongly above anchor in uptrend`

![Breakout chart](img/alert-breakout-chart.jpg)  
*Breakout above the 2h range.*

### Breakdown
Price closes below the lowest price of the past 2 hours.

**Example message:**  
`📉 BREAKDOWN DOWN -3.9% – Price below 2h low, approaching max loss zone`

### Compression
The 2h range shrinks sharply → often a precursor to a big move.

**Example message:**  
`🗜️ COMPRESSION – 2h range only 1.1% (lowest in 24h), prepare for volatility expansion`

![Compression chart](img/alert-compression-chart.jpg)  
*Volatility contraction pattern (VCP) in the 2h timeframe.*

### Mean Reversion
Price deviates strongly from the 2h average and starts returning.

**Example message:**  
`↩️ MEAN REVERSION UP – Price +2.7% back toward 2h average after -8% dip`

![Mean reversion chart](img/alert-mean-reversion-chart.jpg)  
*Price pulls back toward the average after an extreme move.*

### Trend Change
Change in the 2h trend direction (e.g., from higher highs to lower highs).

**Example message:**  
`🔄 TREND CHANGE – From uptrend to potential downtrend (lower high formed)`

### Anchor Outside Range
The anchor price lies outside the current 2h range.

**Example message:**  
`⚠️ ANCHOR OUTSIDE RANGE – Your anchor is below current market, increased risk`

![Anchor outside range](img/alert-anchor-outside.jpg)  
*Situation where the anchor is no longer within the current price action.*

## 7.4 Interpretation and Tips
- **Emojis** provide quick visual indication of urgency and direction.
- Always read the context after the emoji: relation to anchor and trend is crucial.
- With the **Conservative** preset, you mainly receive high-quality 2h contextual alerts.
- With **Aggressive**, you get more frequent short-term alerts (spikes/momentum).

By combining these alerts with your own analysis, you have a powerful tool to notice meaningful market events without constantly watching charts.

---

*Go to [Chapter 6: Understanding Core Concepts](06-Core-Concepts.md) | [Chapter 8: Integration with External Systems](08-External-Systems-Integration.md)*