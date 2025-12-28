# Serial Monitor Analyse - Opstart Sequence

**Datum:** 2025-01-XX  
**Versie:** 4.06  
**Platform:** CYD28

## Analyse Resultaten

### ✅ Goed Functionerend

1. **LVGL Initialisatie**: Succesvol
   - Display: 240x320 pixels
   - Buffer: 2880 bytes (INTERNAL+DMA)
   - Heap na init: 186212 bytes free

2. **WiFi Verbinding**: Succesvol
   - Verbonden met AP
   - IP verkregen: 192.168.68.4
   - Web server gestart

3. **WarmStart**: Werkt correct
   - 1m=60, 5m=2, 30m=2, 2h=2 candles geladen
   - Status: WARMING_UP
   - Duration: 2844 ms

4. **FreeRTOS Tasks**: Succesvol gestart
   - API Task op Core 1
   - UI Task op Core 0
   - Web Task op Core 0

### ⚠️ Opgemerkte Issues

1. **Task WDT Warning**:
   ```
   E (2476) task_wdt: esp_task_wdt_init(517): TWDT already initialized
   ```
   - **Impact**: Laag - dit is een waarschuwing, geen error
   - **Oorzaak**: Task watchdog wordt mogelijk meerdere keren geïnitialiseerd
   - **Aanbeveling**: Check of task_wdt_init() meerdere keren wordt aangeroepen

2. **Langzame API Calls**:
   ```
   [API] OK -> BTCEUR 74534.11 (tijd: 3277 ms) - langzaam
   [API Task] WARN: Call duurde 3287 ms (langer dan interval 2000 ms)
   ```
   - **Impact**: Medium - kan leiden tot gemiste updates
   - **Oorzaak**: Netwerk latency of Binance API vertraging
   - **Aanbeveling**: Monitor dit en overweeg timeout aanpassing

3. **Veel Herhaalde API Calls**:
   - Veel "[API] Fetching price from..." berichten zonder direct resultaat
   - **Impact**: Laag - normaal gedrag, maar kan logging verminderen
   - **Aanbeveling**: Zet "[API] Fetching price from..." achter DEBUG_BUTTON_ONLY

4. **Heap Telemetry Logging**:
   - [Heap] berichten worden nog steeds getoond
   - **Impact**: Laag - debug informatie
   - **Aanbeveling**: Controleren of DEBUG_BUTTON_ONLY correct werkt voor Serial_printf

### 📊 Verbeteringen Mogelijk

1. **Debug Logging Optimalisatie**:
   - [API] Fetching price messages → achter DEBUG_BUTTON_ONLY
   - [WarmStart] logging → achter DEBUG_BUTTON_ONLY (behalve errors)
   - [Heap] telemetry → al achter DEBUG_BUTTON_ONLY (controleren of het werkt)

2. **Task WDT Initialisatie**:
   - Check of task_wdt_init() meerdere keren wordt aangeroepen
   - Voeg guard toe om dubbele initialisatie te voorkomen

3. **API Call Performance**:
   - Monitor langzame calls (> 2000ms)
   - Overweeg timeout aanpassing als dit frequent voorkomt
   - Log alleen langzame calls (> 1200ms) - dit gebeurt al

4. **Error Handling**:
   - `[Ret2h] ERROR: availableMinutes=1 < 2` - dit is normaal bij opstart
   - Geen actie nodig, dit is verwacht gedrag

## Aanbevolen Acties

### ✅ Uitgevoerd
1. ✅ **[API] Fetching messages**: Achter DEBUG_BUTTON_ONLY gezet
2. ✅ **[API] WARN/OK messages**: Achter DEBUG_BUTTON_ONLY gezet
3. ✅ **[API Task] WARN messages**: Achter DEBUG_BUTTON_ONLY gezet
4. ✅ **[Heap] logging**: Al achter DEBUG_BUTTON_ONLY (via Serial_printf macro)
5. ✅ **[NetMutex] logging**: Al achter DEBUG_BUTTON_ONLY gezet
6. ✅ **[UI] logging**: Al achter DEBUG_BUTTON_ONLY gezet

### Medium Prioriteit
3. ⚠️ **Task WDT warning**: Onderzoek dubbele initialisatie
   - `E (2476) task_wdt: esp_task_wdt_init(517): TWDT already initialized`
   - **Impact**: Laag - cosmetisch, geen functionele impact
   - **Oorzaak**: Mogelijk meerdere initialisaties in setup()
   - **Aanbeveling**: Check setup() voor dubbele task_wdt_init() calls

4. ⚠️ **API performance**: Monitor langzame calls
   - Langzame calls (> 2000ms) komen voor
   - **Impact**: Medium - kan leiden tot gemiste updates
   - **Oorzaak**: Netwerk latency of Binance API vertraging
   - **Aanbeveling**: Huidige timeout (4000ms) is voldoende, monitor dit

### Lage Prioriteit
5. 📝 **[WarmStart] logging**: Overweeg om uitgebreide logging achter DEBUG_BUTTON_ONLY te zetten
   - Veel detail logging tijdens warm-start
   - **Impact**: Laag - alleen bij opstart
   - **Aanbeveling**: Laat zoals het is, of zet alleen detail logging achter DEBUG_BUTTON_ONLY

## Conclusie

De opstart sequence functioneert goed. Alle core functionaliteit werkt correct:
- ✅ LVGL initialisatie
- ✅ WiFi verbinding
- ✅ WarmStart
- ✅ FreeRTOS tasks
- ✅ API calls (soms langzaam, maar functioneel)

De belangrijkste verbeteringen zijn:
1. Debug logging optimalisatie (minder noise in serial monitor)
2. Task WDT warning onderzoeken (cosmetisch, maar goed om op te lossen)

