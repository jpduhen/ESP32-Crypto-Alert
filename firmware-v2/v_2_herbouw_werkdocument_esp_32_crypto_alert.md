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

**Actiepunten:** zie §10 (prioriteit na skeleton).

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

**Fase:** eerste **ESP-IDF skeletonfase** is **afgerond** in `firmware-v2/` (branch **`v2/foundation`**).

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

* basis display bring-up op de S3-GEEK is afgerond; volgende displaystap is pas later de keuze hoe en wanneer **LVGL** op deze route landt

* **T-103 softwarebasis** is neergezet (`exchange_bitvavo`, `net_runtime`, façade `market_data`); **field-test** op GEEK met WiFi + echte feed blijft gebruikerscheck

* **M-002** verder uitwerken (uitbreiding `net_runtime` / backoff / scheiding services) na eerste field-ervaring

* migratiematrix blijft **verfijnen** (geen tegenstrijdigheid met §11 bedoeld)

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

## 10. Prioriteitenlijst

\

### Prioriteit nu (na T-103 softwarebasis)

\

1. V1 **formeel bevriezen** als referentie (geen grote functionele uitbreidingen zonder besluit).

2. **Field-test T-103** op GEEK: WiFi via **onboarding (`CryptoAlert`)** of NVS, REST + WS tegen Bitvavo; logs/heartbeat controleren.

3. **M-002** verdiepen: backoff, eventuele task-queue, scheiding MQTT/NTFY/WebUI wanneer die tickets starten.

4. Voorbereiden van de volgende display/UI-beslissing: wanneer en hoe **LVGL** op de bevestigde `esp_lcd`-route landt.

5. **Migratiematrix** blijven aanscherpen.

6. Dit **werkdocument** bij elke mijlpaal bijwerken (§0 leidend).

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

De onderstaande componenten zijn als **skeleton** aanwezig in `firmware-v2/` (tickets S-001 … S-007; zie §9). Volgende stap: echte displaystack en exchange-laag, niet opnieuw dit blok “herdefiniëren”.

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

\

### Beheersmaatregelen

\

* kleine iteraties

* heldere besluitenlog

* vaste prioriteitenlijst

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

**Stap 1 (lopend):** dit werkdocument (§0) als leidende referentie houden; `docs/architecture/V2_WORKDOCUMENT_MASTER.md` alleen voor governance-overzicht — bij verschil eerst hier bijwerken.

\

**Stap 2 — klaar (T-101):** reproduceerbare **build**: ESP-IDF **v5.4.2**, `BUILD.md`, CI-smoke workflow.

\

**Stap 3 (T-102):** **display bring-up** op GEEK met expliciete fasepoort:

\

* displayroute / ADR kiezen en vastleggen ✅

* backlight + basis-init valideren ✅

* zichtbare output op echt scherm aantonen ✅

* build/CI groen houden ✅

* **LVGL** alleen meenemen na expliciet en onderbouwd besluit ✅

\

**Stap 4 — klaar:** **D-007** bevestigd op hardware; displaygeometrie getuned (**T-102**).

\

**Stap 5 — uitgevoerd (T-103):** `exchange_bitvavo` + `net_runtime` + façade `market_data` (**D-008**, **ADR-002**). Mock via `CONFIG_MD_USE_EXCHANGE_BITVAVO=n`.

\

**Stap 5b — uitgevoerd (WiFi onboarding):** `wifi_onboarding` + NVS-credentials (**D-009**, **ADR-003**); SoftAP **`CryptoAlert`**, portal `[http://192.168.4.1/`](http://192.168.4.1/`).

\

**Stap 6:** **M-002** verder uitwerken (backoff, queues, MQTT/NTFY/WebUI-scheiding) — niet blokkerend voor kleine V2-stappen.

\

**Stap 7 — uitgevoerd (T-103d):** **LVGL** op de `esp_lcd`-route (**ADR-004**); eerste echte UI: symbool + prijs + bron via `market_data::snapshot()` (geen exchange in `ui`).

\

**Stap 8 — deels uitgevoerd (M-002a):** STA **backoff** in `net_runtime` + disconnect-reason logging; zie § **M-002a**. Nog open: queues, MQTT/NTFY/WebUI-scheiding, worker-task.

\

**Stap 8b — uitgevoerd:** minimale verfijning van de bestaande live view (hiërarchie, spacing, bronregel); zie § **Stap 8b**. **Volgende kleine UI-stap (optioneel):** tweede view / thema — buiten deze scope tot besluit.

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

**Backlog (ongewijzigd t.o.v. M-002a):** MQTT/NTFY/WebUI achter mutex/queue; optionele net-worker-task — zie [M002_NETWORK_BOUNDARIES.md](../docs/architecture/M002_NETWORK_BOUNDARIES.md). **REST-HTTP hergebruik (Bitvavo):** § **M-002b**. **Outbound servicegrens (stub):** § **M-002c**.

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

## M-002c — Minimale servicegrens outbound events (stub) (uitgevoerd)

\

**Doel:** één neutrale **event-grens** voor toekomstige outbound acties (MQTT publish, NTFY, WebUI-push) **zonder** transports of queues te bouwen — alleen contract + stub dispatcher.

\

**Wat is aangescherpt**

\

- **Component `service_outbound`:** types `service_outbound::Event` (o.a. `ApplicationReady`) en API `init()` / `emit()` — geen protocolstrings omhoog vanuit exchange; **UI** en **display_port** blijven onwetend.

\

- **`app_core` orkestreert:** na `market_data::init` volgt `service_outbound::init`; vóór de hoofdlus één keer `emit(ApplicationReady)` — **stub** logt onder tag **`svc_out`** (geen netwerk, geen side effects).

\

- **Ownership:** `market_data` blijft façade; `exchange_bitvavo` blijft transport; toekomstige echte services kunnen achter `emit` of een latere interne sink-hook aansluiten zonder architectuur opnieuw open te breken.

\

**Bewust open:** echte MQTT-/NTFY-clients; WebUI; FreeRTOS-queues; worker-task; uitbreiding van `Event` met payloads zodra een transport kiest.

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
