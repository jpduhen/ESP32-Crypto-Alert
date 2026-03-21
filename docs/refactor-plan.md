# Refactorplan ESP32 Crypto Alert

## 1. Doel

- **Primair:** stabiele, correcte werking op **JC3248W535CIY + AXS15231B** (`PLATFORM_ESP32S3_JC3248W535`), `esp_lcd`-backend waar van toepassing.
- **Secundair:** andere **actief onderhouden** **ESP32-S3**-varianten (o.a. SUPERMINI, GEEK, LCDWIKI) waar de codebase dat nu al doet — geen breuk zonder expliciete beslissing.
- **`PLATFORM_ESP32S3_4848S040`:** **niet meer in de codebasis** — geen actief, latent of buildbaar pad; de 4848-platformcluster (define, PINS, UI) is uitgefaseerd. Gedeelde trend/returnlogica (`ret_1d`, `ret_7d`, …) blijft bewust bestaan.
- **Legacy:** **CYD24 / CYD28** gefaseerd en **compile-time gecontroleerd** uitfaseren (`platform_config.h`); **TTGO** is uit de codebasis verwijderd (geen `PLATFORM_TTGO` meer). Geen “big bang”.
- **Prioriteit:** **stabiliteit en correctheid vóór** cosmetische opschoning.

---

## 2. Scope

### Wel binnen deze refactor

- Correctheid datastroom ↔ UI (o.a. 2h, footer) voor de **JC3248-referentielijn** en andere **actief gebruikte** moderne ESP32-S3-platforms — **4848S040-code is verwijderd**; resterende 4848-verwijzingen zitten vooral in documentatie/historie (zie §4, werklog).
- Inventarisatie en documentatie van legacy-paden; **kleine, gereviewde** patches.
- Commentaar en documentatie **neutraliseren** waar misleidend (CYD/TTGO/JC3248).
- **Veilige** verwijdering van code die **alleen** legacy boards raakt, na inventaris + test.

### Expliciet nog niet (of alleen na expliciet besluit)

- **4848-specifieke** refactors of feature-pariteit: **niet meer van toepassing** (geen 4848-platformcode meer in de tree).
- Grote herstructurering van `ESP32-Crypto-Alert.ino` of UI-module in één keer.
- Verwijderen van **Arduino_GFX** / display-abstractie zonder impactanalyse.
- Nieuwe features; dit traject is **onderhoud, correctheid, opschoning**.

---

## 3. Stabiele referentie

| Referentie | Opmerking |
|------------|-----------|
| **`v5.53-jc3248-stable`** | Git-tag als terugvalpunt voor JC3248-lijn. |
| **Huidige `main` (richting)** | AXS15231B-backend, TE-sync, NTFY Bearer-token route, WS disconnect/restore rond NTFY, gefaseerde boot-gates — *exacte commit-SHA’s: `d5f258b` (HEAD ten tijde van doc-update)* |

*Placeholder:* bij elke grotere mijlpaal: tag + korte notitie hier of in release-notes koppelen.

---

## 4. Reeds genomen besluiten

| Besluit | Status |
|---------|--------|
| Legacy boards **compile-time gated** in `platform_config.h` (o.a. `CRYPTO_ALERT_ENABLE_LEGACY_BOARDS` / boardmacros) | **Actief** |
| Eerst **inventariseren** (runtime/UI-paden, comments), daarna **gericht** verwijderen | **Actief** |
| Geen **blinde** verwijdering van Arduino_GFX-gerelateerde code; altijd impact op JC3248/S3 eerst afwegen | **Actief** |
| **Cross-file verificatie** (2h-datastroom, footer JC3248) vóór agressieve comment/code-cleanup | **Uitgevoerd** (analyse; zie WP3) |
| Kleine **bugfix** `ret_2h` ↔ `prices[3]` voor JC3248 via sync in `.ino` (zelfde patroon als CYD) | **Toegepast in code** (`d5f258b`) — *hardware smoke-test: uitkomst niet in repo vastgelegd; zie WP4* |
| **`PLATFORM_ESP32S3_4848S040`** | **Verwijderd uit de codebasis** (platform-define, PINS, 4848-UI-cluster); eerder: buiten actieve refactorscope. Trendlabels en gedeelde `ret_1d` / `ret_7d` (e.d.) blijven ongewijzigd beschikbaar. |

---

## 5. Open risico's en onzekerheden

| Risco | Toelichting |
|-------|-------------|
| **`ret_2h` vs `prices[3]` (JC3248)** | Geadresseerd in code (`d5f258b`) door `PLATFORM_ESP32S3_JC3248W535` toe te voegen aan dezelfde `#if`-blokken als `prices[3] = ret_2h` (warm-start + fetch). *Hardware-bevestiging van de 2h-tegel nog niet in dit document vastgelegd.* |
| **`averagePrices[3]` (JC3248)** | Live pad voor gemiddelde kan afwijken van CYD-only regressie-updates; mogelijk **niet** foutkritisch voor %-tegel; *follow-up alleen als UX/consistentie dat vereist*. |
| **Verouderde comments** (footer, CYD, TTGO) | Meerdere plekken: UI volgt gedeelde `#else`-tak terwijl comment “CYD” zegt — *neutraliseren in WP5*. |
| **Gedeelde 4-symbol / 2h-logica** | JC3248 deelt macro’s met o.a. LCDWIKI; **4848** heeft geen codepad meer (zie §4, werklog). |
| **Dode preprocessor-tak** (o.a. `UIController::createHeaderLabels`) | *Placeholder: apart verifiëren en eventueel opruimen zonder gedrag te wijzigen.* |

---

## 6. Werkpakketten

| ID | Titel | Status | Omschrijving |
|----|-------|--------|--------------|
| **WP1** | Legacy boardselectie gegated | `gereed` | `platform_config.h`: legacy alleen met expliciete gate. |
| **WP2** | Inventarisatie runtime/UI legacy in `.ino` | `gereed` | `#if`-clusters, buffers, LVGL, comments — inventaris als basis voor WP6. |
| **WP3** | Cross-file verificatie 2h + footer JC3248 | `gereed` | Datastroom `ret_2h` / `prices[]` / `findMinMaxInLast2Hours`; footer via `createFooter` `#else` + `updateFooter`. |
| **WP4** | Kleine bugfix 2h-kaart JC3248 (`prices[3]` sync) | `bezig` | Patch gemerged (`d5f258b`); *hardware-smoketest: uitkomst nog niet vastgelegd — WP4 blijft `bezig` tot operator bevestigt (→ `gereed` / `geblokkeerd`)*. |
| **WP5** | Comment-neutralisatie / doc-opruiming | `todo` | Footer/2h/CYD/TTGO-comments en dit document bijwerken. |
| **WP6** | Veilige verwijdering zuivere CYD/TTGO-legacy | `bezig` | **TTGO-cluster:** `PLATFORM_TTGO` + `PINS_TTGO_T_Display.h` verwijderd (2026-03). **CYD:** nog gated; *geen impact JC3248*. |
| *[placeholder]* | *LCDWIKI / andere S3-boards gelijk trekken met JC3248 waar nodig* | `todo` | *Alleen indien uit analyse blijkt dat zelfde bug geldt.* |

---

## 7. Testmatrix

Compacte regressie-checklist (aanvullen per release):

- [ ] Build met **`PLATFORM_ESP32S3_JC3248W535`**
- [ ] Boot stabiel (geen watchdog-reset, geen hang na WiFi)
- [ ] Display init: beeld, rotatie, backlight zoals verwacht
- [ ] Kaarten **1m / 30m / 2h**: waarden plausibel, 2h-% niet structureel 0 bij geldige `ret_2h`
- [ ] **2h** min/max/diff labels nog zinvol na data-opbouw
- [ ] **Footer**: RSSI/RAM/IP/versie zichtbaar en updatend
- [ ] **WS**: connect/disconnect/recovery (incl. NTFY-context indien van toepassing)
- [ ] **API** polling / mutex geen zichtbare regressie
- [ ] **MQTT** (indien enabled): publish OK
- [ ] **NTFY**: send + Bearer-route waar geconfigureerd
- [ ] Geen regressie door cleanup (herhalen na WP5/WP6)

---

## 8. Niet nu doen

- Grote split van `ESP32-Crypto-Alert.ino` zonder voorbereid plan.
- Verwijderen van legacy `#ifdef`-takken **zolang** gate nog buildbare legacy ondersteunt en er geen test is.
- Uniform “alles op `ret_*` direct in UI” zonder eerst `prices[]`-contract vast te leggen.
- **4848-platformcode:** reeds verwijderd (zie §4, werklog); geen aparte “wacht op besluit”-actie meer.
- *Placeholder: [eigen items invullen]*

---

## 9. Werklog / changelog

*Kort logboek van refactor-stappen; nieuwste bovenaan.*

| Datum | Notitie |
|-------|---------|
| 2026-03-19 | **4848:** platformcode (`PLATFORM_ESP32S3_4848S040`, PINS/config, 4848-UI-cluster) **verwijderd** uit de codebasis; trend/`ret_1d`/`ret_7d` en gedeelde returnlogica blijven; restanten vooral docs/historie. |
| 2026-03-21 | Scopebeslissing: **`PLATFORM_ESP32S3_4848S040`** niet meer actief doelplatform; **buiten actieve refactorscope**; **robuuste 2h-semantiekfix** richt zich op **moderne actieve S3** (o.a. JC3248), niet op 4848. *Code-verwijdering: zie 2026-03-19.* |
| 2026-03-21 | Refactorplan: repo-HEAD `d5f258b` (JC3248 `prices[3]`↔`ret_2h` sync) genoteerd; **hardware-smoketest 2h-fix: uitkomst niet in repo beschikbaar** — WP4 niet op `gereed`/`geblokkeerd` gezet. |
| *YYYY-MM-DD* | *Document aangemaakt: refactorplan als levend spoorboek.* |

---

*Einde document — bij elke merge van refactorwerk: sectie 4/6/9 bijwerken.*
