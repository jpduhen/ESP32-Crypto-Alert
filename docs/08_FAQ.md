# 08 – FAQ (veelgestelde vragen)

## Algemeen

**Wat is dit project?**  
Een crypto-prijsscherm voor ESP32/ESP32-S3 met Bitvavo-prijzen, timeframe-returns (1m t/m 7d), trend/volatiliteit, anchor (take-profit/max-loss) en alerts via NTFY en MQTT. De UI draait op LVGL 9.

**Welke boards worden ondersteund?**  
TTGO T-Display, CYD 2.4/2.8, ESP32-S3 Super Mini, GEEK, LCDwiki 2.8, 4848S040, AMOLED 2.06. Keuze in `platform_config.h`.

**Waar staat de hoofdlogica?**  
In `UNIFIED-LVGL9-Crypto_Monitor.ino`: globals, setup/loop, tasks, fetchPrice, warm-start, WiFi/MQTT/NTFY. Modules in `src/` voor Net, ApiClient, PriceData, TrendDetector, VolatilityTracker, AnchorSystem, AlertEngine, UIController, SettingsStore, Memory, WarmStart, WebServer.

---

## Data en timeframes

**Hoe worden 1m en 5m returns berekend?**  
Via ringbuffers: `secondPrices` (60) en `fiveMinutePrices` (300). Elke API-call en de priceRepeatTask (elke 2 s) vullen de buffers via `priceData.addPriceToSecondArray(price)`. ret_1m = (huidige prijs − prijs 60 s geleden) / prijs 60 s geleden × 100; ret_5m analoog over 300 s. **Eenheid:** percentagepunten (0,12 = 0,12%); drempels idem (zie doc 02, 07).

**Wat is warm-start?**  
Bij opstart worden Bitvavo candles (1m, 5m, 30m, 2h) opgehaald en de buffers gevuld. Daardoor zijn ret_2h en ret_30m meteen (of snel) beschikbaar in plaats van na 30–120 minuten live data.

**Wat is minuteAverages?**  
Array van 120 waarden: elk volle minuut wordt het gemiddelde van de 60 seconden in één element gezet. Gebruikt voor ret_30m en ret_2h.

---

## Alerts

**Waarom krijg ik geen notificatie terwijl de beweging groot is?**  
Controleer: (1) cooldown – er moet voldoende tijd sinds de laatste notificatie van dat type zijn; (2) max per uur – bijv. max 3 voor 1m; (3) drempel – effective threshold (inclusief nachtstand/auto-vol) moet zijn overschreden.

**Hoe werkt cooldown precies?**  
Per type (1m, 5m, 30m) wordt `lastNotification*` bijgehouden. AlertEngine beslist of er een alert is; bij “ja” bouwt ze de payload en roept `sendNotification()` aan. De daadwerkelijke verzending (NTFY) gebeurt in .ino (`sendNotification()` → `sendNtfyNotification()`). Er wordt alleen een notificatie *aangevraagd* als `(now - lastNotification*) >= cooldown*Ms` en het uurlimiet niet is bereikt. Zie doc 03_ALERTING_RULES.

**Wat is Smart Confluence?**  
Als binnen een kort tijdvenster zowel een 1m- als een 5m-event in dezelfde richting plaatsvindt en de 30m-trend die richting ondersteunt, wordt één gecombineerde “Confluence”-alert verstuurd. Beide events worden dan als “used” gemarkeerd.

**Wat zijn 2h PRIMARY en SECONDARY alerts?**  
PRIMARY = breakout up/down (regime-wijziging); die overslaan throttling. SECONDARY = mean touch, compress, trend change, anchor context; die onderhevig aan throttling en global secondary cooldown.

---

## UI en display

**Waar wordt het scherm opgebouwd?**  
In `UIController::buildUI()`: chart, header/footer, price boxes, anchor/trend/volatiliteit-labels. `updateUI()` werkt alles periodiek bij (elke 1 s in uiTask).

**Waarom flikkert de prijs niet?**  
Er worden caches gebruikt (lastPriceLblValue, lastAnchorValue, …); labels worden alleen geüpdatet als de waarde is veranderd.

**Welke display-driver gebruik ik?**  
Per board in `PINS_*.h`: Arduino_GFX (ST7789, ILI9341, ST7701, …) met SPI (of I2C) bus. Resolutie en pinnen staan daar.

---

## Configuratie en geheugen

**Waar zet ik MQTT-wachtwoord of NTFY-topic?**  
Via de web-interface (instellingenpagina) of MQTT-commando’s; opgeslagen in NVS. NTFY-topic kan automatisch uit device-ID worden gegenereerd. Geen secrets in code of docs.

**Hoeveel RAM gebruikt het?**  
Afhankelijk van platform: secondPrices + 5m + minute + hourly arrays; LVGL draw buffer(s); globale buffers. HeapMon (`logHeap("tag")`) geeft inzicht; op CYD zonder PSRAM zijn arrays op INTERNAL heap, op S3 met PSRAM kan een deel op SPIRAM.

**Wat als NVS vol is?**  
Load valt terug op defaults; save kan falen. Oude keys kunnen worden opgeruimd of namespace gewist (let op verlies instellingen).

---

## Fouten en debugging

**API haalt geen prijs op.**  
Controleer WiFi, Bitvavo-bereikbaarheid, symbol (bijv. BTC-EUR). Timeouts en retries staan in .ino; ApiClient logt fouten.

**Mutex timeout in apiTask.**  
Er is langdurig door een andere taak op dataMutex gewacht. Meestal tijdelijk; volgende cycle probeert opnieuw. Bij structureel probleem: stack of duur van kritieke sectie verminderen.

**Display blijft zwart.**  
Controleer PINS (backlight-pin, DC/CS/RST), DEV_DEVICE_INIT(), LVGL init en flush-callback. Board-specifieke notities in PINS_*.h.

**Geen alerts ondanks ingestelde thresholds.**  
Zie “Waarom krijg ik geen notificatie” hierboven. Controleer ook of NTFY-topic en (bij MQTT) broker/credentials kloppen; fouten worden gelogd maar geven geen crash.

---

## Open questions (documentatie)

- **Notifier/transport:** Er is geen aparte notifier-module in `src/`. AlertEngine en AnchorSystem roepen extern `sendNotification(title, message, colorTag)` aan. De implementatie staat in de hoofdschets: `sendNotification()` (.ino, ca. regel 3348) roept `sendNtfyNotification()` aan (NTFY HTTPS). Anchor-events gaan daarnaast via `publishMqttAnchorEvent()` in .ino. Voor een expliciete “transportlaag”-module: zie .ino als referentie.
- **DataSource-naam:** In `src/PriceData/PriceData.h` heet de enum SOURCE_BINANCE (warm-start) en SOURCE_LIVE (live). De warm-start data komt in de repo van het **Bitvavo** candles-endpoint, niet Binance. Of SOURCE_BINANCE hernoemd moet worden naar bijv. SOURCE_WARMSTART: open; definitie staat in PriceData.h.
- **WebSocket:** HTTP polling is de primaire prijsingang. WS is feature-flagged (`WS_ENABLED`, platform_config.h) en wordt in .ino gebruikt (maybeInitWebSocketAfterWarmStart, processWsTextMessage). Exacte rol (alleen candles, of ook ticker) en of WS in jouw build actief is: zie .ino en platform_config.h.

---
**[← 07 Woordenlijst](07_GLOSSARY.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking)
