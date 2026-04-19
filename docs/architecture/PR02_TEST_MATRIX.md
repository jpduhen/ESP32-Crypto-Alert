# PR-02 — Testmatrix + eerste run (V2)

**Status:** matrix en run-template **vastgelegd** (documentaire stap; invulling = uitvoering op hardware).  
**Branch / board:** `v2/foundation` · **ESP32-S3 GEEK** (enige release-referentie in deze fase).  
**Bronnen:** [PR01_RELEASE_READINESS_BASELINE.md](PR01_RELEASE_READINESS_BASELINE.md) (categorieën §5–6), [werkdocument §9a/§10/§15](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md), [WP-03a](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md#wp-03a--v1-gap-review--scopekeuzes-alert-engine-consolidatie-stuurversie-2026-04-15) (toetsbaarheid boven V1-pariteit).

**Doel PR-02:** van “we denken dat het werkt” naar **aantoonbare** checks per categorie — handmatig mag; artifact verplicht per rij.

---

## 1. Gebruik

1. Voer tests uit in de volgorde van de matrix (Build eerst; daarna device-tests).
2. Vul per categorie **status** in: `PASS` | `FAIL` | `SKIP` | `N/A` (bij `N/A`: reden in opmerking).
3. Bewaar **artifact**: pad naar logfragment, screenshot, opgeslagen JSON, of noteer **commit-hash** + opdracht.
4. Kopieer **§4 Eerste invulrun** naar een nieuw blok per sessie (of voeg een regel toe in een apart bestand als je versioning wilt — niet verplicht in PR-02).

---

## 2. Globale precondities (alle device-runs)

| Veld | Invullen |
|------|----------|
| Firmware | `PROJECT_VER` uit `firmware-v2/CMakeLists.txt` + **git SHA** (`git rev-parse --short HEAD`) |
| IDF | Zelfde major/minor als `BUILD.md` / CI (lokaal of Docker) |
| Board | ESP32-S3 GEEK, bekende flash-layout (`partitions_v2_16mb_ota.csv`) |
| Netwerk | WiFi met internet (Bitvavo HTTPS/WS bereikbaar) tenzij categorie anders vereist |

---

## 3. Testmatrix (uitvoerbaar)

Kolommen: **ID** · **Doel** · **Precondities** · **Uitvoerstappen** · **Pass** · **Artifact** · **Status** · **Opmerking / link**

### TM-01 — Build / CI

| Veld | Inhoud |
|------|--------|
| **Doel** | Reproduceerbare groene build voor `esp32s3`, identiek aan CI-bedoeling. |
| **Precondities** | ESP-IDF geïnstalleerd; `export.sh` actief; toolchain **esp32s3** aanwezig. |
| **Uitvoerstappen** | `cd firmware-v2` → `idf.py set-target esp32s3` → `idf.py build`. Optioneel: zelfde als `.github/workflows/firmware-v2-smoke.yml` (Docker `espressif/idf:v5.4.2`). |
| **Pass** | Build eindigt zonder error; `build/*.bin` aanwezig. |
| **Artifact** | Terminal-log (laatste 30 regels) of CI-run URL + commit SHA. |
| **Status** | *invullen* |
| **Opmerking** | Bij toolchain-fout: geen FAIL van applicatie — vastleggen als omgevingsissue. |

### TM-02 — Boot / sanity

| Veld | Inhoud |
|------|--------|
| **Doel** | Device start stabiel; geen reset-loop; versie zichtbaar. |
| **Precondities** | Flash met build uit TM-01; seriële monitor aan (115200 of projectdefault). |
| **Uitvoerstappen** | Cold boot (power cycle) → 60 s observeren → noteer bootlog tot “app main” / geen panic. Check `PROJECT_VER` in log of WebUI. |
| **Pass** | Geen watchdog/panic; app draait; versie komt overeen met verwachte build. |
| **Artifact** | Seriële logfragment of screenshot WebUI met versie. |
| **Status** | *invullen* |
| **Opmerking** | |

### TM-03 — WiFi / onboarding

| Veld | Inhoud |
|------|--------|
| **Doel** | STA-verbinding of SoftAP-onboarding volgens [ADR-003](ADR-003-wifi-onboarding-v2.md); credentials blijven na reboot (NVS). |
| **Precondities** | Bekende SSID of leeg NVS voor onboarding-test. |
| **Uitvoerstappen** | (A) Verbind met bekende WiFi → wacht op STA IP. Of (B) SoftAP-portal → wijs netwerk toe → reboot → STA actief. Optioneel: forget + herhaal B. |
| **Pass** | STA `GOT_IP` of equivalent; ping/HTTPS naar broker/API slaagt in volgende stappen. |
| **Artifact** | Logregels met IP / WiFi state of screenshot portal. |
| **Status** | *invullen* |
| **Opmerking** | |

### TM-04 — Live WS-feed (Bitvavo)

| Veld | Inhoud |
|------|--------|
| **Doel** | WebSocket naar Bitvavo ontvangt prijs/ticker-updates; geen structurele disconnect in testvenster (≥ 10 min). |
| **Precondities** | TM-03 PASS; markt open of tickers actief. |
| **Uitvoerstappen** | Start app; monitor logs `[WS` / market tick of `market_data` updates; laat ≥10 min lopen. Noteer `ws_feed_observability` in `/api/status.json` indien aanwezig. |
| **Pass** | Periodieke updates zichtbaar; geen reconnect-storm; bij gap: herstel binnen acceptabele tijd (subjectief vastleggen). |
| **Artifact** | Logfragment + JSON-snippet `ws_feed_observability` of “n.v.t. in build”. |
| **Status** | *invullen* |
| **Opmerking** | Zie [M002_NETWORK_BOUNDARIES.md](M002_NETWORK_BOUNDARIES.md). |

### TM-05 — Alert-kern 1m / 5m / confluence

| Veld | Inhoud |
|------|--------|
| **Doel** | Drempel/cooldown/confluence/suppress gedrag in lijn met C1/C2 — minstens observability, bij voorkeur één echte trigger of duidelijke “below threshold”. |
| **Precondities** | TM-04; drempels bekend (Kconfig/NVS). |
| **Uitvoerstappen** | Bekijk `alert_decision_observability`, `alert_engine_runtime_stats`, `alerts_1m`/`alerts_5m`/`alerts_conf_1m5m` in status.json. Optioneel: tijdelijk lagere drempel via toegestane WebUI-write (alleen als al geconfigureerd). |
| **Pass** | Snapshot toont consistente statusstrings; bij fire: NTFY/MQTT/WebUI-ring consistent (C4). |
| **Artifact** | JSON-export of log + screenshot NTFY/MQTT. |
| **Status** | *invullen* |
| **Opmerking** | [C1_FIELD_TEST_1M5M.md](C1_FIELD_TEST_1M5M.md), [C2_EDGE_CASES_1M5M.md](C2_EDGE_CASES_1M5M.md). |

### TM-06 — 30m / 2h sanity

| Veld | Inhoud |
|------|--------|
| **Doel** | Metrics/observability voor 30m en 2h aanwezig; beslisvelden `tf_30m` / `tf_2h` zinvol; `alerts_30m` / `alerts_2h` structuur aanwezig (trigger optioneel). |
| **Precondities** | TM-04; voldoende uptime voor metric warmup (≥ 1 min; 2h-alert niet verplicht in eerste run). |
| **Uitvoerstappen** | Open `/api/status.json` → controleer `metric_30m_observability`, `metric_2h_observability` (of equivalente keys), `tf_30m`, `tf_2h`, `alerts_30m`, `alerts_2h`. |
| **Pass** | Geen crash; velden aanwezig; `ready`/`not_ready` verklaarbaar; bij geen 2h-fire: expliciet “geen trigger in venster” OK. |
| **Artifact** | JSON-snippet (anoniem) of beschrijving. |
| **Status** | *invullen* |
| **Opmerking** | Geen langetermijn-stresstest vereist voor PR-02. |

### TM-07 — WebUI

| Veld | Inhoud |
|------|--------|
| **Doel** | Read-only endpoints bereikbaar; geen 500 op `/` en `/api/status.json`. |
| **Precondities** | TM-03; `CONFIG_WEBUI_ENABLE=y` of runtime aan. |
| **Uitvoerstappen** | Browser: `http://<device-ip>:<port>/` en `/api/status.json` (poort uit Kconfig/NVS). |
| **Pass** | HTTP 200; JSON parseert; HTML rendert secties (alerts, observability). |
| **Artifact** | Screenshot of `curl -s` output (truncated). |
| **Status** | *invullen* |
| **Opmerking** | Geen POST-tests vereist voor minimale release-readiness tenzij scope uitbreidt. |

### TM-08 — OTA

| Veld | Inhoud |
|------|--------|
| **Doel** | Dual-slot OTA-begrip aangetoond of **bewust manual flash** met risico genoteerd. |
| **Precondities** | TM-02; partition table bekend. |
| **Uitvoerstappen** | (A) OTA-upload via WebUI/API volgens `BUILD.md`/docs **of** (B) documenteer: “manual `esptool` flash slot0” + herhaal TM-02. |
| **Pass** | (A) succesvolle swap + boot **of** (B) expliciete keuze + TM-02 opnieuw PASS na flash. |
| **Artifact** | OTA-log of flash-commando + versie-check. |
| **Status** | *invullen* |
| **Opmerking** | Als OTA niet getest: `SKIP` + P1-risico voor PR-03. |

### TM-09 — NTFY / MQTT

| Veld | Inhoud |
|------|--------|
| **Doel** | Minstens één kanaal end-to-end bij alert (of expliciet `N/A` als builds zonder service). |
| **Precondities** | Broker/topic/NTFY volgens Kconfig/NVS; TM-05 desgewenst. |
| **Uitvoerstappen** | Configureer NTFY **of** MQTT → forceer of wacht op alert → ontvang push of zie MQTT-client log `publish`. |
| **Pass** | Payload sluit aan op C4 (`kind` / `Soort:`); geen stille drop (check `service_outbound` logs bij queue full). |
| **Artifact** | Screenshot NTFY of MQTT payload log. |
| **Status** | *invullen* |
| **Opmerking** | Als beide uit: `N/A` + risico registreren in PR-03. |

---

## 4. Eerste invulrun — template

*Kopieer dit blok per testdag of per build.*

| Veld | Waarde |
|------|--------|
| **Run-ID** | PR02-RUN-001 |
| **Datum** | YYYY-MM-DD |
| **Tester** | naam |
| **Board** | ESP32-S3 GEEK |
| **Branch / commit** | `v2/foundation` @ `<git short SHA>` |
| **Firmware (`PROJECT_VER`)** | bv. `3.0.0` (uit `CMakeLists.txt` op dat commit) |
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

## 5. Voorbeeldinvulling (placeholders — geen uitgevoerde run in repo)

| Veld | Waarde |
|------|--------|
| **Run-ID** | PR02-RUN-EXAMPLE |
| **Datum** | *niet uitgevoerd* |
| **Tester** | *—* |
| **Branch / commit** | `v2/foundation` @ *`<vul in na test>`* |
| **Firmware** | *`<PROJECT_VER op commit>`* |

Alle TM-status: *leeg tot eerste echte run*.

---

## 6. Vervolg

- **PR-03:** P0/P1-blockers en known-issues-register vullen op basis van FAIL/SKIP/N/A met risico ([PR01 §7](PR01_RELEASE_READINESS_BASELINE.md#7-release-blockers-en-known-issues-structuur)).
- **PR-04:** RC-checklist wanneer matrix structureel PASS heeft op één referentie-build.

---

## Bronnen

- [BUILD.md](../../firmware-v2/BUILD.md) — lokale build.
- [C5_ROADMAP_30M_2H.md](C5_ROADMAP_30M_2H.md) — 30m/2h scope (geen RWS-03 vereiste).
