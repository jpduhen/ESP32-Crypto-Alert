# 06 – Operations (bedrijfsvoering)

## Opstartvolgorde (setup)

1. Serial en device (backlight, pinnen).
2. Display (Arduino_GFX, PINS).
3. LVGL (uiController.setupLVGL()).
4. Watchdog.
5. WiFi-event handlers.
6. Mutexen (dataMutex, gNetMutex).
7. Dynamische arrays alloceren (fiveMinutePrices, minuteAverages, hourlyAverages; PSRAM indien beschikbaar).
8. Optioneel: Bitvavo stream-buffer op heap.
9. Source-arrays initialiseren (SOURCE_LIVE).
10. WiFi verbinden + eerste prijs ophalen (wifiConnectionAndFetchPrice).
11. PriceData.syncStateFromGlobals().
12. WarmStartWrapper binden (settings, logger); bij WiFi + warmStartEnabled: performWarmStart(), warmWrap.endRun().
13. buildUI().
14. Eerste render (lv_refr_now of lv_timer_handler).
15. Start FreeRTOS-tasks: apiTask, priceRepeatTask, uiTask, webTask.

Daarna: loop() voor OTA, MQTT reconnect, deferred acties; apiTask voor fetch en alerts; uiTask voor updateUI; webTask voor handleClient.

---

## Normale werking

- **apiTask**: elke UPDATE_API_INTERVAL (2 s) fetchPrice(); daarbij: mutex, prijs in buffers, updateMinuteAverage (elke 60 s), returns berekenen, checkAnchorAlerts(), alertEngine.checkAndNotify(), mutex release; daarna priceRepeatTask vult elke 2 s dezelfde prijs in de ringbuffer.
- **uiTask**: elke UPDATE_UI_INTERVAL (1 s) korte mutex-check, dan updateUI(); regelmatig lv_task_handler() of lv_refr_now().
- **webTask**: elke 100 ms handleClient().
- **loop()**: OTA (indien OTA_ENABLED), MQTT reconnect/backoff, processMqttQueue(), deferred MQTT IP publish.

---

## Foutafhandeling (hoog niveau)

- **WiFi weg**: apiTask en webTask wachten tot WL_CONNECTED; fetch wordt niet uitgevoerd; priceRepeatTask blijft laatste prijs herhalen.
- **API timeout/fout**: ApiClient geeft false; fetchPrice() retry/backoff in .ino; geen crash; buffers ongewijzigd.
- **Mutex timeout**: apiTask logt en probeert later opnieuw; uiTask slaat één update over.
- **NTFY/MQTT fout**: Transport zit in .ino (sendNotification → sendNtfyNotification). Alleen logging; alert state (lastNotification*, alerts*ThisHour) wordt bijgewerkt bij “sent”, dus geen dubbele pogingen bovenop cooldown.
- **NVS vol/corrupt**: SettingsStore load() valt terug op defaults; save() kan falen, dan blijft oude state in RAM.

---

## Geheugen en stabiliteit

- **HeapMon**: logHeap("tag") met rate limit (bijv. elke 60 s in apiTask) voor fragmentatie-audit.
- **Buffers**: secondPrices vast; fiveMinutePrices, minuteAverages, hourlyAverages dynamisch (INTERNAL of SPIRAM); Bitvavo stream buffer heap of fallback.
- (Legacy: op boards zonder PSRAM was voorheen een array-guard actief; na CYD-removal niet meer in gebruik.)

---

## OTA (indien ingeschakeld)

- Alleen op boards met OTA_ENABLED en partitieschema met twee app-partities.
- Web-endpoints: /update (GET/POST), chunk upload; na succes Update.end() en reboot.
- Geen gevoelige data in OTA-payload; firmware wordt via browser geüpload.

---

## Wat niet in docs/commits

- Geen API-keys, wachtwoorden of tokens in broncode of documentatie.
- NTFY-topic, MQTT-credentials, webPassword, DuckDNS-token alleen via NVS of veilige configuratie (web UI met wachtwoord of lokaal netwerk).

---
**[← 05 Configuratie](05_CONFIGURATION.md)** | [Overzicht technische docs](../README_NL.md#technische-documentatie-code-werking) | **[07 Woordenlijst →](07_GLOSSARY.md)**
