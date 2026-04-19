# C5 — Roadmap 30m / 2h (verticale slices)

**Status:** vastgelegd als onderdeel van consolidatiestap **C5** (WP-03a, D-010, §2, §9a, §10, §15).  
**Doel:** een **uitvoerbare volgorde** van kleine verticale slices — **niet** de volledige implementatie in één keer.

**Leidende principes**

- **Functioneel gewenst:** gebruikers willen alerts op **30m** en **2h** naast **1m/5m** (D-010, §2).
- **Technisch bewust níet overnemen uit V1:** geen monolithische `AlertEngine.cpp`-port; geen V1 **2h-alertmatrix** als geheel; geen **AnchorSystem**-, **TrendDetector**- of brede **RegimeEngine**-port; geen policy-explosie.
- **V2-patroon:** dezelfde discipline als 1m/5m: `domain_metrics` → `alert_engine` → `service_outbound` (+ NTFY/MQTT) + **observability** (read-only waar mogelijk).

---

## 1. Huidige codebase (korte inventaris)

| Laag | Aanwezig voor 1m/5m | Ontbreekt voor 30m/2h |
|------|---------------------|------------------------|
| **`domain_metrics`** | Canonieke ~1 Hz-ring (**S30-1**, ~**32 min**); `compute_1m/5m/30m`; **S2H-1:** `compute_2h_move_pct` op **minuut-decimatie-ring** (RAM << 7200×1 Hz); vol-proxy (`compute_vol_mean_abs_step_bps`). | **S2H-2:** 2h **alert_engine** op deze metric. |
| **`alert_engine`** | Drempels, cooldowns, confluence 1m+5m, suppress-venster, mini-regime op 1m/5m/conf.; **S30-2/S30-3:** 30m + outbound; **S2H-2/S2H-3:** 2h-pad op `compute_2h_move_pct`, logs + `tf_2h` + outbound. | **S2H-3** afgerond: zelfde productkanalen als 30m. |
| **`service_outbound`** | Events + payloads 1m, 5m, **30m**, confluence; NTFY/MQTT `kind` (C4). | Payloads + MQTT-topics/Kconfig voor **alert_2h** (namen ter ontwerpkeuze) — **S2H-3**. |
| **`alert_observability` / WebUI** | `alerts_1m` / `alerts_5m` / **`alerts_30m`**, confluence; `alert_decision_observability`, stats. | Spiegelen voor **2h** wanneer **S2H-2**/**S2H-3** bestaan. |

**Kernbeperking (vóór S30-1):** de ring was **~400** samples (**~6,7 min**) — genoeg voor 5m, **onvoldoende** voor 30m. **S30-1** lost 30m op door **één canonieke ring te vergroten** (1 Hz, `DOMAIN_METRICS_30M_WINDOW_S` + `DOMAIN_METRICS_CANONICAL_RING_EXTRA_S`). **S2H-1** volgt **niet** dezelfde truc voor 2 uur (te veel RAM): **minuut-decimatie-ring** + `compute_2h_move_pct`.

---

## 2. Wat we níet willen (expliciet)

- Geen **V1 2h-matrix** of volledige **2h-alertfamilie** zoals in V1 geknoopt.
- Geen **AnchorSystem**- of **TrendDetector**-port.
- Geen **grote RegimeEngine** (calm/normal/hot blijft de lichte M-010f-laag; geen V1-RegimeEngine).
- Geen **night mode**, **HA discovery**-uitbreiding, of **dashboards** als onderdeel van deze TF-uitbreiding — tenzij later afzonderlijk besloten.
- Geen **RWS-03** (SecondSampler/aggregate) verplicht koppelen aan 30m/2h; parallel spoor blijft optioneel nuttig voor langere horizons.

---

## 3. Verticale slices (volgorde)

Elke slice is **klein**, reviewbaar, en sluit aan op de bestaande componentgrenzen.

### Slice S30-1 — 30m **metrics** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | Betrouwbare **`pct`-move over ~30 minuten** op basis van het bestaande canonieke prijspad (of expliciet goedgekeurde variant). |
| **Componenten** | `domain_metrics`; `Kconfig` `DOMAIN_METRICS_30M_WINDOW_S` + `DOMAIN_METRICS_CANONICAL_RING_EXTRA_S`; read-only **`metric_30m_observability`** in `GET /api/status.json` (WebUI). |
| **Uitvoering** | Ringvergroting (default **1920** × `Sample` @ 1 Hz); `compute_30m_move_pct()` + `canonical_ring_count()`; zelfde referentie-regel als 1m/5m (nieuwste sample met `ts ≤ now−window`). |
| **Buiten scope (bewust)** | 30m-alerts, outbound, MQTT/NTFY — zie **S30-2** / **S30-3**. |
| **Risico’s / afhankelijkheden** | **~30 KiB** extra SRAM t.o.v. oude 400-slotring; warmup **~30 min** live voordat `ready=true`. |

### Slice S30-2 — 30m **alert_engine** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | Eenvoudige **drempel + cooldown** voor 30m (analogaan 1m/5m), zonder confluence/suppress-koppeling. |
| **Componenten** | `alert_engine`; Kconfig `ALERT_ENGINE_30M_THRESHOLD_BPS`, `ALERT_ENGINE_30M_COOLDOWN_S`; M-010f **‰** op 30m-basisdrempel; `alert_decision_observability.tf_30m` + runtime stats / edges. |
| **Buiten scope (bewust)** | **Geen** `service_outbound` / NTFY/MQTT — **S30-3**; geen NVS-runtime voor 30m; geen 30m+5m confluence. |
| **Gedrag** | 30m vuurt **onafhankelijk** van 1m/5m/confluence-suppress; eigen cooldown. |

### Slice S30-3 — 30m **outbound + observability** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | `emit_domain_alert_30m`, NTFY/MQTT met **`kind: alert_30m`** (C4-patroon), read-only **`alerts_30m`** + HTML op `/`. |
| **Componenten** | `service_outbound` (`DomainAlert30mMovePayload`), `ntfy_client`, `mqtt_bridge` (`MQTT_TOPIC_DOMAIN_ALERT_30M`), `alert_observability`, `webui`. |
| **Buiten scope (bewust)** | 2h; 30m-confluence; nieuwe WebUI-settingspagina’s; HA discovery. |

---

### Slice S2H-1 — 2h **metrics** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | **`pct`-move over ~2 uur** (`DOMAIN_METRICS_2H_WINDOW_S`, default **7200 s**): referentie = nieuwste **minuut-slot** met `ts ≤ now − window`; `now` uit canonieke 1 Hz-ring. |
| **Componenten** | `domain_metrics`: rolling **minuut-ring** (±`ceil(window/60)+extra` slots) gevuld vanuit elke canonieke seconde + carry-pad; `Metric2hMovePct`; **`metric_2h_observability`** in status.json. |
| **Trade-off** | Geen **7200×** canonieke `Sample`s: **~2–3 KiB** minuut-buffer i.p.v. **~115 KiB** extra 1 Hz-RAM; referentie is **minuut-slot** (niet elke seconde op exact \(t-2h\)). |
| **Buiten scope (bewust)** | Geen 2h-alerts, geen outbound — **S2H-2** / **S2H-3**. |

### Slice S2H-2 — 2h **alert_engine** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | Drempel + cooldown op **\|pct\|** uit `compute_2h_move_pct`; M-010f **‰** op Kconfig-basisdrempel; logs + `tf_2h` + `emit_total_2h` (log-only). |
| **Componenten** | `alert_engine`; Kconfig `ALERT_ENGINE_2H_THRESHOLD_BPS`, `ALERT_ENGINE_2H_COOLDOWN_S`; `alert_decision_observability.tf_2h`, runtime stats / `edge_2h`; regime JSON `move_pct_2h`. |
| **Buiten scope (bewust)** | **Geen** `service_outbound` / NTFY/MQTT — **S2H-3**; geen NVS-runtime voor 2h; geen confluence/suppress-koppeling. |

### Slice S2H-3 — 2h **outbound + observability** ✅ (geïmplementeerd)

| | |
|--|--|
| **Doel** | Zelfde patroon als S30-3; MQTT-topics/Kconfig parallel aan 1m/5m/30m. |
| **Resultaat** | `emit_domain_alert_2h` → queue → NTFY (`Soort: alert_2h`) + MQTT (`kind":"alert_2h"`, topic `MQTT_TOPIC_DOMAIN_ALERT_2H`) + `alerts_2h` in status.json + HTML-sectie op `/`. |

---

## 4. Eerstvolgende lens (aanbevolen)

**Productierijpheid / releasekwaliteit** — 30m- en 2h-verticale slices zijn afgerond; verdere TF-uitbreiding niet automatisch eerst.

**RWS-03** (SecondSampler/aggregate) blijft een **parallel** architectuurspoor — geen verplichte voorloper voor de afgeronde 30m/2h-slices.

Ter referentie (voltooid **S2H-2/S2H-3**): `tf_2h`, `emit_domain_alert_2h`, `ALERT_ENGINE_2H_*` Kconfig, firmware **3.0.0**.

---

## 5. Relatie met RWS

**RWS-03+** (trades → aggregate) kan **later** kwaliteit of alternatieve prijsinput geven voor langere TF’s; **geen** harde afhankelijkheid in S30-1/S2H-1 tenzij expliciet besloten. C5 houdt TF-uitbreiding en RWS gescheiden.

---

## 6. Changelog

| Datum | |
|-------|---|
| 2026-04-19 | Eerste vastlegging (C5 documentaire stap). |
| 2026-04-19 | **S30-1** geïmplementeerd: vergrote canonieke ring + `compute_30m_move_pct`, `metric_30m_observability` in status.json. |
| 2026-04-19 | **S30-2** geïmplementeerd: 30m drempel/cooldown in `alert_engine`, logs, `tf_30m` decision + stats (geen outbound). |
| 2026-04-19 | **S30-3** geïmplementeerd: `emit_domain_alert_30m`, NTFY/MQTT `alert_30m`, `alerts_30m` + WebUI-sectie; firmware **2.7.0**. |
| 2026-04-19 | **S2H-1** geïmplementeerd: `compute_2h_move_pct` + minuut-decimatie-ring, `metric_2h_observability`; firmware **2.8.0**. |
| 2026-04-19 | **S2H-2** geïmplementeerd: 2h drempel/cooldown in `alert_engine`, `tf_2h` + stats; firmware **2.9.0** (geen outbound). |
| 2026-04-19 | **S2H-3** geïmplementeerd: `emit_domain_alert_2h`, NTFY/MQTT `alert_2h`, `alerts_2h` + WebUI; firmware **3.0.0**. |
