# Migratiematrix V2 — concept (draft)

**V2-traject:** status en besluiten staan in [../architecture/V2_WORKDOCUMENT_MASTER.md](../architecture/V2_WORKDOCUMENT_MASTER.md). Deze matrix is een levend hulpmiddel en volgt de prioriteiten uit dat document.

**Doel:** eerste onderbouw voor besluitvorming. Geen vast contract.  
**Legenda status:** *Behouden* = concept/logica waardevol; *Herschrijven* = opnieuw in V2 (andere HAL/build); *Schrappen* = verwacht geen rechtstreekse port; *Nader beoordelen* = afhankelijk van scope/risico.

| Huidige functionaliteit / module | Huidige locatie (indicatief) | Status V2 | Beoogde V2-component (concept) | Opmerkingen / risico’s / afhankelijkheden |
|--------------------------------|-----------------------------|-----------|-----------------------------------|-------------------------------------------|
| Entry & task orchestratie | `ESP32-Crypto-Alert.ino` (zeer groot) | Herschrijven | `app_main` + IDF tasks/events | Hoogste complexiteit: bootvolgorde, WDT, mutexen; geleidelijk ontvluchten. |
| Platform/board defines | `platform_config.h`, `PINS_*.h` | Herschrijven | Board support package / `sdkconfig` + headers | Veel compile-time switches; V2: GEEK eerst als referentie. |
| HTTP(S) fetch (streaming) | `src/Net/HttpFetch.*` | Behouden → Herschrijven | Netwerkcomponent (IDF) | API stabiel; implementatie koppelen aan IDF client. |
| Bitvavo REST + WS | `src/ApiClient/*` | Herschrijven | `exchange/bitvavo` service | Groot bestand; TLS, timeouts, mutex-interactie met web. |
| Prijsbuffers & returns | `src/PriceData/*` | Behouden (logica) → Herschrijven | `domain/pricedata` | Kern van metrics; testbaar maken helpt migratie. |
| Trend / volatiliteit | `src/TrendDetector/*`, `src/VolatilityTracker/*` | Idem | `domain/analysis` | Afhankelijk van PriceData-contract. |
| Regime | `src/RegimeEngine/*` | Idem | `domain/regime` | UI/settings-koppeling. |
| Alerts & notificaties | `src/AlertEngine/*`, `AlertAudit.h` | Idem | `domain/alerts` + `notify/*` | NTFY-throttling en 2h-logica zijn fragiel; regressietests wenselijk. |
| Anchor | `src/AnchorSystem/*` | Idem | `domain/anchor` | Koppeling UI + alerts. |
| Warm start | `src/WarmStart/*` | Nader beoordelen | `bootstrap/warmstart` | Afhankelijk van API + flash. |
| LVGL UI | `src/UIController/*`, `lv_conf.h` | Herschrijven | `ui/lvgl` + layout | Sterk board-gekoppeld; visuele kwaliteit behouden als eis. |
| Display backends | `src/display/*` | Herschrijven | `hal/display` | Meerdere drivers (GFX, `esp_lcd` AXS15231B); abstractie herhalen. |
| Webserver & HTML | `src/WebServer/*` | Behouden (UX) → Herschrijven | `http` server + static assets | Grote bron van waarde; modulariseren (routes, JSON). |
| OTA | `src/OtaWebUpdater/*` | Behouden → Herschrijven | `system/ota` | IDF native OTA vs huidige aanpak afstemmen. |
| Settings / NVS | `src/SettingsStore/*` | Herschrijven | `storage/settings` | Migratiepad NVS-keys documenteren. |
| MQTT | In `.ino` + helpers | Herschrijven | `notify/mqtt` | PubSubClient → IDF MQTT of vergelijkbaar. |
| NTFY | In `.ino` / Api / Alert-keten | Herschrijven | `notify/ntfy` | TLS + tokens; zie `secrets_local.h`-patroon. |
| Heap/diagnose | `src/Memory/HeapMon.*`, diverse `WEBTRACE`/flags in `platform_config.h` | Nader beoordelen | `diag/*` | Veel tijdelijke A/B-flags; opschonen bij V2. |
| CYD / TTGO / legacy boards | `platform_config`, docs CYD | Schrappen (prioriteit) | — | Geen V2-prioriteit; code kan blijven tot opschoonmoment. |
| Docs metrics contract | `docs/METRICS_CONTRACT.md` | Behouden (inhoudelijk) | Living doc | Blijft leidend voor JSON/UI/MQTT-congruentie. |

## Gekoppelde onderdelen (ontvlechten)

1. **`platform_config.h` ↔ `ESP32-Crypto-Alert.ino`:** diagnose, web-runtime, boot-flags en feature toggles — naar expliciete configuratie + kleinere modules.
2. **Netwerkmutex ↔ WebServer ↔ ApiClient:** concurrent access en volgorde — centraal netwerk-/event-model in V2.
3. **Display factory ↔ UIController ↔ board pins:** hardware abstractielaag strak trekken voor GEEK → LCDWIKI → JC3248.

## Onzekerheden

- Exacte feature-pariteit (welke diagnoseflags blijven productie).
- Of WebSocket in V2 dezelfde rol behoudt vs alleen REST.
- Migratie van bestaande NVS-keys zonder brick van gebruikersdevices (strategie “eerste flash V2” vs migratie-tool).
