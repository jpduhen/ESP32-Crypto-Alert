# Hoofdstuk 6: Begrip van Kernconcepten

## 6.1 Inleiding
Dit hoofdstuk legt de belangrijkste technische en trading-concepten uit die ten grondslag liggen aan de alert-logica van ESP32-Crypto-Alert. Een goed begrip hiervan helpt je om alerts correct te interpreteren, de presets te kiezen die bij jouw stijl passen en eventueel zelf thresholds aan te passen.

Alle berekeningen gebeuren lokaal op de ESP32 met kandledata van Binance over vier timeframes: 1 minuut, 5 minuten, 30 minuten en 2 uur.

![Multi-timeframe overzicht](img/multi-timeframe-overview.jpg)  
*Overzicht van de vier gebruikte timeframes en hun rol in de analyse.*

## 6.2 Multi-Timeframe Analyse
Het systeem combineert informatie van meerdere timeframes om ruis te verminderen en alleen bevestigde bewegingen te signaleren.

- **1 minuut & 5 minuten**: Detecteren snelle, korte-termijn spikes en moves.
- **30 minuten**: Bevestigt momentum (aanhoudende directionele kracht).
- **2 uur**: Levert de bredere marktcontext (trendrichting, range, volatiliteit).

Door alleen alerts te geven wanneer meerdere timeframes overeenstemmen (confluence), vermijd je valse signalen in zijwaartse of choppy markten.

![Multi-timeframe confluence](img/multi-timeframe-confluence.jpg)  
*Voorbeeld hoe een korte-termijn spike alleen een alert triggert als deze past in de 2h-trend.*

## 6.3 Anchor Price
De **anchor price** is jouw persoonlijke referentieprijs (bijv. gemiddelde aankoopprijs, belangrijk support/resistance-niveau of psychologisch rond getal).

Belangrijke functies:
- Alle alerts worden gecontextualiseerd ten opzichte van de anchor ("+4.2% boven anchor").
- Definieert twee risicobeheer-zones:
  - **Take Profit-zone**: Percentage boven de anchor.
  - **Max Loss-zone**: Percentage onder de anchor.
- Trend-adaptive: de interpretatie houdt rekening met de huidige 2h-trendrichting.

![Anchor price zones](img/anchor-price-zones.jpg)  
*Visuele weergave van anchor price met take profit- en max loss-zones op een crypto-chart.*

## 6.4 2-Uur Context en Structurele Alerts
De 2-uur timeframe is de basis voor alle langere-termijn alerts:

- **Range**: Hoogste en laagste prijs van de laatste 2 uur.
- **Gemiddelde**: Gewogen middenprijs.
- **Volatiliteit**: Breedte van de range en gemiddelde candle-grootte.

Belangrijke structurele alerts op dit niveau:

- **Breakout / Breakdown**: Prijs sluit buiten de 2h-range → mogelijke trendversnelling.  
  ![Breakout voorbeeld](img/breakout-2h.jpg)  
  *Breakout boven de 2h-high.*

- **Compression**: De 2h-range krimpt sterk → vaak voorloper van een explosieve beweging.  
  ![Compression voorbeeld](img/compression-2h.jpg)  
  *Volatility compression in de 2h-timeframe.*

- **Mean Reversion**: Prijs wijkt sterk af van het 2h-gemiddelde en keert terug.  
  ![Mean reversion voorbeeld](img/mean-reversion-2h.jpg)  
  *Prijs trekt terug naar het 2h-gemiddelde.*

- **Trend Change**: Verandering van higher highs/higher lows naar het tegenovergestelde.
- **Anchor Outside Range**: De anchor ligt niet meer binnen de huidige 2h-range → waarschuwing voor verhoogd risico.

## 6.5 Korte-Termijn Alerts
Deze alerts (Spike, Move, Momentum) worden alleen verstuurd als ze passen binnen de 2h-context:

- **Spike**: Snelle prijsverandering op 1m/5m.
- **Move**: Bevestigde directionele shift op 5m/30m.
- **Momentum**: Aanhoudende kracht op 30m.

![Korte-termijn spike](img/short-term-spike.jpg)  
*Voorbeeld van een spike die een alert triggert binnen een uptrend.*

## 6.6 Samenvatting van de Filosofie
ESP32-Crypto-Alert is gebouwd als **decision support tool**:
- Het filtert ruis door multi-timeframe confluence.
- Het biedt context via de anchor price en 2h-structurele analyse.
- Het geeft geen handelsadvies, maar helpt je om relevante marktgebeurtenissen sneller op te merken.

Met dit begrip kun je in het volgende hoofdstuk de verschillende alert-types en hun voorbeeldberichten beter begrijpen.

---

*Ga naar [Hoofdstuk 5: Configuratie via de Web Interface](05-Configuratie-Web-Interface.md) | [Hoofdstuk 7: Alert Types en Voorbeelden](07-Alert-Types.md)*