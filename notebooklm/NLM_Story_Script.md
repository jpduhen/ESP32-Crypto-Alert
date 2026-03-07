# NLM Story Script – UNIFIED-LVGL9 Crypto Monitor

Nederlands videoscript, ca. 6–10 minuten. Gebaseerd op de bestaande /docs; geen codewijzigingen.

“Spelling: ESP32, Bitvavo, NTFY, MQTT.”

“Gebruik exact de formule voor ret_*.”

“Noem core pinning als ‘afhankelijk van config’.”

“Gebruik alleen sendNotification/sendNtfyNotification/publishMqttAnchorEvent.”
---

## (1) Wat is het?

De UNIFIED-LVGL9 Crypto Monitor is embedded firmware voor ESP32 en ESP32-S3 boards: een crypto-prijsscherm dat live prijzen van de Bitvavo API ophaalt, timeframe-returns berekent, trend en volatiliteit classificeert, een ankerprijs met take-profit en max-loss bijhoudt, en alerts *besluit* en als payload aanlevert — de verzending zelf gebeurt in de hoofdschets. De UI draait op LVGL 9. Alles is modulair opgezet: netwerk, API, prijsdata, trend, volatiliteit, anchor, alerts, UI, instellingen en webserver zitten in aparte src-modules; de .ino coördineert via FreeRTOS-tasks.

---

## (2) Platform, LVGL en tasks

Je kiest één platform in platform_config.h — bijvoorbeeld CYD 2.4, CYD 2.8, TTGO T-Display of een ESP32-S3 variant. Per board is er een PINS_*-bestand voor display en bus; LVGL wordt geconfigureerd via lv_conf.h. De applicatie draait op twee cores: apiTask op Core 1 voor fetch en buffer-updates; uiTask en webTask op Core 0 voor de UI en de webinterface. Daarnaast is er een priceRepeatTask die elke twee seconden de laatste prijs opnieuw in de ringbuffer zet voor stabiele 1m- en 5m-berekeningen. Gedeelde data wordt beschermd door dataMutex; netwerk-aanroepen door gNetMutex.

---

## (3) Net en ApiClient

De Net-module levert streaming HTTP: `httpGetToBuffer` leest de body naar een buffer zonder String-allocaties. ApiClient gebruikt die laag en doet de Bitvavo-aanroepen: `fetchBitvavoPrice` haalt de actuele prijs op, met stream-parsing waar mogelijk. Timeouts en foutlogging zitten in ApiClient; retries en backoff in de .ino. Er is geen aparte notifier-module: AlertEngine en AnchorSystem bouwen alleen de payload en roepen extern `sendNotification` aan; de transportlaag zit in de .ino.

---

## (4) PriceData, buffers en returns (ret_* in percentagepunten)

PriceData beheert de ringbuffers: 60 seconden, 300 seconden voor 5 minuten, 120 minuten voor 30m en 2h, en een uur-buffer voor lange termijn. Elke nieuwe prijs gaat via `addPriceToSecondArray` in de seconden- en 5m-buffer; elk volle minuut wordt het gemiddelde van 60 seconden in `minuteAverages` gezet. De returns — ret_1m, ret_5m, ret_30m, ret_2h — worden berekend als **percentagepunten**: (prijs_nu minus prijs_oud) gedeeld door prijs_oud maal 100. Dus 0,12 betekent 0,12 procent beweging; alle drempels (spike, move, enz.) gebruiken dezelfde eenheid. Warm-start vult bij opstart optioneel de buffers met Bitvavo-candles zodat ret_2h en ret_30m snel beschikbaar zijn.

---

## (5) Trend en Volatility

TrendDetector bepaalt de 2u-trend (UP, DOWN, SIDEWAYS) op basis van ret_2h en ret_30m en drempels; er is ook medium trend (1d) en long-term (7d). Bij een trendwissel kan een notificatie worden aangevraagd, met een cooldown van tien minuten. VolatilityTracker houdt de absolute 1m-returns bij over een lookback; daaruit komt een volatiliteitsstate (LOW, MEDIUM, HIGH). In auto-volatility-modus worden de effectieve drempels voor spikes en moves daarop aangepast.

---

## (6) AnchorSystem

AnchorSystem beheert de ankerprijs: zetten, bijwerken van min en max bij elke prijs, en de checks voor take-profit en max-loss. De effectieve drempels kunnen trend-adaptief zijn. Bij overschrijding bouwt AnchorSystem de notificatie-payload en roept `sendNotification` aan; voor anchor-events doet de .ino daarnaast een MQTT-publish via `publishMqttAnchorEvent`. Elke grens (take-profit of max-loss) geeft één keer een notificatie tot het anker opnieuw wordt gezet.

---

## (7) AlertEngine: confluence en 2h primary/secondary throttling

AlertEngine beslist over 1m-, 5m- en 30m-alerts: spikes en moves met cooldowns en een maximum per uur. Smart Confluence combineert een 1m- en 5m-event in dezelfde richting binnen een tijdvenster, ondersteund door de 30m-trend. Voor 2u-alerts is er een duidelijk onderscheid: **PRIMARY** — breakout up en down — mag altijd door en overslaat throttling. **SECONDARY** — mean touch, compress, trend change, anchor context — valt onder een throttling-matrix en een **global secondary cooldown** (standaard 120 minuten). Binnen secondaries is er ook coalescing om korte bursts te dempen. AlertEngine bouwt de payload en roept `sendNotification` of `send2HNotification` aan; de daadwerkelijke verzending gebeurt in de .ino.

---

## (8) Transport in .ino: sendNotification → sendNtfyNotification + publishMqttAnchorEvent

Tot hier hebben we bepaald wanneer er een alert is; nu kijken we hoe die payload daadwerkelijk het apparaat verlaat (NTFY/MQTT). Er is geen aparte notifier-module in src. In de hoofdschets staat `sendNotification(title, message, colorTag)`; die roept `sendNtfyNotification` aan — dus de NTFY-HTTP-verzending zit in de .ino. Anchor-events gaan daarnaast via `publishMqttAnchorEvent` in de .ino naar MQTT. Dus: beslissing en payload in AlertEngine of AnchorSystem; transport (NTFY en MQTT voor anchor) in de .ino.

---

## (9) WebServer, SettingsStore/NVS en WarmStart

De WebServer-module levert de instellingenpagina, save-handlers, anchor-set, NTFY-reset, WiFi-reset, status-JSON en OTA-upload. Alle instelbare waarden — thresholds, cooldowns, anchor, warm-start, MQTT, nachtstand, enz. — worden via SettingsStore in NVS (Preferences) opgeslagen. WarmStart is een wrapper voor status en logging; de echte warm-start (candles ophalen en buffers vullen) zit in de .ino. WebSocket is feature-flagged; HTTP polling is de primaire prijsingang.

---

## (10) Failure modes

- **API down:** ApiClient geeft false; fetchPrice doet retry/backoff in .ino; buffers blijven ongewijzigd; geen crash.
- **WiFi down:** apiTask en webTask wachten tot WL_CONNECTED; priceRepeatTask blijft de laatste prijs elke 2s in de buffer zetten.
- **PriceRepeat flattening:** Bij API-uitval of trage responses herhaalt priceRepeat steeds dezelfde prijs; 1m- en 5m-returns kunnen daardoor tijdelijk afgevlakt zijn en volatiliteit onderschatten tot er weer nieuwe API-prijzen zijn.
- **Mutex timeouts:** apiTask logt en probeert later opnieuw; uiTask slaat één update over als de mutex niet beschikbaar is.
- **UI snapshot niet geïmplementeerd:** De uiTask neemt de dataMutex zeer kort en leest daarna alle globals *zonder* mutex tijdens updateUI. Daardoor kan één frame een mix van oude en nieuwe waarden tonen. Het aanbevolen patroon — snapshot under mutex: kopie van de benodigde velden onder mutex, daarna alleen op die snapshot renderen — is in de huidige code niet geïmplementeerd.

Einde script.

---
[Overzicht technische docs (README NL)](../README_NL.md#technische-documentatie-code-werking) | **[Key Points →](NLM_Key_Points.md)** | **[Examples →](NLM_Examples.md)**
