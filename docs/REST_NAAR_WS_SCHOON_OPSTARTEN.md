## Doel
Een praktische handleiding om vanaf de originele, stabiele REST‑versie opnieuw te migreren naar WebSocket, met minimale belasting en duidelijke tussenstappen.

## Uitgangspunt
- Start vanuit een bewezen werkende REST‑build.
- Gebruik WebSocket alleen voor live prijzen.
- Gebruik REST voor historische candles/warm‑start (optioneel).
- Werk stap voor stap en verifieer na elke stap.

## Stap 0 — Schoon vertrekpunt
1. Zet de code terug op de laatste stabiele REST‑versie.
2. Verwijder tijdelijk WS‑gerelateerde code/flags als die al in de branch zitten.
3. Compileer en test:
   - REST prijs ophalen
   - Warm‑start OK
   - UI OK
   - MQTT/NTFY OK

## Stap 1 — Minimale WS‑verbinding (geen subscribe)
Doel: alleen de TLS/WS handshake stabiel krijgen.
- Voeg WebSocketsClient toe.
- Alleen `beginSSL()` + `loop()`; nog geen subscribe.
- Log:
  - `[WS] Connecting`
  - `[WS] Connected`
  - `[WS] Disconnected`

**Succescriterium:** stabiel connecten zonder crash of memory errors.

## Stap 2 — Subscribe op ticker (live prijs)
Doel: live prijs via WS ontvangen.
- Verstuur subscribe payload na connect.
- Parse alleen `bestBid` (of `last`) en update `wsLastPrice`.
- Schrijf WS‑prijs naar `prices[0]` in één mutex‑section.
- Schakel REST prijs op dit punt nog niet uit.

**Succescriterium:** regelmatige `[WS] Price parsed` zonder disconnects.

## Stap 3 — WS als primaire prijsbron
Doel: REST prijs alleen als fallback.
- In `fetchPrice()`:
  - Als WS verbonden + prijs: gebruik WS.
  - Alleen REST als WS niet beschikbaar is.
- Voeg eenvoudige backoff toe bij REST (tegen flood).

**Succescriterium:** prijs blijft up‑to‑date ook bij WS hapering.

## Stap 4 — Warm‑start via REST, daarna WS
Doel: historische candles via REST, live via WS.
- Warm‑start blijft REST‑gebaseerd.
- Na warm‑start:
  - REST prijs uit
  - WS prijs aan
- Laat REST candles voor trends intact.

**Succescriterium:** trends kloppen, live prijs via WS.

## Stap 5 — Lichtgewicht modus voor CYD
Doel: heap stabiel houden.
- Webserver uit of ultra‑light.
- MQTT/NTFY uitstellen tot WS eerste prijs heeft.
- LVGL pas na warm‑start init.
- Geen zware String‑allocaties in loops.

**Succescriterium:** geen `largest` dip < 2000, geen TLS alloc errors.

## Stap 6 — Stabiliteit & recovery
Doel: WS herstel zonder vastlopen.
- Herconnect backoff (bijv. 5s/10s).
- Geen WS‑handshake als heap te laag is.
- Log timestamps voor WS‑events.

**Succescriterium:** WS herstelt zonder reboot, REST fallback blijft werken.

## Stap 7 — Webconfig of MQTT‑config
Kies één:
- **MQTT‑config** (licht): topics voor instellingen.
- **Web‑config**: tijdelijk WS pauzeren, settings opslaan, WS herstart.

**Tip:** houd MQTT‑prefix vast (niet afhankelijk van handmatige NTFY‑topic).

## Checks per stap
Gebruik dit korte checklist‑blok na elke stap:
- ✅ WS connect OK
- ✅ Geen memory errors
- ✅ UI blijft draaien
- ✅ Heap largest > 2000
- ✅ MQTT/NTFY alleen als WS stabiel

## Minimalistische WS‑payload (voorbeeld)
```
{"action":"subscribe","channels":[{"name":"ticker","markets":["BTC-EUR"]}]}
```

## Tip: Log‑tijden toevoegen
Gebruik `[millis()]` in WS logs om reconnect‑gaten te kunnen zien.

## Einde
Deze route is bewust conservatief. Als een stap instabiel wordt, rollback en pas alleen die stap aan.
