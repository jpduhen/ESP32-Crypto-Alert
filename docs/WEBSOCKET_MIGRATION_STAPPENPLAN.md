# Stappenplan: migratie naar Bitvavo WebSocket + REST warm-start

Dit stappenplan is bedoeld om gefaseerd over te stappen van HTTP polling naar een
persistente WebSocket-verbinding voor real-time prijzen en candle-updates, met
alleen REST-calls bij opstart voor warm-start data. Elke stap bevat een concreet
tussenresultaat en een testactie zodat je op de ESP32 tussentijds kunt valideren.

## Voorwaarden
- Huidige codebase compileert en draait stabiel met REST polling.
- Netwerkverbinding (WiFi) is betrouwbaar.
- Je hebt serial logging aan (115200) om JSON events te inspecteren.

## Stap 0 — Voorbereiding & inventarisatie (geen code wijziging)
**Doel:** inzicht krijgen in de huidige flow en de functies die vervangen worden.

1. Noteer waar de prijs wordt opgehaald (bijv. `fetchBitvavoPrice()` en de timers).
2. Noteer waar candles worden opgehaald (`fetchBitvavoCandles()`), inclusief
   huidige warm-start calls en interval-beheer.
3. Maak een lijstje van de data die direct afgeleid is van live prijs of candles.

**Tussenresultaat / test**
- Geen code change; enkel notities.

**Inventaris (uitgevoerd)**
- Prijs polling:
  - `fetchPrice()` roept `ApiClient::fetchBitvavoPrice()` aan en schrijft `prices[0]`,
    `lastFetchedPrice`, en triggert average/ret updates.
  - `apiTask()` draait elke `UPDATE_API_INTERVAL` en roept `fetchPrice()` aan.
  - `priceRepeatTask()` herhaalt elke 2s de laatste prijs in de ring buffer.
- Candle REST calls:
  - Warm-start blok haalt candles op voor `1m`, `5m`, `30m`, `2h`, `4h`, `1d`, `1W`
    via `fetchBitvavoCandles()` tijdens boot.
  - `updateLatestKlineMetricsIfNeeded()` doet periodieke REST calls voor laatste 1m/5m.
  - `AlertEngine` doet on-demand REST calls voor `4h`/`1d` in `maybeUpdateAutoAnchor()`.
- Data afhankelijk van live prijs/candles:
  - `priceData` second/minute buffers, `minuteAverages`, `ret_1m/5m/30m/2h/4h/1d/7d`,
    en trend/alert logic in `TrendDetector`/`AlertEngine`.

---

## Stap 1 — WebSocket library toevoegen + minimale connect
**Doel:** ESP32 kan een WS-verbinding openen naar Bitvavo.

1. Voeg een WebSocket library toe (bijv. `ArduinoWebsockets` of `WebSocketsClient`).
2. Maak een minimale WS-connectie in `setup()` (geen subscribe, enkel connect).
3. Log bij connect/disconnect.

**Tussenresultaat / test**
- Serial log toont `WS connected` en reconnects als WiFi wegvalt.

---

## Stap 2 — Subscribe op `ticker` voor real-time prijs
**Doel:** live prijs updates ontvangen via WS.

1. Stuur na connect het subscribe bericht:
   ```json
   {"action":"subscribe","channels":[{"name":"ticker","markets":["BTC-EUR"]}]}
   ```
2. Parse WS messages en filter `event == "ticker"`.
3. Lees `lastPrice` als string en parse naar float.
4. Update de bestaande `currentPrice` flow (vervang de polling-call).

**Tussenresultaat / test**
- Serial log toont ontvangen `ticker` events met `lastPrice`.
- UI toont live prijs updates zonder polling.

---

## Stap 3 — REST warm-start voor candles (1m, 5m, 30m, 2h, 1d)
**Doel:** candles arrays gevuld bij opstart, 1x per boot.

1. Houd de bestaande REST calls, maar beperk ze tot `setup()` of éénmalige init.
2. Voor elk interval: vul lokale arrays (prijzen, timestamps, high/low/volume).
3. Sla laatste timestamp per interval op voor latere WS updates.

**Tussenresultaat / test**
- UI/logic blijft werken zoals voorheen direct na boot.
- Serial log toont aantal opgehaalde candles per interval.

---

## Stap 4 — Subscribe op `candles` voor 1m/5m/30m/2h/1d
**Doel:** real-time candle updates ontvangen via WS.

1. Stuur na connect een candles subscribe, bijv.:
   ```json
   {"action":"subscribe","channels":[{"name":"candles","markets":["BTC-EUR"],"interval":["1m","5m","30m","2h","1d"]}]}
   ```
2. Parse events met `event == "candle"` (of volgens Bitvavo WS spec).
3. Extract interval, timestamp, open/high/low/close/volume.

**Tussenresultaat / test**
- Serial log toont candle updates voor meerdere intervals.

---

## Stap 5 — Candle update logic (replace/append)
**Doel:** lokale candle arrays correct updaten.

1. Voor elk interval: vergelijk de inkomende candle timestamp met de laatste in
   je array.
2. Als timestamp gelijk is: update de laatste candle (open/high/low/close/volume).
3. Als timestamp groter is: shift array en append nieuwe candle.

**Tussenresultaat / test**
- Trendberekeningen blijven correct.
- Geen double-counting van dezelfde candle.

---

## Stap 6 — 7d trends warm-start + rolling updates
**Doel:** 7-dagen trends blijven consistent.

1. Bij warm-start: haal 7× `1d` candles via REST (limit=7).
2. Of gebruik live `1d` candle updates en houd lokaal de laatste 7 bij.

**Tussenresultaat / test**
- 7d trend indicatoren overeen met bekende data.

---

## Stap 7 — Polling verwijderen + backoff/reconnect
**Doel:** volledige overgang naar WS zonder HTTP polling.

1. Verwijder periodieke `fetchBitvavoPrice()` calls.
2. Houd alleen REST calls bij boot (en eventueel fallback bij WS outage).
3. Voeg reconnect/backoff toe (bijv. 5–10s retry).

**Tussenresultaat / test**
- Geen HTTP traffic behalve bij boot.
- Real-time updates blijven werken na WiFi reconnect.

---

## Stap 8 — Stabiliteit & geheugen
**Doel:** geheugen en heap stabiel houden.

1. Vermijd grote JSON buffers; parse alleen benodigde velden.
2. Test minimaal 1 uur op live updates.

**Tussenresultaat / test**
- Geen heap leaks of reboots.

---

## Stap 9 — Cleanup & documentatie
**Doel:** codebase opgeruimd en goed gedocumenteerd.

1. Verwijder oude REST polling timers en unused helpers.
2. Documenteer de WS flow en fallback.

**Tussenresultaat / test**
- Code compileert schoon, README aangevuld.

---

## Notities
- Bitvavo WS URL: `wss://ws.bitvavo.com/v2/`
- `ticker` events bevatten `lastPrice`, `bestBid`, `bestAsk` (strings met decimals).
- `candles` events gebruiken de interval in het event object.
- Als parsing moeilijk wordt: overweeg ArduinoJson alleen voor WS events.
