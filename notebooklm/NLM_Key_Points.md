# NLM Key Points – UNIFIED-LVGL9 Crypto Monitor

Max één pagina bullets; gebaseerd op /docs. Geen codewijzigingen.

---

- **ret_* eenheid:** Alle returns (ret_1m, ret_5m, ret_30m, ret_2h, …) zijn **percentagepunten**: `(priceNow - priceXAgo) / priceXAgo * 100`. Voorbeeld: 0,12 = 0,12%. Drempels (spike1m, move5m, enz.) gebruiken dezelfde eenheid.
- **Cooldowns en max-per-uur:** Per alerttype (1m, 5m, 30m) een cooldown in ms (bijv. 1m default 2 min, 30m 15 min) en een max aantal alerts per uur (bijv. 1m: 3, 30m: 2). Pas als cooldown verstreken is én het uurlimiet niet bereikt, mag er een notificatie worden *aangevraagd* (AlertEngine roept `sendNotification` aan; verzending in .ino).
- **2h throttling:** **PRIMARY** (breakout up/down) overslaat throttling. **SECONDARY** (mean touch, compress, trend change, anchor context) valt onder: (1) **global secondary cooldown** (default 7200 s = 120 min), (2) matrix-cooldowns tussen laatste en volgende 2h-alerttype, (3) **coalescing**-window (bijv. 90 s) om bursts te dempen. `shouldThrottle2HAlert` suppresseert secondaries als één van deze actief is.
- **PriceRepeat waarschuwing:** Bij API-uitval of trage responses zet priceRepeatTask elke 2 s de *laatste* prijs in de ringbuffer; 1m/5m-returns kunnen daardoor tijdelijk afgevlakt zijn (onderschatting volatiliteit) tot er weer nieuwe API-prijzen zijn.
- **WS feature-flag, HTTP primary:** WebSocket is feature-flagged (WS_ENABLED in platform_config.h). HTTP polling via ApiClient is de **primaire** prijsingang; WS wordt stap-voor-stap in .ino gebruikt.
- **Decision vs delivery:** AlertEngine en AnchorSystem **beslissen** en bouwen de payload (title, message, colorTag); zij roepen extern `sendNotification()` aan. De **verzending** (NTFY via `sendNtfyNotification()` in .ino; anchor daarnaast MQTT via `publishMqttAnchorEvent()` in .ino) zit in de hoofdschets; er is geen aparte notifier-module in src/.
- **DataSource:** SOURCE_BINANCE = legacy enum-naam; functioneel warm-start/historische candles (Bitvavo). SOURCE_LIVE = live API. Zie PriceData.h.
- **State:** Volatile (RAM): prices, ringbuffers, anchor, trend/vol state, cooldown-timestamps. Persistent: NVS via SettingsStore (CryptoMonitorSettings). Derived: ret_*, trend/vol states, hasRet*-flags.

**Trade-offs / Known limitations**

- **UI snapshot niet geïmplementeerd:** Snapshot under mutex is aanbevolen maar niet in code; uiTask leest globals zonder mutex tijdens updateUI(), met risico op een inconsistent frame (mix van oude/nieuwe waarden).

---
**[← Story Script](NLM_Story_Script.md)** | [Overzicht technische docs (README NL)](../README_NL.md#technische-documentatie-code-werking) | **[Examples →](NLM_Examples.md)**
