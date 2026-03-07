# 07 – Glossary

| Term | Meaning |
|------|---------|
| **Anchor** | Reference price set by the user (or auto); take-profit and max-loss are computed relative to this price. |
| **Anchor take profit** | Percentage above anchor; when exceeded a notification is sent (once until anchor is set again). |
| **Anchor max loss** | Percentage below anchor (negative); when undershot a notification is sent (once). |
| **API interval** | Period between two Bitvavo price fetches (UPDATE_API_INTERVAL, default 2000 ms). |
| **Auto-volatility** | Mode where spike/move thresholds are adjusted dynamically from recent 1m volatility (sliding window). |
| **Bitvavo** | Exchange API for prices and candles; market e.g. BTC-EUR. |
| **Buffer (ring buffer)** | Circular array for time series: secondPrices (60), fiveMinutePrices (300), minuteAverages (120), hourlyAverages (168). |
| **Candle** | OHLCV (open/high/low/close/volume) over an interval (1m, 5m, 30m, 2h, …); used in warm-start and 2h metrics. |
| **Cooldown** | Minimum time (ms) between two notifications of the same type (1m, 5m, 30m or 2h secondary). |
| **Confluence** | Smart Confluence: 1m and 5m event in the same direction within a time window, supported by 30m trend; produces one combined alert. |
| **DataSource** | Enum in PriceData.h. **SOURCE_BINANCE:** legacy/historical enum name in code; functionally: warm-start with historical candles (Bitvavo endpoint). **SOURCE_LIVE:** live API source. Per sample in secondPrices/fiveMinutePrices/minuteAverages. |
| **Effective threshold** | Threshold after adjustment (e.g. by auto-volatility or night mode). |
| **Fetch** | One HTTP call to Bitvavo to get the current price (and optionally candles). |
| **FreeRTOS tasks** | apiTask (Core 1), uiTask (Core 0), webTask (Core 0), priceRepeatTask; dataMutex protects shared state. |
| **LVGL** | Light and Versatile Graphics Library (v9); used for all UI widgets and rendering. |
| **Max alerts per hour** | Per alert type (1m, 5m, 30m) a maximum number of notifications per hour; then no send until the next hour. |
| **Mean touch (2h)** | Alert when price approaches the 2h average from far (above or below), within band. |
| **Minute average** | Average price over the last 60 seconds; written each minute into minuteAverages[minuteIndex]. |
| **NTFY** | Notification service (ntfy.sh); topic often device-specific (e.g. ESP32 ID + "-alert"). |
| **NVS** | Non-Volatile Storage (Preferences); used by SettingsStore for persistent settings. |
| **PINS** | PINS_*.h files with display and bus pins per board. |
| **Price repeat** | Task that every 2 s puts the last fetched price back into the ring buffer for stable 1m/5m calculation with slow API. **Note:** with API failure or slow responses this can briefly underestimate/smooth volatility (same price repeated). |
| **Return (ret_1m, ret_5m, …)** | Percentage change in **percentage points**: (price_now − price_old) / price_old × 100. Example: 0.12 = 0.12%; thresholds (e.g. spike1m 0.31) are also in percentage points. Calculation e.g. in `calculatePercentageReturn()` (.ino). |
| **Spike (1m/5m)** | Fast move: |ret_1m| or |ret_5m| above a threshold; can trigger a notification. |
| **Sync state from globals** | Module reads global variables (.ino) and sets its own internal state to match (e.g. after warm-start or load). |
| **Throttling (2h)** | Limiting of 2h secondary alerts: matrix cooldowns and optional global secondary cooldown + coalescing. |
| **Trend (2h, medium, long-term)** | UP / DOWN / SIDEWAYS from ret_2h, ret_1d, ret_7d and thresholds. |
| **Trend-adaptive anchor** | Take-profit and max-loss percentages are adjusted depending on 2h trend (uptrend/downtrend). |
| **UI interval** | Period between two full updateUI() calls (UPDATE_UI_INTERVAL, default 1000 ms). |
| **Volatility (state)** | LOW / MEDIUM / HIGH from average absolute 1m returns over the lookback period. |
| **Warm-start** | On boot: fetch Bitvavo candles (1m, 5m, 30m, 2h) and fill buffers so ret_2h/ret_30m are available quickly. |
| **WarmStartStatus** | WARMING_UP (still warm-start data), LIVE (fully live), LIVE_COLD (warm-start failed, live only). |

---
**[← 06 Operations](06_OPERATIONS_EN.md)** | [Technical docs overview](../README.md#technical-documentation-code--architecture) | **[08 FAQ →](08_FAQ_EN.md)**
