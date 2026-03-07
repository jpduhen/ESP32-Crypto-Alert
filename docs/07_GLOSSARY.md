# 07 – Woordenlijst

| Term | Betekenis |
|------|-----------|
| **Anchor** | Referentieprijs die de gebruiker zet (of auto); take-profit en max-loss worden ten opzichte van deze prijs berekend. |
| **Anchor take profit** | Percentage boven anchor; bij overschrijding wordt een notificatie verstuurd (eenmalig tot anchor opnieuw wordt gezet). |
| **Anchor max loss** | Percentage onder anchor (negatief); bij onderschrijding wordt een notificatie verstuurd (eenmalig). |
| **API interval** | Periode tussen twee Bitvavo price-fetches (UPDATE_API_INTERVAL, default 2000 ms). |
| **Auto-volatility** | Modus waarbij spike/move-drempels dynamisch worden aangepast op basis van recente 1m-volatiliteit (sliding window). |
| **Bitvavo** | Beurs-API voor prijzen en candles; market bijv. BTC-EUR. |
| **Buffer (ringbuffer)** | Circulaire array voor tijdreeks: secondPrices (60), fiveMinutePrices (300), minuteAverages (120), hourlyAverages (168). |
| **Candle** | OHLCV (open/high/low/close/volume) over een interval (1m, 5m, 30m, 2h, …); gebruikt bij warm-start en 2h-metrics. |
| **Cooldown** | Minimale tijd (ms) tussen twee notificaties van hetzelfde type (1m, 5m, 30m of 2h-secondary). |
| **Confluence** | Smart Confluence: 1m- en 5m-event in dezelfde richting binnen een tijdvenster, ondersteund door 30m-trend; levert één gecombineerde alert. |
| **DataSource** | Enum in PriceData.h. **SOURCE_BINANCE:** legacy/historische enum-naam in code; functioneel: warm-start met historische candles (Bitvavo endpoint). **SOURCE_LIVE:** live API-bron. Per sample in secondPrices/fiveMinutePrices/minuteAverages. |
| **Effective threshold** | Drempel na aanpassing (bijv. door auto-volatility of nachtstand). |
| **Fetch** | Eén HTTP-aanroep naar Bitvavo om de huidige prijs (en optioneel candles) op te halen. |
| **FreeRTOS tasks** | apiTask (Core 1), uiTask (Core 0), webTask (Core 0), priceRepeatTask; dataMutex beschermt gedeelde state. |
| **LVGL** | Light and Versatile Graphics Library (v9); gebruikt voor alle UI-widgets en rendering. |
| **Max alerts per hour** | Per alerttype (1m, 5m, 30m) een maximum aantal notificaties per uur; daarna geen send tot volgend uur. |
| **Mean touch (2h)** | Alert wanneer prijs het 2h-gemiddelde nadert vanuit ver (boven of onder), binnen bandbreedte. |
| **Minute average** | Gemiddelde prijs over de laatste 60 seconden; wordt elke minuut in minuteAverages[ minuteIndex ] gezet. |
| **NTFY** | Notificatieservice (ntfy.sh); topic vaak device-specifiek (bijv. ESP32-ID + "-alert"). |
| **NVS** | Non-Volatile Storage (Preferences); gebruikt door SettingsStore voor persistente instellingen. |
| **PINS** | Bestanden PINS_*.h met display- en buspinnen per board. |
| **Price repeat** | Task die elke 2 s de laatste opgehaalde prijs opnieuw in de ringbuffer zet, voor stabiele 1m/5m-berekening bij trage API. **Let op:** bij API-uitval of trage responses kan dit korte tijd volatiliteit onderschatten/afvlakken (zelfde prijs herhaald). |
| **Return (ret_1m, ret_5m, …)** | Procentuele verandering in **percentagepunten**: (prijs_nu − prijs_oud) / prijs_oud × 100. Voorbeeld: 0,12 = 0,12%; drempels (bijv. spike1m 0,31) zijn ook in percentagepunten. Berekening o.a. in `calculatePercentageReturn()` (.ino). |
| **Spike (1m/5m)** | Snelle beweging: |ret_1m| of |ret_5m| boven een drempel; kan een notificatie triggeren. |
| **Sync state from globals** | Module leest globale variabelen (.ino) en zet eigen interne state daarmee gelijk (bijv. na warm-start of load). |
| **Throttling (2h)** | Beperking van 2h-secondary alerts: matrix-cooldowns en optioneel global secondary cooldown + coalescing. |
| **Trend (2h, medium, long-term)** | UP / DOWN / SIDEWAYS op basis van ret_2h, ret_1d, ret_7d en drempels. |
| **Trend-adaptive anchor** | Take-profit en max-loss percentages worden aangepast afhankelijk van 2h-trend (uptrend/downtrend). |
| **UI interval** | Periode tussen twee volledige updateUI()-aanroepen (UPDATE_UI_INTERVAL, default 1000 ms). |
| **Volatility (state)** | LOW / MEDIUM / HIGH op basis van gemiddelde absolute 1m-returns over de lookback-periode. |
| **Warm-start** | Bij opstart: Bitvavo candles (1m, 5m, 30m, 2h) ophalen en buffers vullen zodat ret_2h/ret_30m snel beschikbaar zijn. |
| **WarmStartStatus** | WARMING_UP (nog warm-start data), LIVE (volledig live), LIVE_COLD (warm-start mislukt, alleen live). |

---
**[← 06 Operations](06_OPERATIONS.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking) | **[08 FAQ →](08_FAQ.md)**
