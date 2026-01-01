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

**Voorbeeldbericht:**  
`🔥 SPIKE UP +3.8% op 5m – Prijs breekt boven anchor (+2.1%) in sterke uptrend`

![Spike chart](img/alert-spike-chart.jpg)  
*Typisch chart-patroon van een spike die deze alert triggert.*

### Move
Bevestigde directionele verandering op 5m of 30m.

**Voorbeeldbericht:**  
`➡️ MOVE DOWN -2.4% op 30m – Momentum keert om onder anchor`

### Momentum
Aanhoudende directionele kracht op 30m.

**Voorbeeldbericht:**  
`🚀 MOMENTUM UP – Sterke voortzetting, +5.6% boven 2h gemiddelde`

![Momentum chart](img/alert-momentum-chart.jpg)  
*Momentum-buildup binnen een bestaande trend.*

## 7.3 2-Uur Contextuele Alerts
Deze alerts zijn strategischer en wijzen op grotere marktveranderingen.

### Breakout
Prijs sluit boven de hoogste prijs van de afgelopen 2 uur.

**Voorbeeldbericht:**  
`📈 BREAKOUT UP +4.2% – Prijs boven 2h high, sterk boven anchor in uptrend`

![Breakout chart](img/alert-breakout-chart.jpg)  
*Breakout boven de 2h-range.*

### Breakdown
Prijs sluit onder de laagste prijs van de afgelopen 2 uur.

**Voorbeeldbericht:**  
`📉 BREAKDOWN DOWN -3.9% – Prijs onder 2h low, nadert max loss-zone`

### Compression
De 2h-range krimpt sterk → vaak voorloper van een grote beweging.

**Voorbeeldbericht:**  
`🗜️ COMPRESSION – 2h range slechts 1.1% (laagste in 24h), voorbereiden op volatility expansion`

![Compression chart](img/alert-compression-chart.jpg)  
*Volatility contraction pattern (VCP) in de 2h-timeframe.*

### Mean Reversion
Prijs wijkt sterk af van het 2h-gemiddelde en begint terug te keren.

**Voorbeeldbericht:**  
`↩️ MEAN REVERSION UP – Prijs +2.7% terug naar 2h gemiddelde na -8% dip`

![Mean reversion chart](img/alert-mean-reversion-chart.jpg)  
*Prijs trekt terug naar het gemiddelde na een extreme beweging.*

### Trend Change
Verandering in de 2h-trendrichting (bijv. van higher highs naar lower highs).

**Voorbeeldbericht:**  
`🔄 TREND CHANGE – Van uptrend naar mogelijke downtrend (lower high gevormd)`

### Anchor Outside Range
De anchor price ligt buiten de huidige 2h-range.

**Voorbeeldbericht:**  
`⚠️ ANCHOR OUTSIDE RANGE – Je anchor ligt onder huidige markt, verhoogd risico`

![Anchor outside range](img/alert-anchor-outside.jpg)  
*Situatie waarin de anchor niet meer binnen de actuele prijsactie ligt.*

## 7.4 Interpretatie en Tips
- **Emoji’s** geven snelle visuele indicatie van urgentie en richting.
- Lees altijd de context achter de emoji: relatie tot anchor en trend is cruciaal.
- Met de **Conservative** preset ontvang je voornamelijk 2h-contextuele alerts van hoge kwaliteit.
- Met **Aggressive** krijg je vaker korte-termijn alerts (spikes/momentum).

Door deze alerts te combineren met je eigen analyse, heb je een krachtig hulpmiddel om betekenisvolle marktgebeurtenissen op te merken zonder continu naar charts te hoeven kijken.

---

*Ga naar [Hoofdstuk 6: Begrip van Kernconcepten](06-Kernconcepten.md) | [Hoofdstuk 8: Integratie met Externe Systemen](08-Integratie-Externe-Systemen.md)*