# Hoofdstuk 2: Functies en Mogelijkheden

## 2.1 Overzicht van de Kernfuncties
ESP32-Crypto-Alert biedt een reeks geavanceerde functies om cryptocurrency-prijzen te monitoren zonder overmatige notificaties. Het systeem haalt realtime data op van Bitvavo en richt zich op **contextuele alerts** door multi-timeframe analyse en slimme filters.

Belangrijkste functies:
- Realtime prijsmonitoring van een gekozen Bitvavo-market (bijv. BTC-EUR).
- Multi-timeframe analyse over 1m, 5m, 30m, 2h, 1d en 7d.
- Contextuele alert-generatie gebaseerd op prijsveranderingen, trend, volatiliteit en **anchor price**.
- Notificaties via NTFY.sh, lokale web-interface, display en MQTT.
- Volledig configureerbaar via web-UI en MQTT (geen hercompilatie nodig).
- Warm-start: historische data ophalen bij opstarten.

## 2.2 Multi-Timeframe Analyse
Het systeem analyseert prijsactie op meerdere timeframes tegelijk om ruis te filteren:

- **1m en 5m**: Detecteren snelle spikes en korte moves.
- **30m**: Bevestigt richting en filtert micro-ruis.
- **2h**: Biedt bredere context (trend, range, volatiliteit).
- **1d en 7d**: Context voor langetermijntrend (weergave/labeling).

![Multi-timeframe analyse voorbeeld 1](img/multi-timeframe-1.png)  
*Voorbeeld van multi-timeframe analyse: hogere timeframe toont trend, lagere timeframes entry-signalen.*

## 2.3 Anchor Price en Risicobeheer
De **anchor price** is jouw referentieprijs (bijv. instapprijs). Het systeem beoordeelt alerts relatief hieraan:

- **Take Profit-zone**: Percentage boven anchor.
- **Max Loss-zone**: Percentage onder anchor.
- Trend-adaptive aanpassing.

![Anchor zones voorbeeld 1](img/anchor-zones-1.jpg)  
*Voorbeeld van anchor zones met profit- en loss-gebieden.*

## 2.4 Alert Types

### Korte-termijn Alerts (1m / 5m / 30m)
- **Spike**: Plotselinge beweging.
- **Move**: Bevestigde directionele shift.

### 2-Uur Contextuele Alerts
- **Breakout / Breakdown**: Prijs breekt uit de 2h-range.  
  ![Breakout pattern](img/breakout-pattern.jpg)  
  *Voorbeeld van een breakout in crypto.*

- **Compression**: Volatiliteit daalt sterk (vaak voorloper van grote move).  
  ![Volatility compression](img/volatility-compression.jpg)  
  *Volatility Contraction Pattern (VCP) voorbeeld.*

- **Mean Reversion**: Prijs keert terug naar gemiddelde.  
  ![Mean reversion voorbeeld](img/mean-reversion.jpg)  
  *Prijs trekt terug naar het gemiddelde na extreme beweging.*

- **Trend Change**: Verandering in trendrichting.
- **Anchor Outside Range**: Anchor niet meer in huidige context.

## 2.5 Slimme Filters en Aanpassingen
- Cooldown-periodes per timeframe.
- Confluence Mode: alerts alleen bij meerdere bevestigingen.
- Auto-Volatility Mode: thresholds passen zich aan.
- Nachtstand met tijdvenster en extra filters om ruis te beperken.

## 2.6 Integraties en Uitvoer
- NTFY.sh voor mobiele push.
- Lokale web-interface voor monitoring.
- MQTT voor Home Assistant.
- Display voor directe visuele feedback.

## 2.7 Samenvatting
Dit systeem fungeert als **decision support tool**: het filtert ruis en biedt context, zodat jij betere beslissingen kunt nemen – zonder handelsadvies te geven.

---

*Ga naar [Hoofdstuk 1: Inleiding](01-Inleiding.md) | [Hoofdstuk 3: Hardwarevereisten](03-Hardwarevereisten.md)*