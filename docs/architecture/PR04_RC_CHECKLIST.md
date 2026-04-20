# PR-04 — RC-checklist + eerste release-candidate (V2)

**Status:** **structuur vastgelegd** — checklist is normatief voor **eerste interne RC** op referentieconfiguratie; **RC-go** vereist ingevulde velden + eventuele acceptaties (zie §3).  
**Branch:** `v2/foundation`  
**Board:** **ESP32-S3 GEEK** (enige release-referentie deze fase)  
**Parallel:** **RWS-03** blijft **los** van deze beslislaag — geen verplichte volgorde ([werkdocument §RWS](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md)).

**Bronnen:** [PR01_RELEASE_READINESS_BASELINE.md](PR01_RELEASE_READINESS_BASELINE.md), [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md), [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md), [werkdocument §3/§9a](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md).

---

## 1. Waarom PR-04 nu de juiste stap

**PR-01** definieert readiness-niveaus; **PR-02** levert testruns; **PR-03** classificeert open punten en go/no-go-regels. **PR-04** vertaalt dat naar **één plek**: wat hoort bij een **release candidate** (artifact, notes, scope) en welke **checks** moeten groen zijn — of **bewust geaccepteerd** — vóór een **tag** of interne “RC-build”. Dit is **geen featurestap** maar een **releasebeslisstructuur** ([PR01 §4](PR01_RELEASE_READINESS_BASELINE.md#4-drie-readiness-niveaus-verplicht-onderscheid) — niveau *Release candidate*).

---

## 2. Doel van deze eerste RC

| Aspect | Inhoud |
|--------|--------|
| **Doel** | Eerste **gelabelde** firmware-candidate op **GEEK** die als **interne RC** dient: reproduceerbare build, bekende scope, gedocumenteerde known issues, minimale release notes. |
| **Niet het doel** | V1-pariteit, extra boards, RWS-03, nieuwe WebUI-features ([PR01 §3](PR01_RELEASE_READINESS_BASELINE.md#3-wat-wél-en-niet-in-scope-van-die-eerste-release)). |

---

## 3. Scope van de RC (afgestemd op PR-01 §2–§3)

- **Bitvavo** marktdata (WS), **1m / 5m / confluence / mini-regime**, **30m / 2h** zoals in huidige V2-build.  
- **NTFY en/of MQTT** en **WebUI** (read-only contract) — **indien in release-scope** voor deze tag ([PR01 §2](PR01_RELEASE_READINESS_BASELINE.md#2-eerste-serieuze-v2-release--concreet-doel-korte-termijn)).  
- **OTA / partitionering** begrepen volgens repo; field-OTA optioneel zie PR-03.  
- **Geen** RWS-03, geen extra boards in deze RC-definitie.

---

## 4. RC-candidate-identiteit (invullen bij tag)

| Veld | Waarde / instructie |
|------|---------------------|
| **Branch** | `v2/foundation` |
| **`PROJECT_VER`** | Uit `firmware-v2/CMakeLists.txt` (huidige baseline: **3.0.0**) |
| **Git commit (RC)** | `git rev-parse --short HEAD` op het moment van RC-besluit — *voorbeeld registratie:* `cde7ba9` |
| **IDF (CI)** | v5.4.2 — zie `.github/workflows/firmware-v2-smoke.yml` |
| **Partition table** | `partitions_v2_16mb_ota.csv` (of vastgelegde variant in repo) |
| **Artifacts** | `build/*.bin`, optioneel `.elf`; CI-run URL of lokale build-log (laatste regels) |

---

## 5. Artifacts die bij de RC horen

| Artifact | Vereist |
|----------|---------|
| **Binary + versie** | `.bin` die bij genoemde commit hoort; `PROJECT_VER` in image ([PR01 §8](PR01_RELEASE_READINESS_BASELINE.md#8-artifact--versie--en-release-notes-discipline)) |
| **Build-bewijs** | Groene `idf.py build` esp32s3 of groene CI-run |
| **Field-bewijs (minimaal)** | Ingevulde PR-02-run (aanbevolen **PR02-RUN-002**) of addendum met status per TM |
| **Known issues** | Verwijzing naar [PR03](PR03_BLOCKERS_KNOWN_ISSUES.md) + status per item (§6 hieronder) |
| **Release notes** | Zie §10 — bestand of sectie in tag beschrijving |

---

## 6. Open PR-03-items — status t.o.v. RC-go

Leg bij **RC-besluit** per item vast: **gesloten** | **open** | **geaccepteerd risico**.

### 6.1 PR02-BF-001 (TM-07 WebUI `GET /`)

| Veld | Inhoud |
|------|--------|
| **Huidige status t.o.v. RC** | **Open** tot veld-soak op RC-build, **of** **geaccepteerd risico** (P1) |
| **Fix in code** | Ja — 32 KiB + logs ([PR03 §3.1](PR03_BLOCKERS_KNOWN_ISSUES.md#31-pr02-bf-001--webui-get--homepage-overflow-tm-07)) |
| **RC-go opties** | **(A)** **PASS vereist:** TM-07 **PASS** na soak (aanbevolen ≥8 h, zie PR03) op **deze** commit — dan **gesloten** voor BF-001. **(B)** **Geaccepteerd:** homepage niet gegarandeerd; observability via `/api/status.json`; **release notes** + impliciete P1-acceptatie — item blijft **open** in register maar **RC mag** als PR03-go regels gevolgd zijn. |
| **Nodig vóór optie A** | **PR02-RUN-002** (of latere run) met TM-07 **PASS** + artifact |

### 6.2 PR03-TM08 (OTA — was SKIP in PR02-RUN-001)

| Veld | Inhoud |
|------|--------|
| **Huidige status t.o.v. RC** | **Open gap** tot TM-08 **PASS** of **N/A** vastgelegd |
| **RC-go opties** | **(A)** TM-08 **PASS** (OTA-swap of gedocumenteerde manual path + TM-02 herhaald). **(B)** **N/A:** alleen lab/manual flash voor upgrade; risico + procedure in release notes — **geaccepteerd** |
| **PASS vereist?** | **Nee** — **PASS of N/A** met acceptatie volstaat ([PR03 §5.3](PR03_BLOCKERS_KNOWN_ISSUES.md#53-tm-08--tm-09-verplicht-pass-of-mag-na)) |

### 6.3 PR03-TM09 (NTFY/MQTT E2E — was SKIP in PR02-RUN-001)

| Veld | Inhoud |
|------|--------|
| **Huidige status t.o.v. RC** | **Open gap** tot TM-09 **PASS** of **N/A** vastgelegd |
| **RC-go opties** | **(A)** TM-09 **PASS** (minstens één kanaal E2E als services aan staan). **(B)** **N/A:** geen outbound in geteste image of expliciet buiten scope voor tag — **geaccepteerd** met risico |
| **PASS vereist?** | **Nee** — **PASS of N/A** met acceptatie ([PR03 §5.3](PR03_BLOCKERS_KNOWN_ISSUES.md#53-tm-08--tm-09-verplicht-pass-of-mag-na)) |

---

## 7. Minimale release notes-inhoud ([PR01 §8](PR01_RELEASE_READINESS_BASELINE.md#8-artifact--versie--en-release-notes-discipline))

1. **Scope** — wat wél/niet in deze RC (GEEK, geen extra boards).  
2. **Config** — IDF-versie, partition table-referentie, relevante Kconfig-highlights.  
3. **Geteste matrix** — verwijzing naar PR-02-run-ID(s) + korte TM-samenvatting.  
4. **Known issues** — PR02-BF-001, PR03-TM08, PR03-TM09: gesloten **of** geaccepteerd met workaround.  
5. **Flash / OTA** — waar `.bin` vandaan komt; manual flash-stappen indien TM-08 niet PASS.  
6. **Commit / tag** — git SHA + `PROJECT_VER`.

---

## 8. RC-checklist (afvinken voor RC-go)

| # | Check | OK / N/A | Artifact / opmerking |
|---|--------|----------|----------------------|
| C1 | **Build groen** (esp32s3, zelfde IDF als CI) | | |
| C2 | **Boot / sanity** — geen panic-loop, versie zichtbaar | | |
| C3 | **WiFi / onboarding** — STA of vastgelegd onboarding-pad | | |
| C4 | **Live WS-feed** — Bitvavo, stabiel in testvenster | | |
| C5 | **Alert-kern 1m/5m/confluence** — observability consistent ([C1/C2](PR01_RELEASE_READINESS_BASELINE.md#5-minimale-testcategorieën)) | | |
| C6 | **30m / 2h sanity** — JSON-velden aanwezig, verklaarbaar | | |
| C7 | **WebUI** — `GET /api/status.json` OK; **TM-07** volgens §6.1 | | |
| C8 | **OTA** — TM-08 PASS of N/A + acceptatie (§6.2) | | |
| C9 | **NTFY/MQTT** — TM-09 PASS of N/A + acceptatie (§6.3) | | |
| C10 | **Known issues** — PR03-items verwerkt in notes | | |
| C11 | **Release notes** — §10 minimaal ingevuld | | |
| C12 | **Geen open P0** — of alleen met formele uitzondering ([PR03 §5.4](PR03_BLOCKERS_KNOWN_ISSUES.md#54-no-go-rc-uitstellen)) | | |

**RC-go:** alle relevante rijen **OK** of **N/A** met gedocumenteerde acceptatie; **P1** alleen met expliciete notes ([PR03 §5.2](PR03_BLOCKERS_KNOWN_ISSUES.md#52-aanbevolen-go-voor-interne-rc-build-geek-v2foundation)).

---

## 9. Aanbevolen directe praktijkactie (vóór RC-tag)

1. **`PR02-RUN-002`** uitvoeren op GEEK met **RC-candidate commit**:  
   - prioriteit: **TM-07 soak** (`GET /` + `status.json`) om **PR02-BF-001** te **sluiten** of bewust te laten als P1.  
2. **TM-08** naar **PASS** (OTA of manual path + herhaalde boot) **of** **N/A** + ondertekende acceptatie.  
3. **TM-09** naar **PASS** (E2E NTFY of MQTT) **of** **N/A** + acceptatie.  
4. Checklist §8 afvinken; release notes opstellen; daarna **RC-go-besluit** en optioneel **git tag**.

---

## 10. Wat nog open is voor echte RC-go (samenvatting)

| Onderdeel | Status |
|-----------|--------|
| **Procesdocs PR-01…PR-04** | **Vastgelegd** — deze checklist maakt PR-04 uitvoerbaar. |
| **Veldtests** | **PR02-RUN-001** alleen — voor “strakke” RC: **PR02-RUN-002** nodig (§9). |
| **PR03-items** | Minstens één pad per item: sluiten of **geaccepteerd** documenteren. |

---

## 11. Wat logisch volgt na PR-04 (document)

| Stap | Inhoud |
|------|--------|
| **RC-go** | Tag op commit + release notes + artifact-archief (intern). |
| **Eventuele “GA”** | Apart besluit: distributie, supportniveau — buiten deze checklist. |
| **RWS-03** | Blijft **parallel**; geen blokker voor RC op GEEK-scope. |
| **Volgende PR-02-runs** | Blijven geldig voor patch-releases en herclassificatie PR03. |

---

## 12. Changelog (document)

| Datum | Wijziging |
|-------|-----------|
| 2026-04-19 | Eerste vastlegging **PR04_RC_CHECKLIST.md** — eerste RC-structuur voor `v2/foundation` + GEEK. |

---

## Bronnen

- [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)
- [PR02_TEST_MATRIX.md](PR02_TEST_MATRIX.md)
- [BUILD.md](../../firmware-v2/BUILD.md)
