# OtaWebUpdater (web-based chunked OTA)

Deze map bevat een **zelfstandige web-based chunked OTA updater** (dus **zonder ArduinoOTA**).

Doel: je kunt deze map naar een ander ESP32 project kopieren en vervolgens alleen integreren met een bestaande `WebServer`.

## Wat zit erin?

- `OtaWebUpdater.h` / `OtaWebUpdater.cpp`: OTA web updater module (routes + handlers + upload pagina)
- `OtaWebUpdaterConfig.h`: kleine config (o.a. maximale OTA file size)
- `ExampleIntegration.md`: korte integratie gids met code snippet

## Hoe te kopieren naar een ander project

1. Kopieer de volledige folder `src/OtaWebUpdater/` naar je andere ESP32 project.
2. Zorg dat je project ook een `platform_config.h` (of equivalent) heeft waar `OTA_ENABLED` is gedefinieerd.
   - Anders zet je `OTA_ENABLED` op `1`/`0` in je config.
3. Je hoeft geen andere ESP32-Crypto-Alert specifieke code te kopiëren.

## Vereisten / dependencies

Deze module verwacht:

- Een werkende `WebServer` object (Arduino `WebServer` library)
- De ESP32 `Update` library (voor firmware schrijven)
- `ESP.restart()` (reboot na succesvolle update)

## Routes die beschikbaar worden

Bij `OTA_ENABLED == 1` registreert de module:

- `GET  /update`  
  Geeft de OTA upload pagina (file selector + progress bar + status).
- `POST /update/start`  
  Start `Update.begin(total, U_FLASH)` met `{"total":N}` (of via query `?total=N`).
- `POST /update/chunk`  
  Chunked upload endpoint (multipart). `Update.write(...)` gebeurt per chunk via de upload callback.
- `POST /update/end`  
  Eindigt met `Update.end(true)`, stuurt success JSON en doet reboot.

De chunked upload flow en progress feedback zijn identiek aan de huidige werkende implementatie.

## Partition scheme / OTA limiet

De module gebruikt `OTA_WEBUPDATER_MAX_SIZE` als limiet. Default is `0x1E0000` (zoals in de oorspronkelijke WebUI implementatie).

Let op:

- Kies een partition scheme met voldoende spiff / flash space voor de firmware.
- Als je partition layout kleiner is, pas `OTA_WEBUPDATER_MAX_SIZE` aan in `OtaWebUpdaterConfig.h`.

## UX / HTML upload pagina

De OTA upload pagina (HTML + JS) wordt door de module zelf geserveerd via `GET /update`.

## Minimal integration (high level)

Zie `ExampleIntegration.md` voor een concrete snippet.

