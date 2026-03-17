# Phase 1: Short-term notification usefulness – Change-set proposal

**Doel:** 1m / 5m / 30m nuttiger maken zonder notification spam.  
**Scope:** Alleen volume-event cooldown en confluence-suppressie. Geen code wijzigen in dit document; alleen voorstel.

---

## 1. Volume-event cooldown

### 1.1 Exacte locaties

| Wat | Bestand | Regels |
|-----|---------|--------|
| Macro | `src/AlertEngine/AlertEngine.cpp` | **81** |
| State | `src/AlertEngine/AlertEngine.cpp` | **183** |
| Check-functie | `src/AlertEngine/AlertEngine.cpp` | **226–229** |
| Gebruik: confluence | `src/AlertEngine/AlertEngine.cpp` | **492**, **555** |
| Gebruik: 1m spike | `src/AlertEngine/AlertEngine.cpp` | **818**, **879** |
| Gebruik: 30m move | `src/AlertEngine/AlertEngine.cpp` | **972**, **982** |
| Gebruik: 5m move | `src/AlertEngine/AlertEngine.cpp` | **1042**, **1102** |

- **Macro:** `VOLUME_EVENT_COOLDOWN_MS` = `120000UL` (120 s).
- **State:** `static unsigned long lastVolumeEventMs`; gezet bij **succesvolle** send van: 1m spike, 5m move, 30m move, confluence.
- **Check:** `volumeEventCooldownOk(now)` = true als `lastVolumeEventMs == 0` of `(now - lastVolumeEventMs) >= VOLUME_EVENT_COOLDOWN_MS`.

### 1.2 Huidig gedrag

- Eén gedeelde cooldown van **120 s** voor alle vier de paden (1m, 5m, 30m, confluence).
- Zodra één van deze een notificatie verstuurt, wordt `lastVolumeEventMs = now` gezet.
- Gedurende 120 s daarna: geen van de andere paden mag een volume-event sturen (ze doen `if (!volumeEventCooldownOk(now))` → suppress).
- Volgorde in `checkAndNotify`: 1m → 30m → 5m. Confluence wordt alleen vanuit 1m- en 5m-paden aangeroepen; 30m gebruikt alleen de cooldown-check.

### 1.3 Voorgestelde wijziging (klein en effectief)

- **Enige code-change:** `VOLUME_EVENT_COOLDOWN_MS` verlagen van **120000UL** naar **60000UL** (60 s).
- Geen per-pad cooldowns of extra state in Phase 1.

### 1.4 Verwachte effecten

| Pad | Effect |
|-----|--------|
| **1m** | Binnen 2 min kunnen twee “golven” van short-term alerts (1m/5m/30m/confluence) in plaats van één; tweede sterke move na ~1 min kan weer een 1m (of confluence) geven. |
| **5m** | Zelfde: na 60 s kan een nieuwe 5m (of confluence) weer; minder lang “dood” na een eerdere alert. |
| **30m** | Minder lang geblokkeerd door een eerdere 1m/5m/confluence; 30m kan weer na 60 s. |
| **Spamrisico** | Beperkt: per-pad cooldowns (1m/5m/30m per uur, `cooldown1MinMs` etc.) en hourly caps blijven; alleen de gedeelde “volume-event” slot wordt korter. |

### 1.5 Side effects en interacties

- **2 min na eerste alert:** Mogelijk tweede cluster (1m of 5m of 30m of confluence) binnen 2 min; acceptabel als “twee duidelijke moves”.
- **Confluence na 1m:** Als eerst 1m wordt verstuurd, kan confluence pas weer na 60 s; gedrag blijft consistent.
- **Documentatie:** `docs/V509_NOTIFICATION_BASELINE.md` (rond regel 116) noemt “120 s”; bij implementatie aanpassen naar 60 s.

---

## 2. Confluence-suppressie

### 2.1 Exacte locaties

| Wat | Bestand | Regels |
|-----|---------|--------|
| Struct velden | `src/AlertEngine/AlertEngine.h` | **21**, **29** (`usedInConfluence`) |
| Reset bij nieuw event | `src/AlertEngine/AlertEngine.cpp` | **362**, **377** |
| Confluence-functie | `src/AlertEngine/AlertEngine.cpp` | **452–560** (o.a. **463**, **558–559**) |
| 1m: confluence-check + suppress | `src/AlertEngine/AlertEngine.cpp` | **802–818** |
| 5m: confluence-check + suppress | `src/AlertEngine/AlertEngine.cpp` | **1026–1042** |

- **usedInConfluence** wordt `true` alleen in `checkAndSendConfluenceAlert` **na** `sendNotification(…)` en `if (sent)` (regels 554–560).
- **1m-pad:** Als `confluenceFound` → skip individuele 1m. Anders: als `smartConfluenceEnabled && last1mEvent.usedInConfluence` → skip; anders cooldown-check; anders mogelijk send en `lastVolumeEventMs = now`.
- **5m-pad:** Idem voor 5m met `last5mEvent.usedInConfluence`.

### 2.2 Huidig gedrag

- Als confluence **daadwerkelijk** wordt verstuurd: `last1mEvent.usedInConfluence = true`, `last5mEvent.usedInConfluence = true`.
- In de **zelfde** `checkAndNotify`-ronde: 1m-pad ziet `confluenceFound == true` → skip 1m. 5m-pad roept `checkAndSendConfluenceAlert` opnieuw aan → die retourneert false (events al used) → `confluenceFound == false`, maar `last5mEvent.usedInConfluence == true` → 5m wordt ook gesuppressed.
- Resultaat: bij confluence krijgt de gebruiker **één** notificatie (confluence), geen aparte 1m en 5m. De expliciete “5m move” als bevestiging ontbreekt.

### 2.3 Interactie met volume-event cooldown

- Na het sturen van confluence wordt `lastVolumeEventMs = now` gezet (zelfde ronde).
- In de 5m-stap wordt daarna `volumeEventCooldownOk(now)` gecheckt → false (0 ms verstreken) → 5m zou ook op grond van cooldown worden onderdrukt. Dus: **alleen** `last5mEvent.usedInConfluence` niet zetten is onvoldoende om “confluence + 5m in dezelfde ronde” te sturen; de 5m-pad moet een **uitzondering** op de volume-event cooldown krijgen wanneer confluence in deze ronde is verstuurd.

### 2.4 Voorgestelde wijziging (klein en effectief)

1. **In `checkAndSendConfluenceAlert` (na succesvolle send):** Alleen `last1mEvent.usedInConfluence = true` zetten; **niet** `last5mEvent.usedInConfluence = true` zetten (regel 559 schrappen of conditioneel maken).
2. **Round-flag voor 5m-uitzondering:** Een vlag bijhouden dat in deze aanroep van `checkAndNotify` confluence is verstuurd (bv. `confluenceSentThisRound`). Die wordt gezet wanneer `checkAndSendConfluenceAlert` true retourneert (in het 1m-blok). In het 5m-blok: als `confluenceSentThisRound` true is, **niet** onderdrukken op `volumeEventCooldownOk(now)` (d.w.z. die check overslaan voor 5m wanneer confluence deze ronde is verstuurd).
3. **5m-pad:** De suppressie op `last5mEvent.usedInConfluence` blijft; omdat we die niet meer zetten na confluence, wordt 5m niet om die reden onderdrukt. Gecombineerd met punt 2 kan 5m in dezelfde ronde na confluence nog worden verstuurd → gebruiker ziet **confluence + 5m** (2 notificaties). 1m blijft onderdrukt bij confluence (ongewijzigd).

**Concrete code-locaties voor punt 2:**

- Aan het begin van `checkAndNotify`: declareer `bool confluenceSentThisRound = false;` (of bestaande lokale hergebruiken).
- In het 1m-blok, waar nu `confluenceFound = checkAndSendConfluenceAlert(now, ret_30m);` staat: na die aanroep, als `confluenceFound`, zet `confluenceSentThisRound = true;`.
- In het 5m-blok: waar nu `} else if (!volumeEventCooldownOk(now)) {` staat, wijzigen naar: als `confluenceSentThisRound` true is, **niet** deze cooldown-check doen (dus 5m toestaan door te gaan naar de volgende checks); anders wel `volumeEventCooldownOk(now)` checken.

### 2.5 Verwachte effecten

| Pad | Effect |
|-----|--------|
| **1m** | Geen wijziging: bij confluence wordt 1m nog steeds onderdrukt (confluence bevat al 1m-info). |
| **5m** | Bij confluence: gebruiker krijgt **confluence + 5m** in dezelfde ronde (2 notificaties); 5m fungeert als expliciete bevestiging. |
| **30m** | Geen wijziging. |
| **Spamrisico** | Beperkt: alleen bij daadwerkelijke confluence 1 extra notificatie (5m) in dezelfde ronde; geen extra 1m. |

### 2.6 Side effects en interacties

- **Serial-logging:** Bestaande regels “[Notify] 5m move onderdrukt (gebruikt in confluence)” zullen bij confluence niet meer voor deze ronde optreden; eventueel log “5m move verstuurd (na confluence)” voor traceerbaarheid.
- **Herhaalde 5m:** Op een **volgende** tick is `last5mEvent.usedInConfluence` nog false (we zetten het niet), maar het 5m-event is hetzelfde (zelfde candle); andere checks (cooldown per pad, hourly cap) bepalen of er nog een 5m komt. Geen dubbele 5m voor hetzelfde event zolang per-pad cooldowns gelden.
- **Confluence zonder 5m in dezelfde ronde:** Als 5m om een andere reden faalt (volume/range, night mode), blijft het bij alleen confluence.

---

## 3. Samenvatting Phase 1 change-set

| # | Onderdeel | Wijziging | Bestand(en) |
|---|-----------|-----------|-------------|
| 1 | Volume-event cooldown | `VOLUME_EVENT_COOLDOWN_MS` 120000 → 60000 | `AlertEngine.cpp` (regel 81) |
| 2a | Confluence: 5m niet als “used” markeren | Na send alleen `last1mEvent.usedInConfluence = true`; regel met `last5mEvent.usedInConfluence = true` verwijderen | `AlertEngine.cpp` (558–559) |
| 2b | Confluence: 5m inzelfde ronde toestaan | `confluenceSentThisRound` introduceren; in 5m-pad volume-event cooldown overslaan indien `confluenceSentThisRound` | `AlertEngine.cpp` (begin checkAndNotify, 1m-blok, 5m-blok ~1042) |
| 3 | Documentatie | “120 s” → “60 s” voor volume-event cooldown | `docs/V509_NOTIFICATION_BASELINE.md` (~116) |

---

## 4. Aanbevolen implementatievolgorde

1. **Eerst: Volume-event cooldown** (macro 120→60 s). Geen gedragsafhankelijkheden; makkelijk te testen en terug te draaien.
2. **Daarna: Confluence 2a** (niet meer `last5mEvent.usedInConfluence = true`). Zonder 2b levert dit alleen effect op een **latere** tick (5m kan dan weer, als hetzelfde 5m-event nog voldoet); in dezelfde ronde blijft 5m geblokkeerd door volume-event cooldown.
3. **Als laatste: Confluence 2b** (round-flag + 5m cooldown-uitzondering). Dan is “confluence + 5m in dezelfde ronde” compleet.

---

## 5. Handmatig testplan

- **Volume-event cooldown (60 s)**  
  - Trigger 1m spike (of 5m/30m/confluence) → 1 notificatie.  
  - Binnen 60 s: geen tweede volume-event (1m/5m/30m/confluence).  
  - Na 60 s: tweede trigger (zelfde type of ander) → verwacht tweede notificatie (onder voorbehoud van per-pad cooldown/caps).  
  - Controle: geen drie notificaties binnen ~60 s tenzij per-pad logica dat toestaat (bv. 1m + 5m met verschillende cooldowns).

- **Confluence zonder 5m-uitzondering (alleen 2a)**  
  - Zorg voor 1m+5m same direction, binnen window, trend, volume/range ok.  
  - Verwacht: confluence notificatie; geen 1m; geen 5m in dezelfde ronde (cooldown blokkeert 5m).  
  - Volgende tick(s): controleren of 5m nog kan (usedInConfluence niet gezet).

- **Confluence met 5m in dezelfde ronde (2a + 2b)**  
  - Zelfde setup als hierboven.  
  - Verwacht: **confluence + 5m** (2 notificaties in korte tijd). Geen 1m.  
  - Logs: geen “[Notify] 5m move onderdrukt (gebruikt in confluence)” in die ronde.

- **Spam-check**  
  - Meerdere sterke moves binnen 2 min: verwacht max 2 “golven” (door 60 s cooldown) plus per-pad limieten; geen ongecontroleerde stortvloed.  
  - Confluence meerdere keren binnen 5 min: bestaande confluence-cooldown (`CONFLUENCE_TIME_WINDOW_MS`) blijft; geen extra confluence-spam.

---

*Document: Phase 1 short-term notification proposal. Geen code gewijzigd; alleen voorstel en locaties.*
