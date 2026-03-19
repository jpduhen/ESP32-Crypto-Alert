# ExampleIntegration.md

Voorbeeld van integratie van `OtaWebUpdater` in een project met een `WebServer`.

## 1) Files toevoegen

Kopieer:
- `src/OtaWebUpdater/OtaWebUpdater.h`
- `src/OtaWebUpdater/OtaWebUpdater.cpp`
- `src/OtaWebUpdater/OtaWebUpdaterConfig.h`

naar je andere project.

## 2) Instantiëren

```cpp
#include <WebServer.h>
#include "OtaWebUpdater.h"

// Je bestaande globale server
extern WebServer server;

OtaWebUpdater otaWebUpdater;

void setup() {
  // ... je normale wifi/credentials ...

  // Koppel de module aan je WebServer
  otaWebUpdater.begin(&server);
  otaWebUpdater.registerRoutes(&server);

  // ... server.begin() ...
}
```

## 3) Route link op de main instellingenpagina

Als je main page een navigatieblok heeft, voeg een link toe naar `GET /update`:

HTML (voorbeeld):

```html
&middot; <a href="/update" style="color:red;text-decoration:none;">
  Firmware update (OTA)
</a>
```

In de huidige ESP32-Crypto-Alert WebUI is de label bijvoorbeeld:
- `Firmware-update (OTA)` / `Firmware update (OTA)`

## 4) Upload flow (wat gebeurt er)

1. User opent `GET /update` (upload pagina).
2. User kiest een `.bin` en klikt Upload.
3. Browser:
   - stuurt `POST /update/start` met `{"total": <bestands-grootte>}`
   - stuurt daarna de firmware in chunks naar `POST /update/chunk`
   - sluit met `POST /update/end`
4. Module gebruikt `Update.begin / write / end(true)` en reboot na success.

## Limitaties / aannames

- Deze module gebruikt **geen ArduinoOTA**.
- De module probeert JSON `{"total":N}` te lezen uit `server->arg("plain")`.
  (De meegeleverde pagina stuurt precies dat.)

