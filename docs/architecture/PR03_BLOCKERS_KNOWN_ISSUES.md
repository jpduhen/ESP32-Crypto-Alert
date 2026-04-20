# PR-03 — Blockers, known issues en go/no-go (V2)

**Status:** register **vastgelegd** (beslislaag tussen testruns en RC).  
**Branch / board:** `v2/foundation` · **ESP32-S3 GEEK** (referentie eerste serieuze V2-release, [PR01 §2](PR01_RELEASE_READINESS_BASELINE.md#2-eerste-serieuze-v2-release--concreet-doel-korte-termijn)).  
**Bronnen:** [PR01_RELEASE_READINESS_BASELINE.md](PR01_RELEASE_READINESS_BASELINE.md), [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md) (o.a. **PR02-RUN-001**), [werkdocument §9a/§13](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md).

**Doel PR-03:** van losse bevindingen naar **geclassificeerde** open punten met **owner**, **exit-criterium** en **go/no-go regels** richting **PR-04** (RC).

---

## 1. Waarom PR-03 nu de juiste stap

PR-01 geeft **definities** (readiness-niveaus, known-issue-structuur); PR-02 levert **meetbare** TM-resultaten (**PR02-RUN-001**). Zonder PR-03 blijft “wat mag er onder welke voorwaarden uit?” **impliciet**. PR-03 dwingt **P0–P3-classificatie**, **release-impact** en **exit-criteria** af — nodig om een **RC** ([PR01 §4](PR01_RELEASE_READINESS_BASELINE.md#4-drie-readiness-niveaus-verplicht-onderscheid)) formeel te kunnen voorstellen.

---

## 2. Ernstniveaus (blockers / known issues)

Afstemming: [PR01 §7](PR01_RELEASE_READINESS_BASELINE.md#7-release-blockers-en-known-issues-structuur), uitgewerkt voor besluitvorming.

| Niveau | Naam | Betekenis | Typische release-impact |
|--------|------|-----------|-------------------------|
| **P0** | Blocker | Geen release zonder oplossing of **formeel** goedgekeurde uitzondering met mitigatie. | Stop; RC uitgesteld. |
| **P1** | Hoog | Release alleen met **expliciete** productbeslissing + vermelding in **release notes** + eventueel workaround. | Go alleen met ondertekende acceptatie. |
| **P2** | Medium | Bekend issue; geen automatische stop; plannen of monitoren. | RC mogelijk met lijst in notes. |
| **P3** | Laag | Cosmetisch / rand / defer; documenteren. | Geen blokkade RC. |

**P0 vs P1 (scheiding):** P0 = onaanvaardbaar risico voor **veiligheid**, **brick**, **datalek**, **structurele onbruikbaarheid** van de afgesproken scope. P1 = ernstig maar **acceptabel** onder voorwaarde (bijv. beperkte observability, niet-geteste maar gedocumenteerde upgrade-path).

---

## 3. Open items uit PR02-RUN-001 (register)

Onderstaande rijen zijn **normatief** voor releasebespreking tot ze gesloten zijn of herclassificeerd.

### 3.1 PR02-BF-001 — WebUI `GET /` homepage overflow (TM-07)

| Veld | Inhoud |
|------|--------|
| **ID** | **PR02-BF-001** |
| **Koppeling TM** | **TM-07** — WebUI |
| **Component** | `webui` (`handle_root_html`) |
| **Symptoom** | HTTP **500** `overflow` op **`GET /`** na langere uptime; serial o.a. `M-013c/d: HTML deel2 overflow`; listener op WebUI-poort blijft actief. **`GET /api/status.json`** kan wel werken (ander pad). |
| **Reproduceerbaar** | **Deels** — treedt op wanneer cumulatieve HTML de buffer overschrijdt (historisch 16 KiB); afhankelijk van uptime/gevulde observability-secties. |
| **Workaround** | **Ja** — read-only observability via **`/api/status.json`**; eventueel korte sessie/herstart om HTML kleiner te houden (niet structureel). |
| **Fixstatus (code)** | **Aanwezig op branch:** `k_html_alloc` **32 KiB**, uitgebreide overflow-log, aparte log bij `cJSON_Print`-falen voor status.json ([PR02 §3.1](PR02_TEST_MATRIX.md#31-pr-02--bevinding-pr02-bf-001-webui-homepage-get)). |
| **Classificatie (nu)** | **P1** — primaire read-only **HTML-homepage** was in veldrun **niet betrouwbaar** tot fix; **veld-PASS na soak** nog niet aangetoond in PR02-RUN-001. |
| **Owner** | **Firmware + field** (zelfde persoon/team dat GEEK flasht en PR-02-run uitvoert). |
| **Target fix** | **PR-02-RUN-002** (of latere run): TM-07 **PASS** op build met 32 KiB-fix, of structurele follow-up (PR-04 / volgende minor) indien soak faalt. |
| **Release-impact** | Eerste V2-release op GEEK met **WebUI als menselijke entry** vereist **expliciete** keuze: ship met P1 + notes **tot** TM-07 PASS, of RC uitstellen. |
| **Exit-criterium (sluiten)** | **TM-07 = PASS** op vastgelegde build: `GET /` en **`/api/status.json`** HTTP 200 in **soak** (aanbevolen: ≥ **8 uur** continu, of projectafspraak; minimaal > venster waarin overflow eerder optrad) **zonder** `M-013c/d` overflow-log en **zonder** HTTP 500 op `/`. Artifact: log/screenshot + run-ID in [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md). |

---

### 3.2 PR03-TM08 — OTA niet uitgevoerd (SKIP)

| Veld | Inhoud |
|------|--------|
| **ID** | **PR03-TM08** |
| **Koppeling TM** | **TM-08** — OTA |
| **Component** | `ota_service` / partition layout / WebUI OTA-uploadpad |
| **Symptoom** | Geen bewijs in **PR02-RUN-001** voor (A) succesvolle **OTA-swap** via UI/API of (B) vastgelegd **manual flash**-pad + herhaalde boot-check. |
| **Reproduceerbaar** | **n.v.t.** (test niet uitgevoerd). |
| **Workaround** | **Ja** — **manual `esptool`** naar bekende slot/layout ([PR01 §5](PR01_RELEASE_READINESS_BASELINE.md#5-minimale-testcategorieën)); risico en stappen in release notes. |
| **Classificatie (nu)** | **P1 gap** t.o.v. volledige **dual-slot OTA-fieldproof** — geen P0 zolang **brick-risk** niet aangetoond is en manual path gedocumenteerd blijft. |
| **Owner** | **Field / release** |
| **Target fix** | **PR-04 vooraf** of **PR02-RUN-002**: TM-08 **PASS** (route A of B) **of** formeel **N/A** (alleen lab-flash) met ondertekend risico. |
| **Release-impact** | [PR01 §3](PR01_RELEASE_READINESS_BASELINE.md#3-wat-wél-en-niet-in-scope-van-die-eerste-release) vereist **OTA en partitioning begrepen en gecommit** — dat kan zonder volledige field-OTA als **procedure + risico** vastliggen. |
| **Exit-criterium (sluiten)** | Eén van: **(1)** TM-08 **PASS** met artifact, **(2)** **N/A** met release notes: “OTA field niet getest; upgrade via …” + acceptatie, **(3)** herclassificeer naar P2 na besluit “manual only voor v1.0 release”. |

---

### 3.3 PR03-TM09 — NTFY / MQTT E2E niet uitgevoerd (SKIP)

| Veld | Inhoud |
|------|--------|
| **ID** | **PR03-TM09** |
| **Koppeling TM** | **TM-09** — NTFY / MQTT |
| **Component** | `service_outbound`, NTFY-client, `mqtt_bridge` |
| **Symptoom** | Geen **end-to-end** bewijs (alert → push/MQTT publish) in PR02-RUN-001. |
| **Reproduceerbaar** | **n.v.t.** (test niet uitgevoerd). |
| **Workaround** | **Deels** — logs/`service_outbound`-queues bekijken; C4-semantiek elders getest ([PR01 scope](PR01_RELEASE_READINESS_BASELINE.md#2-eerste-serieuze-v2-release--concreet-doel-korte-termijn) vermeldt NTFY/MQTT). |
| **Classificatie (nu)** | **P1 gap** als **minimaal één kanaal** in release-scope zit — [PR01 §5](PR01_RELEASE_READINESS_BASELINE.md#5-minimale-testcategorieën) (“minimaal één kanaal end-to-end **indien in release-scope**”). |
| **Owner** | **Field + services-config** |
| **Target fix** | **PR02-RUN-002** of vóór PR-04: TM-09 **PASS** **of** scope verkleinen naar **N/A** (geen outbound in build) met risico. |
| **Release-impact** | Zonder TM-09 PASS: release **alleen** als **expliciet** wordt besloten outbound niet te garanderen voor deze tag. |
| **Exit-criterium (sluiten)** | **TM-09 PASS** (artifact: NTFY screenshot of MQTT log) **of** **N/A**: beide services uit in geteste image + risico geaccepteerd in notes. |

---

## 4. Samenvattende classificatie (snapshot)

| ID | Classificatie | Kort |
|----|---------------|------|
| **PR02-BF-001** | **P1** | TM-07; fix in tree; soak-herverificatie vereist om te sluiten. |
| **PR03-TM08** | **P1 gap** | TM-08 SKIP; OTA-field of expliciet manual-only + acceptatie. |
| **PR03-TM09** | **P1 gap** | TM-09 SKIP; E2E services of scope **N/A** + acceptatie. |

**Geen open P0** in deze snapshot op basis van PR02-RUN-001 — wel **drie P1(-gap)** punten die een **bewuste** go/no-go vereisen.

---

## 5. Go / no-go richting PR-04 (RC)

### 5.1 Minimum voor “PR-04 kan starten” (proces)

- Dit document is **ingelezen** en open items zijn **besproken** (owner + exit of acceptatie).
- [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md) bevat minimaal één run na PR02-RUN-001 **of** een **addendum** dat PR03-items zijn geaccepteerd/uitgesteld.

### 5.2 Aanbevolen go voor **interne RC-build** (GEEK, `v2/foundation`)

| Criterium | Vereiste |
|-----------|----------|
| **P0** | **Geen** open P0 zonder goedgekeurde uitzondering. |
| **PR02-BF-001** | **TM-07 PASS** na soak op release-candidate commit **of** **P1 geaccepteerd**: ship met beperkte WebUI (homepage) + duidelijke notes + workaround `status.json`. |
| **PR03-TM08** | TM-08 **PASS** **of** **N/A** met gedocumenteerde manual-upgrade + risicoacceptatie. |
| **PR03-TM09** | TM-09 **PASS** **of** **N/A** (services uit scope voor deze tag) met risicoacceptatie. |
| **PR-01 §4** | Geen RC zolang **P1-blockers** (in de zin van: niet geaccepteerde P1’s) de afgesproken scope ondermijnen — zie [PR01 §4](PR01_RELEASE_READINESS_BASELINE.md#4-drie-readiness-niveaus-verplicht-onderscheid) (*Release candidate*: geen open P0/P1 **voor de afgesproken scope** + known issues ingedeeld). |

**Interpretatie:** “P1 geaccepteerd” = product/engineering **expliciet** kiest voor release met vermelde beperking (release notes + PR03-register update).

### 5.3 TM-08 / TM-09: verplicht PASS of mag N/A?

- **Niet beide** verplicht **PASS** voor elke RC-variant — wel **één van**:
  - **PASS** met artifact, of
  - **`N/A`** met **expliciet risico** (waarom niet getest, wat ontbreekt, wie accepteert).

### 5.4 No-go (RC uitstellen)

- Nieuw **P0** (crash-loop, brick-pad, credential leak) zonder mitigatie.
- Onbesliste **P1**-lijst: geen owner, geen target, geen acceptatie — dan geen **RC-go**.

---

## 6. PR-04 — uitgevoerd (RC-checklist)

| Wat | Inhoud |
|-----|--------|
| **Document** | **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)** — eerste RC-structuur, artifacts, checklist §8, PR03-items §6, aanbevolen **PR02-RUN-002**. |
| **Invoer** | PR03-register + bijgewerkte PR-02-run(s) waar exit-criteria zijn gehaald of waived. |

---

## 7. Changelog (document)

| Datum | Wijziging |
|-------|-----------|
| 2026-04-19 | Eerste vastlegging PR-03 op basis van **PR02-RUN-001**. |
| 2026-04-19 | Verwijzing naar **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)** toegevoegd (PR-04 vastgelegd). |

---

## Bronnen

- [PR01_RELEASE_READINESS_BASELINE.md](PR01_RELEASE_READINESS_BASELINE.md)
- [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md)
- [PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)
- [BUILD.md](../../firmware-v2/BUILD.md)
