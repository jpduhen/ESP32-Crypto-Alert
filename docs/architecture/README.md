# Architectuurdocumentatie (V2)

- **Primaire projectstatus (besluiten, prioriteiten, matrix-stuur):** [../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md)
- **[Governance & repo-overzicht](V2_WORKDOCUMENT_MASTER.md)** — rollen, consistentie; geen volledige duplicaat van het `firmware-v2`-werkdocument.
- [V2 skeletonfase — technische keuzes](V2_SKELETON_NOTES.md) — S-001…S-007, open punten.
- [ADR-001 — GEEK display ST7789 + esp_lcd](ADR-001-geek-display-st7789-esp-lcd.md) — T-102 displayroute.
- [ADR-002 — Bitvavo exchange + M-002-grenzen](ADR-002-bitvavo-exchange-and-m002.md) — T-103 `exchange_bitvavo` / `net_runtime`.
- [ADR-003 — WiFi-onboarding (SoftAP CryptoAlert)](ADR-003-wifi-onboarding-v2.md) — NVS + browserportal.
- [ADR-004 — LVGL op esp_lcd + eerste UI-laag](ADR-004-lvgl-esp-lcd-ui-layer.md) — `esp_lvgl_port`, `ui` ← `market_data`.
- [M-002 — Netwerkgrenzen (hardening)](M002_NETWORK_BOUNDARIES.md) — componenten, reconnect, snapshot, TODO’s.
- [B-001 — V1 referentie-freeze](../V1_REFERENCE_FREEZE_B001.md) — `main` + tag-beleid vs V2.
- [Uitgangspunten V2-herbouw](V2_OUTGANGSPUNTEN_NL.md) — scope, doelen, randvoorwaarden (detail).
- [C5 — Roadmap 30m/2h (verticale slices)](C5_ROADMAP_30M_2H.md) — consolidatie **C5**; volgorde metrics → alert → outbound; uitsluitingen V1.

Aanvullende ADR’s en diagrammen kunnen hier worden toegevoegd naarmate V2 concreet wordt.
