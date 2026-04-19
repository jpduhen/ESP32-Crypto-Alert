# C2 — Randgevallen 1m/5m-kern (verwacht vs twijfel vs defect)

**Primaire bronnen:** [`firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md`](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) (§ **WP-03a**, **D-010**, § **C1**/**C2**), **[C1_FIELD_TEST_1M5M.md](C1_FIELD_TEST_1M5M.md)**.

**Doel:** randgedrag van de **huidige** kern expliciet maken — **geen** nieuwe alerttypes, **geen** beleidslaag in code als documentatie + observability volstaat.

**Branch:** `v2/foundation`.

---

## Observability (samenvatting)

| Bron | Gebruik |
|------|---------|
| `alert_decision_observability` | Per pad: `status`, `reason`, `remaining_cooldown_ms`, `remaining_suppress_ms` |
| `alert_engine_runtime_stats` | Emits, suppress-episodes, **`edge_1m` / `edge_5m` / `edge_confluence_1m5m`** (transities naar cooldown / suppressed / not_ready + laatste epoch) |
| `regime_observability` | `regime`, vol, effectieve drempels, **`last_regime_change_epoch_ms`**, **`threshold_scale_permille_raw`**, **`threshold_scale_clamped`**, **`regime_calm_max_step_bps`** / **`regime_hot_min_step_bps`** (C3) |
| `status.json` | `valid`, `connection`, `tick_source`, `last_tick_ms`, **`ws_inbound_ticks_last_sec`** (feed-dichtheid) |
| Seriële log | `alert_eng`: FIRE, suppress, regime, confluence-skip |

---

## Randgevallen

### 1. Oscillatie rond een drempel

| | |
|--|--|
| **Verwacht** | `below_threshold` ↔ `cooldown`/`fired` wisselt met markt; geen emit **elke** tick; cooldown beperkt herhaling. |
| **Twijfelachtig** | Zeer veel `edge_*`.`enter_cooldown` terwijl markt “rustig” aanvoelt — defaults mogelijk te scherp voor dit symbool. |
| **Defect** | Emits of queue-storm **zonder** verhoging van \|move\| in logs/snapshot; `status` blijft `fired` vast terwijl metric niet klopt. |
| **Velden** | `alert_decision_observability.*`, `edge_*`.`enter_cooldown`, `emit_total_*`, `effective_threshold_move_pct` |

### 2. Snelle richtingwisselingen

| | |
|--|--|
| **Verwacht** | 1m/5m kunnen tegengestelde richting tonen; confluence vereist **zelfde richting** (policy) → `direction_mismatch` of geen confluence-FIRE. |
| **Twijfelachtig** | Veel `invalid`/`direction_mismatch` zonder duidelijke markt — timing 1m vs 5m venster. |
| **Defect** | Confluence FIRE met tegengestelde tekens in payload t.o.v. policy (log + snapshot). |
| **Velden** | `confluence_1m5m.reason`, logs `M-010d: confluence skip — tegenstrijdige richting` |

### 3. Confluence net wel / net niet

| | |
|--|--|
| **Verwacht** | Bij drempelpoort net niet: `below_threshold`; bij richting: `direction_mismatch`; bij cooldown confluence: `cooldown`. |
| **Twijfelachtig** | Confluence zeldzaam terwijl losse TF vaak vuurt — policy (`confluence_require_both_thresholds`, …) bewust afstemmen in C3/C4. |
| **Defect** | `below_threshold` terwijl \|1m\| én \|5m\| duidelijk boven effectieve drempel uit logs; of confluence FIRE zonder beide passes. |
| **Velden** | `alert_confluence_policy`, `regime_observability.effective_threshold_move_pct`, `confluence_1m5m` |

### 4. Suppressie direct na confluence (M-010e)

| | |
|--|--|
| **Verwacht** | Na confluence: losse 1m/5m met **zelfde richting** kort onderdrukt → `suppressed` + `confluence_priority_window`; `suppress_after_conf_window_episodes_*` stijgt per episode. |
| **Twijfelachtig** | Geen enkele suppress ooit — venster 0 s of policy `emit_loose_when_conf_fails` altijd aan. |
| **Defect** | Dubbele losse melding **zelfde richting** binnen suppress-venster zonder emit die vooraf ging; of suppress op **tegenovergestelde** richting. |
| **Velden** | `remaining_suppress_ms`, `reason` `confluence_priority_window`, `suppress_after_conf_window_episodes_*`, `edge_*`.`enter_suppressed` |

### 5. Cooldown actief, markt beweegt opnieuw

| | |
|--|--|
| **Verwacht** | `status=cooldown`, `remaining_cooldown_ms` daalt; geen nieuwe emit tot venster voorbij — **bedoeld** “gemiste” piek. |
| **Twijfelachtig** | Structureel relevante bewegingen **alleen** tijdens cooldown — overweeg tuning (C3) of langere sessie. |
| **Defect** | `cooldown` met `remaining_cooldown_ms` negatief of vast op onmogelijke waarde; emit zonder reset `last_emit`. |
| **Velden** | `remaining_cooldown_ms`, `edge_*`.`enter_cooldown`, `last_emit_epoch_ms_*` |

### 6. WS-stilte / lage tickdichtheid

| | |
|--|--|
| **Verwacht** | `connection`/`valid` volgen feed; `ws_inbound_ticks_last_sec` laag → metrics traag; `not_ready` of verouderde `last_tick_ms`. |
| **Twijfelachtig** | Lange periodes `valid=0` — netwerk/WiFi, niet per se alert-defect. |
| **Defect** | `valid=1` maar `last_tick_ms` stopt minutenlang terwijl exchange live is (feed-bug). |
| **Velden** | `connection`, `valid`, `last_tick_ms`, `ws_inbound_ticks_last_sec`, `edge_*`.`enter_not_ready` |

### 7. Vol-warmup / `metrics_not_ready`

| | |
|--|--|
| **Verwacht** | `vol_metric_ready=false` → `vol_unavailable_fallback`, drempels basis-normal; `not_ready` op paden tot buffers gevuld. |
| **Twijfelachtig** | `vol_pairs_used` blijft lang laag na stabiele feed — check `domain_metrics` / feed-frequentie. |
| **Defect** | `vol_metric_ready` nooit true na lange uptime; of `not_ready` permanent terwijl prijs wel tickt. |
| **Velden** | `regime_observability.vol_metric_ready`, `vol_pairs_used`, `reason` `metrics_not_ready`, `edge_*`.`enter_not_ready` |

### 8. Regime-overgang calm ↔ normal ↔ hot

| | |
|--|--|
| **Verwacht** | Bij wissel: log `M-010f: regime →`; `last_regime_change_epoch_ms` update; effectieve drempels wijzigen ‰-gewijs. |
| **Twijfelachtig** | Flappen calm/hot bij grens — grens BPS in C3 reviewen. |
| **Defect** | `regime` springt zonder vol-verandering; of `last_regime_change_epoch_ms` nooit wijzigt terwijl log wel regime wisselt. |
| **Velden** | `regime`, `vol_mean_abs_step_bps`, `threshold_scale_permille`, `threshold_scale_permille_raw`, `threshold_scale_clamped`, `regime_calm_max_step_bps`, `regime_hot_min_step_bps`, `last_regime_change_epoch_ms` |

---

## Wanneer is C2 “voldoende”?

- Dit document is **leidend** voor interpretatie tijdens debug; er is **geen** verplichte tweede veldronde.
- **C3** (mini-regime review) kan starten als C1-bewijs er is en C2-tabellen **geen open “defect”** zonder ticket achterlaten — of defecten zijn **gearchiveerd** als bekende issues.

---

## Buiten scope (C2)

- 30m / 2h  
- Nieuwe tuning-UI, dashboards  
- Grote refactor `alert_engine`
