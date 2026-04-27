# Crypto-Alert V3

Crypto-Alert V3 is een modulaire ESP-IDF firmware voor realtime crypto-monitoring op de ESP32-S3-GEEK. De kern van V3 is niet een verzameling losse prijsalarmen, maar een event-driven keten die marktcontext opbouwt en alleen richting contextrijke, verklaarbare notificaties werkt.

## Kernidee in één alinea

V3 leest realtime marktdata via Bitvavo WebSocket, verwerkt die in kleine, gescheiden componenten en bouwt stap voor stap context op van ruwe tickdata naar setup- en triggerniveaus. Het doel is om uiteindelijk alleen meldingen te sturen die inhoudelijk relevant zijn (context, kwaliteit, bevestiging), niet elke koersbeweging. De ontwikkelvolgorde is bewust: eerst betrouwbaarheid en observability, daarna uitbreiding van alerting en UI.

## Waarom V3?

V3 is een bewuste herstart na twee eerdere generaties:

- **V1** groeide functioneel, maar architectuur en verantwoordelijkheden liepen door elkaar, waardoor onderhoud en stabiele doorontwikkeling steeds moeilijker werden.
- **V2** leverde waardevolle bouwstenen en inzichten op, maar had nog lifecycle- en stabiliteitsproblemen in de runtime-keten.

Daarom is in V3 gekozen voor:

- een schone herstart met duidelijke componentgrenzen;
- ESP-IDF als technische basis;
- een modulaire componentarchitectuur met expliciete verantwoordelijkheden;
- een WS-first runtime waarin één live datastroom centraal staat.

## Branchbeleid

- `main` is het hoofdspoor van de repository.
- `v3/allnew` is de actuele V3-ontwikkel- en integratiebranch.
- Kortlopende V3-modules/experimenten krijgen een aparte branch, bijvoorbeeld `v3/alert-engine`, `v3/ntfy-transport` of `v3/ui-model`.
- Nieuwe functionaliteit wordt eerst in zo'n aparte `v3/...` branch ontwikkeld en daarna terug gemerged naar `v3/allnew`.
- Alleen bewezen stabiele of rijpe delen gaan later door naar `main`.

## Versieafspraak (V3)

V3 gebruikt voorlopig een compacte versievorm: `[v3].[spoor].[minor]`.

- eerste cijfer = hoofdlijn (`3` voor V3);
- middelste blok = ontwikkelspoor/modulelijn (bijv. `01` voor `v3/alert-engine`);
- laatste blok = kleine revisie binnen dat spoor.

Voorbeeld: `3.01.01` betekent V3, alert-engine spoor, revisie 1.

## Notificatie-filosofie

De notificatiefilosofie van V3 is expliciet anders dan eenvoudige prijsalarmering:

- geen melding op elke kleine beweging;
- geen ping-spam zonder context;
- geen "prijs is in de buurt" als eindcriterium.

V3 werkt richting een keten waarin kwaliteit toeneemt per laag:

```text
ruwe prijsbeweging
→ contextlaag
→ setup candidate
→ trigger
→ alert / notificatie
```

Dit sluit aan op mean-reversion denken:

- reversals zijn bruikbaarder in range/chop dan in expansion/trend;
- nabijheid van een relevant level is nodig, maar niet voldoende;
- snelheid van approach naar dat level geeft extra context;
- bevestiging van reversal weegt zwaarder dan alleen proximity.

Een uiteindelijke notificatie moet daarom een compacte samenvatting van context zijn (regime, level, kwaliteit, bevestiging), geen losse koersping.

## Waarom Bitvavo WebSocket?

V3 is WS-first omdat dat technisch beter past bij continue marktwaarneming:

- realtime datastroom met lage latency;
- geen polling-loop als primaire runtimebron;
- één live kanaal als basis voor event-driven verwerking;
- natuurlijker koppeling met componenten die op events reageren.

REST wordt in V3 niet gebruikt als primaire runtimebron. Hoogstens kan REST later dienen voor ondersteunende taken zoals debug of backfill, maar niet als kern van de dataketen.

## Waarom ESP32-S3-GEEK?

De ESP32-S3-GEEK is gekozen als primaire V3-basis omdat het een stabiel en praktisch platform is om de V3-herbouw gefaseerd uit te voeren:

- geschikt om de data-core eerst robuust te maken;
- voldoende basis voor continue WS-ingest en analyseketen;
- overzichtelijk hardwaredoel voor iteratieve stabilisatie.

De focus ligt nu op betrouwbaarheid van de keten, niet op brede hardwarevariatie in deze fase.

## Architectuuroverzicht

V3 is opgebouwd uit losse componenten met duidelijke verantwoordelijkheid:

- `settings_store`  
  Beheert configuratie (waaronder WiFi-instellingen) via menuconfig/opslag en fallback-resolutie.

- `wifi_manager`  
  Verzorgt WiFi lifecycle en connectiestates. Netwerkverbinding is een voorwaarde voor WS-transport.

- `market_ws`  
  WebSocket transportlaag naar Bitvavo. Verantwoordelijk voor connect/reconnect, frame-ontvangst en transportstatistiek.

- `market_store`  
  Parseert relevante WS payloads, normaliseert marktdata en publiceert een actuele market snapshot + ingeststats.

- `candle_engine`  
  Bouwt 1s/1m aggregatie op gesloten candles en levert rolling returns/ranges (1m/5m/30m) als rustige analysetoestand.

- `regime_engine`  
  Bepaalt een lichte regime-classificatie op basis van rolling analytics (bijv. range/neutral/trend-context).

- `level_engine`  
  Houdt light level-context bij (nabije support/resistance) op basis van gesloten candle-historie.

- `setup_engine`  
  Combineert regime + levels + approach-kwaliteit tot setup candidates met score/kwaliteitscontext.

- `trigger_engine`  
  Zet setup-context om naar triggerstates (candidate/triggered/invalidated) met bevestigingslogica.

- `alert_engine`  
  Beheert de interne alert lifecycle (nu light/log-only) als schakel tussen detectie en latere notificatie-output.

- `ntfy_client` (later in keten)  
  Transport voor externe notificatie, los van detectielogica.

- `ui_model` en `display_ui` (later verfijnen)  
  Consumenten van bestaande state; UI is geen eigenaar van kernanalyse of transport.

### Ontwerpscheiding

- transport is niet hetzelfde als analyse;
- analyse is niet hetzelfde als notificatie;
- UI consumeert state, maar bestuurt de analysekern niet.

## Datastroom / pipeline

```text
Bitvavo WS
→ market_ws
→ market_store
→ candle_engine
→ analytics (returns/ranges)
→ regime_engine
→ level_engine
→ setup_engine
→ trigger_engine
→ alert_engine (later)
→ ntfy / UI (later)
```

Deze volgorde is belangrijk omdat elke laag input verwacht die in de vorige laag al is genormaliseerd en rustiger is gemaakt. Door dit sequentieel en modulair te houden, blijven fouten beter te lokaliseren en blijft runtimegedrag beter verklaarbaar.

## Huidige status

### Werkend / aanwezig

- [x] ESP-IDF V3 basisproject
- [x] WiFi-config via settings/menuconfig + fallback
- [x] Stabiele Bitvavo WebSocket transportbasis
- [x] `market_store` met parser + snapshot + ingeststats
- [x] 1s/1m aggregatie op gesloten candles
- [x] Rolling returns en ranges (1m/5m/30m)
- [x] `regime_engine light`
- [x] `level_engine light`
- [x] `setup_engine light`
- [x] `trigger_engine light`
- [x] Soak/health observabilitylaag (periodieke status + stale-detectie + runtime counters)

### Nog niet af / later

- [x] Eerste `alert_engine light` lifecycle (log-only, single active snapshot)
- [ ] Volledige `alert_engine` lifecycle in de V3-keten
- [ ] Productierijpe NTFY-uitsturing vanuit V3 alertpad
- [ ] Fijnere triggerlogica (verdere validatie en tuning)
- [ ] Verdere invalidation/FTA/MAE-achtige logica
- [ ] Uitgebreidere regime- en volume/contextanalyse
- [ ] Verdere UI/web/MQTT-lagen
- [ ] Verbreding naar extra boards / hardwaredoelen

## Roadmap

1. Triggerlaag verder valideren met langere runs en kwaliteitsmeting.
2. Alert lifecycle implementeren als aparte, goed testbare component.
3. NTFY-koppeling in de V3-keten aansluiten op alert-uitkomsten.
4. Setupkwaliteit verbeteren met extra contextlagen (zonder monolithische logica).
5. UI-laag verder uitwerken als consument van bestaande state.
6. Web/API-laag toevoegen waar nodig, zonder kernarchitectuur te vervuilen.
7. Eventueel verbreden naar multi-coin / bredere marktverkenning.
8. Daarna pas hardwareverbreding naar extra targets.

## Ontwikkelprincipes

- Kleine, gecontroleerde patches.
- Eerst stabiliteit, dan nieuwe features.
- Buildbaar en testbaar na elke stap.
- WS-first voor live runtime.
- Gesloten candles als basis voor rustiger analysegedrag.
- Geen directe UI-koppeling aan ruwe live datastromen.
- Hergebruik logica uit V1/V2 waar nuttig, maar kopieer geen oude architectuur.

## Build / gebruik

```bash
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py flash monitor
```

## Disclaimer / ontwikkelstatus

V3 is actief in ontwikkeling en is op dit moment geen definitief handelssysteem. De huidige focus ligt op betrouwbaarheid, observability en verklaarbare detectie in de volledige keten.
