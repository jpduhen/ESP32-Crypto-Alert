# M-002 — Netwerkcoördinatie: grenzen en verantwoordelijkheden (V2)

**Status:** hardening-document (levend)  
**Datum:** 2026-04-15  
**Context:** `firmware-v2/`, ESP-IDF. Sluit aan op [ADR-002](ADR-002-bitvavo-exchange-and-m002.md).

## Doel

Voorkomen dat de V2-netwerklaag opnieuw een monoliet wordt (zoals beschreven in de migratiematrix): **duidelijke eigenaarschappen**, minimale kennis tussen componenten, expliciete plekken voor toekomstige **MQTT / NTFY / WebUI** zonder die nu te bouwen.

## Wie initieert wat?

| Component | Initieert | Initieert niet |
|-----------|-----------|----------------|
| **`wifi_onboarding`** | SoftAP + HTTP-portal zolang NVS-credentials ontbreken of geforceerd; daarna **`esp_restart`**. | Geen Bitvavo, geen MQTT. |
| **`net_runtime`** | `esp_netif`, default event loop, WiFi **driver** (`esp_wifi_*`), STA/AP-modi volgens lifecycle; **`net_mutex`**. | Geen HTTP/HTTPS naar exchanges; geen WebSocket-clients. |
| **`exchange_bitvavo`** | `esp_http_client` (REST), `esp_websocket_client` (Bitvavo WS), TLS via cert bundle — **alleen na IP** en met `net_mutex` waar van toepassing. | Geen WiFi-moduswissel; geen provisioning-UI. |
| **`market_data`** | Provider (`exchange_bitvavo` of mock) via `init(cfg)`; geen sockets. | Geen WiFi; geen TLS. |
| **`app_core`** | Volgorde lifecycle: diagnostics → config → onboarding → BSP → display → UI → **`net_runtime::start_sta`** → **`market_data::init`**. | Geen directe URLs naar Bitvavo. |

## Reconnect / backoff — wie beheert wat?

| Mechanisme | Eigenaar | Opmerking |
|------------|----------|-----------|
| WiFi STA **disconnect → opnieuw verbinden** | **`net_runtime` alleen** (`WIFI_EVENT_STA_DISCONNECTED` → **`esp_timer` + backoff** → daarna `esp_wifi_connect()`). Backoff reset bij **`IP_EVENT_STA_GOT_IP`**. | Geen directe reconnect-storm; cap 30 s. Disconnect-log: WiFi-reasoncode (niet naar UI). |
| **Bitvavo WebSocket** reconnect | **`esp_websocket_client`** in **`exchange_bitvavo`** (`reconnect_timeout_ms` in `bitvavo_ws.cpp`); telling in `MarketSnapshot::ws_reconnect_count`. | Los van STA-backoff; geen tweede reconnect-loop in `app_core`. |
| **REST** herhaling | **`exchange_bitvavo::tick`** (interval + skip als WS live — zie T-103b). | Geen aparte REST-task. |
| Toekomst **MQTT / NTFY / WebUI** | Nog **niet** geïmplementeerd. | Moeten achter dezelfde **`net_mutex`** of een expliciete queue-service (**TODO M-002**), zie onder. |

## Wie publiceert snapshots?

- **Enige API naar domein/UI:** **`market_data::snapshot()`** → `market_types::MarketSnapshot`.
- **`exchange_bitvavo`** schrijft alleen naar een **interne** snapshot + mutex; **`market_data`** leest de exchange en geeft een kopie door — `app_core` / toekomstige UI kennen **geen** `exchange_bitvavo`-symbolen.

## Wat mag elkaar niet weten?

| Consumer | Mag weten | Mag niet weten |
|----------|-----------|------------------|
| **`app_core`** | `market_data`, `net_runtime` API, `wifi_onboarding` flow | Bitvavo-URL’s, TLS-details, WS JSON-shapes |
| **`market_data`** | `RuntimeConfig`, gekozen provider | TLS stack, `esp_http_client` handles |
| **`exchange_bitvavo`** | `net_mutex`, marktsymbool, `MarketSnapshot` velden | Display, LVGL, onboarding HTML |
| **`net_runtime`** | WiFi/netif events | Exchange API’s |
| **`wifi_onboarding`** | NVS keys voor WiFi | Market data, prijzen |
| **`display_port` / `ui`** | Geen marktdata direct | Exchange, net_runtime internals |

## Open M-002-risico’s (bewust)

1. **Eén `net_mutex`** — serialiseert vooral **HTTP (Bitvavo REST)** en **NTFY (`esp_http_client`)**. **Bitvavo WebSocket** draait via `esp_websocket_client` en gebruikt **deze mutex niet**; gelijktijdige TLS-last blijft mogelijk. Bij **mutex-timeout** (>20 s wacht): expliciete **WARN**-logs in REST en NTFY (zie M-002h).
2. **WiFi-reconnect (deels afgehandeld in M-002a, 2026-04):** STA-backoff zit nu in `net_runtime` (timer + cap). Nog te tunen: basis/max intervallen.
3. **Main-task stack** — netwerk zwaar (TLS); langere termijn: eigen worker-task (**TODO**). **M-002h (2026-04):** geen aparte worker-task; `service_outbound::poll` verwerkt **maximaal 2 events per app_core-rondje** zodat meerdere HTTPS-sends niet in één `poll()` achter elkaar de hoofdtaak laten blokkeren; backlog → rate-limited **WARN** + counters.
4. **MQTT** — `esp_mqtt_client`, geen `net_mutex` op publish-pad (eigen stack); **WebUI** (`esp_http_server`) geen exchange-eigenaar.

### M-002h — Hardening-batch (consolidatie, 2026-04)

- **Outbound:** `service_outbound` — queue diepte 8 ongewijzigd; **drain-cap** per `poll()`; **drop-teller** + duidelijke **M-002**-logs bij vol; backlog-waarschuwing (max. eens per 5 s zolang er werk blijft wachten).
- **Observability:** `GET /api/status.json` — read-only `outbound_queue_waiting`, `outbound_queue_capacity`, `outbound_drop_total` (geen nieuwe instellingen).
- **Worker-task:** bewust **niet** toegevoegd; herbeoordeling als metingen tonen dat hoofdtaak nog te lang in sinks blokkeert.

### Netwerkstatus richting bovenlagen (afbakening)

- **Link (IP):** `net_runtime::has_ip()` — enige expliciete WiFi/IP-vraag voor «kan ik naar buiten?».
- **Domeinfeed:** `market_data::snapshot()` — `ConnectionState`, `FeedErrorCode`, `last_error_detail` zijn **feed-/exchange-niveau** (geen WiFi-reasoncodes; bij geen IP: `NetworkDown` + korte tekst).
- **UI:** geen `esp_wifi_*`, geen URLs; alleen snapshot-velden die al generiek zijn.

## Verwijzingen

- [ADR-002](ADR-002-bitvavo-exchange-and-m002.md) — oorspronkelijke besluiten T-103.
- [ADR-003](ADR-003-wifi-onboarding-v2.md) — onboarding vs STA.
