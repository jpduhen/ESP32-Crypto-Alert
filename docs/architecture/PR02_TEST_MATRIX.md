# PR-02 — Testmatrix + eerste run (V2)

**Status:** matrix **uitvoerbaar**; **eerste run vastgelegd** als **PR02-RUN-001** (2026-04-19, `v2/foundation`, ESP32-S3 GEEK).  
**Branch / board:** `v2/foundation` · **ESP32-S3 GEEK** (enige release-referentie in deze fase).  
**Bronnen:** [PR01_RELEASE_READINESS_BASELINE.md](PR01_RELEASE_READINESS_BASELINE.md) (categorieën §5–7), [werkdocument §9a/§10/§13/§15](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md), [WP-03a](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#wp-03a--v1-gap-review--scopekeuzes-alert-engine-consolidatie-stuurversie-2026-04-15).

**Doel PR-02:** van “we denken dat het werkt” naar **aantoonbare** checks per categorie — handmatig mag; **artifact** per rij waar `PASS` of `FAIL` wordt geclaimd.

---

## Waarom PR-02 nu de juiste stap is

PR-01 heeft **wat** getest moet worden en **drie readiness-niveaus** gedefinieerd; zonder ingevulde matrix blijft productierijpheid een **intentie**. PR-02 maakt dat **meetbaar**: dezelfde categorieën als PR-01 §5, met **stappen**, **pass-criteria**, **status** en **bewijs**. Daarmee kunnen **blockers/known issues** (nu vastgelegd in [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)) aan concrete TM-resultaten worden gekoppeld i.p.v. losse observaties. De eerste run (PR02-RUN-001) zet die brug — inclusief expliciete vastlegging van de WebUI-homepage-overflow (**PR02-BF-001**).

---

## 1. Gebruik

1. Voer tests uit in de volgorde van de matrix (Build eerst; daarna device-tests).
2. Vul per categorie **status** in: `PASS` | `FAIL` | `SKIP` | `N/A` (bij `N/A`: reden in opmerking).
3. Bewaar **artifact**: pad naar logfragment, screenshot, opgeslagen JSON, CI-run-URL, of **commit-hash** + opdracht.
4. Nieuwe sessies: kopieer **§5 Template — nieuwe run** naar een nieuw blok of verhoog het run-nummer.

---

## 2. Globale precondities (alle device-runs)

| Veld | Invullen |
|------|----------|
| Firmware | `PROJECT_VER` uit `firmware-v2/CMakeLists.txt` + **git SHA** (`git rev-parse --short HEAD`) |
| IDF | Zelfde major/minor als `BUILD.md` / CI (workflow: `espressif/idf:v5.4.2`, zie `firmware-v2-smoke.yml`) |
| Board | ESP32-S3 GEEK, bekende flash-layout (`partitions_v2_16mb_ota.csv`) |
| Netwerk | WiFi met internet (Bitvavo HTTPS/WS bereikbaar) tenzij categorie anders vereist |

---

## 3. Testmatrix (uitvoerbaar)

Kolommen per TM: **Doel** · **Precondities** · **Uitvoerstappen** · **Pass** · **Artifact** · **Status (definitie)** · **Opmerking**

> **Resultaat per categorie voor een concrete run:** zie **§4 PR02-RUN-001** (en toekomstige runs in §5).

### TM-01 — Build / CI

| Veld | Inhoud |
|------|--------|
| **Doel** | Reproduceerbare groene build voor `esp32s3`, identiek aan CI-bedoeling. |
| **Precondities** | ESP-IDF geïnstalleerd; `export.sh` actief; toolchain **esp32s3** aanwezig. |
| **Uitvoerstappen** | `cd firmware-v2` → `idf.py set-target esp32s3` → `idf.py build`. Zelfde bedoeling als `.github/workflows/firmware-v2-smoke.yml` (Docker `espressif/idf:v5.4.2`). |
| **Pass** | Build eindigt zonder error; `build/*.bin` aanwezig. |
| **Artifact** | Terminal-log (laatste 30 regels) of CI-run URL + commit SHA. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Bij toolchain-fout: geen FAIL van applicatie — vastleggen als omgevingsissue. |

### TM-02 — Boot / sanity

| Veld | Inhoud |
|------|--------|
| **Doel** | Device start stabiel; geen reset-loop; versie zichtbaar. |
| **Precondities** | Flash met build uit TM-01; seriële monitor aan (115200 of projectdefault). |
| **Uitvoerstappen** | Cold boot (power cycle) → 60 s observeren → noteer bootlog tot “app main” / geen panic. Check `PROJECT_VER` in log of WebUI. |
| **Pass** | Geen watchdog/panic; app draait; versie komt overeen met verwachte build. |
| **Artifact** | Seriële logfragment of screenshot WebUI met versie. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | |

### TM-03 — WiFi / onboarding

| Veld | Inhoud |
|------|--------|
| **Doel** | STA-verbinding of SoftAP-onboarding volgens [ADR-003](ADR-003-wifi-onboarding-v2.md); credentials blijven na reboot (NVS). |
| **Precondities** | Bekende SSID of leeg NVS voor onboarding-test. |
| **Uitvoerstappen** | (A) Verbind met bekende WiFi → wacht op STA IP. Of (B) SoftAP-portal → wijs netwerk toe → reboot → STA actief. Optioneel: forget + herhaal B. |
| **Pass** | STA `GOT_IP` of equivalent; ping/HTTPS naar broker/API slaagt in volgende stappen. |
| **Artifact** | Logregels met IP / WiFi state of screenshot portal. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | |

### TM-04 — Live WS-feed (Bitvavo)

| Veld | Inhoud |
|------|--------|
| **Doel** | WebSocket naar Bitvavo ontvangt prijs/ticker-updates; geen structurele disconnect in testvenster (≥ 10 min). |
| **Precondities** | TM-03 PASS; markt open of tickers actief. |
| **Uitvoerstappen** | Start app; monitor logs `[WS` / market tick of `market_data` updates; laat ≥10 min lopen. Noteer `ws_feed_observability` in `/api/status.json` indien aanwezig. |
| **Pass** | Periodieke updates zichtbaar; geen reconnect-storm; bij gap: herstel binnen acceptabele tijd (subjectief vastleggen). |
| **Artifact** | Logfragment + JSON-snippet `ws_feed_observability` of “n.v.t. in build”. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Zie [M002_NETWORK_BOUNDARIES.md](M002_NETWORK_BOUNDARIES.md). |

### TM-05 — Alert-kern 1m / 5m / confluence

| Veld | Inhoud |
|------|--------|
| **Doel** | Drempel/cooldown/confluence/suppress gedrag in lijn met C1/C2 — minstens observability, bij voorkeur één echte trigger of duidelijke “below threshold”. |
| **Precondities** | TM-04; drempels bekend (Kconfig/NVS). |
| **Uitvoerstappen** | Bekijk `alert_decision_observability`, `alert_engine_runtime_stats`, `alerts_1m`/`alerts_5m`/`alerts_conf_1m5m` in status.json. Optioneel: tijdelijk lagere drempel via toegestane WebUI-write (alleen als al geconfigureerd). |
| **Pass** | Snapshot toont consistente statusstrings; bij fire: NTFY/MQTT/WebUI-ring consistent (C4). |
| **Artifact** | JSON-export of log + screenshot NTFY/MQTT. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | [C1_FIELD_TEST_1M5M.md](C1_FIELD_TEST_1M5M.md), [C2_EDGE_CASES_1M5M.md](C2_EDGE_CASES_1M5M.md). |

### TM-06 — 30m / 2h sanity

| Veld | Inhoud |
|------|--------|
| **Doel** | Metrics/observability voor 30m en 2h aanwezig; beslisvelden `tf_30m` / `tf_2h` zinvol; `alerts_30m` / `alerts_2h` structuur aanwezig (trigger optioneel). |
| **Precondities** | TM-04; voldoende uptime voor metric warmup (≥ 1 min; 2h-alert niet verplicht in eerste run). |
| **Uitvoerstappen** | Open `/api/status.json` → controleer `metric_30m_observability`, `metric_2h_observability` (of equivalente keys), `tf_30m`, `tf_2h`, `alerts_30m`, `alerts_2h`. |
| **Pass** | Geen crash; velden aanwezig; `ready`/`not_ready` verklaarbaar; bij geen 2h-fire: expliciet “geen trigger in venster” OK. |
| **Artifact** | JSON-snippet (anoniem) of beschrijving. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Geen langetermijn-stresstest vereist voor PR-02. |

### TM-07 — WebUI

| Veld | Inhoud |
|------|--------|
| **Doel** | Read-only endpoints bereikbaar; geen 500 op `/` en `/api/status.json`. |
| **Precondities** | TM-03; `CONFIG_WEBUI_ENABLE=y` of runtime aan. |
| **Uitvoerstappen** | Browser: `http://<device-ip>:<port>/` en `/api/status.json` (poort uit Kconfig/NVS). |
| **Pass** | HTTP 200; JSON parseert; HTML rendert secties (alerts, observability). |
| **Artifact** | Screenshot of `curl -s` output (truncated). |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Geen POST-tests vereist voor minimale release-readiness tenzij scope uitbreidt. |

### TM-08 — OTA

| Veld | Inhoud |
|------|--------|
| **Doel** | Dual-slot OTA-begrip aangetoond of **bewust manual flash** met risico genoteerd. |
| **Precondities** | TM-02; partition table bekend. |
| **Uitvoerstappen** | (A) OTA-upload via WebUI/API volgens `BUILD.md`/docs **of** (B) documenteer: “manual `esptool` flash slot0” + herhaal TM-02. |
| **Pass** | (A) succesvolle swap + boot **of** (B) expliciete keuze + TM-02 opnieuw PASS na flash. |
| **Artifact** | OTA-log of flash-commando + versie-check. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Als OTA niet getest: `SKIP` + P1-risico voor PR-03. |

### TM-09 — NTFY / MQTT

| Veld | Inhoud |
|------|--------|
| **Doel** | Minstens één kanaal end-to-end bij alert (of expliciet `N/A` als builds zonder service). |
| **Precondities** | Broker/topic/NTFY volgens Kconfig/NVS; TM-05 desgewenst. |
| **Uitvoerstappen** | Configureer NTFY **of** MQTT → forceer of wacht op alert → ontvang push of zie MQTT-client log `publish`. |
| **Pass** | Payload sluit aan op C4 (`kind` / `Soort:`); geen stille drop (check `service_outbound` logs bij queue full). |
| **Artifact** | Screenshot NTFY of MQTT payload log. |
| **Status** | *Zie run-tabel §4 / §5.* |
| **Opmerking** | Als beide uit: `N/A` + risico registreren in PR-03. |

---

## 3.1 PR-02 — Bevinding PR02-BF-001 (WebUI homepage / `GET /`)

### Symptoom

- Na **langere uptime**: browser toont fout / HTTP **500** met body **`overflow`** op **`GET /`**; seriële log o.a. `M-013c/d: HTML deel2 overflow (n≈4476)`; **`httpd`** luistert nog op de WebUI-poort (bv. **8080**).

### Oorzaak (root cause)

- **`handle_root_html`** bouwt de homepage in één **`malloc`**, vaste buffer (**historisch 16 KiB**). Cumulatieve HTML (observability-secties + forms + tweede `snprintf`-blok “deel2” met OTA + services/MQTT-velden) overschreed die limiet. **`GET /api/status.json`** gebruikt een **ander pad** (`cJSON_PrintUnformatted`), niet dezelfde buffer.

### Fixstatus (firmware)

- **Geïmplementeerd** op `v2/foundation`: **`k_html_alloc` verhoogd naar 32 KiB**, uitgebreide overflow-log (o.a. `buf`, `w`, `cap_left`, `n`, `truncated`), aparte log bij mislukte JSON-print voor status.json.

### Known issue — open of gesloten?

| Criterium | Status |
|-----------|--------|
| **Codefix in tree** | **Ja** — buffer + diagnostiek. |
| **Veldherverificatie** `GET /` na lange soak **zonder** 500 | **Open** — vereist voor TM-07 **PASS** strikt volgens matrix; tot die tijd blijft **PR02-BF-001** een **P1** (zie [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)). |
| **Na succesvolle soak** | Issue kan als **gesloten voor BF-001** worden gemarkeerd; **restresidu** (toekomstige HTML-groei) blijft onder §7 known issues monitoren. |

---

## 4. Eerste ingevulde run — PR02-RUN-001

**Scope:** samenvoeging van (a) **repo/CI**-contract, (b) **veldobservaties** (langere sessie / nachtrun op GEEK: seriële log, scherm-UI, feed), (c) **expliciete TM-gaten** (`SKIP`) voor wat niet in deze registratie is uitgevoerd.

| Veld | Waarde |
|------|--------|
| **Run-ID** | PR02-RUN-001 |
| **Datum** | 2026-04-19 |
| **Tester / registratie** | Project (repo + vastgelegde field-logs) |
| **Board** | ESP32-S3 GEEK |
| **Branch / commit** | `v2/foundation` @ `cde7ba9` |
| **Firmware (`PROJECT_VER`)** | `3.0.0` (`firmware-v2/CMakeLists.txt` op genoemde commit) |
| **IDF (CI-referentie)** | v5.4.2 (`espressif/idf:v5.4.2`, zie workflow) |
| **Duur sessie (field)** | Langere continu-run (o.a. nacht); exacte minuten niet kritisch voor PR-02-document |

### Resultaat per categorie

| ID | Categorie | Status | Artifact (kort) | Opmerking |
|----|-----------|--------|-----------------|-----------|
| TM-01 | Build / CI | **PASS** | `.github/workflows/firmware-v2-smoke.yml`; build `esp32s3` in container | Reproduceer lokaal: `idf.py set-target esp32s3` + `build` op zelfde commit. |
| TM-02 | Boot / sanity | **PASS** | Seriële log (field): app stabiel; geen panic/watchdog in waargenomen venster | Sluit aan bij nachtrun-fieldlogs. |
| TM-03 | WiFi / onboarding | **PASS** | STA bereikbaar voor WebUI (field) | SoftAP-pad niet afzonderlijk herhaald in deze registratie. |
| TM-04 | Live WS-feed | **PASS** | Log/metrics (field): feed actief; prijs/telemetry in lijn met verwachting | ≥10 min in langere sessie gedekt. |
| TM-05 | Alert 1m/5m/conf | **PASS** | `/api/status.json`-observability (velden aanwezig; consistente strings) | Geen afgedwongen “fire”-scenario verplicht voor deze run; dieptest zie C1-fieldtest. |
| TM-06 | 30m / 2h sanity | **PASS** | JSON: `tf_30m` / `tf_2h`, `alerts_30m` / `alerts_2h`, metric-observability | 2h-trigger niet vereist. |
| TM-07 | WebUI | **FAIL** | Serial: `M-013c/d: HTML deel2 overflow`; HTTP 500 `overflow` op **`GET /`** | **PR02-BF-001**; fix in firmware (32 KiB + logs) op commit **cde7ba9** — **herhaal TM-07 na flash/soak** voor PASS. `/api/status.json` werkte wel in scenario. |
| TM-08 | OTA | **SKIP** | — | Geen OTA-swap-test in deze run; **P1-risico** voor PR-03 tenzij alsnog gedocumenteerd. |
| TM-09 | NTFY / MQTT | **SKIP** | — | Geen end-to-end alert→push in deze registratie afgedwongen; **PR-03** opnemen of `N/A` met risico. |

### Samenvatting PR02-RUN-001

- **Readiness-geschat:** ☑ **werkt technisch** (brede field-stabiliteit) · ☐ **voldoende toetsbaar voor RC** — **nee** zolang TM-07 **FAIL** (homepage) open staat tot soak na fix · ☐ **RC**
- **Open punten (register: [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)):**
  - **PR02-BF-001:** veld-PASS op **`GET /`** na mitigatie (soak; zie PR03 exit-criterium).
  - **PR03-TM08 / PR03-TM09:** `SKIP` opheffen of bewust **N/A** + risicoacceptatie.
  - Vervolg: **PR-04** RC wanneer exit-criteria of geaccepteerde P1’s helder zijn.

---

## 5. Template — nieuwe run (kopieer per sessie)

| Veld | Waarde |
|------|--------|
| **Run-ID** | PR02-RUN-00* |
| **Datum** | YYYY-MM-DD |
| **Tester** | naam |
| **Board** | ESP32-S3 GEEK |
| **Branch / commit** | `v2/foundation` @ `<git short SHA>` |
| **Firmware (`PROJECT_VER`)** | uit `CMakeLists.txt` |
| **IDF** | bv. v5.4.2 |
| **Duur sessie (optioneel)** | minuten |

### Resultaat per categorie

| ID | Categorie | Status | Artifact (kort) | Opmerking |
|----|-----------|--------|-----------------|-----------|
| TM-01 | Build / CI | | | |
| TM-02 | Boot / sanity | | | |
| TM-03 | WiFi / onboarding | | | |
| TM-04 | Live WS-feed | | | |
| TM-05 | Alert 1m/5m/conf | | | |
| TM-06 | 30m / 2h sanity | | | |
| TM-07 | WebUI | | | |
| TM-08 | OTA | | | |
| TM-09 | NTFY / MQTT | | | |

### Samenvatting

- **Readiness-geschat:** ☐ werkt technisch ☐ voldoende toetsbaar (alle relevante TM ingevuld) ☐ nog geen RC
- **Open punten voor PR-03:** (blockers / known issues verwijzen)

---

## 6. Vervolg — PR-03 (uitgevoerd) → PR-04

**PR-03** is vastgelegd in **[PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md)** (P0–P3, register **PR02-BF-001**, **PR03-TM08**, **PR03-TM09**, go/no-go richting RC).

**PR-04** is vastgelegd in **[PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md)** (RC-checklist, artifacts, koppeling PR03-items).

**Eerstvolgende stap:** **RC-go uitvoeren** — checklist **[PR04 §8–§9](PR04_RC_CHECKLIST.md#8-rc-checklist-afvinken-voor-rc-go)**; aanbevolen **PR02-RUN-002** voor TM-07 soak + TM-08/TM-09.

Aanbevolen praktisch vervolg op testzijde:

1. **PR02-RUN-002** (of latere run): TM-07 **PASS** na soak op release-candidate build; artifact vastleggen.
2. TM-08 / TM-09: **PASS** of bewust **N/A** per [PR03 §5](PR03_BLOCKERS_KNOWN_ISSUES.md#5-go--no-go-richting-pr-04-rc) en [PR04 §6](PR04_RC_CHECKLIST.md#6-open-pr-03-items--status-tov-rc-go).

---

## 7. Historisch — voorbeeldplaceholders (vervangen door §4)

De eerdere placeholder “geen uitgevoerde run” is **vervangen** door **PR02-RUN-001** in §4.

---

## Bronnen

- [BUILD.md](../../firmware-v2/BUILD.md) — lokale build.
- [PR03_BLOCKERS_KNOWN_ISSUES.md](PR03_BLOCKERS_KNOWN_ISSUES.md) — blockers / known issues (PR-03).
- [PR04_RC_CHECKLIST.md](PR04_RC_CHECKLIST.md) — RC-checklist + eerste candidate (PR-04).
- [C5_ROADMAP_30M_2H.md](C5_ROADMAP_30M_2H.md) — 30m/2h scope (geen RWS-03 vereiste).
