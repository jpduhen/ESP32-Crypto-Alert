# WP-03a — V1-gap review en alert-engine scope (pointer)

**Primaire bron:** [`firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md`](../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) — sectie **§ WP-03a** en besluit **D-010**.

**V1-referentiecode:** branch **`main`** (`src/AlertEngine/`, `RegimeEngine/`, …), zie ook B-001 in het werkdocument.

**V2-implementatie:** branch **`v2/foundation`** — `firmware-v2/components/alert_engine/`, `domain_metrics/`.

Geen duplicaat van de gap-analyse: alle tabellen, keep/drop/defer en roadmap **C1–C5** staan in het werkdocument. **Koers:** **1m/5m** = huidige actieve kern; **30m/2h** = **gewenst einddoel**, gefaseerd (**C5**), **niet** “uit scope” in algemene zin — zie **D-010**.

**C1 (field-test 1m/5m):** uitgewerkt in **[C1_FIELD_TEST_1M5M.md](C1_FIELD_TEST_1M5M.md)** + werkdocument § **C1**; read-only **`alert_engine_runtime_stats`** in `status.json`.
