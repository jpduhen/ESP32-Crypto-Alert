# C1 — Field-testprotocol + spamreview (1m/5m-kern)

**Primaire bron:** [`firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md`](../../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) — § **WP-03a**, besluit **D-010**, § **C1** (samenvatting).

**Doel:** de actieve V2-kern (1m-, 5m-, confluence-alerts, mini-regime M-010f, cooldown/suppress M-003c/M-010e) **toetsen op echte marktdata** en **kwaliteit van meldingen** vastleggen — **geen** nieuwe features, **geen** 30m/2h.

**Branch / code:** `v2/foundation` — `alert_engine`, `domain_metrics`, `service_outbound`, `webui`.

---

## 1. Testopzet

| Onderdeel | Richtlijn |
|-----------|-----------|
| **Hardware** | ESP32-S3 GEEK (referentieboard), stabiele WiFi, live Bitvavo-feed |
| **Symbool** | Vast kiezen (bijv. BTC-EUR); niet wisselen tijdens een sessie |
| **Config** | Defaults of bewust gekozen NVS-waarden — **documenteren** in het testverslag (screenshot of JSON-export) |
| **Duur** | Minimaal **2× 2 uur** op **verschillende marktmomenten** (bijv. EU-open + rustiger slot); idealiter **1× langere sessie (≥4 uur)** bij hogere volatiliteit |
| **Herstart** | Optioneel: **één** gecontroleerde reboot tussendoor om te zien of tellers/logica verwacht resetten (stats sinds boot) |

---

## 2. Wat vastleggen (bewijs)

1. **Tijdstippen** sessiestart/-einde (lokaal), eventuele reboots.
2. **`GET /api/status.json`** (of WebUI-secties op `/`):  
   - `alert_engine_runtime_stats` — emit-totalen, laatste emit-tijden, suppress-episodes  
   - `alert_decision_observability` — status/reason/cooldown/suppress per pad  
   - `regime_observability` — regime, vol, effectieve drempels  
   - `alert_runtime_config`, `alert_policy_timing`, `alert_confluence_policy` — **read-only** kopie van actieve parameters  
   - `service_outbound` / queue-indicatoren indien aanwezig (drops, backlog).
3. **Seriële log** (filter op tag `alert_eng` of niveau INFO): confluence-, 1m-, 5m-FIRE en suppress-regels.
4. **NTFY/MQTT** (indien ingeschakeld): aantal ontvangen berichten per type **of** kopie van een representatieve reeks (geen volledige log verplicht).
5. **Korte notities** bij opvallende marktfasen (stilte, spike, zijwaarts).

---

## 3. Classificatie gedrag (tester)

| Label | Betekenis |
|-------|-----------|
| **Goed** | Drempels en timing voelen **in lijn** met de markt; confluence levert **duidelijke meerwaarde**; suppress/cooldown **verklaren** waarom een dubbelmelding uitblijft; regime past drempels **plausibel** aan volatiliteit. |
| **Twijfelachtig** | Geen duidelijke fout, maar **herhaald** patroon (bijv. veel emits in rust, of bijna geen melding bij duidelijke beweging) — **nader te bekijken** in C2/C3 of met extra sessie. |
| **Defect** | **Onlogische** combinatie (bijv. tegenstrijdige indruk zonder verklaring in snapshot), **structurele** spam, **structurele** stilte boven drempel, of **duidelijk** foute prioriteit confluence t.o.v. policy. |

---

## 4. Spamreview-criteria (1m/5m-kern)

Gebruik deze lijst als **checklist**; niet elke sessie hoeft elk punt te “falen” — het gaat om **patronen**.

| Thema | Onacceptabel / defect-richting | Reviewvraag |
|-------|----------------------------------|-------------|
| **Herhaling** | Zelfde type melding **veel vaker** dan marktbeweging rechtvaardigt (t.o.v. cooldown-instellingen) | Zijn `emit_total_*` en log-tijdstippen **verdedigbaar**? |
| **Tegenstrijdigheid** | Indruk van **tegenstrijdige** signalen **zonder** verklaring (policy, richting, cooldown) | Klopt richting 1m/5m met snapshot? Zie `reason` in `alert_decision_observability` |
| **Confluence-waarde** | Confluence vuurt **zonder** dat 1m+5m samen **duidelijk** sterker voelen dan losse TF | Verhouding `emit_total_confluence` vs losse emits; beleving vs defaults |
| **Suppress / cooldown** | **Te agressief:** relevante bewegingen **systematisch** gemist · **Te slap:** weinig effect van suppress na confluence | `suppress_after_conf_window_episodes_*`, `remaining_*_ms` in snapshot |
| **Mini-regime (M-010f)** | Effectieve drempels **onlogisch** t.o.v. vol (bijv. hot terwijl markt doodstil) | `regime_observability` vs notities markt |

---

## 5. Wanneer is C1 “voldoende” om door naar C2?

C1 is een **kwalificatie-opdracht**, geen wiskundige drempel. Ga door naar **C2 (randgevallen documenteren)** als:

1. Er minstens **twee** volledige sessies zijn uitgevoerd (of één lange + één extra) met **vastgelegde** status-JSON en korte conclusie.
2. De tester **goed / twijfelachtig / defect** expliciet per thema kan invullen (geen lege checklist).
3. Bij **defect**: **reproducerende** beschrijving + snapshot/log; **geen** blokkerende open punten die alleen met nieuwe code kunnen — die horen in een apart ticket, niet in “C1 afgerond”.

Als alles **twijfelachtig** blijft zonder defect: **mag** doorgaan naar C2 met de notitie dat tuning later nodig kan zijn (C3/C4).

---

## 6. Relatie met code (C1-observability)

- **`alert_engine_runtime_stats`** in `GET /api/status.json`: emit-totalen sinds boot, laatste emit-epoch (ms), aantal **suppress-episodes** na confluence (M-010e), per TF.
- Geen nieuwe instellingen; geen wijziging aan alertlogica behalve tellers.

---

## 7. Buiten scope (C1)

- 30m / 2h metrics of alerts  
- Nieuwe WebUI-settings, nieuwe alerttypes  
- Home Assistant discovery, extra boards  
- Grote refactor `alert_engine`
