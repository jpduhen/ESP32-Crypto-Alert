## 2026-03-27

- **Regime Engine v1 (scaffolding):** `RegimeEngine` berekent marktregime-snapshot (slap / geladen / energiek), gekoppeld aan instellingen via `SettingsStore`; WebUI/UI-integratie waar van toepassing. Fase A: geen alert-gating op regime.
- **NTFY / WebSocket:** opschoning Fase 1–4 afgerond — één duidelijk productiepad (exclusive flow + HTTPS), diagnostiek achter `CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME`. Zie [`docs/ntfy_ws_cleanup_tracker.md`](docs/ntfy_ws_cleanup_tracker.md) (incl. afsluitnotitie en smoke-test op o.a. GEEK-S3).
- **Platform:** o.a. `PLATFORM_ESP32S3_JC3248W535` (3,5" QSPI, AXS15231B) en bijbehorende pinmap; standaardselectie volgens `platform_config.h` op jouw checkout.

## 2026-02-03

- Documentatie NL/EN bijgewerkt voor Bitvavo + NTFY, nachtstand en MQTT/HA.
- Alerts, voorbeelden en WebUI-instellingen afgestemd op huidige code.
- Hardware- en PSRAM-notes uitgebreid (incl. ESP32-4848S040C_i).
