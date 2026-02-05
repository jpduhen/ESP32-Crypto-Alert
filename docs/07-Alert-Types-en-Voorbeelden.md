# Hoofdstuk 7: Alert Types en Voorbeelden

## 7.1 Inleiding
Dit hoofdstuk beschrijft alle alert-types die ESP32-Crypto-Alert kan genereren, inclusief hun betekenis, voorwaarden en concrete voorbeeldberichten. Elke alert bevat altijd rijke context: richting (up/down), timeframe, percentage verandering, relatie tot de anchor price en de huidige trend.

Alerts verschijnen op:
- Het lokale TFT-display
- NTFY.sh push-notificaties
- De web-interface
- Via MQTT (voor Home Assistant)

![Alert op display](img/alert-on-display.jpg)  
*Voorbeeld van een alert weergegeven op een Cheap Yellow Display.*

![NTFY alert voorbeeld](img/ntfy-alert-spike.jpg)  
*Hetzelfde alert als push-notificatie in de NTFY-app.*

## 7.2 Korte-Termijn Alerts (1m / 5m / 30m)
Deze alerts reageren op snelle marktontwikkelingen en worden alleen verstuurd als ze passen binnen de 2h-context.

### Spike
Een plotselinge, sterke prijsbeweging op 1m of 5m.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 1m Spike`  
`68250 (05-02-2026/01:23:45)
1m OP spike: +0.85% (5m: +0.42%)
1m Top: 68410
1m Dal: 67980`

**Betekenis:** 1m‑spike met bevestiging door 5m in dezelfde richting. Kan extra volume‑regels bevatten (VOLUME// of VOLUME=) afhankelijk van volume/range‑status.

![Spike chart](img/alert-spike-chart.jpg)  
*Typisch chart-patroon van een spike die deze alert triggert.*

### Move
Bevestigde directionele verandering op 5m of 30m.

**Voorbeeldbericht (30m move, titel + tekst):**  
`BTC-EUR 30m Move`  
`67820 (05-02-2026/02:10:12)
30m NEER move: -1.24% (5m: -0.62%)
30m Top: 68940
30m Dal: 67610`

**Voorbeeldbericht (5m move, titel + tekst):**  
`BTC-EUR 5m Move`  
`68120 (05-02-2026/02:18:09)
5m OP move: +0.92% (30m: +0.48%)
5m Top: 68260
5m Dal: 67920`

**Betekenis:** directionele move op 5m of 30m met min/max context. In nachtstand kan 5m‑move extra gefilterd worden (30m‑richting moet matchen).

### Samenloop (1m+5m+Trend)
Confluence‑alert: 1m en 5m bevestigen elkaar binnen het tijdsvenster en passen bij de 30m‑trend.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR Samenloop (1m+5m+Trend)`  
`68290 (05-02-2026/02:25:33)
Eensgezind OMHOOG
1m: +0.62%
5m: +1.04%
30m Trend: OP ( +0.55%)`

**Betekenis:** extra sterke bevestiging (minder ruis, hogere kwaliteit signalen).

## 7.3 2-Uur Contextuele Alerts
Deze alerts zijn strategischer en wijzen op grotere marktveranderingen.

### Breakout
Prijs sluit boven de hoogste prijs van de afgelopen 2 uur.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 2h breakout ↑`  
`69120 (05-02-2026/03:05:33)
Prijs > 2h Top 69010
Gem: 68450 Band: 1.82%`

![Breakout chart](img/alert-breakout-chart.jpg)  
*Breakout boven de 2h-range.*

### Breakdown
Prijs sluit onder de laagste prijs van de afgelopen 2 uur.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 2h breakdown ↓`  
`67280 (05-02-2026/03:48:09)
Prijs < 2h Dal: 67340
Gem: 68120 Band: 1.76%`

### Compression
De 2h-range krimpt sterk → vaak voorloper van een grote beweging.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 2h Compressie`  
`68110 (05-02-2026/04:12:27)
Band: 0.92% (<1.10%)
2h Top: 68440
2h Gem: 68190
2h Dal: 67870`

![Compression chart](img/alert-compression-chart.jpg)  
*Volatility contraction pattern (VCP) in de 2h-timeframe.*

### Mean Reversion
Prijs wijkt sterk af van het 2h-gemiddelde en begint terug te keren.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 2h Raakt Gemiddelde`  
`67990 (05-02-2026/04:55:10)
Raakt 2h gem. van onderen
na 2.35% verwijdering`

![Mean reversion chart](img/alert-mean-reversion-chart.jpg)  
*Prijs trekt terug naar het gemiddelde na een extreme beweging.*

### Trend Change (2h)
Verandering in de 2h-trendrichting.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR Trend Wijziging`  
`68214.25 (05-02-2026/05:12:33)
Trend change: 2h// → 2h=
2h: +1.18%
30m: +0.42%
Volatiliteit: Gemiddeld
1d trend: 1d=
7d trend: 7d//`

### 1d Trend Change
Verandering in de 1d‑trendrichting.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 1d Trend Wijziging`  
`68190.10 (05-02-2026/06:05:02)
1d trend change: 1d= → 1d//
1d: +2.36%
2h trend: 2h//`

### 7d Trend Change
Verandering in de 7d‑trendrichting.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR 7d Trend Wijziging`  
`67950.55 (05-02-2026/06:40:18)
7d trend change: 7d\\ → 7d=
7d: -1.05%
2h trend: 2h=`

### Anchor Outside Range
De anchor price ligt buiten de huidige 2h-range.

**Voorbeeldbericht (titel + tekst):**  
`BTC-EUR Anker buiten 2h`  
`68220 (05-02-2026/05:30:44)
Anker 70100 outside 2h
2h Top: 68980
2h Gem: 68420
2h Dal: 67710`

![Anchor outside range](img/alert-anchor-outside.jpg)  
*Situatie waarin de anchor niet meer binnen de actuele prijsactie ligt.*

## 7.4 Interpretatie en Tips
- **Emoji’s** geven snelle visuele indicatie van urgentie en richting.
- Lees altijd de context achter de emoji: relatie tot anchor en trend is cruciaal.
- Nachtstand kan 5m-alerts extra filteren (richting‑confirmatie met 30m en hogere thresholds).

Door deze alerts te combineren met je eigen analyse, heb je een krachtig hulpmiddel om betekenisvolle marktgebeurtenissen op te merken zonder continu naar charts te hoeven kijken.

---

*Ga naar [Hoofdstuk 6: Begrip van Kernconcepten](06-Kernconcepten.md) | [Hoofdstuk 8: WebUI-instellingen](08-WebUI-Instellingen.md)*