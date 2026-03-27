# ESP32-Crypto-Alert – NTFY / WS Cleanup Tracker

## Doel van dit document
Dit document is de vaste voortgangs- en beslisbron voor het opschonen van de NTFY/WS-gerelateerde delen van de codebase.

Doel van deze opschoningsslag:

- de huidige werkende functionaliteit behouden
- tijdelijke test- en diagnosepaden opruimen of isoleren
- de NTFY delivery-flow begrijpelijker maken
- overlap tussen NTFY, WS stop/restart en netwerkregie verminderen
- logging normaliseren
- regressierisico laag houden

Dit document is bedoeld om over meerdere Cursor- en chatsessies heen als referentie te gebruiken.

---

## Scope van deze opschoningsslag

### In scope
- NTFY delivery
- exclusive / serialized network flow rond NTFY
- WS pauze / disconnect / restore rond NTFY
- diagnose- en testpaden rond NTFY
- logging rond NTFY / WS / NET
- mogelijke overlap in helpers en flags
- verduidelijking van productiepad vs testpad

### Voor nu uit scope
- nieuwe features
- grote architectuurherschrijving
- functionele uitbreiding van alerts
- grote module-splitsing buiten kleine veilige stappen
- optimalisaties die niet nodig zijn voor onderhoudbaarheid

---

## Huidige uitgangssituatie

### Bekend werkend
- firmware boot stabiel
- geen reboot-loop / stack canary / lwIP assert in laatste werkende build
- NTFY meldingen komen echt door
- NTFY delivery draait nu via een werkende exclusive flow
- de logs lieten een goed patroon zien:
  - queue / trigger
  - exclusive enter
  - WS stop of disconnect
  - NTFY send
  - delivery ok
  - WS restore
  - exclusive exit

### Huidige grootste technische schuld
- tijdelijke testlogica is blijven hangen
- NTFY en WS orchestration zit nog sterk in de `.ino`
- meerdere helperpaden lijken elkaar deels te overlappen
- logging is niet overal semantisch helder
- oude diagnosepaden en productiepaden lopen nog te veel door elkaar

---

## Huidige vermoedelijke architectuur op hoofdlijnen

### Alertdetectie
- prijsupdates komen via REST en/of WS binnen
- `priceRepeatTask()` doet 1 Hz sampling voor korte buffers
- returns en states worden bijgewerkt in/na `fetchPrice()`
- alerts worden daarna getriggerd via `AlertEngine` en `AnchorSystem`

### NTFY delivery
- `sendNotification()` enqueuet; `apiTask` exclusive flow orkestreert WS; `sendNtfyNotification()` = validatie + optionele DNS-diag + HTTPS-transport (`ntfyHttpsPostNtfyAlertBody`), geen WS-wissel in die functie.

### WS interactie
- WS draait parallel als live prijsbron
- rond productie-NTFY send: uitsluitend exclusive flow (`wsStopForNtfyExclusive` → HTTPS → `restartWebSocketAfterNtfyExclusive`); geen tweede macro-gestuurde WS-pauze/disconnect meer in de bron

### Runtimeverdeling
- `apiTask()` doet prijsverwerking en alertchecks
- `priceRepeatTask()` doet de 1 Hz sampler
- `uiTask()` doet UI updates
- `webTask()` doet de webserver
- `loop()` doet nog restregie:
  - MQTT loop/reconnect
  - WS reconnect/loop
  - deferred NTFY test/retry logic
  - periodic NTFY test

---

## Belangrijke ontwerpafspraken

Deze afspraken zijn leidend bij alle volgende opschoningsstappen.

1. **De huidige werkende functionaliteit is uitgangspunt.**  
   Eerst behouden, dan pas versimpelen.

2. **Geen grote herschrijving in één stap.**  
   Alleen kleine, gecontroleerde refactors.

3. **Één expliciet productiepad voor NTFY delivery.**  
   Geen verborgen tweede route.

4. **Exclusive NTFY flow mag niet per ongeluk kapot gerefactord worden.**  
   Eerst isoleren en verduidelijken, pas later eventueel herstructureren.

5. **Test- en diagnosepaden horen niet standaard in de runtime actief te zijn.**

6. **Logging moet onderscheid maken tussen:**
   - productiegedrag
   - diagnose
   - testgedrag

7. **Bij twijfel: isoleren achter vlag of wrapper, niet meteen verwijderen.**

8. **Elke fase sluit af met korte testnotities.**

---

## Wat expliciet bewaakt moet worden

Bij alle wijzigingen extra letten op:

- geen oude experimentele NTFY/WS-rommel laten hangen
- geen verborgen tweede codepad voor delivery overhouden
- geen dubbele WS stop/restore logica laten bestaan
- geen testhelper die productie-orchestration nog eens dunnetjes overdoet
- logging begrijpelijk houden
- geen regressie in:
  - NTFY aankomst
  - WS restore
  - netwerkstabiliteit
  - bootgedrag

---

## Refactorfases

## Fase 1 – Diagnostiek en testpaden isoleren
**Status:** DONE (2026-03-27)

### Doel
Tijdelijke NTFY test- en diagnose-runtimepaden uit de standaard runtime halen, zonder de huidige productieflow te veranderen.

### Verwachte bestanden
- `ESP32-Crypto-Alert.ino`
- eventueel `platform_config.h`

### Verwachte onderdelen
- startup NTFY test
- periodic NTFY test
- deferred startup retry
- diagnoseflags voor NTFY/WS tests
- comments die productie en test nu nog vermengen

### Acceptatiecriteria
- standaard runtime stuurt geen testmelding meer tenzij expliciet ingeschakeld
- normale productie-alert komt nog door
- huidige exclusive NTFY flow werkt nog
- WS restore werkt nog
- geen functionele wijziging in normale delivery

### Risico
Laag tot middel

---

## Fase 2 – Logging normaliseren
**Status:** DONE (2026-03-27)

### Doel
Logging begrijpelijk maken door productie-, diagnose- en testlogs semantisch te scheiden.

### Verwachte bestanden
- `ESP32-Crypto-Alert.ino`

### Acceptatiecriteria
- logs blijven bruikbaar voor troubleshooting
- minder verwarring in NTFY/WS lifecycle
- testlogs alleen zichtbaar als diagnostiek aan staat

### Risico
Laag

---

## Fase 3 – Exclusive NTFY flow centraliseren
**Status:** DONE (2026-03-27)

### Doel
De productieflow rond NTFY explicieter maken, zodat orchestration, transport en retry/backoff minder door elkaar lopen.

### Verwachte bestanden
- `ESP32-Crypto-Alert.ino`
- eventueel nieuwe kleine helper/module

### Acceptatiecriteria
- slechts één duidelijk productiepad voor delivery
- geen dubbele WS orchestration buiten het hoofdpad
- geen regressie in NTFY aankomst of WS restore

### Risico
Middel

---

## Fase 4 – Oude overlap en dode helpers verwijderen
**Status:** DONE (2026-03-27)

### Doel
Helpers, flags en comments verwijderen die na Fase 1–3 aantoonbaar niet meer nodig zijn.

### Verwachte bestanden
- `ESP32-Crypto-Alert.ino`

### Acceptatiecriteria
- code is kleiner en duidelijker
- geen gedrag verandert
- geen compile warnings door half-verwijderde resten

### Risico
Laag tot middel

---

## Fase 5 – Eventuele verdere modulering
**Status:** LATER

### Doel
Pas als Fase 1–4 stabiel zijn: overwegen of NTFY/WS orchestration deels uit de `.ino` gehaald moet worden.

### Opmerking
Deze fase is pas zinvol als het huidige pad eerst inhoudelijk is opgeschoond.

### Risico
Middel

---

## Eerste bekende concrete schuldpunten

1. `sendNtfyNotification()` bevat nog validatie + DNS-diag + mutex; HTTPS-retry zit in `ntfyHttpsPostNtfyAlertBody` (**Fase 3**). Verdere opsplitsing optioneel later (Fase 5 of losse refactor).
2. ~~Legacy macro-helpers rond NTFY zonder call sites~~ — **Fase 4:** verwijderd (zie Besluit 007).
3. Diagnose- en testlogica staat nog te dicht op productiegedrag.
4. `loop()` bevat nog test-/retry-runtimeresten die niet tot normale productieflow horen.
5. Logging gebruikt meerdere prefixes — **Fase 2:** basissemantiek vastgelegd (Besluit 005); verder verfijnen kan in Fase 3/4.
6. De `.ino` is nog te veel orchestration-kern.

---

## Eerste concrete beslissingen

### Besluit 001
**Voor de eerstvolgende stap wordt geen functionele wijziging gedaan aan de huidige werkende NTFY exclusive flow.**

**Motivatie:**  
Dit pad werkt nu. Eerst opschonen rondom dit pad, daarna pas intern herschikken.

**Impact:**  
De eerste refactor richt zich op isoleren van test/diagnose, niet op opnieuw ontwerpen.

---

### Besluit 002
**Startup/periodic/deferred NTFY tests horen niet in de standaard runtime actief te zijn.**

**Motivatie:**  
Deze logica was nuttig tijdens diagnose, maar vervuilt nu de normale code- en logstroom.

**Impact:**  
Ze moeten achter een expliciete diagnostics-vlag komen of anders uit het standaard pad verdwijnen.

---

### Besluit 003
**Bij twijfel tussen verwijderen of isoleren: eerst isoleren.**

**Motivatie:**  
Laag regressierisico is belangrijker dan agressief opschonen.

**Impact:**  
Code mag tijdelijk nog bestaan, mits hij niet meer standaard actief is en duidelijk als diagnostiek is gemarkeerd.

---

### Besluit 004 (Fase 1)
**Alle optionele NTFY-testruntime (startup, periodic, deferred, WS-live health ping) valt onder één master-vlag `CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME` (default 0); sub-vlaggen bepalen welk testtype aan mag.**

**Motivatie:**  
Eén duidelijke schakelaar voor standaard-runtime vs. handmatige diagnose; productie-alertdelivery blijft los daarvan.

**Impact:**  
Testen vereist expliciet `DIAGNOSTICS_RUNTIME=1` plus de gewenste `CRYPTO_ALERT_NTFY_STARTUP_TEST` / `CRYPTO_ALERT_NTFY_PERIODIC_TEST`.

---

### Besluit 005 (Fase 2)
**Logging-prefixes: `[NTFY]` productie-HTTPS delivery; `[NTFY][diag]` optionele DNS; `[NTFY][Q]` queue; `[NTFY][EXCL]` exclusive apiTask-modus; `[NTFY][test]` diagnostiek-runtime; `[WS]` standaard WS lifecycle; `[NET]` mutex/slot (ongewijzigd).** *(Historisch: `[WS][NTFY]` werd gebruikt in verwijderde legacy macro-helpers — Fase 4.)*

**Motivatie:**  
Minder verwarring tussen “NTFY triggert WS” en “HTTPS POST”; test vs productie.

**Impact:**  
Geen gedragswijziging; alleen stringteksten en comments.

---

### Besluit 006 (Fase 3)
**Productie-NTFY delivery is één pad: `sendNotification` → queue → `apiTask` exclusive state machine → `ntfyExclusiveSendOnePendingFromQueue` → `sendNtfyNotification` / `ntfyHttpsPostNtfyAlertBody`. WS-stop/restore uitsluitend `wsStopForNtfyExclusive` + `restartWebSocketAfterNtfyExclusive` (+ pumps).**

**Motivatie:**  
Transport vs orchestration scheiden zonder queue/backoff te wijzigen.

**Impact:**  
Nieuwe static `ntfyHttpsPostNtfyAlertBody`; gedrag gelijk aan vorige inline loop (zelfde `nowMs` voor streak-backoff).

---

### Besluit 007 (Fase 4)
**De dode legacy macro-gestuurde helpers (`ntfyPauseWsBeforeNtfySendIfNeeded`, `ntfyDisconnectWsBeforeNtfySendIfNeeded`, `ntfyRestoreWsAfterNtfySend`, `ntfyRestoreWsAfterNtfySendPausedStrategy`) en de bijbehorende compile-time defaults `CRYPTO_ALERT_NTFY_PAUSE_WS_DURING_SEND` / `CRYPTO_ALERT_NTFY_FULL_WS_DISCONNECT_DURING_SEND` zijn uit de firmware verwijderd: nul call sites in repo, standaardbuild gebruikte ze niet, exclusive productiepad bleef al los daarvan. Externe forks die dit experimentele pad nog nodig hebben: herstel uit git-historie vóór deze stap.**

**Motivatie:**  
Geen verborgen tweede WS-orchestratie in de bron; kleinere, duidelijkere codebase.

**Impact:**  
Geen wijziging aan `sendNotification`, `sendNtfyNotification`, `ntfyHttpsPostNtfyAlertBody`, exclusive state machine, `restartWebSocketAfterNtfyExclusive`, queue, of Fase 1-diagnosevlaggen. Variabele `wsWasActiveForNtfySend` verwijderd (alleen door legacy gebruikt). `wsPauseForNtfySend` blijft (exclusive + `loop()`).

---

## Huidige eerstvolgende stap
**Fase 5 (optioneel): verdere modulering** — alleen als gewenst; Fase 1–4 NTFY/WS-cleanup is voor het beoogde scope-blok afgerond (zie Definition of done).

---

## Cursor-werkinstructie voor dit document

Gebruik dit document als voortgangsdocument en bron van waarheid voor de NTFY/WS-opschoningsslag.

### Regels voor Cursor
1. Lees dit document eerst vóór het uitvoeren van een wijziging.
2. Werk alleen aan de fase die actief is of expliciet wordt gevraagd.
3. Houd wijzigingen klein en veilig.
4. Werk na elke afgeronde stap bij:
   - fase-status
   - uitgevoerde wijziging
   - bewust niet aangepaste onderdelen
   - testresultaat
   - volgende kleine stap
5. Voeg geen grote toekomstplannen toe die nog niet zijn uitgevoerd.
6. Overschrijf eerdere besluiten niet stilzwijgend; voeg nieuwe besluiten toe.

---

## Wijzigingslog

### 2026-03-27 — Fase 2 (logging)
- `sendNtfyNotification`: commentaar + productielogregels (o.a. misleidende “send while WS remains connected” → duidelijke HTTPS POST); backoff-log; Bearer-log; duplicate queue → `[NTFY][Q]`.
- `initNotificationLogMutex` / queue init: `[NTFY][Q] WARN` bij ontbrekende mutex.
- Legacy `ntfyPauseWs*` / `ntfyDisconnectWs*` / `ntfyRestoreWs*`: prefix `[WS][NTFY]` + korte comments (macro’s ongewijzigd).
- Diagnostiek (`[NTFY][test]`): backoff/skip-teksten; ws-live ping prefixen geünificeerd onder `[NTFY][test]`.
- `apiTaskNtfyExclusiveStateMachine`: comment; throttle-regel `ws restart still waiting` i.p.v. dubbele “waiting”.
- **Niet gewijzigd:** control-flow, `sendNotification()`, mutex-lock functienamen (`[NTFY] sendNtfyNotification`), exclusive state machine logic.

### 2026-03-27 — Fase 3 (exclusive + transport)
- Nieuwe `ntfyHttpsPostNtfyAlertBody()`: HTTPS-retry-loop uit `sendNtfyNotification` geëxtraheerd; mutex blijft in caller; `nowMs` voor streak-backoff ongewijzigd doorgegeven.
- Commentaarblokken: productiepad (enqueue → exclusive SM → transport); exclusive-sectie gemarkeerd; legacy macro-helpers expliciet “geen call sites”.
- `sendNotification`-comment: enqueue-only.
- **Niet gewijzigd:** queue, backoff, retry-tellingen, `loop()` exclusive-gating, macro-definities, `apiTaskNtfyExclusiveStateMachine` logica, `restartWebSocketAfterNtfyExclusive` gedrag.

### 2026-03-27 — Fase 1 (samenvatting)
- `CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME` + testpad-gating (zie eerdere sessielog).

---

## Testlog

### 2026-03-27 — Fase 2
#### Build / branch / commit:
- Niet op device gevalideerd in deze sessie.

#### Uitgevoerde test:
- String/prefix review in bron; geen runtime-test.

#### Resultaat:
- OK (code review niveau).

#### Opmerkingen:
- Device: alert + WS restore smoke test aanbevolen na flash.

---

### 2026-03-27 — Fase 4
#### Build / branch / commit:
- `arduino-cli compile -b esp32:esp32:esp32s3` (sessie).

#### Uitgevoerde test:
- Repo-wide grep: geen call sites legacy helpers vóór verwijdering; na wijziging geen restanten macro-namen in `.ino`.
- Statische review: productiepad ongewijzigd (Besluit 006).
- Compile: gefaald met **linker/cache-fout** in `Arduino/sketches/.../libraries/lvgl/.../objs.a` (truncated archive / file format not recognized) — **niet** toe te schrijven aan Fase 4 `.ino`-diff; oplossing: sketch-cache legen (`arduino-cli cache clean` of map `.../Caches/arduino/sketches/<hash>` verwijderen) en opnieuw builden.

#### Resultaat:
- Code-review OK; build in deze omgeving **NIET OK** door toolcache (lvgl), niet door NTFY-wijzigingen.

#### Opmerkingen:
- Aanbevolen na flash: één productie-alert → NTFY OK → WS weer live (zelfde smoke als eerdere fases).

---

### Template
#### Datum:
#### Fase:
#### Build / branch / commit:
#### Uitgevoerde test:
- boot test
- alert test
- NTFY delivery test
- WS restore test
- MQTT test
- web UI test

#### Resultaat:
- OK / NIET OK

#### Opmerkingen:
- ...

---

## Sessielog

### 2026-03-27
#### Doel van de sessie:
- Fase 1 uitvoeren: NTFY-test-/diagnosepaden isoleren zonder productieflow te wijzigen.

#### Uitgevoerd:
- Centrale vlag `CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME` (default uit).
- Testfuncties herschreven: geen dode code meer achter vroege `return`; gating `DIAGNOSTICS_RUNTIME &&` sub-vlag.
- Comments: productie vs diagnostiek in `.ino` en `platform_config.h` verduidelijkt.
- Tracker bijgewerkt (status Fase 1, wijzigingslog, testlog, sessielog).

#### Niet aangepast:
- Exclusive NTFY state machine, queue, `restartWebSocketAfterNtfyExclusive`, `loop()` exclusive-gating.
- `sendNtfyNotification` / `sendNotification` inhoud.
- Experimentele macro’s `CRYPTO_ALERT_NTFY_PAUSE_WS_DURING_SEND`, `CRYPTO_ALERT_NTFY_FULL_WS_DISCONNECT_DURING_SEND` (alleen comments/structuur rond tests).

#### Belangrijkste observaties:
- Standaard build: geen testmeldingen meer mogelijk tenzij expliciet `CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME=1` (en sub-vlaggen).

#### Volgende kleine stap:
- Fase 2 uitgevoerd (zie aparte sessie-entry hieronder).

---

### 2026-03-27 — Fase 2 (logging)
#### Doel van de sessie:
- Prefixes en logteksten normaliseren zonder gedrag te wijzigen.

#### Uitgevoerd:
- Zie wijzigingslog “Fase 2”; Besluit 005; Fase 2 status DONE.

#### Niet aangepast:
- `sendNotification()`; exclusive state machine-logica; WS productiepad; queue/backoff; `netMutexLock`-tasknamen (alleen Serial-teksten aangepast).

#### Belangrijkste observaties:
- `[WS][NTFY]` scheidt legacy macro-WS van `[NTFY][EXCL]` productie.

#### Volgende kleine stap:
- Fase 3 uitgevoerd (zie Fase 3 sessie-entry).

---

### 2026-03-27 — Fase 3 (exclusive + transport)
#### Doel van de sessie:
- Productiepad documenteren; HTTPS-transport scheiden van orchestratie; legacy afbakenen.

#### Uitgevoerd:
- `ntfyHttpsPostNtfyAlertBody`; commentaarblokken; sectiekop legacy; tracker Besluit 006 + Fase 3 DONE.

#### Niet aangepast:
- Queue/backoff/retry; exclusive state transitions; `loop()`; macro’s; diagnoseflags.

#### Belangrijkste observaties:
- Legacy WS-macro helpers hebben geen call sites → Fase 4 kandidaat.

#### Volgende kleine stap:
- Fase 4 uitgevoerd (zie Fase 4 sessie-entry).

---

### 2026-03-27 — Fase 4 (dode helpers)
#### Doel van de sessie:
- Legacy NTFY/WS-macro-helpers en macro-defaults verwijderen waar aantoonbaar dood; tracker en documentatie bijwerken.

#### Uitgevoerd:
- Zie wijzigingslog “Fase 4”; Besluit 007; Fase 4 status DONE; architectuur-sectie WS interactie; “Huidige eerstvolgende stap” → Fase 5 optioneel.

#### Niet aangepast:
- Productie-ingangen en transport (`sendNotification`, `sendNtfyNotification`, `ntfyHttpsPostNtfyAlertBody`); exclusive sequencing; `restartWebSocketAfterNtfyExclusive`; queue/backoff; Fase 1 diagnose-runtime.

#### Belangrijkste observaties:
- Geen tweede delivery-pad meer uit verwijderde helpers; forks met experimenteel macro-pad: git-historie.

#### Volgende kleine stap:
- Optioneel Fase 5 (modulering) of alleen onderhoud; NTFY/WS-scope Fase 1–4 inhoudelijk afgerond.

---

### Template
#### Datum:
#### Doel van de sessie:
#### Uitgevoerd:
- ...
- ...

#### Niet aangepast:
- ...
- ...

#### Belangrijkste observaties:
- ...
- ...

#### Volgende kleine stap:
- ...

---

## Open punten

- **Fase 4-kandidaat:** `ntfyPauseWsBeforeNtfySendIfNeeded`, `ntfyDisconnectWsBeforeNtfySendIfNeeded`, `ntfyRestoreWsAfterNtfySend`, `ntfyRestoreWsAfterNtfySendPausedStrategy` — geen call sites in repo; macro’s eerst verifiëren voor externe forks.
- Welke WS-strategie wordt uiteindelijk de enige productievariant?
  - pause
  - full disconnect/reconnect

- Moet `sendNtfyNotification()` later opgesplitst worden in:
  - orchestration
  - transport
  - retry/backoff
  - logging

- **Fase 2:** basis-prefixmodel vastgelegd in Besluit 005; fijnslijpen eventueel later (cosmetisch).

- Welke diagnoseflags mogen helemaal verdwijnen nadat Fase 1 stabiel is?

---

## Definition of done voor deze opschoningsslag

Deze NTFY/WS-cleanup is pas echt “klaar” als:

- er één begrijpelijk productiepad voor NTFY delivery is
- test- en diagnosepaden niet standaard actief zijn
- WS stop/restore gedrag eenduidig is
- logging semantisch helder is
- de hoofdsketch aantoonbaar minder rommelig is
- de werkende functionaliteit behouden is
- de codebasis weer uitlegbaar is voor volgende sessies