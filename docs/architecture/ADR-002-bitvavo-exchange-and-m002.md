# ADR-002 — Bitvavo exchange-laag achter `market_data` (T-103) en M-002-grenzen

**Status:** besloten (T-103)  
**Datum:** 2026-04-11  
**Context:** V2 `firmware-v2/`, ESP-IDF v5.4.2.

## Besluit

1. **Component `exchange_bitvavo`** — alle Bitvavo-specifieke logica: REST-bootstrap (`/v2/ticker/price`), WebSocket live (`wss://ws.bitvavo.com/v2/` + ticker subscribe), TLS via **mbedTLS certificate bundle**, fout- en reconnect-tellingen in `market_types::MarketSnapshot`.
2. **Component `net_runtime`** — WiFi STA uit NVS/onboarding (fallback: menuconfig), **`net_mutex`** voor seriële HTTP/WS-transacties — zie ook [ADR-003](ADR-003-wifi-onboarding-v2.md).
3. **Component `market_types`** — gedeelde POD-types (`MarketSnapshot`, `FeedErrorCode`, …) zodat `exchange_bitvavo` en `market_data` niet circulair afhankelijk zijn.
4. **`market_data`** blijft de **enige** API naar `app_core` / toekomstige UI: `init(cfg)`, `tick()`, `snapshot()` — geen Bitvavo-URLs of transportdetails in `ui` of `display_port`.
5. **Managed dependency:** `espressif/esp_websocket_client` via root-`idf_component.yml` (Component Registry).

## Alternatieven (afgewezen voor nu)

| Alternatief | Waarom niet |
|-------------|-------------|
| Bitvavo direct in `app_core` of `ui` | Verpest grenzen; herhaalt V1-monoliet |
| Eén mega-`net_runtime` met alle protocollen | Te groot voor T-103; exchange eerst afbakenen |
| Alleen REST, geen WS | V1-kern is live ticker; WS is vereist voor acceptatiecriteria T-103 |

## M-002 — netwerkverantwoordelijkheden (huidige stand)

| Laag | Verantwoordelijkheid |
|------|----------------------|
| `net_runtime` | `esp_netif` + event loop; optioneel WiFi STA; **`net_mutex`** (FreeRTOS mutex) |
| `exchange_bitvavo` | Bitvavo REST + WS; TLS; alleen `net_mutex` rond `esp_http_client_*` en (indien nodig) blocking WS-send; aparte mutex op snapshot (`exchange_bitvavo` intern) voor WS-callback vs hoofdthread |
| `market_data` | Providerkeuze (Kconfig); mappen naar `MarketSnapshot`; geen sockets |
| `app_core` | Volgorde: config → BSP → display → UI → **net_runtime** → **market_data** |

**Later:** `service_runtime` / uitgebreidere `net_runtime` kan o.a. backoff, queueing en scheiding MQTT/NTFY/WebUI overnemen — **niet** in T-103.

## Risico’s

- **WiFi-credentials** primair via **NVS** (`wifi_onboarding`); menuconfig alleen als ontwikkelaars-fallback — zie ADR-003.
- **WS JSON-parser** is lichtgewicht (substring/`strtod`) — geen volledige JSON-engine; kan bij API-wijziging breken.
- **Eén `net_mutex`** kan later knel worden als parallelle streams nodig zijn — dan expliciet herontwerp (ADR-opvolger).

## Gevolgen

- CI moet `idf.py build` met **network** voor eerste fetch van `managed_components` (of cache in image).
- Mock-feed blijft beschikbaar via `CONFIG_MD_USE_EXCHANGE_BITVAVO=n`.

---

## M-002 hardening (document, 2026-04-11)

Uitgebreide grenzen (init/reconnect/snapshot/scheiding MQTT·NTFY·WebUI nog open): zie **[M002_NETWORK_BOUNDARIES.md](M002_NETWORK_BOUNDARIES.md)**. ADR-002 blijft het **besluit**; het M-002-document is de **operationele matrix** voor reviews en verdere kleine stappen.
