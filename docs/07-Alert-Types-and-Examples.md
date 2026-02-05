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

**Example message (title + text):**  
`BTC-EUR 1m Spike`  
`68250 (05-02-2026/01:23:45)
1m UP spike: +0.85% (5m: +0.42%)
1m High: 68410
1m Low: 67980`

**Meaning:** 1m spike confirmed by 5m in the same direction. May include extra volume lines (VOLUME// or VOLUME=) depending on volume/range status.

![Spike chart](img/alert-spike-chart.jpg)  
*Typical chart pattern of a spike that triggers this alert.*

### Move
Confirmed directional change on 5m or 30m.

**Example message (30m move, title + text):**  
`BTC-EUR 30m Move`  
`67820 (05-02-2026/02:10:12)
30m DOWN move: -1.24% (5m: -0.62%)
30m High: 68940
30m Low: 67610`

**Example message (5m move, title + text):**  
`BTC-EUR 5m Move`  
`68120 (05-02-2026/02:18:09)
5m UP move: +0.92% (30m: +0.48%)
5m High: 68260
5m Low: 67920`

**Meaning:** Directional move on 5m or 30m with min/max context. In night mode, 5m moves can be additionally filtered (30m direction must match).

### Confluence (1m+5m+Trend)
Confluence alert: 1m and 5m confirm each other within the time window and align with the 30m trend.

**Example message (title + text):**  
`BTC-EUR Confluence (1m+5m+Trend)`  
`68290 (05-02-2026/02:25:33)
Confluence UP
1m: +0.62%
5m: +1.04%
30m Trend: UP ( +0.55%)`

**Meaning:** Stronger confirmation (less noise, higher quality signal).

## 7.3 2-Hour Contextual Alerts
These alerts are more strategic and point to larger market changes.

### Breakout
Price closes above the highest price of the past 2 hours.

**Example message (title + text):**  
`BTC-EUR 2h breakout ↑`  
`69120 (05-02-2026/03:05:33)
Price > 2h High 69010
Avg: 68450 Range: 1.82%`

![Breakout chart](img/alert-breakout-chart.jpg)  
*Breakout above the 2h range.*

### Breakdown
Price closes below the lowest price of the past 2 hours.

**Example message (title + text):**  
`BTC-EUR 2h breakdown ↓`  
`67280 (05-02-2026/03:48:09)
Price < 2h Low: 67340
Avg: 68120 Range: 1.76%`

### Compression
The 2h range shrinks sharply → often a precursor to a big move.

**Example message (title + text):**  
`BTC-EUR 2h Compression`  
`68110 (05-02-2026/04:12:27)
Range: 0.92% (<1.10%)
2h High: 68440
2h Avg: 68190
2h Low: 67870`

![Compression chart](img/alert-compression-chart.jpg)  
*Volatility contraction pattern (VCP) in the 2h timeframe.*

### Mean Reversion
Price deviates strongly from the 2h average and starts returning.

**Example message (title + text):**  
`BTC-EUR 2h Mean Touch`  
`67990 (05-02-2026/04:55:10)
Touched 2h avg from below
after 2.35% away`

![Mean reversion chart](img/alert-mean-reversion-chart.jpg)  
*Price pulls back toward the average after an extreme move.*

### Trend Change (2h)
Change in the 2h trend direction.

**Example message (title + text):**  
`BTC-EUR Trend Change`  
`68214.25 (05-02-2026/05:12:33)
Trend change: 2h// → 2h=
2h: +1.18%
30m: +0.42%
Volatility: Normal
1d trend: 1d=
7d trend: 7d//`

### 1d Trend Change
Change in the 1d trend direction.

**Example message (title + text):**  
`BTC-EUR 1d Trend Change`  
`68190.10 (05-02-2026/06:05:02)
1d trend change: 1d= → 1d//
1d: +2.36%
2h trend: 2h//`

### 7d Trend Change
Change in the 7d trend direction.

**Example message (title + text):**  
`BTC-EUR 7d Trend Change`  
`67950.55 (05-02-2026/06:40:18)
7d trend change: 7d\\ → 7d=
7d: -1.05%
2h trend: 2h=`

### Anchor Outside Range
The anchor price lies outside the current 2h range.

**Example message (title + text):**  
`BTC-EUR Anchor outside 2h`  
`68220 (05-02-2026/05:30:44)
Anchor 70100 outside 2h
2h High: 68980
2h Avg: 68420
2h Low: 67710`

![Anchor outside range](img/alert-anchor-outside.jpg)  
*Situation where the anchor is no longer within the current price action.*

## 7.4 Interpretation and Tips
- **Emojis** provide quick visual indication of urgency and direction.
- Always read the context after the emoji: relation to anchor and trend is crucial.
- Night mode can further filter 5m alerts (direction confirmation with 30m and higher thresholds).

By combining these alerts with your own analysis, you have a powerful tool to notice meaningful market events without constantly watching charts.

---

*Go to [Chapter 6: Understanding Core Concepts](06-Core-Concepts.md) | [Chapter 8: Integration with External Systems](08-External-Systems-Integration.md)*