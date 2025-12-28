# ESP32 Crypto Alert Monitor (CYD)

## Wat is dit?
Dit project is een **zelfstandig crypto-monitoringsysteem** dat realtime prijsdata
ophaalt van Binance en hier **contextuele meldingen** van maakt.

Het systeem draait op een ESP32 met scherm (zoals de Cheap Yellow Display)
en stuurt notificaties via **ntfy.sh**.

👉 Het doel is **inzicht geven in marktgedrag**, niet automatisch handelen.

---

## Voor wie is dit bedoeld?
Dit project is bedoeld voor mensen die:
- crypto-prijzen volgen
- willen weten *wanneer iets betekenisvol gebeurt*
- geen trading-bot zoeken, maar **context en overzicht**
- interesse hebben in embedded / ESP32-projecten

Je hoeft **geen programmeur** te zijn om het systeem te gebruiken.
Alle instellingen zijn via een web-interface aanpasbaar.

---

## Wat doet het systeem?
Het systeem:
- haalt periodiek prijsdata op van Binance
- analyseert prijsbewegingen op meerdere tijdschalen
- bepaalt trend en volatiliteit
- vergelijkt de huidige prijs met een referentie (anchor)
- stuurt notificaties wanneer vooraf ingestelde situaties optreden

Alles draait lokaal op het apparaat.

---

## Kernbegrippen (belangrijk)

### Anchor (referentieprijs)
De **anchor** is een prijs die jij instelt als referentiepunt.
Dit kan bijvoorbeeld zijn:
- je instapprijs
- een belangrijk technisch niveau
- een psychologische grens

Veel meldingen zijn **relatief aan deze anchor**.

---

### Tijdschalen
Het systeem kijkt niet naar één moment, maar naar meerdere vensters:

| Tijd | Betekenis |
|----|----|
| 1 minuut | snelle spikes |
| 5 minuten | korte bewegingen |
| 30 minuten | middellange moves |
| 2 uur | marktstructuur & context |

Zo wordt ruis gescheiden van echte bewegingen.

---

### Trend
Op basis van de **2-uurs prijsverandering** bepaalt het systeem:
- UP trend
- DOWN trend
- VLAK

Deze trend kan invloed hebben op risico-instellingen.

---

### Volatiliteit
Volatiliteit beschrijft hoe “rustig” of “wild” de markt is.
Bij hoge volatiliteit gelden andere drempels dan bij een rustige markt.

---

## Hoe werkt het systeem (globaal)
1. ESP32 maakt verbinding met WiFi
2. Prijsdata wordt opgehaald van Binance
3. Data wordt opgeslagen in interne buffers
4. Indicatoren worden berekend
5. Logica bepaalt of een situatie “meldingswaardig” is
6. Notificatie wordt verstuurd (indien nodig)
7. Resultaat wordt op het scherm getoond

---

## Soorten meldingen
Het systeem kan o.a. melden:

- ⚡ Snelle prijs-spikes (1m)
- 📈 Korte moves (5m)
- 📊 Middellange moves (30m)
- 🔄 Terugkeer naar 2h-gemiddelde (mean reversion)
- 📦 Compressie (lage volatiliteit vóór uitbraak)
- 🚀 Breakout / breakdown t.o.v. 2h high/low
- 🎯 Prijs ver buiten anchor-context
- 💰 Take profit / max loss signalen

Cooldowns voorkomen spam.

---

## Instellingen (conceptueel)
Alle instellingen zijn via de web-interface aanpasbaar.

Je stelt o.a. in:
- hoe gevoelig meldingen zijn
- hoeveel procent beweging nodig is
- hoe vaak een melding mag voorkomen
- hoe streng het risico-kader is
- of thresholds automatisch meeschalen met volatiliteit

Je hoeft **geen exacte formules** te kennen om het systeem goed te gebruiken.

---

## Wat dit project niet is
- ❌ geen trading-bot
- ❌ geen automatisch koop/verkoop-systeem
- ❌ geen financieel advies
- ❌ geen high-frequency trading tool

Het systeem is bedoeld als **informatief hulpmiddel**.

---

## Hardware
Getest met:
- ESP32 (CYD / ESP32-2432S028)
- 240×320 TFT scherm
- Geen PSRAM vereist

Het systeem is geoptimaliseerd voor beperkte geheugenomgevingen.

---

## Installatie (kort)
- Flash de firmware
- Verbind met WiFi
- Open de web-interface
- Stel je parameters in
- Klaar

Zie de installatie-instructies verderop in deze repository.

---

## Tot slot
Gebruik dit systeem als:
- extra paar ogen
- context-generator
- rustbrenger in volatiele markten

Niet als automatische waarheid.

