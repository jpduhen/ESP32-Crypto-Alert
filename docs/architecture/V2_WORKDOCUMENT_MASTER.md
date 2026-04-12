# V2 — werkdocument (governance & repo-overzicht)

**Status:** actief — vastlegging van **governance**, **rollen** en **repo-overzicht** voor het V2-traject.  
**Operationele status** (besluiten, prioriteiten, migratiematrix-stuur, ticketstatus): primair in [**firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md**](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) — bij spanning prevaleert dat document tot expliciete herziening hier.  
**Laatste inhoudelijke update:** 2026-04-11 (hiërarchie docs / skeletonfase).  
**Branch:** [`v2/foundation`](https://github.com/jpduhen/ESP32-Crypto-Alert/tree/v2/foundation).

---

## 1. Huidige stand van zaken

| Onderwerp | Stand |
|-----------|--------|
| V2-voorbereiding | **Gestart** — documentaire basis + **eerste ESP-IDF-skeleton** onder `firmware-v2/` (buildbaar met lokale ESP-IDF-toolchain). |
| Branch | **`v2/foundation`** — o.a. `firmware-v2/` met componenten (zie [V2_SKELETON_NOTES.md](V2_SKELETON_NOTES.md)). |
| Referentieboard | **ESP32-S3 GEEK** — eerste concrete BSP (`bsp_s3_geek`). |
| Inventarisatie | Functionele inventarisatie V1 + migratiematrix (draft); skeleton mapped op matrix (zie skeletonnotities). |
| V1 | Blijft de **functionele referentie** voor gedrag en features tot V2 dat expliciet vervangt. |

---

## 2. Besluiten (vastgelegd)

Deze besluiten zijn leidend tenzij ze in een latere revisie van dit document worden gewijzigd.

| # | Besluit |
|---|---------|
| B1 | **V2** wordt een **gecontroleerde herbouw** (geen ongeplande big-bang in productiecode). |
| B2 | **Focusboards:** ESP32-S3 **GEEK**, **LCDWIKI**, **JC3248W535** (in die volgorde qua prioriteit). |
| B3 | **Eerste referentieboard:** **ESP32-S3 GEEK**. |
| B4 | **ESP-IDF** is de **voorkeursrichting** voor build/tooling; exacte versie en migratiepad volgen bij start van implementatie. |
| B5 | **CYD** en **TTGO** hebben **geen prioriteit** meer voor V2 (heropening alleen bij expliciet besluit). |
| B6 | Waardevolle V1-domeinen (o.a. WebUI-concept, OTA, WS/REST waar relevant, NTFY, MQTT) blijven **inhoudelijk** leidend als eisen aan V2; technische vorm wordt per module beslist. |

*Uitgewerkte uitgangspunten en randvoorwaarden:* [V2_OUTGANGSPUNTEN_NL.md](V2_OUTGANGSPUNTEN_NL.md).

---

## 3. Prioriteit nu (actueel)

Wat nu het meeste aandacht vraagt binnen het V2-traject:

1. **V1 formeel bevriezen als referentie** — geen grote functionele wijzigingen aan V1 zonder expliciete beslissing.
2. **Field-test T-103** — WiFi + Bitvavo op GEEK; valideren REST/WS en snapshot-gedrag (zie ADR-002).
3. **M-002 verdiepen** — backoff, queues, scheiding met latere MQTT/NTFY/WebUI.
4. **Migratiematrix aanscherpen** — net-runtime model t.o.v. nieuwe componenten.
5. **Werkdocument-synchronisatie** — primair `firmware-v2/`-werkdocument; dit bestand bij governance-mijlpalen.

---

## 4. Backlogstatus

| Item | Status |
|------|--------|
| V2-branch aanmaken (`v2/foundation`) | **Klaar** |
| Input voor migratiematrix verzamelen | **Bezig** (draft + skeleton-mapping in [V2_SKELETON_NOTES.md](V2_SKELETON_NOTES.md)) |
| ESP-IDF skeleton (`firmware-v2/`, S-001…S-007) | **Klaar** |
| T-101 reproduceerbare build + CI-smoke (IDF **v5.4.2**) | **Klaar** (zie `firmware-v2/BUILD.md`, `.github/workflows/firmware-v2-smoke.yml`) |
| Werkdocument-synchronisatie inrichten | **Bezig** |
| Display bring-up GEEK — `esp_lcd` ST7789 + volledige schermvulling (T-102) | **Klaar** (code); **hardwarecheck** volledig groen zonder restanten |
| LVGL op GEEK | **Todo** (optioneel) |
| Exchange / netwerklaag Bitvavo (T-103) | **Klaar** (code + ADR-002); field-test gebruiker |

---

## 5. Werkwijze en governance

| Rol | Verantwoordelijkheid |
|-----|----------------------|
| **Werkdocument `firmware-v2/v_2_herbouw_…`** | **Primaire bron** voor actuele **status, besluiten, prioriteiten** en **migratierichting** (dagelijks gebruik). |
| **Dit bestand** (`V2_WORKDOCUMENT_MASTER.md`) | **Governance** (rollen, backlog-overzicht, consistentie); geen duplicaat van de volledige matrix. |
| **ChatGPT** (of vergelijkbare assistent) | **Structuur en analyse**; werkdocument in `firmware-v2/` bijwerken na mijlpalen. |
| **Cursor** | **Code en technische docs** in de repo; commits/PR’s refereren aan het `firmware-v2`-werkdocument waar relevant. |
| **Gebruiker / maintainer** | **Prioriteiten**, **validatie**, **go/no-go**. |

---

## 6. Consistentie en scope

- Onderliggende documenten ([§7](#7-verwante-documenten)) vullen details; bij **tegenspraak** over status/prioriteiten prevaleert het [**werkdocument in `firmware-v2/`**](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) tot een bewuste herziening.
- **Geen impliciete scope-uitbreiding:** nieuwe boards of stacks horen hier of in een ADR met datum.

---

## 7. Verwante documenten

| Document | Rol |
|----------|-----|
| [V2_OUTGANGSPUNTEN_NL.md](V2_OUTGANGSPUNTEN_NL.md) | Uitgangspunten herbouw, doelen, boards, open punten. |
| [V2_SKELETON_NOTES.md](V2_SKELETON_NOTES.md) | Skeletonfase: tickets S-001…S-007, ontwerpkeuzes, open punten. |
| [../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) | Regie / strategisch werkdocument (Jan Pieter + AI + Cursor). |
| [../migration/FUNCTIONAL_INVENTORY_V1_NL.md](../migration/FUNCTIONAL_INVENTORY_V1_NL.md) | Korte functionele inventarisatie V1. |
| [../migration/MIGRATION_MATRIX_V2_DRAFT.md](../migration/MIGRATION_MATRIX_V2_DRAFT.md) | Migratiematrix (concept). |
| [../boards/README.md](../boards/README.md) | Board-overzicht; [ESP32-S3-GEEK.md](../boards/ESP32-S3-GEEK.md) referentieboard. |
| [../../firmware-v2/README.md](../../firmware-v2/README.md) | ESP-IDF-project (V2 firmware builden). |

---

## 8. Revisiegeschiedenis (kort)

| Datum | Notitie |
|-------|---------|
| 2026-04-11 | Eerste versie als leidend werkdocument; consolidatie V2-voorbereiding. |
| 2026-04-11 | Skeletonfase: ESP-IDF `firmware-v2/`, notities in `V2_SKELETON_NOTES.md`. |
