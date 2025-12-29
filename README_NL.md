# ESP32 Crypto Alert Systeem

**Slimme crypto-alerts met meerdere tijdschalen**

## 1. Wat is dit systeem?

Dit project is een zelfstandig crypto-alarmapparaat op basis van een ESP32.

Het:
- Haalt live prijzen op bij Binance
- Analyseert meerdere tijdschalen
- Begrijpt trend, volatiliteit en context
- Stuurt gerichte notificaties
- Werkt zonder PC, cloud of abonnement

## 2. Waarvoor is het bedoeld?

Niet om elke koersbeweging te volgen, maar om:

**"Op tijd te zien wanneer er écht iets relevants gebeurt."**

## 3. Kernbegrippen

### 3.1 Anchor prijs

Jouw referentiepunt.  
Alles wordt hieraan gerelateerd.

Denk aan:
- "Mijn belangrijke prijs"
- "Waar ik winst/verlies belangrijk vind"
- "Mijn psychologische basislijn"

**Voorbeelden:**
- +5% boven anchor → Take profit zone
- −3% onder anchor → Max loss waarschuwing
- Prijs oscilleert rond anchor → consolidatie

### 3.2 Multi-timeframe

Het systeem kijkt tegelijk naar:

| Tijdschaal | Doel |
|------------|------|
| 1 minuut | Detecteer plotselinge spikes |
| 5 minuten | Bevestig korte bewegingen |
| 30 minuten | Identificeer betekenisvol momentum |
| 2 uur | Definieer trend, range, context |

Dit voorkomt reageren op ruis.

### 3.3 Alert filosofie

Alerts beantwoorden de vraag:

**"Gebeurd er iets interessants dat mijn aandacht verdient?"**

Niet:
- Elke tick
- Elke candle
- Elke kleine fluctuatie

Het systeem geeft de voorkeur aan:
- Minder alerts
- Hogere relevantie
- Context-rijke berichten

## 4. Soorten meldingen

### 4.1 Korte-termijn alerts (1m / 5m / 30m)

- **Spike alert** – plotselinge korte beweging
- **Move alert** – bevestigde directionele move
- **Momentum alert** – aanhoudende beweging

Deze zijn snel en reactief.

### 4.2 2-uur context alerts (belangrijk!)

Deze alerts beschrijven marktstructuur, niet alleen beweging:

- **Breakout** – prijs verlaat de 2h range
- **Breakdown** – prijs daalt onder range
- **Compressie** – volatiliteit instorting (range verstrakt)
- **Mean reversion** – prijs ver van 2h gemiddelde, keert terug
- **Anchor buiten range** – anchor niet meer binnen huidige marktcontext
- **Trend change** – verschuiving in 2h trendrichting

Deze zijn langzamer, strategischer.

## 5. Instellingen in begrijpelijke taal

Alle instellingen zijn ontworpen om één vraag te beantwoorden:

**"Wanneer wil ik dat het systeem mij stoort?"**

Je bepaalt zelf:
- Hoe gevoelig
- Hoe vaak
- In welke context

### 5.1 Basis & connectiviteit

| Instelling | Betekenis |
|-----------|----------|
| NTFY Topic | Waar alerts worden verstuurd |
| Binance Symbol | Trading pair (bijv. BTCEUR) |
| Taal | UI & alert taal |

### 5.2 Anchor & risicobeheer

| Instelling | Betekenis |
|-----------|----------|
| Take Profit | % boven anchor dat als winst wordt beschouwd |
| Max Loss | % onder anchor dat als onacceptabel verlies wordt beschouwd |

Gebruikt voor risicobewuste alerts, niet voor trading uitvoering.

### 5.3 Signaal generatie drempels

Deze bepalen hoe gevoelig het systeem is.

**Voorbeelden:**
- 1m Spike Drempel = minimum % verandering om een spike te triggeren
- 30m Move Drempel = minimum beweging om betekenisvol te zijn
- Trend Drempel = hoe sterk een 2h move moet zijn om als trend te tellen

**Hogere waarden = minder alerts**  
**Lagere waarden = meer alerts**

### 5.4 Volatiliteitsniveaus

Het systeem classificeert volatiliteit als:
- Laag
- Normaal
- Hoog

Dit beïnvloedt:
- Alert gevoeligheid
- Trend vertrouwen
- Filtering

### 5.5 2-uur alert drempels

Deze bepalen structurele alerts:

| Instelling | Doel |
|-----------|------|
| Breakout Marge | Hoe ver buiten range = breakout |
| Cooldown | Minimum tijd tussen alerts |
| Compress Drempel | Definieert "strakke range" |
| Mean Reversion Afstand | Hoe ver prijs moet afdrijven van gemiddelde |

### 5.6 Slimme logica & filters

Optionele intelligentie lagen:

- **Trend-adaptieve anchors**  
  → Risico drempels passen zich aan aan trendrichting

- **Confluence modus**  
  → Alerts vuren alleen af wanneer meerdere condities overeenkomen

- **Auto-volatiliteit modus**  
  → Drempels passen zich automatisch aan aan marktgedrag

### 5.7 Cooldowns

Voorkom alert spam.

Elke tijdschaal heeft zijn eigen cooldown.

### 5.8 Warm-start (geavanceerd)

Bij opstarten kan het apparaat historische candles ophalen om niet uren te hoeven wachten op context.

Dit maakt het systeem bijna direct bruikbaar na herstart.

## 6. Aanbevolen presets

### Conservatief (weinig alerts)
- Hogere drempels
- Langere cooldowns
- Confluence AAN

### Gebalanceerd (standaard)
- Matige drempels
- Gemengde alerts
- Goede signaal/ruis verhouding

### Agressief (veel alerts)
- Lagere drempels
- Korte cooldowns
- Meer geschikt voor scalpers

## 7. Hoe alerts te interpreteren

**Algemene richtlijnen:**

- 1m alerts → aandacht
- 5m / 30m alerts → momentum
- 2h alerts → context verandering

**Meerdere alerts kort na elkaar duiden meestal op:**
- Transitie tussen regimes
- Breakout of breakdown
- Volatiliteit expansie

## 8. Voor wie is dit?

- Traders die rust willen
- Mensen die niet continu grafieken willen volgen
- Bouwers van slimme ESP32-projecten

## 9. Wat dit systeem NIET is

- ❌ Geen trading bot
- ❌ Geen financieel advies
- ❌ Geen voorspellende AI
- ❌ Geen prijsgrafiek vervanging

**Het is een beslissingsondersteunend hulpmiddel.**

## 10. Samenvatting

Dit systeem:
- Filtert ruis
- Geeft context
- Helpt beslissingen nemen
- Doet niet aan automatisch handelen

---

## Quick Start

1. Flash de firmware
2. Verbind met WiFi
3. Open de web-interface
4. Stel je instellingen in
5. Stel je anchor prijs in
6. Klaar!

Zie `README_QUICKSTART.md` voor gedetailleerde installatiestappen.

---

## Installatie

- Flash de firmware naar je ESP32
- Verbind het apparaat met WiFi
- Open de web-interface (IP-adres staat op het scherm)
- Configureer je instellingen
- Stel je anchor prijs in
- Begin alerts te ontvangen

Geen externe servers of databases vereist.

---

## Tot slot

Gebruik dit systeem als:
- Extra paar ogen
- Context-generator
- Rustbrenger in volatiele markten

**Niet als automatische waarheid.**
