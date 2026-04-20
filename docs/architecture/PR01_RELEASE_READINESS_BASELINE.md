# PR-01 — Release-readiness baseline (V2)

**Status:** vastgelegd als proces- en kwaliteitsbaseline (documentaire stap).  
**Branch:** `v2/foundation` (code wijzigt niet in PR-01 zelf).  
**Primaire afstemming:** [werkdocument §9](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#9-huidige-status), [§9a](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#9a-huidige-projectfase--fasebeoordeling-volwassenheid-werkpakketten), [§10](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#10-prioriteitenlijst), [§13](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#13-risicos-en-aandachtspunten), [§15](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#15-eerstvolgende-concrete-stap), [WP-03a](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#wp-03a--v1-gap-review--scopekeuzes-alert-engine-consolidatie-stuurversie-2026-04-15).

---

## 1. Waarom PR-01 nu

Consolidatie **C1–C5** en de verticale slices **S30-1…S30-3** en **S2H-1…S2H-3** zijn afgerond: de kern is **technisch aanwezig**, maar **productierijpheid** stond in het werkdocument nog als erkenning zonder **scherpe, gedeelde definitie** van wat “release” betekent (§9a, werkpakket 5). PR-01 maakt die basis **expliciet** vóór verdere uitvoering — in lijn met §13 (vermijden van microstappen zonder samenhang) en WP-03a (kwaliteit en toetsbaarheid boven feature-pariteit met V1).

---

## 2. Eerste serieuze V2-release — concreet doel (korte termijn)

**Doel:** een **gelabelde firmware-build** van V2 op het **referentieboard ESP32-S3 GEEK** die:

- op **Bitvavo** een **stabiele 1 Hz-marktdatastroom** hanteert en **1m/5m/confluence/mini-regime** betrouwbaar uitvoert;
- **30m**- en **2h**-alerts volgens de huidige V2-implementatie **functioneel** levert (metrics → engine → outbound), met dezelfde **observability-/notificatiepatronen** als de kortere TF’s;
- **NTFY** en/of **MQTT** en **WebUI** (read-only waar van toepassing) **consistent** gebruikt volgens de vastgelegde C4-semantiek;
- **OTA** en **partitioning** volgens het repo **begrepen en gecommit** zijn voor die release;
- vergezeld gaat van **minimale bewijsstukken**: welke tests zijn gedaan, welke field-checks zijn groen, welke **known issues** en **blockers** resteren.

Dit is bewust **geen** “feature-complete t.o.v. V1” en **geen** multi-board-release.

---

## 3. Wat wél en niet in scope van die eerste release

| **Wél (minimum)** | **Niet (in deze release-definitie)** |
|-------------------|--------------------------------------|
| GEEK + huidige `firmware-v2`-architectuur | Extra boards (JC3248W535, LCDWIKI) |
| 1m/5m + confluence + regime + NTFY/MQTT + WebUI status/obs | V1-pariteit, V1 2h-matrix/familie |
| 30m/2h zoals nu in V2 gebouwd (slices afgerond) | RWS-03 (SecondSampler), nieuwe alertfeatures |
| CI build-smoke zoals nu in repo | Brede CI-herbouw of nieuwe pipelines (tenzij no-regret) |
| Release notes + versie/artifact-discipline | Nieuwe dashboards, grote WebUI-uitbreiding |

---

## 4. Drie readiness-niveaus (verplicht onderscheid)

| Niveau | Betekenis | Typisch bewijs |
|--------|-----------|----------------|
| **Werkt technisch** | Image boot, netwerk, feed, alerts genereren zonder crash in korte sessie | Logs, handmatige check, eventueel één veld-run |
| **Voldoende toetsbaar** | Bovenstaande + **vastgelegde** testmatrix (wie/wat/wanneer) + field-testcriteria **ingevuld of bewust N/A** | Document + resultaat per categorie (zie §5–6) |
| **Release candidate (RC)** | Toetsbaarheid + **geen open P0/P1-blockers** voor de afgesproken scope + **known issues** ingedeeld (zie §7) | Go/no-go met verwijzing naar artifact + notes |

PR-01 **definieert** de ladder; **PR-02–PR-04** vullen uitvoering en besluitvorming in.

---

## 5. Minimale testcategorieën

Elke categorie heeft in uitvoering (PR-02) minimaal: **doel**, **pass/fail**, **artifact** (log, screenshot, commit-hash), **opmerking**.

| Categorie | Minimale inhoud |
|-----------|-----------------|
| **Build / CI** | `idf.py set-target esp32s3` + `build` groen; zelfde IDF-pin als `BUILD.md` / workflow waar van toepassing |
| **Boot / sanity** | Cold boot, geen watchdog-loop, `esp_app_desc` / versie zichtbaar (bv. WebUI of log) |
| **WiFi / onboarding** | STA verbindt of SoftAP-flow volgens ADR-003; NVS persistentie smoke |
| **Live WS-feed** | Verbinding Bitvavo, prijsupdates, geen structurele disconnect in testvenster; observability op gap/stilte waar aanwezig |
| **Alert-kern 1m / 5m / confluence** | Minimaal scenario’s: drempel raak, cooldown zichtbaar, confluence + suppress volgens policy (zie C1/C2) |
| **30m / 2h sanity** | Minstens: metric ready, één alert-pad of bewuste “nog geen trigger” + observability (`tf_*`, `alerts_*` in JSON) — geen volledige langetermijn-stresstest vereist in PR-01 |
| **WebUI** | `GET /` en `GET /api/status.json` (of vastgelegde endpoints); read-only contract gerespecteerd |
| **OTA** | Dual-slot concept: uploadpad of documenteer **manual flash** als tijdelijke uitzondering + risico |
| **NTFY / MQTT** | Minimaal één kanaal end-to-end indien in release-scope; anders expliciet **uit scope** met risico |

---

## 6. Minimale field-testcriteria

Afstemming: [C1_FIELD_TEST_1M5M.md](C1_FIELD_TEST_1M5M.md) blijft leidend voor de **1m/5m-kern**; PR-01 vult aan met **release-breedte**:

- **Duur:** minimaal één sessie van **≥ 2 uur** continu met netwerk + feed (langer aanbevolen voor 2h-randcases, niet verplicht voor PR-01-documentatie).
- **Spam / gedrag:** geen onacceptabele alert-storm op rustige markt; cooldowns en suppress **zichtbaar** in logs of stats (cf. C2).
- **Herstart:** power-cycle of reset → gedrag **reproduceerbaar** (geen “alleen eerste boot”).
- **Versie:** field-log vermeldt **git SHA** of release-tag + `PROJECT_VER`.

---

## 7. Release-blockers en known issues (structuur)

**Blocker (P0):** release mag niet uit — bv. crash-loop, geen connectiviteit, datalek in credentials, OTA brick-risk zonder mitigatie.

**Hoog (P1):** release alleen met **expliciete** acceptatie en vermelding in release notes.

**Medium/Laag (P2/P3):** known issues-lijst; geen automatische stop.

**Known issues-register (minimaal):**

| Veld | Inhoud |
|------|--------|
| ID | PR-XX of issue-ref |
| Component | bv. `alert_engine`, `mqtt_bridge`, `net_runtime` |
| Symptoom | kort |
| Reproduceerbaar | ja/nee/deels |
| Workaround | ja/nee |
| Target fix | RC / volgende minor / defer |

**PR-02 input (veldbevinding + eerste run):** WebUI **`GET /`** — vaste HTML-responsebuffer kon vol raken wanneer observability + forms cumulatief de limiet overschreden (HTTP 500 `overflow` terwijl de listener nog actief is). Zie **[PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md)** (bevinding **PR02-BF-001**, run **PR02-RUN-001**). **Classificatie en exit-criteria:** **[PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)**. **RC-checklist:** **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)**.

---

## 8. Artifact-, versie- en release-notes-discipline

- **Versie:** `PROJECT_VER` in `firmware-v2/CMakeLists.txt` + overeenkomst met **git tag** bij release (handmatig of CI).
- **Build artifact:** `.bin` + optioneel `elf`; **partition table** en **flash-instructie** verwijzen naar repo (`partitions_v2_16mb_ota.csv`, `BUILD.md`).
- **Release notes (minimaal):** scope (§3), geteste configuratie (Kconfig-highlights), known issues (§7), instructies flash/OTA, verwijzing naar commit/tag.

---

## 9. Open risico’s (§13) — nog niet weggenomen door PR-01

PR-01 **documenteert** alleen; het **elimineert** deze risico’s niet:

- Te snelle verwachting t.o.v. V1-referentie zonder meetbare criteria.
- Display-/board-specifieke dominantie vóór release-baseline (GEEK blijft referentie).
- WebUI/config-breedte (nieuwe forms alleen na besluit).
- Technische schuld uit V1 ongemerkt kopiëren (WP-03a: keep/drop/defer blijft leidend).

---

## 10. Vervolgfasen (voorstel — compact)

| ID | Naam | Doel |
|----|------|------|
| **PR-02** | Testmatrix uitvoeren / structureren | **Uitgevoerd:** **[PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md)** — TM-01…TM-09 + **eerste run PR02-RUN-001** (2026-04-19, `v2/foundation`) |
| **PR-03** | Blockers + known issues afdwingen | **Vastgelegd:** **[PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)** — P0–P3, register, go/no-go richting PR-04 |
| **PR-04** | RC-checklist + eerste candidate | **Vastgelegd:** **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)** — checklist, artifacts, PR03-koppeling, **PR02-RUN-002**-aanbeveling |

**Eerstvolgende uitvoerbare substep:** **RC-go uitvoeren** — **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)** §8–§9 invullen (minimaal **PR02-RUN-002** voor TM-07 soak + TM-08/TM-09 PASS of N/A), daarna tag/release notes.

---

## Bronnen (aanvullend)

- [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md) — uitvoerbare testmatrix (PR-02).
- [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md) — blockers / known issues / go-no-go (PR-03).
- [PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md) — RC-checklist + eerste candidate (PR-04).
- [C5_ROADMAP_30M_2H.md](C5_ROADMAP_30M_2H.md) — TF-slices afgerond; productierijpheid volgt los.
- [M002_NETWORK_BOUNDARIES.md](M002_NETWORK_BOUNDARIES.md) — netwerkgedrag en grenzen.
- [WP03a_V1_GAP_ALERT.md](WP03a_V1_GAP_ALERT.md) — scope geen V1-import.
