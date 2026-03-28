# NTFY Alert Correctness / Latency / Usefulness Validation

**Repo:** `ESP32-Crypto-Alert`  
**Basisbranch:** `main`  
**Laatste doc-afstemming (code):** commit `d8831bf` (2026-03-28)  
**Startdatum validatiefase:** 28 maart 2026  
**Status:** werkdocument / levende basis voor vervolgpatches en analyses

---

## 0. Revisie: focus verschuift naar de notificaties zelf

**Besluit:** er wordt **niet verder gesleuteld aan WebUI-pagina’s** (geen verdere uitbreiding van `/notifications`, geen “forensische” analysepagina in de browser als werkprioriteit).

**Gevolg voor deze fase:** validatie en verbeteringen richten zich op het **NTFY-pad in de firmware** (detectie → queue → send), **logging / observability** (seriële audit waar nodig), en **externe bronnen** (NTFY-app, eigen grafieken, exports). De ingebouwde webpagina’s blijven hooguit een **eenvoudige hulplijst**; ze zijn geen onderwerp van doorontwikkeling meer.

Onderstaande secties over “gewenste WebUI-structuur” en uitgebreide analyse-UI blijven als **referentie / achtergrond** staan, maar zijn **geen actief werkplan** voor de webserver.

---

## 1. Doel van dit document

Dit document is de vaste werkbasis voor de validatiefase van de NTFY-notificaties in `ESP32-Crypto-Alert`.

De vorige fase stond in het teken van **stabiliteit en delivery**:
- NTFY delivery werkt
- exclusive mode werkt
- WS stop/restart rond NTFY werkt
- oude experimentele paden zijn verwijderd
- smoke test is afgerond en de cleanup zit op `main`

De nieuwe fase richt zich op drie hoofdvragen:

1. **Correctness**  
   Kloppen notificaties inhoudelijk met de koersdata en alertlogica waarop ze gebaseerd zijn?

2. **Latency / timing**  
   Komen notificaties snel genoeg en op het juiste moment?

3. **Usefulness / bruikbaarheid**  
   Zijn notificaties praktisch nuttig voor de gebruiker, of bevatten ze te veel ruis, te weinig context, of te late informatie?

De **beoordeling** gebeurt primair via **firmwarelogs, NTFY-ontvangst en externe analyse** — niet via uitbreiding van de webinterface.

Dit document bewaakt:
- de onderzoeksvragen
- de werkvolgorde
- de hypotheses
- de meetstrategie
- het onderscheid tussen meetpatches en gedragspatches (UI-wijzigingen zijn geen onderdeel van deze roadmap; zie §8.2)
- de uitkomsten per stap

---

## 2. Scope en expliciete afbakening

### Wel in scope
- alert-correctheid
- data freshness
- semantische consistentie tussen trigger en meldtekst
- latency tussen marktevent, detectie, send en zichtbare log
- suppressiegedrag en mogelijke false positives / false negatives
- bruikbaarheid van meldingen voor monitor-/handelcontext
- **kleine meet-/auditpatches** in de firmware (seriële logging, timing, suppressie-redenen) zonder gedrag te wijzigen
- inhoud en timing van **NTFY-berichten** en de **interne send-queue**

### Bewust buiten scope (geen doorontwikkeling)
- **WebUI** (`/`, `/config`, `/notifications`, …): geen nieuwe features, geen “forensische” uitbreiding van de notificatiepagina als projectdoel
- grote UI-/webserver-refactors

### Niet het hoofdonderwerp van deze fase
- opnieuw opschonen van WS/NTFY-architectuur
- grote refactors
- nieuwe notificatiesystemen
- nieuwe settings of configuratiestructuren als eerste stap

### Belangrijk uitgangspunt
De huidige werkende **WS/NTFY-architectuur op `main`** is voorlopig het vaste uitgangspunt.  
Veranderingen in gedrag komen pas **na** meetbare observaties.

---

## 3. Huidige uitgangssituatie

### Bevestigde uitgangspunten
- NTFY delivery werkt
- exclusive flow werkt
- WS wordt rond NTFY gecontroleerd gestopt/herstart
- oude dubbele/experimentele NTFY-paden zijn verwijderd
- documentatie rond de eerdere cleanup is bijgewerkt

### Wat de code nu doet (kort, ter afstemming van dit document)

**Productiepad alert → NTFY** (zie `ESP32-Crypto-Alert.ino`):
- `sendNotification(...)` roept `enqueueNtfyPending(...)` aan; daarna `appendNotificationLog(...)` met `sent` = 1 als de queue de melding accepteerde, anders 0.
- De **webpagina `/notifications`** is een **eenvoudige read-only tabel**: kolommen index, tijd (`millis` sinds boot), resultaat (OK/FAIL = queue acceptatie), titel, bericht. Er is **geen** HTML-escape van titel/bericht; er is **geen** trigger-snapshot, freshness of NTFY-tag-kolom in de UI.
- De **interne log** is een ringbuffer `NOTIF_LOG_SIZE` = **64** entries; de pagina toont **alle** entries die `getNotificationLogCount()` teruggeeft (dus maximaal 64 wanneer de buffer vol is), **nieuwste eerst** via `getNotificationLogEntry`.

**Gevolg:** “OK” in de webtabel betekent **acceptatie in de send-queue**, niet gegarandeerde aflevering op NTFY; forensische analyse vereist **serial/NTFY-side** observability, niet de browser.

### Werkhypothese voor deze fase
De grootste resterende risico’s zitten nu niet meer in **technische aflevering**, maar in:
- onduidelijke herleidbaarheid van de trigger
- mogelijke mismatch tussen triggerdata en meldtekst
- onvoldoende zicht op freshness
- onvoldoende zicht op latency tussen detectie en verzending
- mogelijke suppressie van nuttige alerts of juist ruis-alerts
- onvoldoende **firmware-/logcontext** om meldingen snel en betrouwbaar te beoordelen (los van WebUI)

---

## 4. Korte beschrijving van de huidige signaalketen

Dit is een functionele samenvatting, bedoeld als denkkader voor validatie.

### 4.1 Prijsbronnen
De firmware gebruikt live koersinformatie uit:
- WebSocket (WS)
- REST fallback / fetchpad

Binnen runtime bestaan meerdere representaties van de “actuele prijs”, waaronder:
- `latestKnownPrice`
- `latestKnownPriceMs`
- `latestKnownPriceSource`
- `lastFetchedPrice`
- `prices[0]`

### 4.2 Buffering / sampling
Live prijsinformatie wordt periodiek verwerkt in buffers, waaronder:
- secondebuffer(s)
- 5m buffer
- minuutgemiddelden / langere vensters
- 2h contextvensters

### 4.3 Afgeleide metrics
Op basis van buffers en/of live toestand worden onder meer gebruikt:
- `ret_1m`
- `ret_5m`
- `ret_30m`
- 2h-contextmetrics
- volume/range confirmatie
- volatility-effectieve thresholds
- confluence-state
- anchor-context

### 4.4 Detectie
Detectie gebeurt in hoofdzaak via:
- `checkAndNotify(...)`
- `check2HNotifications(...)`
- aanpalende alertlogica zoals confluence / anchor checks

### 4.5 Verzending
Bij een alertbeslissing loopt het productiepad via:
- `sendNotification(...)` → `enqueueNtfyPending(...)` (queue) → `appendNotificationLog(...)` (best-effort log)
- daarna leegt `apiTask` (exclusive pad) de queue en voert de echte HTTPS-send uit

### 4.6 Beoordeling (buiten uitbreiding WebUI)
Voor correctness/latency-validatie zijn relevant:
- **Seriële logs** / eventuele auditregels op de ESP
- **NTFY-ontvangst** (telefoon/desktop) met tijdstempels
- **Externe grafieken** of exports, naast de eenvoudige `/notifications`-lijst
- Optioneel: bestaande read-only routes zoals `/config` alleen om configuratie te verifiëren — **geen verdere uitbreiding gepland**

### 4.7 Mogelijke semantische zwakke plekken
De belangrijkste plekken waar fouten of verwarring kunnen ontstaan:
- trigger wordt bepaald op basis van buffer-/returnlogica
- meldtekst gebruikt mogelijk een andere representatie van de prijs
- freshness van de gebruikte live prijs is niet expliciet zichtbaar
- suppressie vindt plaats zonder volledige audittrail
- 2h secondary alerts kennen throttling/coalescing en dus mogelijk extra vertraging
- de ingebouwde `/notifications`-pagina toont bewust alleen basisvelden; forensische context ontbreekt daar en moet uit logging/externe analyse komen

---

## 5. Centrale onderzoeksvragen

### 5.1 Correctness
- Is de alert gebaseerd op de juiste prijsbron?
- Is de gebruikte prijs vers genoeg?
- Past de meldtekst bij de triggercontext?
- Kloppen thresholds, bands, top/dal, returns en contextwaarden?
- Zijn er false positives?
- Zijn er false negatives?
- Worden meldingen soms getriggerd op stale, inconsistente of te ruisgevoelige data?

### 5.2 Timing / latency
- Hoeveel tijd zit tussen marktevent en detectie?
- Hoeveel tijd zit tussen detectie en daadwerkelijke send?
- Hoeveel tijd kost de NTFY-send zelf?
- Leidt exclusive mode tot merkbare vertraging?
- Komen sommige meldingen feitelijk te laat voor praktisch gebruik?
- Worden sommige meldingen juist te vroeg op instabiele data getriggerd?

### 5.3 Usefulness
- Is het signaal bruikbaar of te veel ruis?
- Is de meldtekst snel begrijpelijk?
- Zit er genoeg context in de melding?
- Zijn meldingen dubbel of semantisch overlappend?
- Zijn suppressies praktisch nuttig of te agressief?
- Ondersteunt de timing een zinvolle monitor-/handeltoepassing?

### 5.4 Observability (firmware / extern)
- Kunnen we uit logs + NTFY + eigen grafieken reconstrueren: trigger, prijsbron, freshness, suppressie en latency?
- Is er voldoende seriële (of andere) audit om queue-acceptatie vs werkelijke send te onderscheiden?

---

## 6. Eerste risicohypothesen

Deze hypotheses sturen de meetfase.

### H1 — triggerprijs en meldprijs lopen soms uiteen
De alertlogica kan gebaseerd zijn op een buffersnapshot of return-context, terwijl de meldtekst de prijs van een later moment toont.

### H2 — freshness is niet goed zichtbaar en mogelijk soms onvoldoende
Een melding kan technisch correct verstuurd zijn, maar gebaseerd zijn op een live prijs die al te oud is voor de context van de alert.

### H3 — suppressiegedrag is niet goed genoeg te reconstrueren
Er is onvoldoende zicht op waarom een alert wel of niet is doorgelaten:
- cooldown
- hourly limit
- volume/range confirmatie
- confluence suppressie
- 2h throttling/coalescing
- night mode filters

### H4 — 2h secondary alerts kunnen “te laat” aanvoelen
Niet per se omdat delivery faalt, maar omdat throttling/coalescing en contextlogica bewust vertraging of onderdrukking veroorzaken.

### H5 — sommige alerts zijn technisch waar maar semantisch onduidelijk
De gebruiker ziet een melding die intern klopt, maar niet goed aansluit bij het intuïtieve beeld van de markt op dat moment.

### H6 — volume/range confirmatie kan in sommige situaties te streng of juist te zwak zijn
Hier kunnen zowel false negatives als false positives uit voortkomen.

### H7 — forensische reconstructie zit nog niet in één plek
Zonder gerichte **meet-/auditlogging** in de firmware is het lastig om trigger, suppressie en end-to-end latency naast elkaar te leggen; de minimale webtabel compenseert dat niet.

---

## 7. Beslisregels voor deze validatiefase

Deze regels gelden als vaste werkafspraken.

1. **Eerst meten, dan gedrag wijzigen**
2. **Geen tekstpolish vóór correctness/freshness duidelijk is**
3. **Meetpatches blijven klein en lokaal**
4. **Geen WebUI-wijzigingen als standaardmiddel** — bij twijfel eerst meet-/auditlogging, niet browserwerk
5. **Gedragspatches worden apart benoemd en gelogd**
6. **Geen grote refactor zolang observability nog onvoldoende is**
7. **De huidige werkende WS/NTFY-flow blijft uitgangspunt**
8. **Bij twijfel eerst auditlog (serial) toevoegen, niet direct alertlogica aanpassen**

---

## 8. Onderscheid tussen patchtypes

### 8.1 Meetpatch
Doel:
- observability
- auditlogging
- reconstrueren van oorzaak/timing
- geen functionele gedragswijziging

Voorbeelden:
- extra seriële auditregels
- log van freshness / source / detectietijd
- log van suppressie-redenen
- log van sendduur

### 8.2 Analyse-UI patch (historisch / niet meer actief)
Eerder gepland voor browseruitbreidingen; **geen prioriteit meer**. Eventuele toekomstige UI-wijzigingen vallen buiten de huidige validatierichting (zie §0).

### 8.3 Gedragspatch
Doel:
- daadwerkelijk veranderen van alertgedrag

Voorbeelden:
- threshold-wijziging
- suppressie aanpassen
- meldtekstinhoud wijzigen
- timing/cadans aanpassen
- cooldowns wijzigen
- volume/range logica wijzigen

### Regel
Een patch mag **niet ongemerkt van meetpatch naar gedragspatch schuiven**.  
Dat onderscheid moet expliciet benoemd worden in dit document.

---

## 9. Praktische meetstrategie

### 9.1 Wat we minimaal zichtbaar willen maken
Per relevante alert willen we kunnen reconstrueren:
- welk alerttype het was
- op welk moment detectie plaatsvond
- welke live bron actief was
- hoe oud die live prijs was
- welke returns/metrics de trigger droegen
- welke prijs in de melding getoond werd
- waarom de alert verzonden of onderdrukt werd
- hoe lang verzending duurde
- welke configuratie actief was (via export/serial/notities, niet verplicht via WebUI)
- hoe de melding zich verhoudt tot externe koers- en return-grafieken

### 9.2 Gewenste meetpunten

#### Detectie / evaluatie
- alerttype
- `millis()` bij evaluatie
- `latestKnownPriceSource`
- `millis() - latestKnownPriceMs`
- `prices[0]`
- relevante returns
- min/max indien al berekend
- suppressie-reden indien van toepassing

#### Sendpad
- tijdstip vlak vóór HTTP-send
- tijdstip direct na HTTP-send
- HTTP-resultaat
- retry-attempt indien relevant
- backoff/rate-limit indicatie indien aanwezig

#### Notification-log (device) / web
- moment (`millis`) en queue-resultaat (OK/FAIL) staan in de ringbuffer en op `/notifications` alleen als **basisvelden**
- **geen** alerttype-kolom, **geen** NTFY-tag in de tabel, **geen** HTML-escape — alleen titel/bericht/tijd/queue-flag

#### Wat we **niet** van de webpagina verwachten
- forensische trigger-snapshot, freshness, suppressie-reden — die horen in **meetpatches / serial**, niet in de huidige UI

### 9.3 Wat nog ontbreekt in de huidige situatie
Waarschijnlijk onvoldoende zichtbaar of niet centraal vastgelegd (doel voor **meetpatches**, niet voor WebUI):
- trigger-snapshot
- freshness age
- bron van de live prijs
- suppressie-reden
- duur van sendpad
- relatie tussen detectie, queue-acceptatie en HTTPS-send

---

## 10. Werkvolgorde

### Fase A — observability (eerste prioriteit)
1. kleine auditlaag of gerichte seriële logs waar nodig
2. geen alertgedrag wijzigen
3. draaien op testbuild(s)
4. vastleggen wat “OK” in de queue-log betekent t.o.v. echte NTFY-delivery

### Fase B — forensische beoordeling (NTFY + logs + extern)
1. een testperiode laten lopen
2. meldingen verzamelen (NTFY + serial)
3. logs en externe grafieken naast elkaar leggen; `/notifications` alleen als **snelle device-check**
4. configuratie vastleggen (handmatig export/notities indien nodig)
5. per melding correctness/timing beoordelen

### Fase C — (uitgesteld) WebUI-uitbreidingen
De eerder beschreven uitbreidingen van §11 stap 1–2 en §12 zijn **niet gepland** zolang de focus op firmware/notificaties ligt.

### Fase D — probleemselectie
1. grootste foutbron bepalen
2. prioriteren:
   - stale data?
   - trigger vs meldprijs?
   - suppressie?
   - latency?
   - UX/tekst?

### Fase E — kleine gerichte gedragspatch
Pas na bevestigde observaties:
- één kleine verandering tegelijk
- lage regressiekans
- effect opnieuw meten

*(Oude fase-indeling “eerst WebUI” is vervangen door §0.)*

---

## 11. Eerste concrete stappen (huidige prioriteit)

### Stap 1 — Auditlogging / meetbaarheid (firmware)
**Dit is nu de eerste stap.**

Doel:
- reconstructie van alertbeslissing en send-timing
- reconstructie van suppressie-redenen waar nodig
- geen wijziging van alertdrempels of NTFY-gedrag zonder expliciete gedragspatch

### Stap 2 — Validatie tegen NTFY + externe grafieken
Doel:
- meldingen in de app/desktop naast seriële tijdlijn en eigen prijs-/returngrafieken leggen
- `/notifications` hoogstens als sanity-check (titel, bericht, queue OK/FAIL, `millis`)

### Stap 3 — Gerichte gedragspatches (pas na observaties)
Zie §10 Fase D–E.

---

### Archief: eerdere WebUI-stappen (niet actief)
De onderstaande twee blokken beschreven ooit gewenste browseruitbreidingen; ze zijn **niet langer onderdeel van het werkplan** (zie §0).

**Archief — “Complete read-only configuratiepagina”**  
Oorspronkelijk doel: alle instellingen in vaste volgorde tonen; blijft nuttig als **idee**, maar geen implementatieplannen in dit traject.

**Archief — “Notificatiepagina als forensische analysepagina”**  
Vervangen door: analyse via **logs + NTFY + extern**; de device-webtabel blijft minimaal.

---

## 12. Referentie: oorspronkelijk gewenste WebUI-structuur (niet actief uitwerken)

*Deze sectie is bewaard als ontwerpreferentie; er wordt niet aan verder gebouwd in kader van §0.*

### 12.1 Dashboard
Doel:
- snelle live status en actuele runtime-context

Gewenste inhoud:
- actuele prijs
- live bron
- freshness
- basisreturns
- trend / regime
- WS- en REST-status
- laatste alert

### 12.2 Notificaties
Doel:
- forensische analyse van verstuurde of onderdrukte alerts

Gewenste inhoud:
- notificatielijst
- filters op alerttype / resultaat / bron
- per melding detailinformatie
- triggercontext
- freshness
- returns
- sendduur
- latere koppeling met grafieken

### 12.3 Instellingen
Doel:
- bewerkbare configuratiepagina

Eis:
- dient de canonieke volgorde van instellingen te bepalen

### 12.4 Config snapshot
Doel:
- complete read-only weergave van actieve configuratie en runtime-context

Eis:
- exact dezelfde volgorde als de instellingenpagina
- volledig
- geschikt voor analyse naast notificaties en grafieken

---

## 13. Referentie: gewenste vaste volgorde van een complete instellingenweergave

*Alleen relevant als ooit opnieuw een read-only config-UX wordt overwogen; geen huidige implementatie-eis.*

Indien een read-only configuratiepagina ooit wordt uitgewerkt, zou die in exact dezelfde volgorde kunnen staan als de edit-page.

### A. Systeem / apparaat / sessie
- firmwareversie
- board/platform
- build datum/tijd
- hostname / device name
- uptime
- heap / PSRAM
- WiFi status
- WS status
- laatste REST succes
- taal
- display rotation

### B. Markt / symbool / algemene monitorinstellingen
- market/symbol
- algemene monitor- of fetchinstellingen
- relevante basisparameters

### C. 1m / 5m / 30m alertinstellingen
- `spike1m`
- `spike5m`
- `move5m`
- `move5mAlert`
- `move30m`
- cooldowns 1m / 5m / 30m
- overige korte-signaalinstellingen

### D. Confluence / trend
- `smartConfluenceEnabled`
- `trendThreshold`
- overige trend-/regime-instellingen

### E. Auto-volatility
- `autoVolatilityEnabled`
- `autoVolatilityWindowMinutes`
- `autoVolatilityMinMultiplier`
- `autoVolatilityMaxMultiplier`
- `volatilityLowThreshold`
- `volatilityHighThreshold`

### F. 2h alertinstellingen
- `breakMarginPct`
- `breakResetMarginPct`
- `breakCooldownMs`
- `compressThresholdPct`
- `compressResetPct`
- `compressCooldownMs`
- `meanMinDistancePct`
- `meanTouchBandPct`
- `meanCooldownMs`
- secondary/global cooldown
- coalesce window
- overige 2h throttling-settings

### G. Anchor / auto-anchor / anchor strategy
- handmatige anchor
- auto anchor enabled / source mode
- `anchorTakeProfit`
- `anchorMaxLoss`
- `anchorStrategy`
- `trendAdaptiveAnchorsEnabled`
- auto-anchor subinstellingen
- actieve anchor als runtimewaarde

### H. Night mode
- `nightModeEnabled`
- `nightModeStartHour`
- `nightModeEndHour`
- nacht-thresholds
- nacht-cooldowns
- nacht auto-volatility overrides

### I. Warm start / bootstrap
- `warmStartEnabled`
- candle counts 5m / 30m / 2h
- overige bootstrapinstellingen

### J. NTFY
- topic / enable-status
- retry/backoff-gerelateerde instellingen
- sendgerelateerde runtime-informatie indien relevant

### K. MQTT
- host
- port
- enable/disable
- discovery-gerelateerde instellingen

### L. Netwerk / OTA / externe services
- DuckDNS
- OTA
- externe service-instellingen

### M. Display / UI
- display rotation
- taal / presentatie-instellingen
- schermspecifieke opties

### N. Advanced / debug / platform-specifiek
- debugflags
- board-tuning
- experimentele of geavanceerde opties
- gedragbepalende platforminstellingen

---

## 14. Verwachte eerste beoordelingsvragen (na audit-/meetstappen)

1. Welke configuratie was actief toen de melding plaatsvond? (export, notities, of bestaande `/config` — geen verplichte WebUI-uitbreiding)
2. Komt de inhoud van de NTFY-melding overeen met de triggercontext in de code/logs?
3. Hoe oud was de live prijs op detectiemoment?
4. Welke suppressie-redenen komen in praktijk het vaakst voor?
5. Welke alerttypen hebben de meeste semantische twijfel?
6. Waar zit de meeste tijd: data-age, detectie, queue, HTTPS-send?
7. Wat betekent “OK” in de interne log voor deze melding (queue geaccepteerd vs. bevestigde delivery)?
8. Zijn 2h secondary alerts functioneel nuttig of vooral “achteraf-context”?

---

## 15. Openstaande bevindingen / werklog

> Deze sectie wordt per stap bijgewerkt.

### Bevestigd
- NTFY deliverypad is functioneel werkend
- WS/NTFY-cleanup zit op `main`
- huidige fase verschuift van delivery naar validatie van inhoud/timing
- **`/notifications` is bewust minimaal** (tabel, max. 64 rijen uit de ringbuffer, geen forensische velden)
- **WebUI wordt niet verder uitgebouwd** voor deze validatielijn (zie §0)

### Vermoed
- grootste risico zit in trigger-herleidbaarheid
- freshness is nog onvoldoende zichtbaar in logs
- suppressiegedrag is nog moeilijk forensisch te reconstrueren zonder extra meetregels

### Nog te testen
- daadwerkelijke mismatch tussen triggerprijs en meldprijs
- stale-data incidenten
- sendlatency per alerttype
- impact van 2h secondary throttling op praktische bruikbaarheid
- scheiding queue-acceptatie vs. NTFY-delivery in de praktijk

---

## 16. Patchlog

> Gebruik onderstaand format voor elke stap.

### [2026-03-28] — Document afgestemd op code + focus verschuiving
- **Branch:** `main`  
- **Commit (code-ref.):** `d8831bf`  
- **Type:** documentatie (geen firmwarewijziging via dit item)  
- **Doel:** scope en werkvolgorde laten aansluiten op minimale `/notifications`-pagina en besluit om WebUI niet verder uit te breiden; productiepad `sendNotification` → `enqueueNtfyPending` → `appendNotificationLog` expliciet maken.  
- **Gewijzigde bestanden:** `docs/ntfy_correctness_latency_validation.md`  
- **Regressierisico:** n.v.t.  
- **Samenvatting wijziging:** §0 toegevoegd; WebUI-secties gemarkeerd als referentie; fasering herschikt naar observability eerst.  
- **Waarnemingen:** —  
- **Conclusie:** —  
- **Volgende stap:** meetpatches / serial-audit naar behoefte  

### [YYYY-MM-DD] — Titel van stap
- **Branch:**  
- **Commit:**  
- **Type:** meetpatch / gedragspatch / (zeldzaam) UI-presentatie  
- **Doel:**  
- **Gewijzigde bestanden:**  
- **Regressierisico:** laag / middel / hoog  
- **Samenvatting wijziging:**  
- **Waarnemingen:**  
- **Conclusie:**  
- **Volgende stap:**  

---

## 17. Korte samenvatting van de huidige strategie

De strategie van deze fase is bewust simpel:

1. **niet eerst mooier maken (geen WebUI-focus)**
2. **niet eerst slimmer maken zonder metingen**
3. **eerst observability in de firmware (serial/audit) waar nodig**
4. **validatie via NTFY-ontvangst en externe grafieken**
5. **daarna pas gerichte gedragspatches**

De kernvraag is niet langer:

> “Wordt de melding verstuurd?”

De kernvragen zijn nu:

> “Is de melding inhoudelijk juist, op tijd, en echt bruikbaar?”  
> “Kunnen we dat reconstrueren uit logs en NTFY, zonder afhankelijk te zijn van uitbreidingen in de browser?”