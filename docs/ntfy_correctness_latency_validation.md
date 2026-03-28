# NTFY Alert Correctness / Latency / Usefulness Validation

**Repo:** `ESP32-Crypto-Alert`  
**Basisbranch:** `main`  
**Basiscommit:** `ed4d939`  
**Startdatum validatiefase:** 28 maart 2026  
**Status:** werkdocument / levende basis voor vervolgpatches en analyses

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

Daarnaast speelt de **WebUI** in deze fase een centrale rol, omdat de notificatiepagina’s en instellingenpagina’s worden gebruikt om notificaties te beoordelen naast koers- en return-grafieken.

Dit document bewaakt:
- de onderzoeksvragen
- de werkvolgorde
- de hypotheses
- de meetstrategie
- het onderscheid tussen meetpatches, analyse-UI patches en gedragspatches
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
- WebUI-ondersteuning voor analyse van notificaties
- WebUI-weergave van complete configuratie en runtime-context
- verbeterde samenhang tussen notificatiepagina, instellingenpagina en grafiekanalyse

### Niet het hoofdonderwerp van deze fase
- opnieuw opschonen van WS/NTFY-architectuur
- grote refactors
- nieuwe notificatiesystemen
- grote UI-/webserver-uitbreidingen die losstaan van analyse
- nieuwe settings of configuratiestructuren als eerste stap

### Belangrijk uitgangspunt
De huidige werkende **WS/NTFY-architectuur op `main`** is voorlopig het vaste uitgangspunt.  
Veranderingen in gedrag komen pas **na** meetbare observaties.

### Extra uitgangspunt voor de WebUI
De WebUI mag in deze fase wél worden verbeterd als dat helpt om:
- notificaties beter te beoordelen
- runtime-context zichtbaar te maken
- configuratie vollediger en consistenter te tonen

Maar:
- de WebUI-aanpassingen mogen **niet stilzwijgend alertgedrag veranderen**
- presentatie en analyse-ondersteuning gaan vóór nieuwe interactieve features

---

## 3. Huidige uitgangssituatie

### Bevestigde uitgangspunten
- NTFY delivery werkt
- exclusive flow werkt
- WS wordt rond NTFY gecontroleerd gestopt/herstart
- oude dubbele/experimentele NTFY-paden zijn verwijderd
- huidige codebasis staat op `main @ ed4d939`
- documentatie rond de eerdere cleanup is bijgewerkt

### Werkhypothese voor deze fase
De grootste resterende risico’s zitten nu niet meer in **technische aflevering**, maar in:
- onduidelijke herleidbaarheid van de trigger
- mogelijke mismatch tussen triggerdata en meldtekst
- onvoldoende zicht op freshness
- onvoldoende zicht op latency tussen detectie en verzending
- mogelijke suppressie van nuttige alerts of juist ruis-alerts
- onvoldoende context in de WebUI om meldingen snel en betrouwbaar te beoordelen

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
Bij een geslaagde alertbeslissing loopt de keten via:
- `sendNotification(...)`
- `sendNtfyNotification(...)`
- zichtbare logging via notification-log

### 4.6 Beoordeling via WebUI
De WebUI wordt gebruikt om:
- notificaties terug te lezen
- actieve instellingen te controleren
- notificaties te vergelijken met koers- en return-grafieken
- de analyse te ondersteunen van correctness, timing en bruikbaarheid

### 4.7 Mogelijke semantische zwakke plekken
De belangrijkste plekken waar fouten of verwarring kunnen ontstaan:
- trigger wordt bepaald op basis van buffer-/returnlogica
- meldtekst gebruikt mogelijk een andere representatie van de prijs
- freshness van de gebruikte live prijs is niet expliciet zichtbaar
- suppressie vindt plaats zonder volledige audittrail
- 2h secondary alerts kennen throttling/coalescing en dus mogelijk extra vertraging
- de huidige WebUI toont mogelijk niet alle relevante context om meldingen goed te beoordelen

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

### 5.4 WebUI-ondersteuning
- Toont de WebUI voldoende context om notificaties betrouwbaar te beoordelen?
- Staan alle instellingen zichtbaar in de WebUI?
- Staat de read-only configuratie in dezelfde volgorde als de edit-page?
- Zijn runtime-context en notificatiecontext voldoende beschikbaar naast de grafieken?

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

### H7 — de huidige WebUI biedt nog onvoldoende analysecapaciteit
De notificatiepagina en instellingenpagina geven waarschijnlijk nog niet genoeg complete, consistente en goed geordende context om meldingen efficiënt naast grafieken te beoordelen.

---

## 7. Beslisregels voor deze validatiefase

Deze regels gelden als vaste werkafspraken.

1. **Eerst meten, dan gedrag wijzigen**
2. **Geen tekstpolish vóór correctness/freshness duidelijk is**
3. **Meetpatches blijven klein en lokaal**
4. **Analyse-UI patches blijven analysegericht en veranderen geen alertgedrag**
5. **Gedragspatches worden apart benoemd en gelogd**
6. **Geen grote refactor zolang observability nog onvoldoende is**
7. **De huidige werkende WS/NTFY-flow blijft uitgangspunt**
8. **Bij twijfel eerst auditlog of UI-context toevoegen, niet direct logica aanpassen**

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

### 8.2 Analyse-UI patch
Doel:
- bestaande informatie beter zichtbaar maken
- notificaties beter kunnen beoordelen naast koers- en return-grafieken
- configuratie en runtime-context overzichtelijk beschikbaar maken

Voorbeelden:
- notificatiepagina uitbreiden met triggercontext
- complete read-only configuratiepagina toevoegen
- instellingen in dezelfde volgorde tonen als op de edit-pagina
- runtime-context zichtbaar maken
- notificaties beter koppelbaar maken aan grafiekmomenten

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
Een patch mag **niet ongemerkt van meetpatch of analyse-UI patch naar gedragspatch schuiven**.  
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
- welke configuratie en runtime-context actief waren tijdens beoordeling
- hoe de melding zich verhoudt tot de koers- en return-grafieken

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

#### Zichtbare log / WebUI-context
- moment waarop de notification-log entry wordt bijgewerkt
- alerttype zichtbaar op notificatiepagina
- bron/freshness zichtbaar op notificatiepagina
- relevante returns zichtbaar op notificatiepagina
- runtime-context zichtbaar op WebUI
- volledige configuratie zichtbaar in dezelfde volgorde als de edit-pagina

### 9.3 Wat nog ontbreekt in de huidige situatie
Waarschijnlijk onvoldoende zichtbaar of niet centraal vastgelegd:
- trigger-snapshot
- freshness age
- bron van de live prijs
- suppressie-reden
- duur van sendpad
- relatie tussen detectie en zichtbare log-entry
- complete read-only weergave van alle instellingen
- eenzelfde volgorde tussen read-only configweergave en edit-page
- voldoende context op de notificatiepagina om meldingen naast grafieken te beoordelen

---

## 10. Werkvolgorde

### Fase A — analysebasis in de WebUI op orde brengen
1. complete read-only configuratiepagina toevoegen of uitbreiden
2. deze read-only configuratiepagina in exact dezelfde volgorde zetten als de hoofdinstellingenpagina
3. alle instellingen tonen, inclusief advanced / zelden gebruikte maar gedragbepalende instellingen
4. runtime-context blok toevoegen bovenaan de configuratiepagina
5. notificatiepagina uitbreiden met extra analysevelden
6. notificatiepagina geschikt maken als beoordelingspagina naast koers- en return-grafieken

### Fase B — observability
1. kleine auditlaag toevoegen
2. alleen seriële logging
3. geen gedrag wijzigen
4. draaien op testbuild(s)

### Fase C — forensische beoordeling
1. een testperiode laten lopen
2. meldingen verzamelen
3. seriële logs, notificatiepagina en grafieken naast elkaar leggen
4. configuratiesnapshot en runtime-context meenemen in de beoordeling
5. per melding correctness/timing beoordelen

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

---

## 11. Eerste concrete stappen

### Stap 1
**Complete read-only configuratiepagina op orde brengen**

Doel:
- alle relevante instellingen zichtbaar maken
- analyse eenvoudiger maken
- instellingen sneller vergelijken met het edit-scherm
- voorkomen dat tijdens analyse onduidelijk is welke configuratie actief was

#### Eisen
- exact dezelfde volgorde als de hoofdinstellingenpagina
- dezelfde secties
- dezelfde veldnamen
- **alle instellingen zichtbaar**
- duidelijk onderscheid tussen:
  - configureerbare instellingen
  - runtime-context / read-only status

#### Minimaal zichtbaar maken
- firmwareversie / platform / uptime
- live bronstatus
- WS status
- laatste REST succes
- actuele trend / anchor / freshness-context indien beschikbaar
- alle alertthresholds
- alle cooldowns
- confluence-instellingen
- auto-volatility-instellingen
- 2h-instellingen
- anchor/auto-anchor-instellingen
- night mode-instellingen
- warm-start-instellingen
- NTFY-instellingen
- MQTT/netwerk/UI-gerelateerde instellingen
- advanced/debug/board-specifieke instellingen die gedrag beïnvloeden

### Stap 2
**Notificatiepagina uitbreiden tot analysepagina**

Doel:
- meldingen direct kunnen beoordelen naast koers- en return-grafieken

#### Minimaal zichtbaar maken per notificatie
- alerttype
- resultaat
- titel
- bericht
- detectie-/sendtijd indien beschikbaar
- live bron
- freshness age
- triggerprijs
- zichtbare meldprijs
- relevante returns
- min/max of band-context indien beschikbaar
- suppressie-reden indien van toepassing

#### Ontwerpdoel
De notificatiepagina moet niet alleen een verzendlog zijn, maar een **forensische beoordelingspagina**.

### Stap 3
**Auditlogging toevoegen zonder gedragswijziging**

Doel:
- reconstructie van alertbeslissing
- reconstructie van send-timing
- reconstructie van suppressie-redenen

### Stap 4
**Notificaties en grafiekcontext beter aan elkaar koppelen**

Doel:
- sneller beoordelen of een melding juist, tijdig en bruikbaar was

#### Latere uitbreidingen
- notificatie selecteren en grafiek rond dat moment tonen
- markers in prijs- en returngrafieken
- filtering op alerttype / bron / freshness / resultaat

---

## 12. Gewenste WebUI-structuur voor deze validatiefase

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

## 13. Gewenste vaste volgorde van de complete instellingenweergave

De read-only configuratiepagina moet in exact dezelfde volgorde staan als de edit-page.

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

## 14. Verwachte eerste beoordelingsvragen na stap 1 t/m 3

Na de eerste WebUI- en auditstappen moeten we deze vragen beter kunnen beantwoorden:

1. Welke configuratie was actief toen de melding beoordeeld werd?
2. Staat de read-only configuratie in dezelfde logische volgorde als de edit-page?
3. Ontbreken er nog instellingen op de WebUI die nodig zijn voor analyse?
4. Komt de getoonde meldprijs overeen met de triggercontext?
5. Hoe oud was de live prijs op detectiemoment?
6. Welke suppressie-redenen komen in praktijk het vaakst voor?
7. Welke alerttypen hebben de meeste semantische twijfel?
8. Waar zit de meeste tijd:
   - data-age
   - detectie
   - send
   - zichtbare log?
9. Zijn 2h secondary alerts functioneel nuttig of vooral “achteraf-context”?

---

## 15. Openstaande bevindingen / werklog

> Deze sectie wordt per stap bijgewerkt.

### Bevestigd
- NTFY deliverypad is functioneel werkend
- WS/NTFY-cleanup zit op `main`
- huidige fase verschuift van delivery naar validatie van inhoud/timing
- de WebUI is een belangrijk hulpmiddel voor het beoordelen van notificaties naast koers- en return-grafieken

### Vermoed
- grootste risico zit in trigger-herleidbaarheid
- freshness is nog onvoldoende zichtbaar
- suppressiegedrag is nog moeilijk forensisch te reconstrueren
- de huidige WebUI toont waarschijnlijk nog niet alle instellingen en niet in de meest bruikbare beoordelingsvolgorde

### Nog te testen
- daadwerkelijke mismatch tussen triggerprijs en meldprijs
- stale-data incidenten
- sendlatency per alerttype
- impact van 2h secondary throttling op praktische bruikbaarheid
- welke extra WebUI-context het meest helpt bij snelle en betrouwbare beoordeling van meldingen

---

## 16. Patchlog

> Gebruik onderstaand format voor elke stap.

### [YYYY-MM-DD] — Titel van stap
- **Branch:**  
- **Commit:**  
- **Type:** meetpatch / analyse-UI patch / gedragspatch  
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

1. **niet eerst mooier maken**
2. **niet eerst slimmer maken**
3. **eerst de analysebasis op orde brengen in WebUI en logging**
4. **daarna zichtbaar maken wat het systeem nu echt doet**
5. **dan pas gericht verbeteren**

De kernvraag is niet langer:

> “Wordt de melding verstuurd?”

De kernvragen zijn nu:

> “Is de melding inhoudelijk juist, op tijd, en echt bruikbaar?”  
> “Hebben we in de WebUI genoeg context om dat betrouwbaar te beoordelen?”