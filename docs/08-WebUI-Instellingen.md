## Hoofdstuk 8: WebUI-instellingen (uitleg per onderdeel)

Deze pagina legt elk onderdeel van de WebUI uit in gewone taal. Je ziet wat een instelling doet, waarom je hem zou aanpassen en welk effect het heeft op de meldingen en het gedrag van het systeem.
Wijzigingen worden opgeslagen in het device en worden actief na **Opslaan**, tenzij anders vermeld.

## Status en Anchor

- **Referentieprijs (Anchor)**: Dit is je persoonlijke referentiepunt. Denk aan "mijn basisprijs van vandaag".
  - Effect: Alle alerts die iets zeggen over "boven/onder anchor" gebruiken dit punt.
  - **Stel Anchor in** zet de anchor direct, zonder herstart.
- **Statusbox**: Live-informatie zoals prijs, trends (2h/1d/7d), volatiliteit, volume en returns.
  - Effect: Geen; dit is alleen om snel te zien wat er nu gebeurt.

## Basis & Connectiviteit

- **NTFY Topic**: Waar je push-notificaties naartoe gaan.
  - Effect: Meldingen gaan naar dit topic op NTFY.sh.
- **Standaard uniek NTFY-topic**: Genereert automatisch een nieuw, uniek topic.
  - Effect: Handig bij een nieuw device of als je een "schone" start wilt.
- **Bitvavo Market**: De markt die je volgt, zoals `BTC-EUR`.
  - Effect: De data komt voortaan van deze markt; het device herstart om buffers schoon te maken.
  - Voorbeelden (populaire Bitvavo‑markten):
    `BTC-EUR`, `ETH-EUR`, `SOL-EUR`, `XRP-EUR`, `ADA-EUR`,
    `DOGE-EUR`, `AVAX-EUR`, `LINK-EUR`, `MATIC-EUR`, `LTC-EUR`.
  - USD‑varianten (als beschikbaar op Bitvavo):
    `BTC-USD`, `ETH-USD`, `SOL-USD`, `XRP-USD`, `ADA-USD`,
    `DOGE-USD`, `AVAX-USD`, `LINK-USD`, `MATIC-USD`, `LTC-USD`.
- **Taal**: `0` voor Nederlands, `1` voor Engels.
  - Effect: UI-teksten en meldingen veranderen mee.
- **Display Rotatie**: `0` normaal, `2` 180 graden gedraaid.
  - Effect: Het scherm draait direct, handig als je het device andersom monteert.

## Anchor & Risicokader

- **2h/2h Strategie**: Voorinstellingen voor veilige zones rond je anchor.
  - Effect: Zet automatisch "Take Profit" en "Max Loss" op passende waarden.
- **Take Profit**: Hoeveel procent boven de anchor je een "winstzone" wilt.
  - Effect: Alerts en zones verschuiven naar boven.
- **Max Loss**: Hoeveel procent onder de anchor je een "verlieszone" wilt.
  - Effect: Alerts en zones verschuiven naar beneden.

## Signaalgeneratie

Deze instellingen bepalen wanneer een beweging "groot genoeg" is om een melding te waarderen.

- **1m Spike Threshold**: Snelle korte spike van 1 minuut.
  - Effect: Lager = meer korte spikes; hoger = alleen duidelijke pieken.
- **5m Spike Threshold**: Bevestiging van een spike met 5m data.
  - Effect: Houdt ruis tegen die maar 1 candle duurt.
- **5m Move Threshold**: Beweging over 5 minuten.
  - Effect: Lager = meer meldingen; hoger = alleen echte moves.
- **5m Move Filter**: Extra filter om 30m-bewegingen te bevestigen.
  - Effect: Voorkomt dat 30m-alerts door "kleine" 5m ruis gaan.
- **30m Move Threshold**: Grotere beweging op 30 minuten.
  - Effect: Lager = vaker 30m alerts; hoger = alleen structureel.
- **Trend Threshold**: Wanneer we spreken van een trend (2h).
  - Effect: Bepaalt hoe snel het systeem "UP/DOWN" aangeeft.
- **Volatiliteit Laag/Hoog**: Grenzen voor de volatiliteitsstatus.
  - Effect: Geeft aan of de markt vlak, golvend of grillig is.

## 2-uur Alert Thresholds

Deze groep stuurt de "grotere" 2h alerts zoals breakout/breakdown en mean touch.

- **Breakout/Breakdown Margin (%)**: Hoe ver de koers buiten de 2h range moet gaan.
  - Effect: Hoger = minder, maar duidelijkere breakouts.
- **Breakout Reset Margin (%)**: Hoe ver terug om een nieuwe breakout te mogen melden.
  - Effect: Voorkomt meerdere alerts op bijna dezelfde breakout.
- **Breakout Cooldown (min)**: Minimale tijd tussen twee breakouts.
  - Effect: Remt spam bij snelle ranges.
- **Mean Min Distance (%)**: Minimale afstand tot avg2h voor mean reversion.
  - Effect: Voorkomt "mean" alerts bij kleine schommels.
- **Mean Touch Band (%)**: De band rond avg2h voor een "touch".
  - Effect: Bepaalt hoe strak die band is.
- **Mean Reversion Cooldown (min)**: Tijd tussen mean-alerts.
- **Compress Threshold (%)**: Max range% om compressie te melden.
  - Effect: Lager = sneller compressie alerts.
- **Compress Reset (%)**: Afstand nodig om compressie opnieuw te mogen melden.
- **Compress Cooldown (min)**: Cooldown voor compressie.
- **Anchor Outside Margin (%)**: Hoe ver anchor buiten de 2h range moet liggen.
  - Effect: Alleen meldingen als anchor echt buiten context ligt.
- **Anchor Context Cooldown (min)**: Cooldown voor anchor-context alerts.
- **Trend Hysteresis Factor**: Hoe "ver" trend moet terugvallen om te wisselen.
  - Effect: Stabiliseert trendlabels.
- **Throttle: Trend Change/Trend→Mean/Mean Touch/Compress (min)**:
  - Effect: Extra demping van snelle herhalingen.
- **2h Secondary Global Cooldown (min)**:
  - Effect: Hard cap voor alle SECONDARY alerts.
- **2h Secondary Coalesce Window (sec)**:
  - Effect: Meerdere alerts in korte tijd worden samengevoegd.

## Auto Anchor

Automatisch berekende anchor op basis van 4h/1d EMA. Dit is bedoeld om een "slimme referentieprijs" te hebben die meebeweegt met de markt, zonder dat je handmatig steeds een nieuwe anchor hoeft te zetten.

- **Anchor Bron**: `MANUAL`, `AUTO`, `AUTO_FALLBACK`, `OFF`.
  - **MANUAL**: Je zet de anchor zelf. Auto‑anchor doet niets.
  - **AUTO**: De anchor wordt volledig automatisch berekend en bijgewerkt.
  - **AUTO_FALLBACK**: Auto‑anchor is actief, maar valt terug op de handmatige anchor als er (tijdelijk) te weinig data is.
  - **OFF**: Anchor-logica wordt uitgeschakeld.
- **Update Interval (min)**: Hoe vaak het systeem *mag* updaten.
  - Lager = sneller meebewegen; hoger = stabieler, minder ruis.
- **Force Update Interval (min)**: Maximaal aantal minuten voordat er sowieso een update komt.
  - Handig als de markt lang zijwaarts beweegt en je toch af en toe een verse anchor wilt.
- **4h Candles / 1d Candles**: Hoeveel candles er worden gebruikt voor de EMA‑berekening.
  - Meer candles = gladdere, trager reagerende anchor.
  - Minder candles = sneller, maar gevoeliger voor korte swings.
- **Min Update %**: Minimale procentuele verandering voordat de anchor echt wijzigt.
  - Voorkomt dat de anchor elke kleine tik volgt.
- **Trend Pivot %**: Vanaf welk trendniveau de trend zwaarder meeweegt.
  - Hoger = pas bij duidelijke trend extra bias.
- **4h Base Weight / 4h Trend Boost**: Hoe zwaar 4h meetelt in de auto‑anchor.
  - Base = standaard gewicht.
  - Trend Boost = extra gewicht wanneer de trend sterk is.
- **Laatste Auto Anchor**: Alleen weergave van de laatst berekende waarde.
  - Handig om te zien of het systeem actief bijwerkt.
- **Notificatie bij update**: Stuurt een melding als de anchor automatisch wijzigt.
  - Fijn om op de hoogte te blijven van belangrijke verschuivingen.

## Slimme logica & filters

- **Trend-Adaptive Anchors**: Past TP/ML aan op basis van trend.
  - Effect: In UP-trend kan de winstzone groter worden, in DOWN-trend juist strakker.
  - **UP/DOWN Trend Multiplier**: Hoe sterk die aanpassing is.
- **Smart Confluence Mode**: Extra filter dat meerdere signalen tegelijk vereist.
  - Dit kijkt niet naar één enkel signaal, maar naar een combinatie van korte én iets langere bewegingen.
  - Het systeem wil zien dat de 5‑minuten beweging klopt met de 30‑minuten richting én past binnen de bredere 2‑uur context.
  - Zo voorkom je meldingen op korte tikken die direct weer terugvallen.
  - Effect: Minder meldingen, maar de meldingen die je krijgt zijn meestal relevanter.
- **Nachtstand actief**: Schakelt nachtfilters in.
  - **Nachtstand start/einde (uur)**: Tijdvenster waarin nachtlogica geldt.
  - **Nacht: 5m Spike Filter**: Strenger voor spikes in de nacht.
  - **Nacht: 5m Move Threshold**: Strenger voor 5m moves.
  - **Nacht: 30m Move Threshold**: Strenger voor 30m moves.
  - **Nacht: 5m Cooldown (sec)**: Langere rust tussen meldingen.
  - **Nacht: Auto-Vol Min/Max**: Strakkere band voor auto-vol.
- **Auto-Volatility Mode**: Past drempels automatisch aan op volatiliteit.
  - In rustige markten worden drempels iets lager, in drukke markten juist hoger.
  - Zo krijg je minder ruis‑meldingen als de markt wild is, en mis je minder als hij stil is.
  - **Volatility Window (min)**: De periode waarover het systeem kijkt om te bepalen hoe onrustig de markt is.
  - **Baseline σ (1m)**: Het “normale” niveau waartegen de 1‑minuut bewegingen worden vergeleken.
  - **Min/Max Multiplier**: De onder‑ en bovengrens voor hoe ver het systeem de drempels mag aanpassen.

## Cooldowns

- **1m/5m/30m Cooldown (sec)**: Minimale tijd tussen meldingen per timeframe.
  - Effect: Minder spam bij snelle bewegingen.

## Warm-Start

Vult de buffers met historische data bij het opstarten, zodat signalen sneller betrouwbaar zijn.

- **Warm-Start Ingeschakeld**: Zet deze opstart-hulp aan/uit.
- **Warm-Start 1m/5m overslaan**: Sla specifieke timeframes over.
- **1m Extra Candles**: Extra 1m candles bovenop de volatility window.
- **5m/30m/2h Candles**: Aantal candles om de buffers te vullen.

## Integratie (MQTT)

- **MQTT Host/Port/User/Password**: Instellingen voor je broker.
  - Effect: Na opslaan reconnect het device met deze gegevens.
- **MQTT Topic Prefix**: Read-only; afgeleid van NTFY-topic.
  - Effect: Handig om je topics snel te herkennen in Home Assistant.

## WiFi reset

- **WiFi reset (wis credentials)**: Verwijdert WiFi-gegevens en herstart.
  - Effect: Device start opnieuw in het configuratie-portal.

## Opslaan

- **Opslaan**: Bewaart alles in NVS en past wijzigingen toe.
  - Effect: Sommige wijzigingen (zoals markt) kunnen een herstart triggeren.

---

*Ga naar [Hoofdstuk 7: Alert Types en Voorbeelden](07-Alert-Types-en-Voorbeelden.md) | [Hoofdstuk 9: Integratie met Externe Systemen](09-Integratie-Externe-Systemen.md)*
