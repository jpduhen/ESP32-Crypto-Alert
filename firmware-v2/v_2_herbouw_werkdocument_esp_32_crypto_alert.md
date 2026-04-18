# Werkdocument — ESP32 Crypto Alert V2 herbouw

\

## Doel van dit document

\

Dit document is de centrale referentie voor de herbouw van **ESP32 Crypto Alert V2**.

\

Het document is bedoeld om op elk moment snel antwoord te geven op drie vragen:

\

1. **Waar staan we nu?**

2. **Wat hebben we al besloten en waarom?**

3. **Wat heeft nu prioriteit?**

\

* **Uitgebreide fase-antwoord** (waar we staan, volwassenheid, resterende werkpakketten): zie § **9a**.

\

Het document is geschreven zodat **Jan Pieter**, **ChatGPT** en **Cursor/Codex** er direct mee kunnen werken.

\

---

\

## 0. Leidende repo-referentie

\

**Dit bestand** (`firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md`) is de **primaire** bron in deze repository voor **actuele V2-status**, **besluiten**, **prioriteiten**, **migratierichting** en **eerste componentgrenzen**.

\

* Aanvullend, zonder tegenstrijdigheid bedoeld: technische **skeleton-detailkeuzes** → `docs/architecture/V2_SKELETON_NOTES.md`; formeel **governance-overzicht** → `docs/architecture/V2_WORKDOCUMENT_MASTER.md`.

* **Bij inhoudelijke spanning** tussen documenten: eerst dit werkdocument actualiseren of expliciet een besluit vastleggen in §8 (besluitenlog).

\

---

\

## 1. Projectdoel

\

De huidige codebasis is functioneel sterk, maar architectonisch te ver doorgegroeid. De V2-herbouw moet leiden tot:

\

* een **modulaire** codebasis

* betere **foutopsporing** en **onderhoudbaarheid**

* eenvoudiger **toevoegen/verwijderen van functionaliteiten**

* duidelijke scheiding tussen **hardware**, **UI**, **domeinlogica** en **services**

* een **stabiele en efficiënte** firmware

* een basis die toewerkt naar **productierijpheid**

* betere ondersteuning voor **gestructureerd ontwikkelen via branches**

* een projectstructuur die beter past bij de huidige complexiteit dan een klassieke Arduino-sketch

\

---

\

## 2. Strategische richting

\

### Hoofdkeuze

\

We kiezen voor een **gecontroleerde herbouw naar V2**, in plaats van verder doorbouwen op de bestaande code.

\

### Technische richting

\

Voorkeursrichting:

\

* **nieuwe basis in ESP-IDF**

* gefaseerde migratie van bestaande functionaliteiten

* geen big-bang overzet

* V1 blijft voorlopig beschikbaar als **bevroren referentie en fallback**

\

### Belangrijk uitgangspunt

\

De huidige V1 blijft belangrijk als:

\

* functionele referentie

* visuele referentie

* bron voor migratie van bewezen onderdelen

* fallback bij problemen in V2

\

### 2.1 B-001 — V1 formeel bevroren als referentie (2026-04-11)

\

**Besluit:** V1 (Arduino, repo-root) is de **referentie-/onderhoudslijn** op branch **`main`**. Aanbevolen git-tag: **`v1-reference-frozen`** op de gekozen stabiele `main`-commit (zie instructies in **[docs/V1_REFERENCE_FREEZE_B001.md](../docs/V1_REFERENCE_FREEZE_B001.md)**).

\

| | V1 | V2 |

|---|----|-----|

| Locatie | `ESP32-Crypto-Alert.ino`, `src/`, `platform_config.h` | `firmware-v2/`, ESP-IDF |

| Branch | **`main`** | **`v2/foundation`** (actieve herbouw) |

| Richting | Geen grote nieuwe lagen zonder besluit; wél onderhoud/security/docs | Featuregroei en architectuur volgens dit werkdocument |

\

**M-002 hardening:** netwerk-eigenaarschappen en open risico’s staan in **[docs/architecture/M002_NETWORK_BOUNDARIES.md](../docs/architecture/M002_NETWORK_BOUNDARIES.md)**; ADR-002 is bijgewerkt met verwijzing.

\

---

\

## 3. Wat we expliciet willen behouden

\

De volgende onderdelen uit de huidige code worden gezien als waardevol en moeten in principe behouden of zorgvuldig herbouwd worden:

\

### Kernfunctionaliteit

\

* WebSocket ophalen van koerswaarden

* snelle detectie van koersbewegingen

* keuzevrijheid in symbols zoals BTC-EUR en ETH-EUR

\

### UI / UX

\

* visuele representatie op de schermen

* kleurgebruik

* indeling

* informatiehiërarchie

* prettige schermbeleving

\

### WebUI

\

* huidige prettige vormgeving

* OTA-weergave met blokken/progressie

* instellingenbeheer, maar met toekomstige opschoning op relevantie

\

### Services / integraties

\

* notificatie via NTFY

* MQTT voor analyse, grafieken en Home Assistant

* OTA updatefunctionaliteit

* GitHub repo met duidelijke documentatie

* selectieve logging

\

### Ontwikkelproces

\

* beter gebruik van branches

* duidelijker test- en releaseproces

\

---

\

## 4. Wat minder belangrijk of overbodig is geworden

\

Deze zaken hebben in V2 geen prioriteit of mogen vervallen:

\

* CYD ondersteuning

* TTGO ondersteuning

* historische compatibiliteitslagen die alleen nog ballast zijn

* instellingen in de WebUI die weinig toevoegen of alleen fine-tuning zijn

* Arduino IDE als einddoelomgeving

\

Focus-doelen voor ondersteunde hardware:

\

* **S3-GEEK**

* **LCDWIKI**

* **JC3248W535**

\

---

\

## 5. Eerste architectuurvisie V2

\

Beoogde hoofdlagen / componenten:

\

1. **BSP / board support**

\

* boardprofielen

* pinmapping

* display init

* touch

* backlight

* PSRAM checks

\

2. **Display/UI**

\

* LVGL integratie

* thema

* schermcomponenten

* layout per displayklasse

\

3. **Market data**

\

* websocket

* REST fallback

* symbol management

* buffering / snapshots

\

4. **Domain logic**

\

* alert engine

* thresholds

* cooldowns

* confluence / regels

\

5. **Services**

\

* WiFi

* NTFY

* MQTT

* OTA

* WebUI

* time sync

\

6. **Config**

\

* defaults

* persistente opslag

* schema versioning

* migraties

\

7. **Diagnostics**

\

* logging

* health status

* heap / task info

* debug toggles

\

8. **App orchestration**

\

* opstartvolgorde

* dependency wiring

* systeemstatus

\

---

\

## 6. Eerste migratiestrategie

\

### Algemene aanpak

\

* Eerst een **schoon V2 skelet** bouwen

* Daarna functionaliteiten **één voor één** migreren

* Elke stap moet toetsbaar zijn

* Geen grote ongerichte refactors tegelijk

\

### Voorlopige migratievolgorde

\

1. V1 bevriezen en documenteren

2. V2 projectstructuur opzetten

3. basis build + logging + boardselectie

4. UI prototype met mock data

5. config store

6. WebSocket market data

7. live databinding UI

8. NTFY

9. MQTT

10. WebUI

11. OTA

12. verdere verfijning / Home Assistant instellingen / uitbreidingen

\

---

\

## 7. Parallelle sporen

\

Om voortgang overzichtelijk te houden werken we met meerdere parallelle sporen.

\

### Spoor A — Architectuur

\

Doel:

\

* componentgrenzen vastleggen

* interfaces bepalen

* verantwoordelijkheden scheiden

\

### Spoor B — V1 inventarisatie

\

Doel:

\

* bestaande functies in kaart brengen

* bepalen: behouden / herschrijven / schrappen

* migratiematrix opstellen

\

### Spoor C — V2 skeleton

\

Doel:

\

* nieuwe mapstructuur

* buildbare basis

* basis logging

* boardprofielen

\

### Spoor D — UI/UX continuïteit

\

Doel:

\

* look-and-feel behouden

* schermopzet vroeg valideren

* displayverschillen abstraheren

\

### Spoor E — Services migratie

\

Doel:

\

* websocket

* ntfy

* mqtt

* webui

* ota

\

### Spoor F — Proces en kwaliteit

\

Doel:

\

* branchingstrategie

* testmomenten

* documentatie

* release discipline

* debugmethodiek

\

---

\

## 8. Besluitenlog

\

Gebruik dit hoofdstuk als formeel geheugen van gemaakte keuzes.

\

### Besluittemplate

\

**ID:** D-001

**Datum:** YYYY-MM-DD

**Onderwerp:**

**Besluit:**

**Alternatieven:**

**Argumenten voor besluit:**

**Consequenties:**

**Actiepunten:**

**Status:** concept / besloten / herzien

\

### Reeds bekende besluiten

\

**ID:** D-001

**Datum:** 2026-04-11

**Onderwerp:** Nieuwe ontwikkelrichting

**Besluit:** De huidige codebasis wordt niet verder structureel uitgebouwd als hoofdroute; we werken toe naar een V2-herbouw.

**Alternatieven:** Verder refactoren binnen V1

**Argumenten voor besluit:** Architectuur is organisch gegroeid; onderhoud en foutopsporing worden steeds lastiger; hardware- en displayvarianten trekken te veel aan één codepad.

**Consequenties:** V1 blijft als referentie beschikbaar; V2 krijgt een schone basis.

**Actiepunten:** V1 bevriezen, migratiematrix maken, V2 structuur opzetten.

**Status:** besloten

\

**ID:** D-002

**Datum:** 2026-04-11

**Onderwerp:** Doelplatformen

**Besluit:** Focus in V2 op S3-GEEK, LCDWIKI en JC3248W535. CYD en TTGO krijgen geen prioriteit meer.

**Alternatieven:** Alle historische boards blijven ondersteunen

**Argumenten voor besluit:** Minder ballast, minder conditionele code, betere focus op relevante hardware.

**Consequenties:** Architectuur mag geoptimaliseerd worden voor ESP32-S3-klasse hardware.

**Actiepunten:** Boardprofielen definiëren.

**Status:** besloten

\

**ID:** D-003

**Datum:** 2026-04-11

**Onderwerp:** Doelomgeving

**Besluit:** ESP-IDF is de voorkeursrichting voor V2.

**Alternatieven:** Eerst opnieuw opzetten in Arduino IDE en later migreren

**Argumenten voor besluit:** Complexiteit van project past beter bij componentstructuur en professionelere opzet.

**Consequenties:** Stapsgewijze opbouw nodig; mogelijk tijdelijk gebruik van Arduino-compatibele onderdelen waar dat helpt.

**Actiepunten:** V2 skeleton voorbereiden voor ESP-IDF.

**Status:** voorlopig besloten

\

**ID:** D-004

**Datum:** 2026-04-11

**Onderwerp:** Eerste referentieboard

**Besluit:** De V2-herbouw start op de **ESP32-S3 GEEK (S3-GEEK)** als primair ontwikkel- en validatieplatform.

**Alternatieven:** LCDWIKI of JC3248W535 als eerste doelplatform

**Argumenten voor besluit:** Compacte module, geschikt als schone basis voor de nieuwe architectuur, minder verstorende hardwarecomplexiteit in de eerste fase.

**Consequenties:** Board support en eerste bring-up worden eerst voor de S3-GEEK uitgewerkt; andere boards volgen later als aparte boardprofielen.

**Actiepunten:** BSP-profiel voor S3-GEEK opnemen in de V2-structuur en alle eerste bouwstappen daarop richten.

**Status:** besloten

\

**ID:** D-005

**Datum:** 2026-04-11

**Onderwerp:** Eerste ESP-IDF skeleton (`firmware-v2/`)

**Besluit:** Tickets **S-001 t/m S-007** zijn uitgevoerd: buildbare ESP-IDF-basis met componenten `app_core`, `config_store`, `diagnostics`, `bsp_common`, `bsp_s3_geek`, `display_port`, `ui` (mock), `market_data` (contract + mock). Geen feature-pariteit met V1; geen CYD/TTGO in deze fase.

**Alternatieven:** langer wachten met IDF-structuur; alleen documentatie zonder repo-artefacten.

**Argumenten voor besluit:** migratiematrix P0-blokken (M-001 … M-005 + UI/display/market placeholders) hebben een tastbare landingsplek nodig.

**Consequenties:** volgende werk zit in displaystack, exchange/netwerk en CI — niet in uitbreiding van het monolithische V1-pad.

**Actiepunten:** zie § **9a** en § **10** (consolidatiefase; werkpakketten i.p.v. alleen skeleton-uitbreiding).

**Status:** besloten

\

**ID:** D-006

**Datum:** 2026-04-11

**Onderwerp:** Canonical ESP-IDF-versie voor V2 (toolchain + CI)

**Besluit:** V2 gebruikt **ESP-IDF v5.4.2** (upstream Git-tag `v5.4.2`). Machineleesbare pin: `firmware-v2/ESP_IDF_VERSION`. GitHub Actions smoke build gebruikt Docker **`espressif/idf:v5.4.2`**. Lokale ontwikkelaars: clone/checkout `v5.4.2`, `./install.sh esp32s3`, dan `export.sh` vóór `idf.py build`.

**Alternatieven:** ESP-IDF 5.3 LTS; zwevende tag `release-v5.4`.

**Argumenten voor besluit:** 5.4-lijn sluit aan bij ESP32-S3; patch .2 is expliciet reproduceerbaar; officiële Docker image beschikbaar; sluit aan bij gangbare Espressif-documentatie.

**Consequenties:** elke IDF-bump vereist update van `ESP_IDF_VERSION`, `BUILD.md`, CI-workflow image-tag, en dit besluit of een opvolger (D-00x).

**Actiepunten:** —

**Status:** besloten

\

**ID:** D-007

**Datum:** 2026-04-11

**Onderwerp:** Displayroute S3-GEEK voor T-102

**Besluit:** Voor de S3-GEEK wordt in T-102 uitgegaan van **ST7789, 4-wire SPI, ESP-IDF `esp_lcd`** (`esp_lcd_new_panel_io_spi` + `esp_lcd_new_panel_st7789`) met een minimale bring-up via een volledig scherm RGB565-kleurvulling. **LVGL** blijft buiten scope van T-102. De hardwarevalidatie op de echte S3-GEEK is geslaagd: het volledige scherm wordt correct aangesproken en de basisroute is daarmee bevestigd.

**Alternatieven:** Arduino_GFX-achtige tussenroute; direct LVGL-integratie; andere paneldriver

**Argumenten voor besluit:** kleinste ESP-IDF-conforme route naar zichtbare output; sluit aan bij de architectuurdoelen; beperkt de complexiteit in deze fase. De gekozen route werkt aantoonbaar op echte hardware.

**Consequenties:** T-102 is afgerond; vervolgstappen verschuiven naar `market_data`/exchange, net-runtime en later UI/LVGL-keuzes. Panel-specifieke finetuning kan later nog nodig zijn voor rijkere UI-lagen, maar niet meer voor basis bring-up.

**Actiepunten:** werkdocument/status bijwerken en T-103 openen.

**Status:** besloten

\

**ID:** D-008

**Datum:** 2026-04-11

**Onderwerp:** Bitvavo exchange-laag (T-103) en M-002-start

**Besluit:** Bitvavo-specifieke REST/WS/TLS zit in component **`exchange_bitvavo`**; optioneel WiFi + **`net_mutex`** in **`net_runtime`**; gedeelde snapshot-types in **`market_types`**. **`market_data`** blijft de enige façade naar `app_core` (init/tick/snapshot). Managed dependency: **`espressif/esp_websocket_client`** via `idf_component.yml`. Details: [`docs/architecture/ADR-002-bitvavo-exchange-and-m002.md`](../docs/architecture/ADR-002-bitvavo-exchange-and-m002.md).

**Alternatieven:** Bitvavo in `app_core`; monolithische netlaag met MQTT/WebUI in dezelfde stap.

**Argumenten voor besluit:** scheiding van transport vs domein; geen UI-vervuiling; herleidbaar tot migratiematrix M-002/M-008/M-009.

**Consequenties:** WiFi-credentials via **NVS + onboarding** (**D-009**), niet meer alleen Kconfig; mock-feed blijft via `CONFIG_MD_USE_EXCHANGE_BITVAVO=n`; CI heeft netwerk nodig voor eerste download van managed components.

**Actiepunten:** op apparaat valideren met echte WiFi + markt; backoff/queue later verfijnen.

**Status:** besloten (softwarebasis); field-test gebruiker.

\

**ID:** D-009

**Datum:** 2026-04-11

**Onderwerp:** WiFi-provisioning / onboarding V2

**Besluit:** WiFi STA-credentials staan in **NVS** (`config_store`, schema v2: `wifi_sta_ssid` / `wifi_sta_pass`). Zonder geldige credentials start het apparaat een **SoftAP** met SSID **`CryptoAlert`**, serveert een **minimale HTTP-portal** (formulier), daarna **`esp_restart`**. Daarna normale STA via `net_runtime::start_sta`. Component **`wifi_onboarding`** is logisch gescheiden van **`market_data`**. Detail: [`docs/architecture/ADR-003-wifi-onboarding-v2.md`](../docs/architecture/ADR-003-wifi-onboarding-v2.md).

**Alternatieven:** alleen Kconfig; BLE-provisioning; mobiele app.

**Argumenten:** ESP-IDF-native (`esp_http_server`, `esp_wifi`), kleine scope, testbaar op GEEK, geen Bitvavo-koppeling.

**Consequenties:** eerste boot toont geen display-init vóór succesvolle portal (acceptabel); mislukte STA na geldige NVS vraagt om **force-onboarding** of toekomstige watchdog (M-002).

**Actiepunten:** field-test portal + tweede boot STA.

**Status:** besloten (implementatie).

\

---

\

## 9. Huidige status

\

**Fase:** de eerste **ESP-IDF skeleton- en integratiefase** in `firmware-v2/` (branch **`v2/foundation`**) is **afgerond**; het project zit nu in een **eerste geïntegreerde beta-/consolidatiefase** (zie § **9a** voor fasebeoordeling, volwassenheid en **resterende hoofdwerkpakketten**). We zitten **niet meer** in “alleen fundering”; we zitten ook **niet** op “bijna release”.

**Referentieboard:** **ESP32-S3 GEEK** — enige concrete BSP in code (`bsp_s3_geek`); LCDWIKI/JC3248 alleen in strategie/matrix, nog geen code.

\

### Tickets S-001 … S-007 — status **klaar**

\

| ID | inhoud | component / artefact |

| ----- | -------------------------------- | ---------------------------------------------------------------- |

| S-001 | ESP-IDF-project + mappen | `CMakeLists.txt`, `sdkconfig.defaults`, `main/`, `.gitignore` |

| S-002 | app lifecycle | `components/app_core/` |

| S-003 | config + schemaversie (stub) | `components/config_store/` (NVS-namespace `v2cfg`) |

| S-004 | diagnostics / logconventie | `components/diagnostics/` |

| S-005 | BSP GEEK | `bsp_common/`, `bsp_s3_geek/` (o.a. backlight GPIO7, descriptor) |

| S-006 | display + UI mock | `display_port/`, `ui/` (geen LVGL) |

| S-007 | market_data contract + mock feed | `market_data/` |

\

### Ticket T-101 — reproduceerbare build + CI-smoke — status **klaar**

\

| Onderdeel | Detail |

| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| **ESP-IDF-versie** | **v5.4.2** (zie D-006, `ESP_IDF_VERSION`) |

| **Documentatie** | `BUILD.md` — clone tag, `install.sh esp32s3`, `export.sh`, veelvoorkomende fouten |

| **CI** | `.github/workflows/firmware-v2-smoke.yml` — container `espressif/idf:v5.4.2`, `set-target esp32s3`, `build` |

| **Lokaal bewezen** | **Conditioneel:** bouwen lukt alleen als Xtensa **esp32s3**-toolchain geïnstalleerd is (`./install.sh esp32s3`) én `export.sh` actief is. Op een machine waar alleen `export.sh` draait zonder s3-toolchain faalt CMake (niet door applicatiecode). **Referentiebewijs:** CI-Dockerbuild (zelfde IDF-versie als pin). |

\

**Build:** project is ESP-IDF-conform; zie `README.md` en `BUILD.md`.

\

### Wat nog openstaat (kort)

\

* **LVGL** staat op de GEEK-route; verdere UI-verbreding (tweede view, thema’s) blijft **bewust** achter tot na consolidatie — zie § **9a** / § **10**.

* **Field-test** op GEEK (WiFi + live feed + alerts) blijft de **sanity-check**; geen vervanging voor latere test/releasecriteria (§ **9a** werkpakket 5).

* **M-002** is deels gedaan (o.a. M-002a backoff); de **volledige hardening-batch** (queues, scheiding, randvoorwaarden) is het **eerstvolgende grote werkpakket** na korte afronding WebUI/config — zie `M002_NETWORK_BOUNDARIES.md`, § **9a**, § **10**.

* **Migratiematrix** en **V1-gap**: bewust kiezen wat gelijkwaardigheid betekent — niet eindeloos micro-verfijnen zonder werkpakket (§ **9a** werkpakket 3).

\

### Gate naar T-102

\

T-102 gold als de eerstvolgende harde fasepoort. Deze stap gold als geslaagd zodra minimaal het volgende aantoonbaar was:

\

* gekozen en gedocumenteerde displayroute / ADR voor de GEEK

* zichtbare output op het echte GEEK-display (minimaal testbeeld of eenvoudige view)

* backlight- en basis-initialisatie gevalideerd

* build/CI blijft groen

* **LVGL** blijft buiten scope tenzij de gekozen displayroute daar bewust om vraagt

\

**Status 2026-04-11:**

\

* displayroute gekozen en gedocumenteerd (**ST7789 + SPI + `esp_lcd`**)

* hardwarevalidatie op echte S3-GEEK is **geslaagd**

* **zichtbare full-screen output is bereikt op echte hardware**

* **backlight is functioneel**

* de eerdere geometrie/windowing-problemen zijn opgelost

* **T-102 is afgerond**

\

### Eerder bereikt (context)

\

* probleem/doel, strategie, sporen, besluiten D-001 … D-004

* branch `v2/foundation`, inventarisatie V1, migratiematrix-concept in `docs/migration/`

* hoofd-README verwijzing naar V2

\

---

\

## 9a. Huidige projectfase — fasebeoordeling, volwassenheid, werkpakketten

\

### Waar staan we nu echt?

\

We zitten **niet meer** in de vroege **skeleton-/alleen-funderingfase**. Op **`v2/foundation`** is inmiddels **integraal** aanwezig: reproduceerbare ESP-IDF-baseline, **live market data** (Bitvavo achter `market_data`), **LVGL** op het referentieboard, **NTFY** en **MQTT**, **WebUI** met status en **gecontroleerde writes** (o.a. runtime-drempels/regime, policy-timing, **confluence-policy** — M-003b/c/d + M-013f/g/i/j/k/**l**), **OTA**, **domain_metrics** / **alert_engine** (1m, 5m, confluence, suppressie, mini-regime) met **NVS-runtimeconfig**.

Dat plaatst het project in een **eerste geïntegreerde beta-/consolidatiefase**: de kern is **aanwezig en werkend**, maar **productierijpheid**, **multi-board** en een **afgesproken release** zijn **nog niet** bereikt — en worden hier ook **niet** als impliciet geclaimd.

**Risico:** voortgang **verzandt in microstappen** (losse M-tickets zonder samenhang) terwijl de basis dat ritme niet meer nodig heeft als **primaire** sturing. **Gevolg:** de **primaire** planningseenheid verschuift naar **grotere werkpakketten** (zie hieronder); kleine stappen blijven nuttig **binnen** zo’n pakket, niet als default voor alle werk.

\

### Volwassenheidsinschatting (compact)

\

| Domein | Inschatting |

|--------|-------------|

| **Architectuur / fundering** | Grotendeels op orde (componenten, `config_store`, services-keten, TLS-baseline). **M-002-hardening** (queues, taakscheiding, randvoorwaarden) is nog een **expliciet** restpakket. |

| **Productkern (alerts + marktdata)** | Aanwezig en werkend op GEEK; **spamkwaliteit**, randgevallen en **parity met V1** zijn nog geen afgerond pakket. |

| **Observability / beheer** | Ver gevorderd: WebUI-status, alert-/regime-observability, OTA-info, gerichte JSON-writes. |

| **Productierijpheid** | Nog niet: teststrategie, releasecriteria en operatie ontbreken als **uitgewerkt** blok (wel erkenning in § **9a** werkpakket 5). |

| **Multi-board** | Nog niet: **GEEK** is referentie; **JC3248W535** / **LCDWIKI** horen in een **apart** pakket, niet “erbij” in kleine haakjes. |

\

### Resterende hoofdwerkpakketten (richting eerste serieuze V2-release)

\

Geen vervanging van de migratiematrix, wel **samenhangende blokken** i.p.v. een lange micro-ticketlijst als enige lens:

\

1. **WebUI/config op huidig niveau** — **afgerond** (incl. **M-013l**: minimaal form op **`/`** voor confluence-policy; zelfde subset als M-003d/M-013k). Geen nieuwe brede settings-laag tot expliciet besluit.

2. **M-002 hardening-batch** — netwerk-eigenaarschap, queues/backpressure, scheiding WS/REST/MQTT/NTFY/WebUI, eventueel worker-task; bron: **`M002_NETWORK_BOUNDARIES.md`**. Dit is de **eerstvolgende grote technische sprint** (primaire focus na afronding (1)).

3. **V1-gap review / bewuste scopekeuzes** — wat moet gelijkwaardig zijn aan V1, wat niet; vastleggen in matrix + besluitenlog.

4. **Alert-engine consolidatie** — tuning, spamkwaliteit, randgevallen; observability blijft **in lijn** met gedrag; geen feature-explosie.

5. **Productierijpheid / operatie / test- en releasekwaliteit** — CI waar zinvol, flash/partitiebeleid, release-notes, **concrete** field-testcriteria (geen vage “nog veel werk”).

6. **Extra boards (JC3248W535 / LCDWIKI)** — BSP/flash/display **na** besluit; los trekken van GEEK-consolidatie.

7. **Release candidate / stabilisatiefase** — wat “ship” betekent voor V2, defectenlijst, korte stabilisatie-iteraties.

\

**Bedoelde volgorde:** (1) ~~kort afronden~~ **afgerond** → (2) als eerste **grote** increment (**nu** primaire technische focus) → (3) en (4) deels overlappend → (5) nadat (2)–(4) voortgang hebben → (6)–(7) volgens prioriteit en beschikbaarheid.

\

---

\

## 10. Prioriteitenlijst

\

### Prioriteit nu (geïntegreerde beta / consolidatiefase — zie §9a)

\

1. **WebUI/config-lijn op dit capability-niveau — afgerond** — thresholds/regime (M-003b, M-013f/g), policy-timing (M-003c, M-013i/j), confluence-policy (M-003d, M-013k, form **M-013l**): eenduidige read/write-paden + browserforms waar voorzien. **Geen** verdere microstappen op deze lijn zonder nieuw besluit.

2. **M-002 hardening-batch** — **eerstvolgende primaire focus** (groot werkpakket, **geen** microstap): één samenhangende increment volgens `M002_NETWORK_BOUNDARIES.md` en § **9a** werkpakket 2.

3. **Consolidatie + V1-gap review** — alert-engine gedrag en migratiematrix; **expliciete** keuzes vóór verdere verbreding (UI-views, integraties, “nice-to-have”).

4. **Productierijpheid** (test, release, operatie) — pas als (1)–(3) **duidelijke** voortgang hebben; zie § **9a** werkpakket 5.

5. **Werkdocument en migratiematrix** bijwerken op **werkpakket**-niveau (§ **9a**), niet alleen losse M-nummers.

6. V1 blijft **bevroren referentie** (B-001); geen grote V1-uitbreidingen zonder besluit.

7. **Field-test** op GEEK (WiFi, live feed, alerts) blijft de **lopende sanity-check**, geen vervanging voor een latere releasechecklist.

\

---

\

## 11. Open vragen

\

Gebruik dit hoofdstuk om openstaande discussiepunten vast te leggen.

\

* ~~Welk board wordt het eerste primaire ontwikkelplatform?~~ → **besloten:** ESP32-S3 GEEK (zie D-004, §9).

* ~~Wat is de **exact te bevestigen displaycontroller / paneelroute** van de S3-GEEK voor T-102?~~ → **bevestigd:** ST7789 + SPI + `esp_lcd`; basisroute werkt correct op echte hardware.

* ~~Kiezen we in T-102 voor **`esp_lcd`-first** met minimale pixelroute vóór eventuele LVGL-integratie?~~ → **Ja, besloten voor T-102**.

* Willen we tijdelijk Arduino als component toestaan voor specifieke drivers, maar alleen als er geen nette ESP-IDF-route beschikbaar is?

* Welke instellingen blijven in de primaire WebUI zichtbaar?

* Welke instellingen worden secundair of verdwijnen?

* Hoe willen we loggingcategorieën indelen?

* Welke definitie hanteren we voor “productierijp”?

* Hoe ver willen we gaan in Home Assistant integratie voor configuratie?

\

---

\

## 11b. Aangescherpte migratiematrix — stuurversie

\

Op basis van de eerste inventarisatie uit de repo en de terugkoppeling van Cursor is de migratiematrix inhoudelijk aangescherpt. De kern is dat V2 niet primair rond features wordt opgebouwd, maar rond **ontvlechting van verantwoordelijkheden**.

\

### Prioriteitsindeling

\

* **P0 — Kritisch pad:** zonder deze onderdelen geen stabiele V2-basis

* **P1 — Kernmigratie:** essentieel voor functionele gelijkwaardigheid met V1

* **P2 — Secundair:** belangrijk, maar pas na stabiele basis

* **P3 — Later / optioneel / mogelijk schrappen**

\

### Stuurprincipes

\

1. **Eerst orchestratie en grenzen, daarna functionaliteit.**

2. **Eerst S3-GEEK stabiel, daarna LCDWIKI, daarna JC3248W535.**

3. **Eerst live UI met mock of gesimuleerde data, daarna echte services koppelen.**

4. **Configuratie en logging vanaf het begin goed neerzetten.**

\

### Aangescherpte matrix (compact)

\

| ID | Domein / functionaliteit | V1-bron (globaal) | Status | Prioriteit | Beoogde V2-component | Belangrijkste reden / risico |

| ----- | ---------------------------------------- | ------------------------------------------------------------------------------------------------ | --------------------------- | ---------- | ------------------------------------------------- | ---------------------------------------------------------------------------------------------- |

| M-001 | App-opstart en runtime-orchestratie | `ESP32-Crypto-Alert.ino`, globale init, tasks | **Herschrijven** | **P0** | `app_core` | Grootste architectuurknoop; nu te veel taken, globals en volgorde-afhankelijkheden in één plek |

| M-002 | Netwerkcoördinatie en mutex-/timingmodel | `ApiClient`, `Net/HttpFetch`, globale mutexen zoals `gNetMutex` | **Herschrijven** | **P0** | `net_runtime` / `market_data` / `service_runtime` | Grootste runtime-risico; web/API/NTFY/MQTT grijpen nu in elkaar |

| M-003 | Config-opslag en defaults | `SettingsStore`, settings in V1 | **Herschrijven** | **P0** | `config_store` | Zonder nette configlaag blijft V2 snel vervuilen; ook nodig voor WebUI/MQTT/alerts |

| M-004 | Logging en diagnose-infrastructuur | compile-time flags, diag-logs, `platform_config.h` | **Herschrijven** | **P0** | `diagnostics` | Nu versnipperd en te veel diagnostiek in productiepaden |

| M-005 | Board support S3-GEEK | `platform_config.h`, display/init stukken | **Herschrijven** | **P0** | `bsp_s3_geek` + `bsp_common` | Eerste doelboard; nodig om alle volgende keuzes op te valideren |

| M-006 | UI-thema, layout en schermmodel | `src/UI*`, display/logica, huidige schermopbouw | **Behouden + herschrijven** | **P1** | `ui` + `display_port` | Look-and-feel is waardevol, maar implementatie moet los van data/services |

| M-007 | LVGL/display-port | `src/display/`, Arduino_GFX / `esp_lcd` backends | **Herschrijven** | **P1** | `display_port` | Hardwareabstractie moet schoner; GEEK eerst, andere panels later |

| M-008 | Market data live feed (WebSocket) | `ApiClient`, websocketpad | **Behouden + herschrijven** | **P1** | `market_data` | Door gebruiker expliciet gewaardeerd; functioneel anker van V2 |

| M-009 | REST/bootstrap/warm-start dataflow | API-fetch, bootstraplogica | **Herschrijven** | **P1** | `market_data` | Nodig voor robuuste start; mag niet meer overal in de app lekken |

| M-010 | Alert-engine en metricsdomein | `AlertEngine`, `TrendDetector`, `VolatilityTracker`, `AnchorSystem`, `RegimeEngine`, `PriceData` | **Behouden + herschrijven** | **P1** | `alert_engine`, `domain_metrics` | Sterke functionele kern, maar beter afgrenzen en testbaar maken |

| M-011 | NTFY notificaties | huidige NTFY-keten | **Behouden + herschrijven** | **P1** | `ntfy_client` | Werkt goed, maar moet als losse service achter events/commands komen |

| M-012 | MQTT / Home Assistant bridge | MQTT integratie | **Behouden + herschrijven** | **P1** | `mqtt_bridge` | Waardevol voor analyse/HA, maar niet leidend in bootpad |

| M-013 | WebUI basis | `WebServer`, settings/UI | **Behouden + herschrijven** | **P2** | `webui` | Belangrijk, maar pas ná config/runtime-grenzen; anders sleept V1-koppeling mee |

| M-014 | OTA via web | `OtaWebUpdater` | **Behouden + herschrijven** | **P2** | `ota_service` | Gewaardeerd, maar pas na stabiele appstructuur en partition-strategie |

| M-015 | Extra boards LCDWIKI / JC3248W535 | display- en board-specifieke code | **Later herschrijven** | **P2** | `bsp_lcdwiki_28`, `bsp_jc3248w535` | Niet te vroeg toelaten; anders domineert hardwarevariatie de basis |

| M-016 | Oude compatibiliteitslagen CYD / TTGO | historische boardpaden | **Schrappen / archiveren** | **P3** | n.v.t. | Geen prioriteit meer; niet meenemen in V2-kern |

| M-017 | Oude/inconsistente V1-documentatie | `docs/CODE_INDEX.md`, `CODE_ANALYSIS.md` etc. | **Nader beoordelen** | **P3** | docs cleanup | Belangrijk voor netheid, maar niet blokkerend voor skeleton |

\

### Eerste skeleton-focus — **afgerond (2026-04-11)**

\

De onderstaande componenten zijn als **skeleton** aanwezig in `firmware-v2/` (tickets S-001 … S-007; zie §9). **Displaystack, exchange-laag en vervolgstappen** zijn inmiddels **ingevuld** op GEEK; vervolgwerk volgt **werkpakketten** (§ **9a**), niet herdefiniëren van dit skeleton-blok.

\

1. `app_core`

2. `config_store`

3. `diagnostics`

4. `bsp_common` + `bsp_s3_geek`

5. `display_port`

6. `ui` (mock)

7. `market_data` (contract + mock)

\

## 12. Backlog

\

### Template

\

* **ID:**

* **Titel:**

* **Beschrijving:**

* **Type:** architectuur / code / documentatie / test / besluit

* **Prioriteit:** hoog / midden / laag

* **Afhankelijkheden:**

* **Status:** todo / bezig / geblokkeerd / klaar

* **Eigenaar:** gebruiker / ChatGPT / Cursor

\

### Eerste backlog-items

\

* **ID:** B-001

**Titel:** V1 freeze voorbereiden

**Beschrijving:** Vastleggen welke branch/tag de referentieversie wordt.

**Type:** proces

**Prioriteit:** hoog

**Status:** klaar

**Eigenaar:** gebruiker

\

* **ID:** B-002

**Titel:** V2 mapstructuur uitschrijven

**Beschrijving:** Nieuwe projectstructuur op componentniveau opstellen.

**Type:** architectuur

**Prioriteit:** hoog

**Status:** klaar (ESP-IDF `firmware-v2/` + componenten; zie §9)

**Eigenaar:** Cursor

\

* **ID:** B-003

**Titel:** Migratiematrix opstellen

**Beschrijving:** Huidige functionaliteiten labelen als behouden / herschrijven / schrappen.

**Type:** architectuur

**Prioriteit:** hoog

**Status:** bezig

**Eigenaar:** ChatGPT + gebruiker

\

* **ID:** B-004

**Titel:** Eerste referentieboard kiezen

**Beschrijving:** Beslissen met welk S3-board V2 start.

**Type:** besluit

**Prioriteit:** hoog

**Status:** klaar

**Eigenaar:** gebruiker + ChatGPT

\

* **ID:** B-006

**Titel:** V2-branch in bestaande repo aanmaken

**Beschrijving:** In de huidige GitHub-repo een aparte branch voor de V2-herbouw laten voorbereiden, inclusief basisplek voor nieuwe code en documentatie.

**Type:** proces

**Prioriteit:** hoog

**Status:** klaar

**Eigenaar:** Cursor

\

* **ID:** B-007

**Titel:** Input voor migratiematrix verzamelen

**Beschrijving:** Huidige code en bestaande documentatie laten analyseren om een eerste indeling te maken in behouden / herschrijven / schrappen.

**Type:** architectuur

**Prioriteit:** hoog

**Status:** bezig (stuurversie §11b; verfijnen blijft doorlopend)

**Eigenaar:** Cursor + ChatGPT + gebruiker

\

* **ID:** B-008

**Titel:** Werkdocument synchronisatie inrichten

**Beschrijving:** Vastleggen of canvas of GitHub leidend is, en hoe updates structureel worden doorgevoerd.

**Type:** proces

**Prioriteit:** hoog

**Status:** bezig

**Eigenaar:** gebruiker + ChatGPT + Cursor

\

* **ID:** T-101

**Titel:** Reproduceerbare ESP-IDF build + CI-smoke (`firmware-v2/`)

**Beschrijving:** ESP-IDF **v5.4.2** vastgelegd (`ESP_IDF_VERSION`), `BUILD.md`, workflow `.github/workflows/firmware-v2-smoke.yml`.

**Type:** documentatie / CI

**Prioriteit:** hoog

**Status:** klaar

**Eigenaar:** Cursor

\

* **ID:** T-102

**Titel:** Display bring-up GEEK (`display_port`)

**Beschrijving:** Kies en documenteer een displayroute voor de S3-GEEK, valideer backlight + basis-initialisatie en toon aantoonbaar zichtbare output op het echte display. LVGL alleen meenemen als dit een bewuste en onderbouwde keuze is.

**Type:** code / architectuur / test

**Prioriteit:** hoog

**Afhankelijkheden:** T-101

**Status:** klaar

**Eigenaar:** Cursor + gebruiker

\

* **ID:** T-103

**Titel:** Exchange-koppeling achter `market_data`

**Beschrijving:** Breng REST/WS/TLS voor Bitvavo onder in een aparte exchange/netwerklaag achter `market_data`, zonder UI-vervuiling en zonder terugval naar V1-koppelingen.

**Type:** code / architectuur

**Prioriteit:** hoog

**Afhankelijkheden:** T-102, M-002

**Status:** klaar (code + ADR-002); field-test (WiFi + live prijs) door gebruiker

**Eigenaar:** Cursor + gebruiker (validatie)

\

* **ID:** B-005

**Titel:** Werkafspraken met Cursor definiëren

**Beschrijving:** Vastleggen hoe prompts, reviews en commits worden georganiseerd.

**Type:** proces

**Prioriteit:** midden

**Status:** todo

**Eigenaar:** gebruiker + ChatGPT

\

---

\

## 13. Risico’s en aandachtspunten

\

* risico op te snelle migratie zonder voldoende referentie uit V1

* risico dat display-specifieke issues te vroeg de architectuur domineren

* risico dat WebUI en config te breed blijven

* risico dat technische schuld uit V1 ongemerkt mee verhuist

* risico dat te veel tegelijk wordt opgepakt zonder duidelijke integratiestappen

* risico op **verzanden in microstappen** zonder samenhang (te laat overschakelen naar **werkpakketten** — zie § **9a**)

\

### Beheersmaatregelen

\

* **Werkpakketten** als primaire planseenheid naast de migratiematrix (§ **9a**); microstappen **binnen** pakketten blijven oké

* kleine iteraties **waar** het past (bugfixes, afronding WebUI/config)

* heldere besluitenlog

* vaste prioriteitenlijst (§ **10** — afgestemd op consolidatiefase)

* migratiematrix als leidraad

* V1 als referentie bewaren

\

---

\

## 14. Werkafspraken

\

### Gebruik van dit document

\

* ChatGPT gebruikt dit document als architectuur- en procesreferentie

* Cursor gebruikt dit document als uitvoerreferentie voor code- en structuurwerk

* gebruiker gebruikt dit document om voortgang, besluiten en prioriteiten te bewaken

\

### Manier van werken

\

* eerst doel en scope van een stap expliciet maken

* daarna pas code laten genereren of aanpassen

* elke grotere stap afsluiten met:

\

* wat is gedaan

* wat is besloten

* wat is nog open

* wat is de volgende prioriteit

\

---

\

## 15. Eerstvolgende concrete stap

\

### Faseroute (nu — consolidatiefase)

\

1. **Afgerond:** **WebUI/config-lijn** op dit niveau — incl. **M-013l** (minimaal form op **`/`** voor confluence-policy; POST **M-013k**). Thresholds/regime/timing/confluence: read + write + form waar voorzien. **Geen** nieuwe brede settings-laag tot besluit.

2. **Nu (geen microstap):** **M-002 hardening-batch** als **eerstvolgende** werk — queues, scheiding, randvoorwaarden — `M002_NETWORK_BOUNDARIES.md`, § **9a** werkpakket 2, § **10** punt 2.

3. **Vervolgens:** **consolidatie** (alert-engine, spam/randgevallen) en **V1-gap review** met **bewuste** scopekeuzes (§ **9a** werkpakketten 3–4); **pas daarna** verdere verbreding (extra UI-views, integraties) en **productierijpheid** als apart pakket (§ **9a** werkpakket 5).

\

**Discipline:** geen tegenstrijdigheid tussen “kleine afronding” en “grote pakketten” — de kleine afronding is **bewust de sluiting** van de huidige config-lijn; het **volgende** werk is **per definitie** groter gesneden.

\

### Historisch bereikt (ter referentie — niet de volgende stap)

\

* **T-101 … T-103(d):** build/CI, display bring-up (GEEK), `exchange_bitvavo` + `net_runtime` + `market_data`, WiFi-onboarding.

* **LVGL** op `esp_lcd` (**ADR-004**), live view + verfijning (§ **Stap 8b**).

* **M-002a:** STA-backoff + disconnect-logging; verdere M-002 volgt als **batch** (hierboven).

* **Services / WebUI / OTA / alerts:** zie § **9** en migratiematrix (M-011 … M-014, M-003*, M-010*, enz.); **M-013l** sluit de browserforms voor de **kleine** config-subsets af vóór M-002-batch.

\

---

\

## T-103b — TLS-logspam, flash footprint, partitionering (uitgevoerd)

\

### Deel A — Herhaalde `Certificate validated`

\

**Oorzaak**

\

- Logtag **`esp-x509-crt-bundle`** schrijft bij elke geslaagde TLS-handshake op **INFO** (`Certificate validated`).

- Bronnen in de huidige V2-build:

1. **REST** `fetch_ticker_price`: nieuwe `esp_http_client`-sessie per call (TLS volledig opnieuw).

2. **WebSocket** `wss://`: TLS bij start en bij **elke auto-reconnect** (client staat niet op `disable_auto_reconnect`).

3. Gecombineerd: kort na boot eerste REST + WS ≈ **minstens twee** cert-logs; daarna elke **45 s** opnieuw REST **terwijl WS al live prijs levert** → overbodige tweede TLS-rij.

\

**Diagnose**

\

- Tag **`bv_feed`**: REST start/einde met duur (`REST TLS start|ok|end`), WS `CONNECTED`/`DISCONNECTED` met korte toelichting.

\

**Fix / tussenmaat (M-002-conform, klein)**

\

- Runtime: `esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN)` in `diagnostics::init_early()` — geen spam, echte verificatiefouten blijven zichtbaar.

- **REST** wordt **niet** meer op de 45 s-cyclus uitgevoerd als **WS al verbonden is en er een geldige snapshot is**; volgende optionele REST-poort na **300 s** (sanity). Zonder WS blijft 45 s bootstrap-gedrag.

- WS: `reconnect_timeout_ms` **5000 → 10000** om reconnect-storm iets te dempen (geen architectuurwijziging).

\

**Onzekerheden / vervolg**

\

- Langere termijn: HTTP-client **hergebruik** voor REST Bitvavo: **M-002b** (uitgevoerd); verdere backoff/OTA-task i.p.v. main stack blijft open.

\

### Deel B — Flash footprint

\

**Methode** (lokaal met geactiveerde IDF-omgeving):

\

`idf.py size`, `idf.py size-components`, eventueel `idf.py size-files`.

\

**Verwachting (kwalitatief)**

\

- Zwaar: **mbedTLS + certificate bundle**, **esp_http_client**, **esp_websocket_client**, **esp_lcd** + driver, **WiFi stack**.

- Tijdelijk acceptabel: huidige V2 zonder LVGL/WebUI — structureel blijven monitoren na elke grote merge.

\

### Deel C — Partitionering

\

- Nieuw: **`partitions_v2_16mb_ota.csv`** — **ota_0** en **ota_1** elk **0x300000 (3 MiB)**, **otadata**, **storage** ~10.4 MiB.

- Optioneel dev: **`partitions_v2_16mb_factory.csv`** (één groot factory-slot, geen OTA).

- **`sdkconfig` / `sdkconfig.defaults`**: `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, `CONFIG_PARTITION_TABLE_CUSTOM` + bestandsnaam.

- Documentatie: **`docs/flash/PARTITIONS_V2.md`**, **`BUILD.md`** (size + partities).

\

**Eerste flash na upgrade partitietabel:** `erase-flash` of volledige flash — oude 1 MiB-layout vervangen.

\

### Deel D — No-regret

\

- Cert-bundle **INFO** onderdrukken (zie deel A).

- Diagnostische logs onder **`bv_feed`** beperkt tot feed-laag; routine REST-success op **INFO** met één regel (kan later naar **DEBUG**).

\

---

\

## T-103c — Field-test: WS live prijs door V2-laag (uitgevoerd)

\

**Bewijs**

\

- `market_types::MarketSnapshot` bevat `market_label`, `last_tick_source` (`None` / `Rest` / `Ws`).

- Bitvavo **WS** zet bij `apply_price` → `last_tick_source = Ws`; REST-bootstrap zet `Rest`.

- `app_core` roept na `market_data::tick()` **`t103_field_log_ws_via_market_data()`** aan (alleen `CONFIG_MD_USE_EXCHANGE_BITVAVO=y`): leest **`market_data::snapshot()`**, logt bij **WS** + geldige snapshot:

\

`T-103 field: sym=… price=… EUR bron=WS | market_data::snapshot ok (ts_ms=…)`

\

- **Begrenzing:** log alleen bij **prijsverandering** t.o.v. laatste log **of** minstens **30 s** sinds vorige log (geen dump per WS-tick).

- **Optioneel scherm (T-103c):** `display_port::ws_live_validation_strip_toggle()` — pixelband **zonder** LVGL; bij **T-103d** niet meer gecombineerd met live UI (framebuffer-conflict).

\

**Volgende logische stap (na T-103d):** M-002 backoff / net-hardening, UI-thema/layout verfijnen, of tweede view — geen verplichte «big bang» dashboard-migratie.

\

---

\

## T-103d — Eerste LVGL UI: live prijs op `esp_lcd` (S3-GEEK) (uitgevoerd)

\

**ADR:** [ADR-004 — LVGL op esp_lcd-route](../../docs/architecture/ADR-004-lvgl-esp-lcd-ui-layer.md).

\

- **Component:** `espressif/esp_lvgl_port` (`firmware-v2/main/idf_component.yml`).

- **`display_port`:** behoudt `esp_lcd` + SPI IO; exporteert `io_handle` / `panel_handle` / afmetingen; na init **zwarte** clear vóór LVGL (geen effen groen als eindstaat).

- **`ui`:** `lvgl_port_init` + `lvgl_port_add_disp`; labels: **symbool**, **prijs (EUR)**, **bron** (`WS` / `REST` / —). Updates via `ui::refresh_from_snapshot(market_data::snapshot())` vanuit `app_core` ( **`lvgl_port_lock`** in `ui`).

- **Scheiding:** geen `exchange_bitvavo` in `ui`.

\

**Bewust buiten scope:** dashboard, grafieken, alerts, WebUI, MQTT, NTFY, extra boards.

\

---

\

## M-002a — STA-backoff en netwerkstatus (na T-103d) (uitgevoerd)

\

**Wat is aangescherpt**

\

- **WiFi STA-reconnect** zit **uitsluitend** in **`net_runtime`**: bij `WIFI_EVENT_STA_DISCONNECTED` geen directe `esp_wifi_connect()` meer; **één** plek met **exponentiële backoff** (500 ms → … → max 30 s) via **`esp_timer`**, reset bij **`IP_EVENT_STA_GOT_IP`**. Hiermee is reconnect/backoff voor de datalink niet meer verspreid over andere lagen.

\

- **Zichtbaarheid:** disconnect-log bevat nu **WiFi-reasoncode** (alleen in `net_runtime`, geen lek naar UI). Bovenliggende lagen blijven **linkstatus** afleiden via bestaande façade: `net_runtime::has_ip()` en `market_data::snapshot()` (`ConnectionState` / `FeedErrorCode::NetworkDown` zonder Bitvavo-protocolteksten).

\

- **Onveranderd (bewust):** **WebSocket**-reconnect blijft eigendom van **`esp_websocket_client`** in **`exchange_bitvavo`** (`reconnect_timeout_ms`); **REST**-interval blijft in `exchange_bitvavo::tick`. Geen nieuwe services, geen UI-wijziging, geen worker-task.

\

**Planningslens:** de **primaire sturing** richting een eerste serieuze V2-release staat in § **9a** (fase, volwassenheid, **hoofdwerkpakketten**) en § **10**; onderstaande regel blijft een **technische** spoor-index (M-nummers), geen vervanging van die werkpakketten.

\

**Technische backlog (o.a. openstaand uit M-002; detail):** mutex/queue voor schrijf-paden; optionele net-worker-task — zie [M002_NETWORK_BOUNDARIES.md](../docs/architecture/M002_NETWORK_BOUNDARIES.md). **REST-HTTP hergebruik (Bitvavo):** § **M-002b**. **Outbound queue + dispatch:** § **M-002c**. **Runtime service-config (typed, read-only overlay):** § **M-003a**. **Alert/regime tuning (typed, klein, non-secret):** § **M-003b**. **Alert-policy timing (cooldown/suppress, typed, non-secret):** § **M-003c**. **Confluence-policy subset (typed, status + JSON-write + form):** § **M-003d**, § **M-013k**, § **M-013l**. **NTFY-sink:** § **M-011a**; **domein 1m-alert → NTFY:** § **M-011b**. **MQTT-bridge (boot):** § **M-012a**; **1m-domeinalert → MQTT:** § **M-012b**. **WebUI (status + service-write + form + read-only alert-observability + regime/drempel-observability + alert-runtime JSON-write + minimaal alert-runtime-form + read-only alert-beslissing/cooldown/suppress-snapshot + alert-policy-timing JSON-write + minimaal alert-policy-timing-form + confluence-policy JSON-write + minimaal confluence-policy-form + OTA-basis + OTA-observability):** § **M-013a**–**M-013l**, § **M-014a**–**M-014b**. **Eerste domeinmetric + alert-slice:** § **M-010a**. **Per-seconde canonicalisatie op WS-input:** § **M-010b**. **Tweede timeframe (5m) parallel aan 1m:** § **M-010c**. **Eerste 1m+5m-confluence (eenvoudige regel):** § **M-010d**. **Prioriteit/suppressie losse TF t.o.v. confluence:** § **M-010e**. **Mini-regime (vol → calm/normal/hot) zonder volledige engine:** § **M-010f**.

\

---

\

## M-002b — REST: HTTP-client hergebruik in `exchange_bitvavo` (uitgevoerd)

\

**Doel:** het REST-pad naar Bitvavo (`GET /v2/ticker/price`) minder TLS-overhead zonder nieuwe service-laag of wijziging aan `market_data` / UI.

\

**Wat is aangescherpt**

\

- **Ownership:** één lazy-geïnitialiseerde `esp_http_client` in **`bitvavo_rest.cpp`** (file-static). Alleen deze module beheert de handle; geen globale HTTP-helper buiten de exchange-laag.

\

- **Lifecycle:** eerste succesvolle `fetch_ticker_price` roept `esp_http_client_init` aan (basis-URL `https://api.bitvavo.com/`); elke call zet het volledige pad via **`esp_http_client_set_url`**, daarna open → headers → body → **`esp_http_client_close`**. De handle blijft bestaan voor de volgende call (minder volledige handshake dan «nieuwe sessie per request»).

\

- **Fouten:** bij `set_url`/open/read/niet-2xx/lege body wordt na **`close`** waar nodig **`esp_http_client_cleanup`** aangeroepen (`http_invalidate`); volgende REST-poging bouwt een schone client op. JSON-parsefout na geldige 200 blijft bewust **zonder** invalidate (zelfde transport, alleen inhoud).

\

- **Synchronisatie:** ongewijzigd — volledige REST-call blijft onder **`net_runtime::net_mutex`**; `exchange_bitvavo::tick` blijft de enige aanroeper in de huidige opzet.

\

**Bewust niet in deze stap:** MQTT/NTFY/WebUI; queue-framework; net-worker-task; hergebruik of wijziging aan **WebSocket**-client; wijziging aan `market_data`-API of snapshot-velden; backoff uitbreiden buiten bestaand STA/REST-gedrag.

\

---

\

## M-002c — Minimale servicegrens outbound events + interne queue (uitgevoerd)

\

**Doel:** één neutrale **producer→queue→consumer**-grens: contract (`Event`) + **FreeRTOS-queue** (diepte 8) + **`poll()`**-drain. De eerste echte outbound-sink is **NTFY** (§ **M-011a**); daarvoor was de consumer nog stub-only.

\

**Wat is aangescherpt**

\

- **Component `service_outbound`:** `Event` (o.a. `ApplicationReady`); **`emit()`** / **`poll()`** zoals beschreven; dispatch roept bij `ApplicationReady` de **`ntfy_client`**-sink aan (indien Kconfig aan).

\

- **`app_core` orkestreert:** na `market_data::init` → `service_outbound::init`; vóór de hoofdlus `emit(ApplicationReady)` + **`poll()`**; in de hoofdlus **`poll()`** vóór elke slaap — geen worker-task.

\

- **Ownership:** `market_data` blijft façade; `exchange_bitvavo` blijft transport; **UI** / **display_port** geen NTFY. Zie **M-011a** voor transport-detail.

\

**Bewust open (na M-011a / M-012a / M-013a):** rijkere **payload-structs** per `Event`; net-worker-task; extra event-mappings.

\

---

\

## M-003a — Eerste typed `config_store`-model voor runtime service-instellingen (uitgevoerd)

\

**Doel (migratiematrix M-003, P0):** een **klein, typed** runtime-configmodel zodat WebUI/MQTT/NTFY niet alleen via Kconfig hoeven; **leespad** met **schema v3**, **NVS-overlay** op **Kconfig-defaults**, **zonder** WebUI-schrijven, **zonder** OTA en **zonder** brede domeinmigratie.

\

**Wat levert het op**

\

- **`ServiceRuntimeConfig`** in `config_store` (`webui_enabled`, `webui_port`, `mqtt_enabled`, `mqtt_broker_uri`, `ntfy_enabled`, `ntfy_topic`) — **geen** secrets (MQTT-wachtwoord, NTFY-token blijven Kconfig tot latere stap).

\

- **`kSchemaVersion = 3`**; bij load: eerst Kconfig-defaults voor services, daarna optionele NVS-sleutels (`svc_wui_en`, `svc_wui_pt`, `svc_mqtt_en`, `svc_mqtt_uri`, `svc_ntfy_en`, `svc_ntfy_tp`) — korte namen (≤15) in namespace `v2cfg`.

\

- **`config_store::service_runtime()`** geeft een snapshot na `load_or_defaults` (zelfde inhoud als `RuntimeConfig::services`). **`save()`** schrijft nog **geen** service-velden; **M-013b** voegt een aparte API **`persist_service_connectivity`** toe voor een **kleine mqtt/ntfy-subset** (geen `webui_*`).

\

- **Consumers:** `webui`, `mqtt_bridge`, `ntfy_client` lezen toggles/poort/URI/topic hier; **build-time** `CONFIG_*_ENABLE` blijft de harde compile-grens waar van toepassing.

\

**Bewust open (niet M-003a zelf):** brede settings-API; `save()`-uitbreiding voor alle domeinen; MQTT/NTFY **secrets** in NVS; volledige migratiematrix; market/alert/symbool-keuze buiten dit subset. **Eerste beperkte write:** § **M-013b**.

\

---

\

## M-003b — Typed runtime-config voor alert/regime-tuning (klein, non-secret) (uitgevoerd)

\

**Doel:** na **M-010f** + **M-013e** is het volgende **funderings**-stuk een **kleine typed** NVS-overlay voor **beperkte** tuning — **zonder** secrets en **zonder** brede alert-migratie uit V1. **Eerste WebUI-write** voor deze subset: § **M-013f**.

\

**Wat levert het op**

\

- **`kSchemaVersion`:** v4 introduceerde optionele `alt_*`-sleutels; **v5** (§ **M-003c**) voegt `altp_*` toe; **v6** (§ **M-003d**) voegt `altcf_*` confluence-policy toe — bij mismatch blijft gedeeltelijke load zoals bestaand.

\

- **`AlertRuntimeConfig`:** `threshold_1m_bps`, `threshold_5m_bps`, `regime_calm_scale_permille`, `regime_hot_scale_permille` — defaults uit **Kconfig**, optionele NVS-keys: **`alt_1m_bps`**, **`alt_5m_bps`**, **`alt_sc_calm`**, **`alt_sc_hot`** (namespace `v2cfg`).

\

- **`config_store::alert_runtime()`** snapshot na `load_or_defaults`; **`persist_alert_runtime`** schrijft gevalideerde waarden en werkt **`g_alert_cache`** bij.

\

- **`alert_engine`** gebruikt deze snapshot voor **basis**-drempels en **calm/hot**-‰-schaal; clamp/normal/hot-grenzen blijven uit bestaande Kconfig.

\

- **`webui`:** **`GET /api/status.json`** bevat read-only **`alert_runtime_config`** (vier velden + `schema_version`); **write:** § **M-013f**.

\

**Bewust niet in M-003b:** auth; cooldown-/suppressie-tuning (**volgt § M-003c**); brede confluence-policy-matrix (**kleine typed subset:** § **M-003d**); per-symbol tuning; V1-import; brede `save()`-uitbreiding.

\

---

\

## M-003c — Typed runtime-config voor cooldown- en suppressie-timing (klein, non-secret) (uitgevoerd)

\

**Doel:** na **M-013h** is zichtbaar *waarom* paden blokkeren; de volgende **funderingslaag** is **getunede** cooldown/suppressie-**timing** in NVS — **zonder** brede policy-matrix of V1-migratie. **Eerste WebUI-write** voor deze subset: § **M-013i**.

\

**Wat levert het op**

\

- **`kSchemaVersion`:** v4-`alt_*` blijven bruikbaar; v5 voegt **`altp_*`** toe; **v6** (§ **M-003d**) voegt **`altcf_*`** toe — policy-timing blijft ongewijzigd in betekenis.

\

- **`AlertPolicyTimingConfig`:** `cooldown_1m_s`, `cooldown_5m_s`, `cooldown_conf_1m5m_s`, `suppress_loose_after_conf_s` — defaults uit **Kconfig** (`ALERT_ENGINE_*`), NVS-overlay via **`altp_cd_1m`**, **`altp_cd_5m`**, **`altp_cd_cf`**, **`altp_sup_lo`**; validatie binnen Kconfig-ranges.

\

- **`config_store::alert_policy_timing()`** na `load_or_defaults`; **`persist_alert_policy_timing`** schrijft gevalideerde waarden naar NVS en werkt **`g_policy_cache`** bij (§ **M-013i**).

\

- **`alert_engine`:** leest snapshot **per tick** (zelfde bron als M-013h-observability); fallback is altijd Kconfig via `config_store`-defaults.

\

- **`webui`:** **`GET /api/status.json`** read-only object **`alert_policy_timing`** (+ `schema_version`); **write:** § **M-013i**; **form op `/`:** § **M-013j**.

\

**Bewust niet in M-003c:** auth; secrets; extra drempel/regime-velden buiten M-003b; confluence-policy-matrix; per-symbol; dashboard; V1-import; brede `save()`; brede policy-UI (**minimaal form** in M-013j).

\

---

\

## M-003d — Typed runtime-config voor confluence-policy (kleine subset, non-secret) (uitgevoerd)

\

**Doel:** confluence in **M-010d/e** was **hardcoded**; deze stap legt een **kleine typed** NVS-overlay (vier booleans) zodat **`alert_engine`** één **snapshot** uit **`config_store`** leest — **zonder** brede confluence-matrix en **zonder** V1-migratie. **Eerste WebUI-write:** § **M-013k**; **minimaal form op `/`:** § **M-013l**.

\

**Wat levert het op**

\

- **`kSchemaVersion = 6`:** optionele NVS-keys **`altcf_en`**, **`altcf_sd`**, **`altcf_bt`**, **`altcf_lo`** (u8 0/1) — defaults **alle `true`**, gelijk aan historisch M-010d/e-gedrag als er geen overlay is.

\

- **`AlertConfluencePolicyConfig`:** `confluence_enabled`, `confluence_require_same_direction`, `confluence_require_both_thresholds`, `confluence_emit_loose_alerts_when_conf_fails`.

\

- **`config_store::alert_confluence_policy()`** na `load_or_defaults`; **`persist_alert_confluence_policy`** schrijft naar NVS en werkt de runtime-cache bij (§ **M-013k**).

\

- **`alert_engine`:** leest de snapshot **per tick**; AND/OR-drempelpoort, richting-check en loose-gate volgen deze flags; standaard blijft gedrag gelijk aan vóór M-003d.

\

- **`webui`:** **`GET /api/status.json`** — **`alert_confluence_policy`** (+ `schema_version`); **write:** § **M-013k**; **form op `/`:** § **M-013l**.

\

**Bewust niet in M-003d:** auth; secrets; brede confluence-scorematrix; extra drempels buiten **M-003b**; extra cooldown/suppressie buiten **M-003c**; per-symbol tuning; HTML-form op **`/`** (volgt **M-013l**, niet in de kernstap M-003d zelf).

\

---

\

## M-011a — Eerste `ntfy_client`-sink achter `service_outbound` (uitgevoerd)

\

**Doel:** migratiematrix **M-011** — eerste **werkende** notificatiepad: HTTPS naar ntfy, **achter** de M-002c-queue, **zonder** MQTT/HA/WebUI.

\

**Wat levert het op**

\

- **Component `ntfy_client`:** `init()` + `send_notification(title, body)` — één `esp_http_client_perform` (POST, `text/plain`, optionele `X-Title`, optionele **Bearer**), TLS via certificate bundle, onder **`net_runtime::net_mutex`**. Geen ntfy-protocol in `ui` / `market_data` / `exchange_bitvavo`.

\

- **Kconfig (minimaal):** `CONFIG_NTFY_CLIENT_ENABLE` (default **uit**), `NTFY_SERVER`, `NTFY_TOPIC`, `NTFY_ACCESS_TOKEN`. Leeg topic ⇒ geen push (stil `ESP_OK`). **Effectief topic** (na **§ M-003a**): `config_store::service_runtime().ntfy_topic` (NVS-overlay op Kconfig-default).

\

- **Mapping:** alleen **`ApplicationReady`** → titel `CryptoAlert V2`, body `Application ready` (in `service_outbound` dispatch). Geen tweede event-type in deze stap.

\

- **Runtime:** zonder IP wordt niet geprobeerd te publishen (defensief); fouten → `ESP_LOGW` op `svc_out` / `ntfy`.

\

**Bewust open (vóór M-011b):** retry bij geen IP op eerste poll; meerdere events; persistente topic-opslag; alert-engine; uitgebreide MQTT (zie **M-012a**). **Domein 1m → NTFY:** § **M-011b**.

\

---

\

## M-011b — Eerste echte 1m-domeinalert naar `ntfy_client` (uitgevoerd)

\

**Doel (M-011 vervolg):** na **M-010a**/**M-010b** en het lege **`DomainAlert1mMove`**-event de **eerste echte productnotificatie** in V2: compacte **titel + body** via bestaande **`ntfy_client::send_notification`**, **zonder** MQTT in deze stap.

\

**Wat levert het op**

\

- **`service_outbound`:** queue-elementen zijn **`OutboundEnvelope`** (`kind` + **`DomainAlert1mMovePayload`**: symbool, richting, prijs, `pct`, `ts_ms`). Alleen **`emit_domain_alert_1m`** mag **`DomainAlert1mMove`** plaatsen; platte **`emit(Event)`** weigert dit kind zonder payload.

\

- **Dispatch:** bij **`DomainAlert1mMove`** — titel `CryptoAlert V2 · 1m UP|DOWN · <symbool>`, body met prijs, 1m-%, en **`ts_ms`**; **`ESP_LOGI`** op **`svc_out`** voor queue + resultaat van **`ntfy_client`**.

\

- **`alert_engine`:** vult payload uit **`domain_metrics::Metric1mMovePct`** en **`market_data::snapshot().market_label`**; geen NTFY-strings in **`ntfy_client`** zelf buiten transport.

\

**Bewust niet in M-011b:** MQTT-mapping; HA discovery; brede payload-architectuur; alert-UI; multi-timeframe; V1-pariteit.

\

---

\

## M-012a — Eerste `mqtt_bridge` achter `service_outbound` (uitgevoerd)

\

**Doel:** migratiematrix **M-012** — eerste **MQTT**-publish (één topic, platte payload), **zonder** Home Assistant discovery of entity-model.

\

**Wat levert het op**

\

- **Component `mqtt_bridge`:** `init()` start `esp_mqtt_client` (URI + optionele user/pass uit Kconfig); `request_application_ready_publish()` zet een **pending** publish (`online`, QoS 1) naar **`MQTT_TOPIC_BOOT`** zodra de client **verbonden** is (of direct als al verbonden).

\

- **Kconfig:** `CONFIG_MQTT_BRIDGE_ENABLE` (default **uit**), `MQTT_BROKER_URI`, `MQTT_BRIDGE_USER`, `MQTT_BRIDGE_PASSWORD`, `MQTT_TOPIC_BOOT`. Lege URI ⇒ geen client.

\

- **Mapping:** zelfde **`ApplicationReady`** als M-011a — geen MQTT in `ui` / `market_data` / `exchange_bitvavo`.

\

- **Logging:** tag **`mqtt_br`**; connect/disconnect en geslaagde publish op **INFO**/waarschuwing bij fout.

\

**Bewust open:** HA discovery; retain/LWT-strategie; brede topic-taxonomie; TLS fine-tuning buiten URI. **Eerste domein-alert op MQTT:** § **M-012b**.

\

---

\

## M-012b — Eerste echte 1m-domeinalert naar MQTT (uitgevoerd)

\

**Doel:** hetzelfde **`DomainAlert1mMove`**-domeinsignaal als M-011b ook **via MQTT** beschikbaar maken — **één topic**, **compacte JSON**, **zonder** Home Assistant discovery of brede topic-architectuur.

\

**Wat levert het op**

\

- **`mqtt_bridge::publish_domain_alert_1m(...)`:** compacte JSON (`symbol`, `dir` UP/DOWN, `price_eur`, `pct_1m`, `ts_ms`) naar **`MQTT_TOPIC_DOMAIN_ALERT_1M`** (default `cryptoalert/v2/alert/1m`), QoS 1, geen retain — alleen als MQTT build+runtime aan en client **verbonden**; anders duidelijke log, geen queue/backlog in M-012b.

\

- **`service_outbound`:** bij **`DomainAlert1mMove`** na NTFY-dispatch de MQTT-publish aanroepen; logs: event ontvangen, publish-aanroep, succes/fout in **`mqtt_br`**.

\

- **Ownership:** `domain_metrics` / `alert_engine` ongewijzigd; **geen** MQTT-logica in `ui`, `market_data`, `exchange_bitvavo`. **`ApplicationReady`** / boot-topic (**M-012a**) ongewijzigd.

\

**Bewust niet in M-012b:** HA discovery; retain/LWT-herontwerp; topic-taxonomie voor andere timeframes; offline-buffering; hot-reconnect voor gemiste alerts; payload-schema versioning.

\

---

\

## M-013a — Minimale read-only WebUI-basis (uitgevoerd)

\

**Doel:** migratiematrix **M-013** — eerste **HTTP**-observatiepunt: **geen** OTA, **geen** dashboard — alleen veilige status; **geen** brede settingspagina.

\

**Wat levert het op**

\

- **Component `webui`:** `esp_http_server` op **`WEBUI_PORT`** (default **8080**, Kconfig **uit**); **GET /** (HTML) en **GET /api/status.json** (JSON): app-**versie** (`esp_app_desc`), **STA-IP** (via `WIFI_STA_DEF`), **`market_data::snapshot()`** (symbool, prijs, geldig, verbinding, tick-bron, `ts_ms`). **Read-only recente 1m-alerts** in JSON + HTML: § **M-013d**.

\

- **`app_core`:** `webui::init()` na `service_outbound::init()`.

\

- **Geen** auth; geen koppeling van HTTP aan `market_data`-transport buiten snapshot; eerste write-pad zit in **§ M-013b** (klein subset).

\

**Bewust open (voor M-013a alleen):** WebSocket push; SPA; brede instellingen-UI. **Write + form:** § **M-013b** / § **M-013c**.

\

---

\

## M-013b — Eerste gecontroleerde WebUI-write naar `config_store` (mqtt/ntfy) (uitgevoerd)

\

**Doel:** na **M-003a** + **M-013a** een **klein, veilig** schrijfpad: **HTTP POST JSON** → **`config_store`** (NVS), **zonder** secrets, **zonder** `webui_enabled` / `webui_port` (lock-out vermijden), **zonder** auth/OTA/brede settings.

\

**Wat levert het op**

\

- **Endpoint:** **`POST /api/services.json`** — body max. ca. **1023** bytes, **JSON-object** met minstens één veld: **`mqtt_enabled`** (bool of 0/1), **`mqtt_broker_uri`** (string), **`ntfy_enabled`**, **`ntfy_topic`**. Ontbrekende velden laten de huidige runtime-waarde staan (**partiële update**).

\

- **Validatie (web + store):** ongeldige JSON, te lange strings, verboden regeleinden in strings, of **`mqtt_enabled: true` zonder niet-lege broker-URI** ⇒ **400** met `{"ok":false,"error":"…"}`. NVS-fout ⇒ **500**.

\

- **`config_store::persist_service_connectivity`:** schrijft alleen NVS-keys **`svc_mqtt_en`**, **`svc_mqtt_uri`**, **`svc_ntfy_en`**, **`svc_ntfy_tp`** en zet **`schema`** op v3; werkt **`g_service_cache`** bij. **`webui_*`** blijven ongewijzigd.

\

- **Runtime-gedrag:** **NTFY** leest `service_runtime()` bij elke push — **nieuw topic** geldt bij **volgende** notificatie. **MQTT-client** is al gestart in `mqtt_bridge::init`; **nieuwe broker-URI** is pas na **herstart** effectief (geen hot-reload in M-013b) — response bevat een korte **`note`**.

\

**Bewust niet in M-013b:** schrijven van **WebUI-poort/toggle**; **tokens/wachtwoorden**; **auth**; **symbol/alerts**; **MQTT hot-reconnect**; brede formulieren/dashboard — **minimaal HTML-form:** § **M-013c**.

\

---

\

## M-013c — Minimaal WebUI-form voor mqtt/ntfy (bovenop POST) (uitgevoerd)

\

**Doel:** de werkende **`POST /api/services.json`** (M-013b) **bruikbaar maken vanuit de browser** op **`GET /`**, zonder nieuwe backend, zonder SPA/auth/OTA.

\

**Wat levert het op**

\

- **Hoofdpagina (`/`):** bestaande statusregels blijven; daaronder een **eenvoudig formulier** met **`mqtt_enabled`**, **`mqtt_broker_uri`**, **`ntfy_enabled`**, **`ntfy_topic`** — waarden uit **`config_store::service_runtime()`**, strings **HTML-geescaped** voor `value=""`.

\

- **Submit:** **minimale inline JavaScript** (`fetch` **POST** JSON naar **`/api/services.json`**, zelfde payload als M-013b); toont **succes** (inclusief server-**`note`**) of **fout** (status + JSON-`error` of ruwe body).

\

- **UX:** korte **hint** op de pagina: MQTT-broker pas na **herstart**; NTFY-topic bij volgende push. Geen tweede view, geen framework.

\

**Bewust niet in M-013c:** auth; secrets; **webui-poort/toggle**; symbol/alerts; dashboard; WS/SPA; service-API’s buiten `/api/services.json` (OTA: § **M-014a**).

\

---

\

## M-013d — Minimale alert-observability in WebUI (read-only) (uitgevoerd)

\

**Doel:** na **M-010**–**M-012** de **end-to-end 1m-domeinalert** lokaal **zichtbaar** maken voor tuning/validatie — **read-only**, **in RAM**, **zonder** dashboard, NVS-historiek of alert-instellingen-UI.

\

**Wat levert het op**

\

- **Component `alert_observability`:** ringbuffer (**max. 8**) met velden **symbool, richting (UP/DOWN), prijs EUR, 1m %, ts_ms**; schrijven uitsluitend vanuit **`service_outbound`** bij dispatch van **`DomainAlert1mMove`** (zelfde moment als NTFY/MQTT-transport); **mutex** voor veilige HTTP-lees.

\

- **`webui`:** **`GET /api/status.json`** bevat **`alerts_1m`**: `{ "count", "items": [ … ] }` (nieuwste eerst); **`GET /`** toont een compacte **HTML-sectie** met dezelfde feiten. Geen nieuw productdomein; geen koppeling aan **`market_data`**-transport of **`mqtt_bridge`**.

\

- **Ownership:** **`alert_engine`** blijft beslissen; **`service_outbound`** transporteert én triggert observability-record; **`ui`** / **`exchange_bitvavo`** ongewijzigd.

\

**Bewust niet in M-013d:** tweede timeframe; filter/sort UI; acknowledge/silence; auth; SPA/WebSocket-push; persistente historiek in flash/NVS; analytics/statistieken.

\

---

\

## M-013e — Read-only WebUI-observability: regime, volatiliteit, effectieve drempels (uitgevoerd)

\

**Doel:** na **M-010f** zijn **drempels adaptief** — dat verbetert gedrag maar vermindert **transparantie**. **M-013e** maakt de **huidige** alert-intelligentie **lokaal zichtbaar** (read-only), **zonder** settings-write of tuning-UI — **geen** nieuwe domeinlogica.

\

**Wat levert het op**

\

- **`alert_engine`:** struct **`RegimeObservabilitySnapshot`** + **`get_regime_observability_snapshot`** — bijgewerkt aan het begin van elke **`tick()`** (zelfde waarden als de beslissingen in die slag): **regime** (`calm` / `normal` / `hot`), **vol-metric** (`vol_mean_abs_step_bps`, `vol_pairs_used`, `vol_metric_ready`, `vol_unavailable_fallback`), **basis-** en **effectieve** drempels 1m/5m; **confluence** expliciet als **`confluence_effective_gate_pct`** (zelfde getallen als effectieve 1m/5m — beide benen nodig).

\

- **`webui`:** **`GET /api/status.json`** — object **`regime_observability`** (stabiele veldnamen); **`GET /`** — compact **HTML-blok** (M-013e). **`domain_metrics`** levert alleen de vol-berekening; **`market_data` / `exchange_bitvavo`** ongewijzigd.

\

**Bewust niet in M-013e:** write/tuning voor regime of alerts (**eerste write:** § **M-013f**); auth; dashboard/grafieken; SPA/WebSocket-push; uitgebreide historiek-analytics; regime-engine-uitbreiding; extra timeframe; HA-discovery-uitbreiding.

\

---

\

## M-013f — Eerste gecontroleerde WebUI-write: alert/regime runtime-subset (M-003b) (uitgevoerd)

\

**Doel:** na **M-003b** + **M-013e** een **klein, veilig** schrijfpad voor **exact dezelfde vier velden** als M-003b — **HTTP POST JSON** → **`persist_alert_runtime`** — **zonder** HTML-form in deze stap, **zonder** auth en **zonder** scope-uitbreiding.

\

**Wat levert het op**

\

- **`webui`:** **`POST /api/alert-runtime.json`** — body moet **exact vier** sleutels bevatten: `threshold_1m_bps`, `threshold_5m_bps`, `regime_calm_scale_permille`, `regime_hot_scale_permille` (alleen **niet-negatieve gehele getallen**); bereik gehandhaafd door **`config_store::persist_alert_runtime`**. Bij fout: **400** met compact `{"ok":false,"error":"…"}`; bij succes: **200** met `ok`, `alert_runtime_config` (echo), `note`. **`g_alert_cache`** bijgewerkt — **`alert_engine`** gebruikt nieuwe waarden **zonder herstart** (volgende `tick()`).

\

- **Observability:** ongewijzigd — **`GET /api/status.json`** `alert_runtime_config` toont na write de nieuwe waarden.

\

**Bewust niet in M-013f:** HTML-form (**volgt § M-013g**); auth; secrets; cooldown/suppressie/confluence-policy-write; per-symbol tuning; dashboard; tweede view; V1-migratie.

\

---

\

## M-013g — Minimaal WebUI-form op `/` voor alert-runtime (M-003b/M-013f) (uitgevoerd)

\

**Doel:** na **M-013f** is de **POST**-route er, maar nog niet **bruikbaar vanuit de browser** zonder handmatige `curl`. **M-013g** voegt een **kleine sectie** op **`GET /`** toe met **exact vier velden**, zelfde namen/waarden als M-003b — **geen** nieuwe API.

\

**Wat levert het op**

\

- **`webui`:** op de **hoofdpagina** een **formulier** (M-013g) met huidige waarden uit **`config_store::alert_runtime()`**; submit → **`POST /api/alert-runtime.json`** (inline JS, zelfde patroon als services); succes/fout in **`#alert-runtime-msg`**; korte toelichting **volgende tick / geen herstart**.

\

**Bewust niet in M-013g:** auth; extra velden; cooldown/suppressie/confluence; per-symbol; dashboard; tweede view; framework/SPA; V1-migratie.

\

---

\

## M-013h — Read-only WebUI-observability voor alertbeslissingen en suppressieredenen (uitgevoerd)

\

**Doel:** na **M-013g** (tuning) is de volgende **kwaliteitswinst** **transparantie**: waarom **1m** / **5m** / **confluence** wel of niet **firen** — **zonder** nieuwe settingslaag, **zonder** beslislogica in de WebUI.

\

**Wat levert het op**

\

- **`alert_engine`:** compacte **snapshot** per tick (`AlertDecisionObservabilitySnapshot`): per pad **status** (`not_ready`, `below_threshold`, `cooldown`, `suppressed`, `fired`, `invalid`), korte **reason**-code waar nodig (`metrics_not_ready`, `direction_mismatch`, `confluence_priority_window`, …), **resterende** cooldown/suppressie in **ms** (`-1` = n.v.t.); bijgewerkt aan het **einde** van `tick()` na dezelfde voorwaarden als de emit-paden.
- **`webui`:** veld **`alert_decision_observability`** in **`GET /api/status.json`** (keys **`1m`**, **`5m`**, **`confluence_1m5m`**) + compact **HTML-blok** op **`GET /`**.

\

**Bewust niet in M-013h:** auth; write voor cooldown/suppressie (**eerste write:** § **M-013i**); confluence-policy-matrix-write; per-symbol; **persistente** decision-history; dashboard/grafieken; tweede view; V1-migratie; duplicatie van alert-logica in JS/HTML.

\

---

\

## M-013i — Eerste gecontroleerde WebUI-write: alert-policy timing (M-003c) (uitgevoerd)

\

**Doel:** **`alert_policy_timing`** was read-only in status; **M-013i** voegt een **kleine POST** toe — **exact** de vier velden uit **M-003c**; **HTML-form op `/`:** § **M-013j**.

\

**Wat levert het op**

\

- **`webui`:** **`POST /api/alert-policy-timing.json`** (JSON-object met precies **`cooldown_1m_s`**, **`cooldown_5m_s`**, **`cooldown_conf_1m5m_s`**, **`suppress_loose_after_conf_s`**) → **`config_store::persist_alert_policy_timing`** — validatie en NVS (`altp_*`), bij fout **400** met compacte JSON-fout.

\

- **`config_store`:** cache-update op succes; **`GET /api/status.json`** toont direct de nieuwe waarden; **`alert_engine`** gebruikt ze **vanaf de volgende tick** (geen herstart).

\

**Bewust niet in M-013i:** HTML-form (**volgt § M-013j**); auth; secrets; threshold/regime-write (blijft **M-013f**); confluence-policy-matrix; per-symbol; dashboard; tweede view; V1-migratie.

\

---

\

## M-013j — Minimaal WebUI-form op `/` voor alert-policy timing (M-003c/M-013i) (uitgevoerd)

\

**Doel:** na **M-013i** is de **POST**-route er, maar nog niet **bruikbaar vanuit de browser** zonder handmatige `curl`. **M-013j** voegt een **kleine sectie** op **`GET /`** toe met **exact vier velden**, zelfde namen/waarden als M-003c — **geen** nieuwe API.

\

**Wat levert het op**

\

- **`webui`:** op de **hoofdpagina** een **formulier** (M-013j) met huidige waarden uit **`config_store::alert_policy_timing()`**; submit → **`POST /api/alert-policy-timing.json`** (inline JS, zelfde patroon als M-013g); succes/fout in **`#alert-policy-msg`**; korte toelichting **volgende tick / geen herstart**.

\

**Bewust niet in M-013j:** auth; extra velden; threshold/regime; confluence-matrix; per-symbol; dashboard; tweede view; framework/SPA; V1-migratie.

\

---

\

## M-013k — Eerste gecontroleerde WebUI-write: confluence-policy (M-003d) (uitgevoerd)

\

**Doel:** na **M-003d** is **`alert_confluence_policy`** zichtbaar in **`GET /api/status.json`**, maar nog niet wijzigbaar via HTTP. **M-013k** voegt **één POST** toe — **exact** de vier booleans uit **M-003d** — **zonder** HTML-form in deze stap, **zonder** auth en **zonder** scope-uitbreiding.

\

**Wat levert het op**

\

- **`POST /api/alert-confluence-policy.json`:** JSON-body = **exact vier sleutels** (`confluence_enabled`, `confluence_require_same_direction`, `confluence_require_both_thresholds`, `confluence_emit_loose_alerts_when_conf_fails`); waarden = JSON-boolean of getal (0 = onwaar, anders waar), zelfde patroon als **`cjson_to_bool`** elders in `webui`.

\

- **`config_store::persist_alert_confluence_policy`:** schrijft **`altcf_*`**, zet **schema v6**, werkt **`g_conf_policy_cache`** bij — **`alert_engine`** leest de volgende **`tick()`** zonder herstart.

\

- **Fouten:** compact **`400`** JSON bij parse-/sleutel-/typefouten; **`500`** bij NVS-falen.

\

- **`webui`:** hintregel op **`/`** vermeldt de POST-route; **minimaal HTML-form:** § **M-013l**.

\

**Bewust niet in M-013k:** HTML-form op **`/`** (**volgt § M-013l**); auth; secrets; brede confluence-matrix; extra drempels (**M-003b**) of timing (**M-003c**); per-symbol; tweede view; V1-migratie.

\

---

\

## M-013l — Minimaal WebUI-form op `/` voor confluence-policy (M-003d/M-013k) (uitgevoerd)

\

**Doel:** **M-013k** levert de **POST** voor dezelfde vier booleans als **M-003d**, maar nog geen **browserformulier**. **M-013l** sluit de **WebUI/config-lijn** op dit capability-niveau af: één **kleine sectie** op **`GET /`**, **zelfde velden** en **zelfde endpoint** (`POST /api/alert-confluence-policy.json`) — **zonder** nieuwe backendroute, **zonder** auth en **zonder** scope-uitbreiding. **Volgende primaire focus** verschuift naar de **M-002 hardening-batch** (§ **9a** werkpakket 2), **niet** naar verdere config-microstappen.

\

**Wat levert het op**

\

- **`webui`:** op de **hoofdpagina** een **formulier** met checkboxes voor `confluence_enabled`, `confluence_require_same_direction`, `confluence_require_both_thresholds`, `confluence_emit_loose_alerts_when_conf_fails`; huidige waarden uit **`config_store::alert_confluence_policy()`**; submit → **`POST /api/alert-confluence-policy.json`** (inline JS, zelfde patroon als M-013g/j); succes/fout in **`#alert-conf-msg`**; toelichting **volgende tick / geen herstart**.

\

**Bewust niet in M-013l:** auth; secrets; extra velden; tweede view; dashboard; nieuwe configuratiedomeinen; **M-002**-uitbreidingen in deze stap.

\

---

\

## M-014a — Minimale OTA-basis via WebUI (upload → next slot) (uitgevoerd)

\

**Doel (migratiematrix **M-014**, technische basis):** met **bestaande dual-OTA-partitietabel** (`partitions_v2_16mb_ota.csv`, 3 MiB/slot) een **kleine, gecontroleerde** upload-flow: **HTTP POST** met **ruwe firmware-binary** → **`ota_service`** → **`esp_ota_*`** — **zonder** auth, **zonder** signing, **zonder** voortgangs-UX.

\

**Wat levert het op**

\

- **Component `ota_service`:** `init()` logt **running** / **next** OTA-partitie; **`handle_firmware_upload(httpd_req_t *)`** accepteert alleen **`Content-Type: application/octet-stream`** + **`Content-Length`**, streamt naar de **niet-actieve** slot (**`esp_ota_begin` / `write` / `end`**), zet **boot-partitie** bij succes, stuurt **JSON**, **`esp_restart()`** na korte delay. **Mislukt** blijft het **lopende** image ongewijzigd.

\

- **WebUI:** **`POST /api/ota`** geregistreerd; op **`/`** een **minimale sectie** (bestand kiezen + knop) die **`fetch`** met **ruwe body** gebruikt — geen multipart-parser op het apparaat.

\

- **Grenzen:** max. imagegrootte **3 MiB** (partition size); minimum **1 KiB**; validatie via **IDF** (`esp_ota_end`). **Geen** MQTT-OTA, **geen** rollback-policy in firmware.

\

**Bewust niet in M-014a:** login; **cryptografische handtekening**; voortgangsbalk; automatische versiecheck; uitgewerkt rollback/anti-brick-beleid; OTA via MQTT; tweede UI-view — **observability/post-boot:** § **M-014b**.

\

---

\

## M-014b — OTA observability + post-boot bevestiging (M-014a vervolg) (uitgevoerd)

\

**Doel:** na werkende **M-014a**-upload de OTA-keten **transparanter** maken (slots, image-state, reset-reden) en een **netjes gecontroleerde** post-boot stap (`esp_ota_mark_app_valid_cancel_rollback` wanneer IDF dat ondersteunt) — **zonder** signing, **zonder** volledige rollback-architectuur, **zonder** extra UX-laag.

\

**Wat levert het op**

\

- **`ota_service`:** uitgebreide **boot-log** (running/next partitie, `esp_ota_get_state_partition`, `esp_reset_reason`); daarna **`esp_ota_mark_app_valid_cancel_rollback()`** — bij succes **`marked_valid`** in snapshot; bij `ESP_ERR_NOT_SUPPORTED` (rollback uit in build) **`rollback_disabled`** met duidelijke log.

\

- **`get_status_snapshot`:** levert o.a. **`running_*`**, **`next_update_*`**, **`img_state`**, **`boot_confirm`**, **`reset_reason`** voor WebUI/JSON.

\

- **WebUI:** **`/api/status.json`** bevat een **`ota`-object** met bovenstaande velden; **`/`** toont een compact **OTA/boot-blok** + bestaande **versie**-regel. Geen tweede pagina.

\

**Bewust open (niet M-014b):** cryptografische verificatie; online versiecheck; MQTT-OTA; uitgewerkte rollback-policy; progress-UI; auth.

\

---

\

## M-010a — Minimale `domain_metrics` + `alert_engine` vertical slice (1m-move) (uitgevoerd)

\

**Doel (migratiematrix **M-010**, eerste productkern):** na stabiele service-/beheerbasis (**M-003a**, **M-013**–**M-014**) de **kern van Crypto Alert** weer zichtbaar maken: **rolling prijsbuffer** + **1m %-move** + **drempel + cooldown** + **traceerbare logs** + **minimaal outbound-event** — **zonder** V1-pariteit, regimes, anchors of brede NTFY/MQTT-mapping.

\

**Wat levert het op**

\

- **`domain_metrics`:** vaste **ringbuffer** (tijd + EUR-prijs) gevoed vanuit **`market_data::snapshot`**; **`compute_1m_move_pct()`** berekent een **signed %** t.o.v. de **nieuwste sample ≤ now−60s** (geen interpolatie). **`feed`** negeert ongeldige ticks; tijdstempel valt terug op **`esp_timer`** als de tick geen `ts_ms` heeft.

\

- **`alert_engine`:** Kconfig-**drempel** (`ALERT_ENGINE_1M_THRESHOLD_BPS`, default 16 = **0,16%**) en **cooldown** (`ALERT_ENGINE_1M_COOLDOWN_S`); bij overschrijding van **|1m%|**: **`ESP_LOGI`** met richting **UP/DOWN**, daarna **`service_outbound::emit_domain_alert_1m(...)`** (symbool uit `market_data::snapshot`, metric uit `domain_metrics`) — **NTFY-koppeling:** § **M-011b**.

\

- **`service_outbound`:** event **`DomainAlert1mMove`** met **`DomainAlert1mMovePayload`** — **M-011b:** dispatch naar **`ntfy_client`** (geen MQTT in deze mapping).

\

- **`app_core`:** na **`market_data::tick`** → **`domain_metrics::feed`** → **`alert_engine::tick`** → UI; **`poll()`** blijft outbound leegmaken.

\

**Bewust niet in M-010a (NTFY-mapping volgt):** V1-alertpariteit; **confluence/regime/anchor**-logica; multi-timeframe; grafieken/dashboard; tweede UI-view; **MQTT** bij domeinalerts; symbol-/alert-settings-UI — **eerste NTFY domeinalert:** § **M-011b**.

\

---

\

## M-010b — Per-seconde canonicalisatie voor `domain_metrics` (WS-input normaliseren) (uitgevoerd)

\

**Doel (M-010 vervolg):** hoge-frequentie tick-instroom niet direct als ruwe punten in de metricbuffer gebruiken, maar eerst terugbrengen naar **één representatieve secondewaarde** zodat de 1m-move robuuster/leesbaarder wordt zonder extra besliscomplexiteit.

\

**Wat levert het op**

\

- **`domain_metrics` canonicalisatie:** `feed(snapshot)` bouwt nu een **per-seconde bucket** (`sec_epoch`, `tick_count`, `sum_price`, `open`, `close`). Bij seconde-overgang wordt de vorige seconde gefinaliseerd naar een **canonical prijs** via eenvoudige **TWAP-achtige mean** (`sum / count`) en pas dan in de rolling ringbuffer gezet.

\

- **Tracebaarheid:** logregels tonen expliciet:
  - **aantal ticks in die seconde** (`ticks=...`)
  - gekozen **canonical waarde** (`canonical=...`)
  - **open/close** binnen die seconde
  - opbouw van de **1m-metric** (`now/ref/pct`) op basis van deze canonical samples.

\

- **`alert_engine` ongewijzigd in concept:** zelfde 1m%-move + drempel + cooldown; alleen invoerkwaliteit van `domain_metrics` is verbeterd.

\

**Bewust niet in M-010b:** geavanceerde statistische filters; multi-timeframe; regime/confluence/anchor-logica; extra outbound payloads; settings-UI voor tuning; dashboard/grafieken.

\

---

\

## M-010c — Tweede domeinmetric: 5m-move vertical slice (parallel aan 1m) (uitgevoerd)

\

**Doel:** functionele **breedte**: een **tweede eenvoudig timeframe** (5m) naast de bestaande **1m-keten**, op dezelfde **gecanonicaliseerde secondes** — **zonder** confluence, regime-engine of gedeelde beslislaag.

\

**Wat levert het op**

\

- **`domain_metrics`:** ringbuffer-capaciteit en prune-horizon voldoende voor **≥5 min** historie; **`compute_5m_move_pct()`** — signed % t.o.v. **nieuwste sample ≤ now−300s** (zelfde patroon als 1m). **1m blijft ongewijzigd in gedrag.**

\

- **`alert_engine`:** eigen Kconfig-**drempel** (`ALERT_ENGINE_5M_THRESHOLD_BPS`, default **32** = **0,32%**) en **cooldown** (`ALERT_ENGINE_5M_COOLDOWN_S`, default **300** s); eigen **`s_last_fire_5m`**; bij trigger **`emit_domain_alert_5m`** met duidelijke **UP/DOWN**-logs.

\

- **`service_outbound`:** nieuw event **`DomainAlert5mMove`** + **`DomainAlert5mMovePayload`** (`pct_5m`); dispatch parallel: **NTFY**, **MQTT** (`MQTT_TOPIC_DOMAIN_ALERT_5M`), **WebUI-observability** (`alerts_5m` + HTML-sectie).

\

**Bewust niet in M-010c:** kruising **1m∩5m** (volgt **§ M-010d**); scores; anchors/TP/loss; alert-settings-UI; HA-discovery-uitbreiding; extra views/dashboards.

\

---

\

## M-010d — Eerste eenvoudige confluence-regel (1m + 5m) (uitgevoerd)

\

**Doel:** na **M-010c** een **minimale samenhang** tussen timeframes: alleen wanneer **1m** en **5m** **beide geldig** zijn, **dezelfde richting** hebben, en **beide** hun **eigen drempel** halen — **zonder** losse 1m/5m-alerts te vervangen of complexe suppressie.

\

**Wat levert het op**

\

- **`alert_engine`:** eigen **`ALERT_ENGINE_CONF_1M5M_COOLDOWN_S`** (default **600** s); bij vuur **`emit_domain_confluence_1m5m`** met **pct_1m**, **pct_5m**, prijs, **ts_ms**; logs bij **tegenstrijdige richting** (INFO) en bij **vuur** (INFO); geen spam bij normaal «onder drempel».

\

- **`service_outbound`:** event **`DomainAlertConfluence1m5m`** + payload; **NTFY**, **MQTT** (`MQTT_TOPIC_DOMAIN_ALERT_CONF_1M5M`), **WebUI** (`alerts_conf_1m5m` + HTML-blok).

\

**Bewust niet in M-010d:** regime-engine; scoremodel; multi-factor confluence; prioriteitsmatrix; suppressie van 1m/5m (**eerste stap:** § **M-010e**); V1-pariteit; settings-UI.

\

---

\

## M-010e — Eenvoudige prioriteit en suppressie (confluence vs. losse 1m/5m) (uitgevoerd)

\

**Doel:** na **M-010d** **dubbelmeldingen beperken** — confluence heeft **voorrang**; losse 1m- en 5m-alerts blijven bestaan maar worden **kort** onderdrukt wanneer ze dezelfde richting zouden herhalen als een net gevoerde confluence — **zonder** brede policylaag of multi-factor matrix.

\

**Wat levert het op**

\

- **`alert_engine`:** evaluatieorde **confluence vóór** losse 1m/5m; bij confluence-fire **`ALERT_ENGINE_CONF_SUPPRESS_LOOSE_S`** (default **8** s): geen losse **DomainAlert1mMove** / **DomainAlert5mMove** met **dezelfde richting** als die confluence (tegenovergestelde richting binnen het venster blijft mogelijk). Logs: **FIRE** (confluence), **suppressed** met reden (`reason=same_dir_as_conf`, `rem_ms`), en init met suppress-duur; max. **één suppress-log per TF per venster** (geen log-spam iedere tick).

\

- **Outbound ongewijzigd in vorm:** **NTFY** / **MQTT** / **WebUI** blijven dezelfde paden; alleen **minder** dubbele losse events na confluence.

\

**Bewust niet in M-010e:** regime-engine; scoremodel; multi-factor prioriteitsmatrix; lange dedup-horizon; settings-UI; V1-pariteit; extra UI-views; suppressie op basis van symbolen of orderboek.

\

---

\

## M-010f — Mini-regime (calm / normal / hot) uit volatiliteit, zonder volledige regime-engine (uitgevoerd)

\

**Doel:** na **M-010a–e** een **uitlegbare** eerste stap naar «marktregime» — alleen op basis van een **korte-horizon vol-proxy** op de bestaande canonieke prijsring — **zonder** V1-achtige regime-engine, scoremodel of extra timeframes.

\

**Wat levert het op**

\

- **`domain_metrics`:** **`compute_vol_mean_abs_step_bps()`** — gemiddelde **|Δprijs/prijs|×10⁴** (bps) over opeenvolgende **~1s**-canonieke samples binnen venster (**`ALERT_REGIME_VOL_*`**).

\

- **`alert_engine`:** drie regimes — **calm** (lage gem. stap), **normal** (tussengrenzen), **hot** (hoge gem. stap); **één toepassing:** **effectieve 1m- en 5m-drempels** (en daarmee confluence-drempels) worden met **‰-schaal** vermenigvuldigd (calm iets **lager** / hot iets **hoger**); **cooldowns** en **M-010e-suppressie** ongewijzigd. Logs: **vol** + **regime** + **schaal** + **eff_thr** bij eerste vol-klaar en bij **regimewissel**; eenmalige melding bij vol-warmup.

\

**Bewust niet in M-010f:** volledige regime-engine; scoremodel; V1-pariteit; anchors/TP/loss; extra TF; alert-settings-UI / WebUI-tuning; dashboard/grafieken; HA-discovery; cooldown/suppressie meeschalen (bewust alleen drempels).

\

---

\

## Stap 8b — Minimale UI-verfijning live view (S3-GEEK) (uitgevoerd)

\

**Doel:** zelfde databron (`market_data::snapshot()`), rustigere product-UI — **geen** dashboard, tweede scherm of nieuwe velden.

\

**Uitgevoerd in `ui`**

\

- **Hiërarchie:** symbool bovenin (muted koel grijs, gecentreerd, vaste breedte i.v.m. smalle 135 px; lange labels met **dots** i.p.v. aflopen buiten het scherm); **prijs** dominant in het midden; **EUR** er direct onder in hetzelfde blok; **bron** onderaan als `Bron · WS` / `REST` / `—`.

\

- **Layout / compositie:** prijs + EUR staan in één **flex-column** (transparante container, `pad_row` 5) i.p.v. losse `align_to` op het scherm — rustiger verticaal ritme en één optische eenheid; het blok licht **omhoog** gezet (`CENTER`, y≈−4) t.o.v. symbool en bron.

\

- **Typografie / kleur (zelfde default font):** prijs iets helderder (`#F4FAF8`), iets meer **letter-spacing** op het bedrag; EUR zachter mint (`#86EFAC`, opacity ~70%); bronregel donkerder grijs, lagere opacity, lichte letter-spacing; scherm nog `#0B0D0C`, iets ruimere padding (top/bottom).

\

- **Bewust níet in deze stap:** grotere LVGL-fonts (extra `CONFIG_LV_FONT_*`), tweede view/thema, grafieken, netwerkstatus in UI, extra snapshot-velden, animaties.

\

- **Onveranderd:** `app_core`, `display_port`, `market_data`-API; geen netwerk-/exchange-logica in `ui`.

\

---

\

## Display-diagnose GEEK — LVGL `swap_bytes` / SPI-klok (klein, revertible)

\

**Doel:** wazige LVGL-tekst of kleurverschuiving **A/B** onderzoeken zonder pin-/architectuurwijziging; vergelijk met V1-pinmapping (die inhoudelijk gelijk blijft aan `geek_pins.hpp`).

\

- **Kconfig** (`idf.py menuconfig` → *ESP32 Crypto Alert V2*): **Display: A/B diagnoseprofiel** (0–3) zet afgeleid `swap_bytes`, `rgb_order` en SPI-klok. **Default voor S3-GEEK:** `CONFIG_DISPLAY_DIAG_PROFILE_SWAP_OFF` (swap_bytes=0, RGB, 27 MHz). Na profielwissel: **`sdkconfig` verwijderen** en opnieuw `set-target` + build (zie `GEEK_DISPLAY_DIAG.md`). In `sdkconfig.defaults` alleen `CONFIG_DISPLAY_DIAG_PROFILE_*`.

\

- **Documentatie / procedure:** [firmware-v2/docs/display/GEEK_DISPLAY_DIAG.md](../docs/display/GEEK_DISPLAY_DIAG.md).

\

- **Logs:** `display_port` logt **pclk**; `ui` logt **swap_bytes** bij LVGL-init.

\

**Conclusie na hardware:** invullen in dat document (hypothese A offset/gap, B LVGL byte order, C SPI-timing).

\

\
