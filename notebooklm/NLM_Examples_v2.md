# NLM_Examples_v2 — Concrete scenario’s (met cijfers)

Doel van dit document:
- Twee voorbeelden stap-voor-stap uitwerken (A en B), met dezelfde terminologie als de docs.
- ret_* is **percentagepunten**: 0,35 = 0,35%.
- Decision vs delivery: modules beslissen/bouwen payload; verzending gebeurt centraal in de **.ino** via:
  - `sendNotification()` → `sendNtfyNotification()`
  - anchor-events via `publishMqttAnchorEvent()`

---

## Begrippen (kort)
- **ret_1m / ret_5m / ret_30m / ret_2h**: return over dat timeframe in **percentagepunten**.
- **Threshold (drempel)**: minimale |ret_*| om een event te kwalificeren (bijv. 0,31 voor 1m spike).
- **Cooldown**: minimumtijd tussen twee alerts van hetzelfde type (bijv. 120s voor 1m spike).
- **Max-per-hour**: limiet om spam te voorkomen (waarde kan per config verschillen; hieronder als (voorbeeld)).
- **Primary vs Secondary (2h)**:
  - PRIMARY: urgent, komt door (behalve algemene safety/transport failures).
  - SECONDARY: context, wordt streng gethrottled met o.a. **global secondary cooldown = 7200s (120 min)**.

---

## Voorbeeld A — 1m spike alert (ret_1m = 0,35%, threshold 0,31)

### Situatie (cijfers)
We ontvangen twee prijzen:
- Prijs 60 seconden geleden: **P_then = 50 000 EUR**
- Prijs nu: **P_now = 50 175 EUR**

Berekening return (percentagepunten):
\[
ret_{1m} = \frac{(P_{now} - P_{then})}{P_{then}} \times 100
= \frac{(50\,175 - 50\,000)}{50\,000} \times 100
= 0{,}35
\]
Interpretatie: **0,35 = 0,35%**

### Stap 1 — Threshold check (spike)
Configuratie:
- 1m spike threshold: **thr_1m = 0,31** (0,31%) *(volgens docs)*

Check:
- |ret_1m| = |0,35| = 0,35
- 0,35 ≥ 0,31 → **drempel overschreden** → “candidate spike event” = **JA**

### Stap 2 — Cooldown check
Configuratie:
- 1m spike cooldown: **cooldown_1m = 120s** *(volgens docs/voorbeeld)*

Stel:
- laatste 1m spike alert timestamp: **t_last_1m = 12:00:10**
- huidige tijd: **t_now = 12:02:30**

Check:
- Δt = 140s ≥ 120s → **cooldown OK**

### Stap 3 — Max-per-hour check (anti-spam)
Let op: max-per-hour kan per config verschillen. Hier een voorbeeld:
- max 1m spike alerts per uur: **max_1m_per_hour = 6** *(voorbeeld)*

Stel:
- spikes in afgelopen 60 min: **count_1m_last_hour = 2**

Check:
- 2 < 6 → **OK**

### Stap 4 — Decision (AlertEngine)
Resultaat van stap 1–3:
- event mag → AlertEngine maakt een **Alert object/payload** met o.a.:
  - type: `SPIKE_1M`
  - ret_1m: `0,35`
  - price_now: `50 175`
  - threshold: `0,31`
  - timestamp: `t_now`
  - context: (optioneel) trend/vol states als die beschikbaar zijn

Belangrijk:
- AlertEngine **beslist** en **bouwt payload**.
- AlertEngine doet **niet** het transport zelf.

### Stap 5 — Delivery (transport in .ino)
De .ino voert het versturen uit:
1) `.ino`: `sendNotification(alertPayload)`
2) `.ino`: `sendNtfyNotification(alertPayload)`

Als er daarnaast een anchor-event moet worden gepubliceerd:
- `.ino`: `publishMqttAnchorEvent(anchorPayload)` (alleen voor anchor-events; niet per se voor 1m spikes)

### Contrast (geen alert)
Als ret_1m **onder** threshold is, bv. ret_1m = **0,20**:
- |0,20| < 0,31 → **geen 1m spike alert**
- er wordt dus ook **geen** `sendNotification()` aangeroepen.

---

## Voorbeeld B — 2h SECONDARY suppressie door global secondary cooldown (120 min)

### Situatie (concept)
Er komt een 2h-event binnen dat als **SECONDARY** geclassificeerd is (bijv. “mean touch” of “compress”).
SECONDARY betekent: context-informatie, dus streng gefilterd.

### Stap 1 — Classificatie: PRIMARY vs SECONDARY
- Event type: `MEAN_TOUCH_2H` *(voorbeeldnaam; conceptueel volgens docs)*
- Classificatie: **SECONDARY** *(volgens regelset)*

### Stap 2 — Global secondary cooldown check (7200s)
Configuratie:
- global secondary cooldown: **cooldown_secondary_global = 7200s = 120 min** *(volgens docs)*

Stel:
- laatste SECONDARY 2h alert timestamp: **t_last_secondary = 10:15**
- huidige tijd: **t_now = 11:00**

Check:
- Δt = 45 min
- 45 < 120 → **NIET OK** → suppressie = **JA**

### Stap 3 — Coalescing (optioneel gedrag)
Wanneer suppressie gebeurt, kan het systeem:
- óf het event volledig droppen,
- óf “coalescen”: onthouden als context om later mee te geven.

Omdat de exacte implementatie kan verschillen, beschrijven we dit neutraal:
- **Resultaat:** er gaat **nu geen notificatie** uit.
- Het systeem doet dit expres om “ruis” te beperken.

### Stap 4 — Decision vs delivery (expliciet)
Omdat het event wordt onderdrukt:
- AlertEngine maakt **geen** “send-worthy” alert-output
- `.ino` roept dus **geen** `sendNotification()` aan

### Contrast (PRIMARY zou wél door kunnen)
Als hetzelfde 2h-regime een **PRIMARY** event geeft (bijv. “breakout”):
- PRIMARY wordt niet tegengehouden door de global secondary cooldown (die geldt voor SECONDARY).
- PRIMARY kan dus wel leiden tot `sendNotification()` (tenzij andere safety/transport failures spelen).

---

## Extra toelichting (voor langere video) — Interpretatie na netwerkproblemen

Dit stuk is bedoeld om in de video kort toe te lichten als “failure mode + interpretatie”.

### Scenario: WiFi/API tijdelijk down → priceRepeat flattening
- Als WiFi wegvalt of de API traag is, kan `priceRepeatTask` ervoor zorgen dat buffers gevuld blijven.
- Effect: de prijsreeks kan tijdelijk “plat” worden, waardoor returns en volatiliteit **lager lijken** dan ze werkelijk zijn.

**Waarom is dit relevant?**
- Na herstel kunnen returns “achterlopen” op de echte marktbeweging.
- Praktische interpretatieregel: neem de **eerste 1–2 fetch-cycli** na herstel niet als het hardste signaal; wacht tot data weer “echt” binnenkomt.

*(Als je in je docs een specifieke recovery-strategie hebt, vervang deze algemene regel door die concrete strategie.)*

---

## Samenvatting in 5 regels (voor voice-over)
1) ret_* is percentagepunten: 0,35 = 0,35% en vergelijk je direct met thresholds.
2) Voorbeeld A: threshold + cooldown + max-per-hour OK → AlertEngine bouwt payload → .ino verstuurt via NTFY.
3) Voorbeeld B: 2h SECONDARY binnen 120 min → suppressie → geen transport.
4) Suppressie is feature: het verbetert signaal-ruis.
5) Bij WiFi/API issues kan priceRepeat volatiliteit afvlakken → wees voorzichtig direct na herstel.