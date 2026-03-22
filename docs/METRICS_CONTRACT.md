# Metric-contract (ESP32-Crypto-Alert)

## 1. Inleiding

In deze firmware worden veel velden met `ret*` (intern), `ret*m` (HTTP JSON) of `return_*` (MQTT) **in de praktijk** als **regressie- / trend-%** berekend (helling over een venster, omgezet naar procenten), en **niet** consequent als klassieke **endpoint-return** \((P_{\mathrm{nu}} - P_{\mathrm{oud}}) / P_{\mathrm{oud}}\).

Dezelfde horizon kan **verschillende betekenis** hebben afhankelijk van **warm-start** (API-candles) versus **live** (ringbuffers), zie de tabel en §2.

**Readiness** (hoe “echt” live de data):

| Conditie | Betekenis |
|----------|-----------|
| **warm** | Waarde of buffer vóór volledig live verkeer: API-candles en/of `SOURCE_BINANCE`-placeholder in seconden-/minuutbuffers. |
| **mixed** | Ring buffer deels live (`SOURCE_LIVE`), deels nog historisch/placeholder. |
| **live-clean** | Actief venster voor de berekening bestaat uit **louter** live ticks/minuten/uren volgens de gebruikte drempels (o.a. `hasRet*Live`, `calcLivePctMinuteAverages`). |

---

## 2. Tabel metrics

| Firmware (globaal/lokaal) | JSON (`/status`) | MQTT-topic (suffix onder prefix) | Echte berekening | Canonieke databron | Readiness | Semantische opmerking |
|---------------------------|------------------|----------------------------------|------------------|---------------------|-----------|------------------------|
| `ret_1m` (lokaal in `fetchPrice`) | `ret1m` | `values/return_1m` | Lineaire regressie over de laatste minuut `secondPrices` → **% per minuut** (`PriceData::calculateReturn1Minute`) | `secondPrices` (60 s ring) | warm → mixed → live-clean | **“Return”** in topicnaam; feitelijk **trend- / slope-%**. |
| `ret_5m` | `ret5m` | `values/return_5m` | `computeRegressionPctFromSeries` op `fiveMinutePrices` (300 s) | `fiveMinutePrices` | warm → mixed → live-clean | Idem; warm-start kan **constante prijs** over 300 s geven. |
| `ret_30m` | `ret30m` | `values/return_30m` | `calculateLinearTrend30Minutes`: regressie over **laatste 30** minuutpunten → **% per 30m** | `minuteAverages` | warm (API 30m + minuut-placeholder) → mixed → live-clean | **“30m return”** vs **30m trend**; niet strikt endpoint-return. |
| `ret_2h` | `ret2h` | `values/return_2h` | **Warm:** `computeRegressionPctFromSeriesWithTimes` op **2h API-candles**. **Live:** `calculateReturn2Hours` → `calculateLinearTrend2Hours` op **tot 120** `minuteAverages` | Warm: API **2h** candles · Live: `minuteAverages` | warm vs live (o.a. `hasRet2hLive`: ≥120 min data, ≥80% live in 120m-venster) | Zelfde symbool, **twee definities** warm vs live; zie §3. |
| — (intern `TwoHMetrics`) | `avg2h` | *niet via apart MQTT-veld in `publishMqttValues`* | Gemiddelde geldige minuutprijzen over **laatste 120** minuten | `minuteAverages` | warm (placeholder) → mixed → live-clean | **EUR-prijsstatistiek** (weergave), geen return. |
| — | `high2h` | *idem* | Maximum minuutprijs over **120** minuten | `minuteAverages` | idem | Prijsstatistiek. |
| — | `low2h` | *idem* | Minimum minuutprijs over **120** minuten | `minuteAverages` | idem | Prijsstatistiek. |
| — (`metrics.rangePct`) | `range2hPct` | *idem* | \((\mathrm{high2h}-\mathrm{low2h})/\mathrm{avg2h}\times 100\) | Afgeleid van **avg/high/low** op `minuteAverages` | idem | **Range-%** t.o.v. gemiddelde; geen `ret_2h`. |
| `ret_1d` | `ret1d` | `values/return_1d` | **Live:** `calculateLinearTrend1Day` → regressie over **tot 24** uur op `hourlyAverages`. **Warm:** API-regressie op **1d** en/of **1h**-candles (volgorde in `performWarmStart`) | Live: `hourlyAverages` · Warm: API candles | warm tot `availableHours >= 24` voor live-tak; daarna live | **“1d”** suggereert kalenderdag; live is **24h-trend%** uit uren. |
| `ret_7d` | `ret7d` | `values/return_7d` | **Live:** `calculateLinearTrend7Days` op **tot 168** uur `hourlyAverages` (output als week-trend%). **Warm:** `1W` en/of **7× daily** API-regressie | Live: `hourlyAverages` · Warm: API | warm tot `availableHours >= 168` voor volledige live-tak | **“7d”** vs warm **1W**-pad; geen eenvoudige spot-“7d return”. |

*HTTP JSON: `WebServerModule::handleStatus()` — MQTT: `publishMqttValues()` + topics in `ESP32-Crypto-Alert.ino`.*

---

## 3. Special case: 2h metrics

- **`ret_2h`**, **`ret2h`**, **`return_2h`**: dit is een **trend-%** (regressie-helling in % over het gekozen venster), niet de EUR **min/max/avg** van het dashboard.
- **`avg2h`**, **`high2h`**, **`low2h`**, **`range2hPct`**: komen uit **`computeTwoHMetrics()`** — **prijsstatistieken** over **120 minuten** `minuteAverages` (gemiddelde, min, max, range t.o.v. avg). Ze schrijven **`ret_2h` niet**.
- Conclusie voor reviewers: **twee parallelle “2h”-betekenissen** — trend (`ret_2h`) versus **2h prijsbox** (vier JSON-velden). Verwarring is een **bekende semantische risico** (zie §4).

---

## 4. Known semantic risks (max. 5)

1. **`ret_2h`**: Zelfde naam voor **API 2h-candle regressie** (warm) en **minuut-regressie** over 2h (live); orthogonal aan **avg2h/high2h/low2h/range2hPct**.
2. **`ret_1d`**: Naam suggereert **één kalenderdag / spot return**; live-implementatie is **24u trend-regressie** op `hourlyAverages`; warm-start kan **1h/24h API-regressie** zijn.
3. **`ret_7d`**: Naam suggereert **7 kalenderdagen**; warm kan **1W** zijn; live is **lange uren-regressie** (tot 168h), niet per se “prijs exact 7 dagen geleden”.
4. **`ret_5m` na warm-start**: **5m-buffer** kan met **één prijs** over 300 seconden gevuld zijn → **gekunsteld vlakke** regressie tot de ring volledig live is vervangen.
5. **Drie naamstijlen**: **firmware** (`ret_1m`, `ret_2h`, …), **JSON** (`ret1m`, `ret2h`, …), **MQTT** (`return_1m`, `return_2h`, …) — dezelfde grootheid drie keer anders; integraties moeten **mapping** expliciet houden.

---

*Document alleen ter referentie; geen gedragsgarantie. Gebaseerd op codepad `ESP32-Crypto-Alert.ino`, `src/PriceData/`, `src/WebServer/WebServer.cpp`, warm-start in `performWarmStart()`.*
