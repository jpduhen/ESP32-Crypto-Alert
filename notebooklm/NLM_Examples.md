# NLM Examples – UNIFIED-LVGL9 Crypto Monitor

Twee uitgewerkte voorbeelden met getallen. Terminologie consistent met /docs. Waarden die niet exact in docs staan zijn als *(voorbeeld)* aangeduid.

---

## Voorbeeld A: 1m-spike — berekening, threshold, cooldown, payload, transport

**Situatie:** We kiezen prijsgetallen zó dat ret_1m precies een “echte” alert triggert: 0,35 percentagepunten (0,35%). Daarna volgen threshold-check (0,31), cooldown, max-per-uur, payload en transportpad.

**1. ret_1m in percentagepunten**

- Formule: `(priceNow - priceXAgo) / priceXAgo * 100`.
- *(Voorbeeld)* Prijs 60 seconden geleden: 50 000 EUR. Prijs nu: 50 175 EUR.  
  ret_1m = (50 175 − 50 000) / 50 000 * 100 = 175 / 50 000 * 100 = **0,35** (dus 0,35%).
- Eenheid: percentagepunten; 0,35 betekent 0,35%.

**2. Threshold-check**

- 1m-spike voorwaarde: `|ret_1m| >= effectiveSpike1m`. Default spike1m (docs) is 0,31 (0,31%).
- *(Voorbeeld)* effectiveSpike1m = 0,31. |0,35| = 0,35 ≥ 0,31 → **drempel overschreden** → spike-alert mag (mits cooldown en uurlimiet OK).
- Contrast: als ret_1m &lt; 0,31 (bijv. 0,20), dan wordt er geen 1m-spike-alert getriggerd, ongeacht cooldown.

**3. Cooldown- en uurlimiet-check**

- `checkAlertConditions(now, lastNotification1Min, cooldown1MinMs, alerts1MinThisHour, MAX_1M_ALERTS_PER_HOUR)` moet true zijn.
- Default cooldown1MinMs = 120 000 (2 min). MAX_1M_ALERTS_PER_HOUR = 3.
- *(Voorbeeld)* Laatste 1m-notificatie was 3 minuten geleden; dit uur zijn er nog maar 1 van de 3 alerts geweest → cooldown verstreken en uurlimiet niet bereikt → **conditions OK**.

**4. Payload**

- AlertEngine bouwt title, message en colorTag (richting en eventueel sterkte). Geen verzending in de module; alleen aanroep `sendNotification(title, msg, colorTag)`.

**5. Transportpad**

- In .ino: `sendNotification()` wordt aangeroepen → roept `sendNtfyNotification(title, message, colorTag)` aan → NTFY HTTPS-verzoek naar ntfy.sh. Anchor-events gaan daarnaast via `publishMqttAnchorEvent()` naar MQTT; voor een gewone 1m-spike alleen NTFY.

**Samenvatting A:** ret_1m in percentagepunten → drempel check (zelfde eenheid) → cooldown + max-per-uur → payload in AlertEngine → verzending in .ino (sendNotification → sendNtfyNotification).

---

## Voorbeeld B: 2h secondary (mean touch of compress) gesuppresseerd door global secondary cooldown/throttling

**Situatie:** Er zou een 2h-secondary alert moeten komen — bijvoorbeeld “mean touch” (prijs nadert 2h-gemiddelde) of “compress” (2h-range onder drempel). Toch gaat er **geen** notificatie uit. We laten zien waarom.

**1. Type: SECONDARY**

- Mean touch en compress zijn **SECONDARY** 2h-alerts. Alleen breakout up/down zijn PRIMARY (en overslaan throttling).
- Omdat het hier een secondary is, wordt `shouldThrottle2HAlert(alertType, now)` geëvalueerd vóór verzending.

**2. Global secondary cooldown**

- `twoHSecondaryGlobalCooldownSec` heeft default 7200 (120 minuten).
- *(Voorbeeld)* Er is 45 minuten geleden al een andere secondary alert verstuurd (bijv. trend change). Tijd sinds laatste secondary = 45 * 60 = 2700 s. 2700 < 7200 → **gesuppresseerd** door global secondary cooldown; er wordt geen notificatie aangevraagd.
- Zelfs als de mean-touch- of compress-voorwaarden wél zijn bereikt, komt er nu geen sendNotification-aanroep voor deze secondary.

**3. Matrix-cooldown (als global cooldown wél verstreken was)**

- Stel de global cooldown was wél verstreken. Dan kijkt de throttling-matrix naar het *laatste* 2h-alerttype en de *volgende* (bijv. mean touch). Er geldt een minimale wachttijd tussen bepaalde type-combinaties (bijv. mean→mean 60 min, trend→mean 60 min; exacte matrix in code/docs).
- *(Voorbeeld)* Laatste alert was 30 min geleden een “mean touch”; nu opnieuw mean touch. Matrix-cooldown mean→mean = 60 min. 30 min < 60 min → **gesuppresseerd** door matrix-cooldown; weer geen notificatie.

**4. Geen notificatie**

- Zodra `shouldThrottle2HAlert` true teruggeeft, wordt `send2HNotification` / `sendNotification` voor deze secondary **niet** aangeroepen. De gebruiker ziet dus geen tweede secondary alert tot de betreffende cooldowns zijn verstreken.

**Samenvatting B:** 2h secondary (mean touch of compress) → shouldThrottle2HAlert: eerst global secondary cooldown (bijv. 120 min), anders matrix-cooldown → bij suppress geen sendNotification → geen notificatie. PRIMARY (breakout) zou wél doorgegaan zijn.

---
**[← Story Script](NLM_Story_Script.md)** | **[← Key Points](NLM_Key_Points.md)** | [Overzicht technische docs (README NL)](../README_NL.md#technische-documentatie-code-werking)
