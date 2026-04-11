# V2 — leidend werkdocument (projectstatus)

**Status:** actief — formele bron van waarheid voor het V2-traject in deze repository.  
**Laatste inhoudelijke update:** 2026-04-11 (V2-voorbereiding en consolidatie documentatie).  
**Branch voorbereiding:** [`v2/foundation`](https://github.com/jpduhen/ESP32-Crypto-Alert/tree/v2/foundation) (naast `main`).

---

## 1. Huidige stand van zaken

| Onderwerp | Stand |
|-----------|--------|
| V2-voorbereiding | **Gestart** — documentaire en organisatorische basis ligt in de repo. |
| Branch | **`v2/foundation`** bestaat (lokaal en op GitHub); bevat o.a. `firmware-v2/`-placeholder en migratie-/architectuurdocs. |
| Referentieboard | **ESP32-S3 GEEK** is het eerste referentieboard voor V2. |
| Inventarisatie | Een eerste **functionele inventarisatie V1** en een **migratiematrix (draft)** zijn aanwezig (zie §7). |
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

1. **V1 formeel bevriezen als referentie** — geen grote functionele wijzigingen aan V1 zonder expliciete beslissing; wijzigingen alleen ter ondersteuning van vastgelegde releases of noodgevallen.
2. **Migratiematrix aanscherpen en prioriteren** — draft uitbreiden met volgorde, risico’s en afhankelijkheden per blok; afstemmen met dit werkdocument.
3. **V2-mapstructuur concretiseren** richting een **eerste uitvoerbare skeleton** (bijv. ESP-IDF-achtige layout onder `firmware-v2/`, zonder volledige feature-port in deze fase).
4. **Werkdocument-synchronisatie vastleggen** — dit bestand bij elke relevante mijlpaal bijwerken; PR’s die V2-richting raken verwijzen hiernaar waar passend.
5. **Eerste uitvoerbare tickets opstellen** voor Cursor (repo-taken: docs, scaffolding, kleine refactors) — concreet en reviewbaar.

---

## 4. Backlogstatus

| Item | Status |
|------|--------|
| V2-branch aanmaken (`v2/foundation`) | **Klaar** |
| Input voor migratiematrix verzamelen | **Bezig** (draft bestaat; verfijning loopt) |
| V2-mapstructuur uitschrijven / richting eerste skeleton | **Bezig** (placeholder + README; uitwerking IDF-skeleton volgt) |
| Werkdocument-synchronisatie inrichten | **Todo** (proces: dit masterdocument als ankert; cadans: bij milestone of na merge V2-gerelateerde PR’s) |
| Eerste Cursor-tickets | **Todo** |

---

## 5. Werkwijze en governance

| Rol | Verantwoordelijkheid |
|-----|----------------------|
| **Dit GitHub-werkdocument** (`V2_WORKDOCUMENT_MASTER.md`) | **Formele bron van waarheid** voor status, besluiten en prioriteiten van het V2-traject in deze repo. |
| **ChatGPT** (of vergelijkbare assistent) | Levert **structuur**, **analyse** en **voorstellen voor besluiten**; geen automatische overschrijving van dit document zonder menselijke review. |
| **Cursor** | Voert **repo-documentatie** en **codewijzigingen** uit volgens tickets en dit werkdocument; wijzigingen aan dit bestand via duidelijke commits/PR’s. |
| **Gebruiker / maintainer** | Stuurt **prioriteiten**, **validatie** en definitieve **go/no-go** op inhoud en merges. |

---

## 6. Consistentie en scope

- Onderliggende documenten ([§7](#7-verwante-documenten)) vullen details; bij **tegenspraak** prevaleert **dit masterdocument** tot een bewuste herziening.
- **Geen impliciete scope-uitbreiding:** nieuwe boards of stacks horen hier of in een ADR met datum.

---

## 7. Verwante documenten

| Document | Rol |
|----------|-----|
| [V2_OUTGANGSPUNTEN_NL.md](V2_OUTGANGSPUNTEN_NL.md) | Uitgangspunten herbouw, doelen, boards, open punten. |
| [../migration/FUNCTIONAL_INVENTORY_V1_NL.md](../migration/FUNCTIONAL_INVENTORY_V1_NL.md) | Korte functionele inventarisatie V1. |
| [../migration/MIGRATION_MATRIX_V2_DRAFT.md](../migration/MIGRATION_MATRIX_V2_DRAFT.md) | Migratiematrix (concept). |
| [../boards/README.md](../boards/README.md) | Board-overzicht; [ESP32-S3-GEEK.md](../boards/ESP32-S3-GEEK.md) referentieboard. |
| [../../firmware-v2/README.md](../../firmware-v2/README.md) | Placeholder en beoogde ESP-IDF-layout voor nieuwe firmware. |

---

## 8. Revisiegeschiedenis (kort)

| Datum | Notitie |
|-------|---------|
| 2026-04-11 | Eerste versie als leidend werkdocument; consolidatie V2-voorbereiding. |
