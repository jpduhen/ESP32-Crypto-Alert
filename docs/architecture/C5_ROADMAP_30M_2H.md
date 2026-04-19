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
| **`domain_metrics`** | Canonieke ~1 Hz-ring (`k_ring_cap` = **400** samples ≈ **6,7 min** historie); `compute_1m_move_pct`, `compute_5m_move_pct`; vol-proxy voor mini-regime (`compute_vol_mean_abs_step_bps`). | **`Metric30mMovePct` / `Metric2hMovePct`** (of equivalent); ring/decastie-strategie voor **≥30 min** resp. **≥2 uur** terugkijk — zie §3. |
| **`alert_engine`** | Drempels, cooldowns, confluence 1m+5m, suppress-venster, mini-regime op 1m/5m/conf. | Aparte drempel-/cooldown-/policy-paden voor **30m** en **2h**; observability-uitbreiding voor nieuwe TF’s. |
| **`service_outbound`** | Events + payloads 1m, 5m, confluence; NTFY/MQTT `kind` (C4). | Events/payloads + MQTT-topics/Kconfig voor **alert_30m**, **alert_2h** (namen ter ontwerpkeuze). |
| **`alert_observability` / WebUI** | `alert_decision_observability`, `alert_engine_runtime_stats`, randen C2. | Spiegelen van beslis-/edge-paden voor 30m/2h wanneer die bestaan. |

**Kernbeperking:** de huidige ring dekt **~6,7 minuten** — ruim genoeg voor 5m, **onvoldoende** voor een naïeve 30m %-move over canonieke secondes. Een 30m/2h-metricslice moet daarom expliciet kiezen voor **ringvergroting**, **decimatie** (bv. buckets), of **hybride referentieprijs** — dat is onderdeel van **slice S30-1** / **S2H-1**, niet van deze roadmap alleen.

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

### Slice S30-1 — 30m **metrics**

| | |
|--|--|
| **Doel** | Betrouwbare **`pct`-move over ~30 minuten** op basis van het bestaande canonieke prijspad (of expliciet goedgekeurde variant). |
| **Componenten** | `domain_metrics` (primair); evt. `Kconfig` voor venster/lengte. |
| **Binnen scope** | Nieuwe `compute_30m_move_pct()` (of naam volgens conventie); `ready`-semantiek gelijk aan 1m/5m; logging/ESP_LOG op debugniveau. |
| **Buiten scope** | Alerts, outbound, WebUI (behalve wat al via generieke metrics-hooks zichtbaar wordt — liever niet). |
| **Risico’s / afhankelijkheden** | **RAM/ring:** huidige 400 samples reikt niet; ontwerpkeuze: grotere ring, of lagere resolutie voor lange TF, of rolling anchor — **moet** in implementatie-review. Afhankelijk van stabiele `market_data`-feed. |

### Slice S30-2 — 30m **alert_engine**

| | |
|--|--|
| **Doel** | Eenvoudige **drempel + cooldown** voor 30m (analogaan 1m/5m), zonder nieuwe confluence-complexiteit tenzij expliciet gewenst. |
| **Componenten** | `alert_engine`; `config_store` / drempels (Kconfig/NVS-patroon zoals bestaand). |
| **Buiten scope** | 30m+5m confluence in deze slice — **optioneel later**; geen dubbele policy-laag. |
| **Risico’s** | Interactie met bestaande suppress/confluence: **definieer** of 30m “los” mag vuren naast 1m/5m (standaard: ja, met eigen cooldown). |

### Slice S30-3 — 30m **outbound + observability**

| | |
|--|--|
| **Doel** | `emit_domain_alert_30m` (of equivalent), NTFY/MQTT met **`kind`**-consistentie (C4-patroon), read-only stats/observability. |
| **Componenten** | `service_outbound`, `ntfy_client`, `mqtt_bridge`, `alert_observability`, evt. `webui` read-only JSON. |
| **Buiten scope** | Nieuwe WebUI-settingspagina’s; HA discovery. |

---

### Slice S2H-1 — 2h **metrics**

| | |
|--|--|
| **Doel** | **`pct`-move over ~2 uur** — zelfde architecturale scheiding als 30m; kan **andere** historie-strategie vereisen (langer venster → sterker argument voor decimatie of aparte buffer). |
| **Componenten** | `domain_metrics` (+ eventueel kleine helper voor gedeelde “long history”-opslag als S30-1 die introduceert). |
| **Risico’s** | Geheugen en CPU bij volledige 1 Hz × 7200 s; **verplicht** trade-off-document in PR. |

### Slice S2H-2 — 2h **alert_engine**

| | |
|--|--|
| **Doel** | Drempel + cooldown voor 2h; **geen** V1-2h-takkenmatrix — hoogstens **één** duidelijk alerttype per richting (move) in eerste iteratie. |
| **Buiten scope** | Trend/compress/breakout/anchor-context zoals V1 — tenzij later bewust herontworpen. |

### Slice S2H-3 — 2h **outbound + observability**

| | |
|--|--|
| **Doel** | Zelfde patroon als S30-3; MQTT-topics/Kconfig parallel aan 1m/5m/30m. |

---

## 4. Eerstvolgende implementatieslice (aanbevolen)

**S30-1 — 30m metrics** is de logische eerste bouwsteen: zonder betrouwbare lange-horizon metric geen zinvolle engine/outbound-laag.

Concrete startpunten in code (ter referentie):

- `firmware-v2/components/domain_metrics/domain_metrics.{hpp,cpp}` — patronen `Metric5mMovePct` / ring.
- `firmware-v2/components/domain_metrics/include/domain_metrics/domain_metrics.hpp`

---

## 5. Relatie met RWS

**RWS-03+** (trades → aggregate) kan **later** kwaliteit of alternatieve prijsinput geven voor langere TF’s; **geen** harde afhankelijkheid in S30-1/S2H-1 tenzij expliciet besloten. C5 houdt TF-uitbreiding en RWS gescheiden.

---

## 6. Changelog

| Datum | |
|-------|---|
| 2026-04-19 | Eerste vastlegging (C5 documentaire stap). |
