# Impactanalyse: Verwijderen legacy CYD-platforms

**Doel:** Inventarisatie van het verwijderen van PLATFORM_CYD24, PLATFORM_CYD28_1USB, PLATFORM_CYD28_2USB en afgeleide macro PLATFORM_CYD28. Geen codewijzigingen in dit document; alleen analyse.

**Gerelateerde macros (verdwijnen mee met CYD):**
- `PLATFORM_CYD28` – alias, gedefinieerd in platform_config.h wanneer CYD28_1USB of CYD28_2USB actief is.
- `PLATFORM_CYD24_INVERT_COLORS` – alleen in PINS_CYD-ESP32-2432S024.h.
- `PLATFORM_CYD28_INVERT_COLORS` – alleen in PINS_CYD-ESP32-2432S028-2USB.h.

---

## 1. Overzicht per referentie / bestand

### platform_config.h
| Regels | Functie/blok | Type gebruik |
|--------|----------------|--------------|
| 5–7 | Commented-out platform defines | build/selectie |
| 15–17 | `#if defined(PLATFORM_CYD28_1USB) \|\| defined(PLATFORM_CYD28_2USB)` → `#define PLATFORM_CYD28` | build/selectie, alias |
| 48 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| defined(PLATFORM_TTGO)` → DEBUG_CALCULATIONS 0 | build/selectie, logging |
| 111–139 | `#elif defined(PLATFORM_CYD24)` – device name, PINS include, MQTT prefix, CHART_*, FONT_*, SYMBOL_COUNT 4 | board-selectie, pinmapping, displaydriver |
| 142–206 (ongeveer) | `#elif defined(PLATFORM_CYD28)` – idem + keuze 1USB/2USB PINS | board-selectie, pinmapping, displaydriver, kleurinversie |
| 341 | `#error "Please define ... PLATFORM_CYD24, PLATFORM_CYD28 ..."` | build/selectie |

- **PLATFORM_CYD28:** Alleen als alias na keuze CYD28_1USB of CYD28_2USB. Geen eigen top-level define in config.
- **Verwijderen:** CYD24/CYD28-blokken en CYD uit #error; DEBUG_CALCULATIONS-condition alleen PLATFORM_TTGO laten (geen impact op andere platforms).

---

### UNIFIED-LVGL9-Crypto_Monitor.ino
| Regels | Functie/blok | Type gebruik |
|--------|----------------|--------------|
| 314 | `#elif defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| defined(PLATFORM_ESP32S3_LCDWIKI_28)` – `symbols[]` met SYMBOL_2H_LABEL (4 symbolen) | build/selectie, UI data |
| 2282 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| defined(PLATFORM_ESP32S3_4848S040)` – prices[3] = ret_2h na warm-start | build/selectie, runtime init |
| 2729 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28)` – wsMinFreeHeap 14000, wsMinLargestBlock 1200 | build/selectie, runtime (warm-start heap guard) |
| 3270–3272 | Forward decl `findMinMaxInLast2Hours` – CYD + 4848S040 | build/selectie |
| 6106 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28)` – averagePrices[3] = avg | runtime (2h average) |
| 6644–6651 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| ... \|\| PLATFORM_ESP32S3_4848S040` – definitie findMinMaxInLast2Hours() | build/selectie, helper |
| 6660 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| defined(PLATFORM_ESP32S3_4848S040)` – computeTwoHMetrics() avg2h / findMinMaxInLast2Hours | runtime (2h metrics) |
| 7114 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28)` – availableMinutes >= 2 herberekening | runtime (2h return) |
| 7266 | `#if defined(PLATFORM_CYD24) \|\| defined(PLATFORM_CYD28) \|\| defined(PLATFORM_ESP32S3_4848S040)` – ret_2h berekening | runtime (2h return) |
| 7699–7704 | `PLATFORM_CYD24_INVERT_COLORS` / `PLATFORM_CYD28_INVERT_COLORS` – gfx->invertDisplay(true/false) | displaydriver, kleurinversie |
| 7756–7770 | CYD: useDoubleBuffer = false; bufLines CYD_BUF_LINES_NO_PSRAM / 40 | LVGL/displaybuffers, PSRAM/bufferallocatie |
| 7834–7837 | boardName = "CYD24" / "CYD28" | logging/debug |
| 7914–7925 | `#ifdef PLATFORM_CYD24` – esp_task_wdt_deinit() (watchdog uit voor CYD24) | runtime (watchdog) |
| 8266–8270 | CYD: lv_refr_now(disp) direct na UI create | LVGL/displaybuffers |
| 8283–8295 | CYD: guard arrays vóór task start (fiveMinutePrices, minuteAverages) | runtime (guard) |
| 8822–8825 | `#ifdef PLATFORM_CYD24` – lv_refr_now(disp) ergens in loop/render | LVGL/displaybuffers |

- **Gedeelde codepaden:** Regels 314, 2282, 6644, 6660, 7266 delen CYD met PLATFORM_ESP32S3_LCDWIKI_28 of PLATFORM_ESP32S3_4848S040. Alleen CYD uit de condition halen; S3-platforms blijven. Geen blokken verwijderen.
- **Alleen CYD:** 2729, 6106, 7114, 7756, 7775, 7834–7837, 7914, 8266, 8283, 8822 – kunnen worden verwijderd of omgezet naar alleen-TTGO waar van toepassing (bijv. heap guards).

---

### src/UIController/UIController.cpp
| Regels | Functie/blok | Type gebruik |
|--------|----------------|--------------|
| 204–206 | Extern decl `findMinMaxInLast2Hours` – CYD + LCDWIKI + 4848S040 | build/selectie |
| 978 | Conditie voor 2h-labels/UI (CYD + LCDWIKI + 4848S040) | build/selectie, UI |
| 1874 | Idem | build/selectie, UI |
| 1897 | Idem | build/selectie, UI |
| 1941 | CYD + 4848S040 (geen LCDWIKI) | build/selectie, UI |
| 2032 | CYD + LCDWIKI + 4848S040 | build/selectie, UI |
| 2155 | CYD + LCDWIKI + 4848S040 | build/selectie, UI |
| 2170 | CYD + LCDWIKI + 4848S040 | build/selectie, UI |
| 2376–2395 | useDoubleBuffer/bufLines voor CYD vs rest | LVGL/displaybuffers, PSRAM/bufferallocatie |
| 2453–2456 | boardName "CYD24"/"CYD28" | logging/debug |

- Alle CYD-vermeldingen zitten in `#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || ...`. CYD uit condition halen; PLATFORM_ESP32S3_LCDWIKI_28 en PLATFORM_ESP32S3_4848S040 blijven. Geen risico voor andere platforms als alleen CYD wordt weggelaten.

---

### src/WebServer/WebServer.cpp
| Regels | Functie/blok | Type gebruik |
|--------|----------------|--------------|
| 272–275 | platformName "CYD24"/"CYD28" voor HTML | build/selectie, UI |
| 1957–1965 | CYD: geen sPageCache.reserve(16000) (DRAM-besparing) | netwerk/webserver, PSRAM/bufferallocatie |

- Geen gedeelde condition met andere platforms; verwijderen CYD-tak = alleen “CYD24”/“CYD28” naam en reserve-logica weg. Andere platforms ongewijzigd.

---

### PINS-bestanden (alleen CYD)
- **PINS_CYD-ESP32-2432S024.h** – Definieert PLATFORM_CYD24_INVERT_COLORS; wordt alleen geïnclude bij PLATFORM_CYD24.
- **PINS_CYD-ESP32-2432S028-1USB.h** – Definieert PLATFORM_CYD28; include bij PLATFORM_CYD28_1USB.
- **PINS_CYD-ESP32-2432S028-2USB.h** – Definieert PLATFORM_CYD28 en PLATFORM_CYD28_INVERT_COLORS; include bij PLATFORM_CYD28_2USB.

Type: pinmapping, displaydriver, kleurinversie. Geen andere platforms; veilig te verwijderen samen met de platform-opties.

---

### upload.sh / upload_and_monitor.sh
| Bestand | Regels | Type gebruik |
|---------|--------|--------------|
| upload.sh | 22–26 | Grep op PLATFORM_CYD24 / PLATFORM_CYD28 → PLATFORM_NAME + PARTITION_SCHEME | build/selectie |
| upload_and_monitor.sh | 20 | Grep PLATFORM_CYD28 → board | build/selectie |

- Alleen gebruikt om CYD-board te herkennen. Verwijderen = deze takken vallen weg; andere platforms ongewijzigd.

---

### Documentatie (alleen tekst)
- **docs/03-Hardware-Requirements.md**, **docs/03-Hardwarevereisten.md** – CYD24/CYD28 in hardwaretabel en beschrijving.
- **docs/04-Installatie.md**, **docs/04-Installation.md** – PSRAM “Disabled voor CYD24/CYD28”, boardlijst.
- **docs/05_CONFIGURATION.md**, **docs/05_CONFIGURATION_EN.md** – Platformlijst met CYD24/CYD28_1USB/CYD28_2USB.
- **docs/10-Geavanceerd-Gebruik-en-Aanpassingen.md**, **docs/10-Advanced-Usage-and-Customization.md** – Voorbeeld platform define.
- **docs/00_OVERVIEW.md**, **docs/00_OVERVIEW_EN.md** – “CYD24/CYD28” in platform_config-beschrijving.
- **docs/CODE_INDEX.md**, **docs/CODE_ANALYSIS.md** – Uitleg CYD-varianten, PINS, inversie.
- **docs/CYD_DRAM_OPTIMALISATIE_SAMENVATTING.md** – Voorbeelden `#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)`.

Type: docs/comments. Geen runtime; aanpassen of verwijderen voor consistentie.

---

## 2. Samenvattende tabel

| Macro/platform | Aantal referenties (ca.) | Alleen compile-time? | Runtime-effect? | Kritisch voor andere platforms? | Opmerking |
|----------------|--------------------------|------------------------|-----------------|----------------------------------|-----------|
| PLATFORM_CYD24 | ~25 | Ja | Nee (alleen CYD-builds) | Laag | Overal te verwijderen of uit condition halen; TTGO blijft waar nodig (DEBUG_CALCULATIONS, buffers). |
| PLATFORM_CYD28_1USB | 4 | Ja | Nee | Laag | Alleen platform_config + PINS include + docs. |
| PLATFORM_CYD28_2USB | 4 | Ja | Nee | Laag | Idem + PLATFORM_CYD28_INVERT_COLORS. |
| PLATFORM_CYD28 | ~35 | Ja | Nee | Laag | Alias; verdwijnt als CYD28_1USB/2USB weg zijn. Overal waar “CYD28” staat: uit condition halen. |
| PLATFORM_CYD24_INVERT_COLORS | 2 | Ja | Nee | Laag | Alleen in PINS + .ino invertDisplay. |
| PLATFORM_CYD28_INVERT_COLORS | 2 | Ja | Nee | Laag | Idem. |

- **Alleen compile-time:** Alle gebruik is `#if`/`#ifdef` of grep in scripts; geen CYD-specifieke runtime-logica die andere boards raakt.
- **Runtime-effect:** Alleen binnen CYD-builds (watchdog uit, lv_refr_now, heap guards, geen page cache reserve). Geen andere platforms afhankelijk van deze paden.
- **Kritisch voor andere platforms:** Laag. Overal waar CYD samen met ESP32S3_LCDWIKI_28 of ESP32S3_4848S040 staat: alleen CYD uit de condition halen; S3-code blijft hetzelfde.

---

## 3. Risico-inschatting

- **Gedeelde codepaden:**  
  - `.ino` symbols[] (314), warm-start prices[3] (2282), findMinMaxInLast2Hours (6644, 6660), computeTwoHMetrics (6660), ret_2h (7266).  
  - UIController: findMinMaxInLast2Hours extern + alle 2h-UI-blokken.  
  **Risico:** Als je per ongeluk het hele `#if`-blok verwijdert in plaats van alleen `PLATFORM_CYD24` en `PLATFORM_CYD28` uit de condition, dan breken PLATFORM_ESP32S3_LCDWIKI_28 en PLATFORM_ESP32S3_4848S040 (2h-symbolen en 2h-logica).  
  **Mitigatie:** Alleen de twee CYD-macro’s uit de condition verwijderen; `#if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_4848S040)` (evt. met andere S3) laten staan.

- **platform_config.h #error (341):**  
  Na verwijderen CYD moet de #error-lijst korter (zonder PLATFORM_CYD24, PLATFORM_CYD28). Anders blijft de build geldig voor alle overige platforms.

- **DEBUG_CALCULATIONS (platform_config.h 48):**  
  Condition is nu `PLATFORM_CYD24 || PLATFORM_CYD28 || PLATFORM_TTGO`. Na CYD-verwijdering alleen `PLATFORM_TTGO` laten voor de “geen PSRAM, DRAM-besparing”-tak. ESP32-S3 blijft in de `#else`-tak. Geen risico.

- **CYD-only blokken (.ino 2729, 6106, 7114, 7756, 7775, 7914, 8266, 8283, 8822; UIController 2376, 2395, 2453–2456; WebServer 272–275, 1957–1965):**  
  Gehele `#if`-blokken kunnen worden verwijderd of (waar logisch) vervangen door een TTGO-only variant. Geen impact op S3 of andere boards.

- **Upload-scripts:**  
  Alleen CYD-takken verwijderen; rest blijft. Geen risico.

---

## 4. Voorstel verwijdering in stappen

- **Fase 1 – Docs/comments/build-flags**  
  - In alle docs CYD24/CYD28 vermeldingen schrappen of aanpassen (hardware, installatie, configuratie, overzicht, CODE_INDEX, CODE_ANALYSIS, CYD_DRAM_OPTIMALISATIE_SAMENVATTING).  
  - In platform_config.h: commented-out `#define PLATFORM_CYD24` en CYD28_1USB/2USB + het blok `#if defined(PLATFORM_CYD28_1USB) || defined(PLATFORM_CYD28_2USB)` / `#define PLATFORM_CYD28` verwijderen.  
  - Risico: minimaal.

- **Fase 2 – Platformselectie en #error**  
  - In platform_config.h: hele `#elif defined(PLATFORM_CYD24)` en `#elif defined(PLATFORM_CYD28)` blokken (inclusief PINS-includes en device names) verwijderen.  
  - #error-regel aanpassen: PLATFORM_CYD24 en PLATFORM_CYD28 uit de lijst halen.  
  - Upload-scripts: CYD24/CYD28-takken verwijderen.  
  - Risico: laag; andere platforms niet in die blokken.

- **Fase 3 – Board-specifieke init en display**  
  - .ino: CYD-inversie (PLATFORM_CYD24_INVERT_COLORS / PLATFORM_CYD28_INVERT_COLORS) verwijderen.  
  - .ino: boardName "CYD24"/"CYD28" (7834–7837, 7914 watchdog, 8266 lv_refr_now, 8283 array-guard, 8822 lv_refr_now) – hele CYD-takken verwijderen.  
  - .ino: useDoubleBuffer/bufLines (7756, 7775) – CYD-takken verwijderen; TTGO en S3 blijven.  
  - UIController: useDoubleBuffer/bufLines (2376, 2395) en boardName (2453–2456) – CYD uit condition of takken verwijderen.  
  - Risico: laag; geen gedeelde code met S3 in deze takken.

- **Fase 4 – Gedeelde conditions (CYD + S3)**  
  - .ino: regels 314, 2282, 3270–3272, 6644, 6660, 7266 – alleen `PLATFORM_CYD24` en `PLATFORM_CYD28` uit de `#if`/`#elif` halen; `PLATFORM_ESP32S3_LCDWIKI_28` en `PLATFORM_ESP32S3_4848S040` behouden.  
  - .ino: 2729, 6106, 7114 – hele CYD-only blokken verwijderen.  
  - UIController: alle CYD-conditions – alleen CYD verwijderen uit de `defined(...)`-lijst; LCDWIKI en 4848S040 blijven.  
  - WebServer: platformName CYD24/CYD28 (272–275) en sPageCache.reserve (1957–1965) – CYD-takken verwijderen.  
  - Risico: midden als je per ongeluk de hele condition wist; door alleen CYD te schrappen blijft het veilig.

- **Fase 5 – PINS en cleanup**  
  - Bestanden verwijderen: PINS_CYD-ESP32-2432S024.h, PINS_CYD-ESP32-2432S028-1USB.h, PINS_CYD-ESP32-2432S028-2USB.h.  
  - Eventuele resterende commentaar over CYD opruimen.  
  - Optioneel: CYD_DRAM_OPTIMALISATIE_SAMENVATTING.md hernoemen of archiveren (CYD-specifiek).  
  - Risico: laag.

---

## 5. Bestanden met belangrijkste risico’s

- **UNIFIED-LVGL9-Crypto_Monitor.ino** – Meeste CYD-referenties; meerdere gedeelde conditions met PLATFORM_ESP32S3_LCDWIKI_28 en PLATFORM_ESP32S3_4848S040. Hier alleen CYD uit de conditions halen, geen hele blokken weghalen waar S3 nog van afhankelijk is.  
- **src/UIController/UIController.cpp** – Zelfde patroon: overal waar CYD samen met LCDWIKI/4848S040 staat, alleen CYD uit de `#if` halen.  
- **platform_config.h** – CYD-blokken en #error aanpassen; DEBUG_CALCULATIONS-condition tot PLATFORM_TTGO beperken.

Overige bestanden (WebServer, upload-scripts, PINS, docs) zijn voornamelijk CYD-only of documentatie; risico laag mits geen copy-pastefouten.

---

## 6. Kort antwoord op de gestelde vragen

- **Alleen board-selectie?** Ja; alle gebruik is ofwel board-selectie (device name, PINS, MQTT prefix, partition in scripts) ofwel selectie van compile-time gedrag (buffers, inversie, 2h-symbolen, guards).  
- **Runtime-effect?** Alleen binnen CYD-builds (watchdog, lv_refr_now, heap/array guards, geen page cache). Geen andere platforms.  
- **Top-level vs alias?** PLATFORM_CYD24 en PLATFORM_CYD28_1USB/2USB zijn top-level in platform_config.h; PLATFORM_CYD28 is een alias (gedefinieerd als CYD28_1USB of CYD28_2USB actief is).  
- **Andere platforms geraakt?** Niet als je alleen CYD uit de conditions verwijdert en geen gedeelde S3-code wist. Hoogste aandacht in .ino en UIController.

Dit document is puur een impactanalyse; er zijn nog geen codewijzigingen doorgevoerd.
