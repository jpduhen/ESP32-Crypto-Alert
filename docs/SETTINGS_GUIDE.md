# Settings Guide & Recommended Presets

This document explains all configuration options
and provides **recommended presets** for different user profiles.

---

## Anchor & Risk Settings

### Anchor Price
Reference price used for context and risk calculations.

Typical anchors:
- Entry price
- Daily open
- Key technical level

---

### Take Profit (%)
Defines how far price must move above anchor to trigger a profit alert.

Higher values:
- Fewer alerts
- Larger moves

Lower values:
- More frequent alerts
- Faster signals

---

### Max Loss (%)
Defines risk boundary below anchor.

Negative value.

Important:
This does NOT execute a stop loss.
It only notifies.

---

## Signal Thresholds

### 1m / 5m / 30m Thresholds
Minimum percentage return required to trigger alerts.

Rule of thumb:
- Short timeframe → higher noise → higher threshold
- Longer timeframe → lower noise → lower threshold

---

### Trend Threshold (2h)
Determines when market is considered trending.

Lower values:
- More trend changes
Higher values:
- More stable trend state

---

## Volatility Settings

### Volatility Low / High
Defines boundaries for volatility classification.

Used by:
- Auto-Volatility Mode
- Adaptive thresholds

---

## 2-Hour Context Alerts

### Breakout Margin
Minimum % outside 2h high/low to qualify as breakout.

---

### Mean Reversion Distance
Minimum % away from 2h average before reversion logic arms.

---

### Compression Threshold
Maximum allowed 2h range % for compression alert.

---

## Cooldowns
Minimum time between alerts of same type.

Higher cooldown:
- Fewer alerts
- Cleaner signal

Lower cooldown:
- Faster feedback
- More noise

---

## Warm-Start
Fetches historical Binance data on boot.

Purpose:
- Immediate context
- No cold start blindness

Recommended:
Always enabled unless debugging.

---

# ✅ Recommended Presets

## 🟢 Conservative (Context-only)
Best for long-term holders.

- 1m Spike: OFF or >0.5%
- 5m Move: >1.0%
- 30m Move: >1.5%
- Trend Threshold: >1.5%
- Compression: ON
- Mean Reversion: ON
- Cooldowns: Long

---

## 🔵 Balanced (Default)
Best general-purpose configuration.

- 1m Spike: 0.25–0.35%
- 5m Move: 0.7–1.0%
- 30m Move: 1.2–1.5%
- Trend Threshold: ~1.2%
- Auto-Volatility: ON
- Confluence Mode: ON
- Cooldowns: Medium

---

## 🔴 Active (Fast reaction)
For short-term traders.

- 1m Spike: 0.15–0.25%
- 5m Move: 0.5–0.7%
- 30m Move: 0.8–1.2%
- Trend Threshold: <1.0%
- Auto-Volatility: ON
- Cooldowns: Short

⚠️ Expect more alerts.

---

## Final advice
Start conservative.
Observe behavior.
Adjust gradually.

If alerts feel emotional → thresholds are too tight.
If nothing ever triggers → thresholds are too wide.
