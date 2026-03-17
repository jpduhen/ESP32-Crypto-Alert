# Title ↔ colorTag congruence audit (v5.09 rebuild)

Per alerttype: titleformaat, tag/colorTag, congruent ja/nee, en bij nee: aanbevolen fix. Geen code gewijzigd.

---

## 1. 1m spike

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s 1m %s"` → e.g. `BTC-EUR 1m Spike` (geen richting in titel) |
| **Tag/colorTag** | `determineColorTag(ret_1m, …)` → up: `purple_square,⏫️` of `blue_square,🔼`; down: `red_square,⏬️` of `orange_square,🔽` |
| **Congruent** | **Ja** – Titel = type (spike), tag = richting (up/down); samen één verhaal. |

---

## 2. 5m move

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s 5m %s"` → e.g. `BTC-EUR 5m Move` (geen richting in titel) |
| **Tag/colorTag** | `determineColorTag(ret_5m, …)` → up: `purple_square,⏫️` of `blue_square,🔼`; down: `red_square,⏬️` of `orange_square,🔽` |
| **Congruent** | **Ja** – Titel = type (move), tag = richting. |

---

## 3. 30m move

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s 30m %s"` → e.g. `BTC-EUR 30m Move` (geen richting in titel) |
| **Tag/colorTag** | `determineColorTag(ret_30m, …)` → up: `purple_square,⏫️` of `blue_square,🔼`; down: `red_square,⏬️` of `orange_square,🔽` |
| **Congruent** | **Ja** – Titel = type, tag = richting. |

---

## 4. Confluence

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s (1m+5m+Trend)"` → e.g. `BTC-EUR Samenloop (1m+5m+Trend)` (geen richting in titel) |
| **Tag/colorTag** | Up: `green_square,📈`; down: `red_square,📉` |
| **Congruent** | **Ja** – Titel = type (samenloop), tag = richting. |

---

## 5. 2h breakout / breakdown

| Veld | Waarde |
|------|--------|
| **Title format** | Up: `"%s 2h %s ↑"` (breakout); down: `"%s 2h %s ↓"` (breakdown). Richting in titel. |
| **Tag/colorTag** | Up: `"\xF0\x9F\x9F\xAA"` (🟪); down: `"\xF0\x9F\x9F\xA5"` (🟥) |
| **Congruent** | **Ja** – Titel ↑/↓ en tag 🟪/🟥 vertellen hetzelfde (up/down). |

---

## 6. 2h compress

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s 2h %s"` → e.g. `BTC-EUR 2h Compressie` (neutraal: range-squeeze, geen prijsrichting) |
| **Tag/colorTag** | `"yellow_square,📉"` |
| **Congruent** | **Nee** – Titel/event is neutraal (compressie); tag suggereert “daling” (📉). |
| **Aanbevolen fix** | Tag neutraal maken: gebruik alleen `"yellow_square"` of `"yellow_square,📐"` / `"yellow_square,📊"` (geen 📉). Exact: in `AlertEngine.cpp` rond regel 1168: `"yellow_square,📉"` → `"yellow_square,📐"` (of `"yellow_square"`). |

---

## 7. 2h mean touch

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s 2h %s"` → e.g. `BTC-EUR 2h Raakt Gemiddelde` (neutraal/info) |
| **Tag/colorTag** | `"green_square,📊"` |
| **Congruent** | **Ja** – Groen + 📊 past bij “raakt gemiddelde” (informatief/neutraal). |

---

## 8. 2h anchor context

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s 2h"` → e.g. `BTC-EUR Anker buiten 2h` |
| **Tag/colorTag** | `"purple_square,⚓"` |
| **Congruent** | **Ja** – Titel = anchor buiten band; tag = anchor (⚓) + paars. |

---

## 9. 2h trend change

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s %s %s"` → `🟩 ↗️ BTC-EUR Trend Wijziging` / `🟥 ↘️ …` / `🟨 ↔️ …` (kleur + pijl in titel) |
| **Tag/colorTag** | `getTrendColorTag(trendState)` → `🟩` / `🟥` / `🟨` |
| **Congruent** | **Ja** – Tag is dezelfde kleur als het eerste teken van de titel. |

---

## 10. 1d trend change

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s %s %s"` → `🟩 ↗️ BTC-EUR 1d Trend Wijziging` (of 🟥/🟨) |
| **Tag/colorTag** | `getTrendColorTag(newMediumTrend)` → `🟩` / `🟥` / `🟨` |
| **Congruent** | **Ja** – Zelfde schema als 2h trend change. |

---

## 11. 7d trend change

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s %s %s"` → `🟩 ↗️ BTC-EUR 7d Trend Wijziging` (of 🟥/🟨) |
| **Tag/colorTag** | `getTrendColorTag(newLongTermTrend)` → `🟩` / `🟥` / `🟨` |
| **Congruent** | **Ja** – Zelfde schema. |

---

## 12. Anchor take profit

| Veld | Waarde |
|------|--------|
| **Title format** | `"⏫️⚓️⚠️ %s %s: %s"` → e.g. `⏫️⚓️⚠️ BTC-EUR Anker: Winstpakker` |
| **Tag/colorTag** | `"\xF0\x9F\x9F\xA9"` (🟩) |
| **Congruent** | **Ja** – Titel ⏫️ = omhoog, tag groen = positief. |

---

## 13. Anchor max loss

| Veld | Waarde |
|------|--------|
| **Title format** | `"⏬️⚓️⚠️ %s %s: %s"` → e.g. `⏬️⚓️⚠️ BTC-EUR Anker: Verliesbeperker` |
| **Tag/colorTag** | `"\xF0\x9F\x9F\xA5"` (🟥) |
| **Congruent** | **Ja** – Titel ⏬️ = omlaag, tag rood = negatief. |

---

## 14. Anchor set

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s %s Anchor Set"` → `🟫 ⚓️ BTC-EUR Anchor Set` |
| **Tag/colorTag** | `"\xF0\x9F\x9F\xAB"` (🟫) |
| **Congruent** | **Ja** – Zelfde bruin in titel en tag. |

---

## 15. Auto anchor (AlertEngine)

| Veld | Waarde |
|------|--------|
| **Title format** | `"%s %s"` → e.g. `BTC-EUR Auto Anker` (geen emoji in titel) |
| **Tag/colorTag** | `"anchor"` (tekst) |
| **Congruent** | **Nee** – Andere alerts gebruiken emoji/kleur in tag; hier alleen het woord "anchor". Visueel niet gelijk met bv. Anchor set (🟫). |
| **Aanbevolen fix** | Tag gelijk trekken met Anchor set: `"anchor"` → `"\xF0\x9F\x9F\xAB"` (🟫). Exact: in `AlertEngine.cpp` rond regel 1867: `"anchor"` → `"\xF0\x9F\x9F\xAB"`. |

---

## Korte lijst van mismatches

1. **2h compress** – Tag `yellow_square,📉` suggereert daling; event is neutraal (compressie).  
   **Fix:** `"yellow_square,📉"` → `"yellow_square,📐"` (of `"yellow_square"`).

2. **Auto anchor** – Tag `"anchor"` (tekst) wijkt af van emoji-tags (o.a. Anchor set 🟫).  
   **Fix:** `"anchor"` → `"\xF0\x9F\x9F\xAB"` (🟫).

---

## One-patch aanbeveling (alleen deze mismatches)

**Bestanden:**  
- `src/AlertEngine/AlertEngine.cpp`

**Wijzigingen (alleen deze twee strings):**

1. **2h compress (rond regel 1168)**  
   - Huidig: `send2HNotification(ALERT2H_COMPRESS, title, msg, "yellow_square,📉");`  
   - Nieuw: `send2HNotification(ALERT2H_COMPRESS, title, msg, "yellow_square,📐");`

2. **Auto anchor (rond regel 1867)**  
   - Huidig: `sendNotification(title, msg, "anchor");`  
   - Nieuw: `sendNotification(title, msg, "\xF0\x9F\x9F\xAB");`  // 🟫

Geen titel-, body-, logica-, threshold-, cooldown- of transportwijzigingen; alleen deze twee colorTag-strings aanpassen voor congruentie.
