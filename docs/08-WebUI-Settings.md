## Chapter 8: Web UI settings (explained)

This page explains every Web UI component in plain language. You will see what each setting does, why you might change it, and how it affects alerts and behavior.
Changes are saved on the device and applied after **Save**, unless noted otherwise.

## Status and Anchor

- **Reference price (Anchor)**: Your personal baseline price.
  - Effect: All "above/below anchor" logic uses this point.
  - The **Set Anchor** button applies immediately (no reboot).
- **Status box**: Live info like price, trends (2h/1d/7d), volatility, volume, and returns.
  - Effect: None. Monitoring only.

## Basic & Connectivity

- **NTFY Topic**: Where push notifications are delivered.
  - Effect: Changes the target topic on NTFY.sh.
- **Default unique NTFY topic**: Generates a new unique topic.
  - Effect: Useful for a new device or a clean start.
- **Bitvavo Market**: The market/symbol, e.g. `BTC-EUR`.
  - Effect: Switches the data feed; triggers a reboot to reset buffers.
  - Examples (popular Bitvavo markets):
    `BTC-EUR`, `ETH-EUR`, `SOL-EUR`, `XRP-EUR`, `ADA-EUR`,
    `DOGE-EUR`, `AVAX-EUR`, `LINK-EUR`, `MATIC-EUR`, `LTC-EUR`.
  - USD variants (if available on Bitvavo):
    `BTC-USD`, `ETH-USD`, `SOL-USD`, `XRP-USD`, `ADA-USD`,
    `DOGE-USD`, `AVAX-USD`, `LINK-USD`, `MATIC-USD`, `LTC-USD`.
- **Language**: `0` for Dutch, `1` for English.
  - Effect: UI and notification texts follow this setting.
- **Display Rotation**: `0` normal, `2` rotated 180 degrees.
  - Effect: Applies rotation immediately.

## Anchor & Risk Framework

- **2h/2h Strategy**: Presets for anchor take profit / max loss.
  - Effect: Sets TP/ML values directly.
- **Take Profit**: Percent above anchor for a "profit zone".
  - Effect: Moves the upper alert/zone around anchor.
- **Max Loss**: Negative percent below anchor for a "loss zone".
  - Effect: Moves the lower alert/zone around anchor.

## Signal Generation

These settings decide when a move is "big enough" to alert.

- **1m Spike Threshold**: Minimum 1m return for spike alerts.
  - Effect: Lower = more spikes; higher = only strong spikes.
- **5m Spike Threshold**: Minimum 5m return to confirm a 1m spike.
  - Effect: Filters out short-lived noise.
- **5m Move Threshold**: Minimum 5m return for move alerts.
  - Effect: Lower = more alerts; higher = only real moves.
- **5m Move Filter**: Minimum 5m return for 30m confirmation.
  - Effect: Prevents 30m alerts from small 5m noise.
- **30m Move Threshold**: Minimum 30m return for move alerts.
  - Effect: Lower = more 30m alerts; higher = more structural moves.
- **Trend Threshold**: Minimum 2h return for trend detection.
  - Effect: Controls how fast the system labels UP/DOWN.
- **Volatility Low/High**: Thresholds for volatility state.
  - Effect: Decides when the market is flat/waves/volatile.

## 2-hour Alert Thresholds

These settings control 2h breakouts, mean touch, compression, and trend events.

- **Breakout/Breakdown Margin (%)**: Distance beyond 2h high/low.
  - Effect: Higher = fewer, clearer breakouts.
- **Breakout Reset Margin (%)**: How far price must return to re-arm.
  - Effect: Prevents repeat alerts on the same breakout.
- **Breakout Cooldown (min)**: Minimum time between breakouts.
- **Mean Min Distance (%)**: Minimum distance to avg2h.
  - Effect: Avoids "mean" alerts on tiny moves.
- **Mean Touch Band (%)**: Band around avg2h for a "touch".
  - Effect: Tighter band = more precise touches.
- **Mean Reversion Cooldown (min)**: Cooldown between mean alerts.
- **Compress Threshold (%)**: Max range% for compression alerts.
  - Effect: Lower = earlier compression warnings.
- **Compress Reset (%)**: Reset distance for compression arming.
- **Compress Cooldown (min)**: Cooldown for compression.
- **Anchor Outside Margin (%)**: Distance outside 2h range for anchor alert.
  - Effect: Only alerts when anchor is clearly outside context.
- **Anchor Context Cooldown (min)**: Cooldown for anchor context alerts.
- **Trend Hysteresis Factor**: Exit factor for trend changes.
  - Effect: Stabilizes trend labels.
- **Throttle: Trend Change/Trend→Mean/Mean Touch/Compress (min)**:
  - Effect: Extra damping of quick repeats.
- **2h Secondary Global Cooldown (min)**:
  - Effect: Hard cap for all SECONDARY alerts.
- **2h Secondary Coalesce Window (sec)**:
  - Effect: Multiple alerts inside the window are merged.

## Auto Anchor

Automatic anchor based on 4h/1d EMA. This gives you a "smart reference price" that moves with the market so you do not have to reset the anchor manually all the time.

- **Anchor Source**: `MANUAL`, `AUTO`, `AUTO_FALLBACK`, `OFF`.
  - **MANUAL**: You set the anchor yourself. Auto-anchor does nothing.
  - **AUTO**: The anchor is calculated and updated automatically.
  - **AUTO_FALLBACK**: Auto-anchor is active, but falls back to the manual anchor if data is missing.
  - **OFF**: Anchor logic is disabled.
- **Update Interval (min)**: How often the system *may* update.
  - Lower = more responsive; higher = more stable, less noise.
- **Force Update Interval (min)**: Maximum time before an update is forced.
  - Useful if the market is flat but you still want a fresh anchor occasionally.
- **4h Candles / 1d Candles**: How many candles are used for EMA.
  - More candles = smoother, slower anchor.
  - Fewer candles = faster, but more sensitive.
- **Min Update %**: Minimum percent change required before updating.
  - Prevents the anchor from following every tiny move.
- **Trend Pivot %**: The level where trend influence becomes stronger.
  - Higher = trend bias only when the trend is clear.
- **4h Base Weight / 4h Trend Boost**: How much weight the 4h EMA gets.
  - Base = normal weight.
  - Trend Boost = extra weight when the trend is strong.
- **Last Auto Anchor**: Read-only last calculated value.
  - Helps you confirm the system is updating.
- **Notify on update**: Sends a notification when the anchor changes.
  - Useful to stay aware of bigger shifts.

## Smart Logic & Filters

- **Trend-Adaptive Anchors**: Adjusts TP/ML based on trend.
  - Effect: In UP trend, profit zone can be wider; in DOWN, tighter.
  - **UP/DOWN Trend Multiplier**: How strong the adjustment is.
- **Smart Confluence Mode**: Extra filter that needs multiple signals to agree.
  - It does not rely on a single short signal, but checks whether a short move fits the broader move and context.
  - In practice, the 5‑minute move should align with the 30‑minute direction and make sense in the 2‑hour picture.
  - This prevents alerts on tiny, short-lived spikes.
  - Effect: Fewer alerts, but usually more meaningful ones.
- **Night mode enabled**: Activates night filters.
  - **Night mode start/end (hour)**: Time window for night logic.
  - **Night: 5m Spike Filter**: Stricter spike filter at night.
  - **Night: 5m Move Threshold**: Stricter 5m moves at night.
  - **Night: 30m Move Threshold**: Stricter 30m moves at night.
  - **Night: 5m Cooldown (sec)**: Longer rest between alerts.
  - **Night: Auto-Vol Min/Max**: Tighter auto-vol range.
- **Auto-Volatility Mode**: Auto-adjusts thresholds to volatility.
  - In calmer markets the thresholds go a bit lower, in busy markets they go higher.
  - This reduces noisy alerts when the market is wild, and helps you catch moves when it is quiet.
  - **Volatility Window (min)**: The time period used to judge how “busy” the market is.
  - **Baseline σ (1m)**: The “normal” level used as a reference for 1‑minute moves.
  - **Min/Max Multiplier**: The lower and upper limits for how far thresholds are allowed to move.

## Cooldowns

- **1m/5m/30m Cooldown (sec)**: Minimum time between alerts per timeframe.
  - Effect: Reduces spam during fast moves.

## Warm-Start

Pre-fills buffers with historical data so alerts are reliable sooner.

- **Warm-Start Enabled**: Toggle warm-start on/off.
- **Skip Warm-Start 1m/5m**: Skip specific timeframes.
- **1m Extra Candles**: Extra 1m candles beyond the volatility window.
- **5m/30m/2h Candles**: Candle count to seed buffers.

## Integration (MQTT)

- **MQTT Host/Port/User/Password**: Broker settings.
  - Effect: Device reconnects after saving.
- **MQTT Topic Prefix**: Read-only; derived from NTFY topic.
  - Effect: Helps identify your topics in Home Assistant.

## WiFi reset

- **WiFi reset (clear credentials)**: Clears WiFi and reboots.
  - Effect: Device returns to the configuration portal.

## Save

- **Save**: Stores everything in NVS and applies immediately.
  - Effect: Some changes (like market) can trigger a reboot.

---

*Go to [Chapter 7: Alert Types and Examples](07-Alert-Types-and-Examples.md) | [Chapter 9: Integration with External Systems](09-External-Systems-Integration.md)*
