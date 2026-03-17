# Release notes — v5.10

**Base:** v5.09 rebuild (branch `rebuild-from-v509`, baseline commit 82d22b4).  
**Version:** 5.10 (see `platform_config.h`: `VERSION_STRING "5.10"`).

---

## Major changes since v5.09 baseline

- **Manual-anchor-only (Phase 1):** Anchor runtime uses only manually set anchor. `getActiveAnchorPrice()` returns manual anchor or 0 (OFF); auto-anchor logic is not executed. All anchor sets (Web, MQTT, button, UI long-press) go through `queueAnchorSetting` and run in apiTask.
- **NTFY cleanup order:** In `sendNtfyNotification`, cleanup order is: get stream, `stream->stop()`, `http.end()`, `ntfyClient.stop()`.
- **Short-term notification tuning (Phase 1A/1B):** Volume-event cooldown reduced from 120 s to 60 s (`VOLUME_EVENT_COOLDOWN_MS`). After a successful confluence notification, only `last1mEvent.usedInConfluence` is set; `last5mEvent.usedInConfluence` is no longer set (no Phase 1C same-round 5m exception).
- **Notification title ↔ tag congruence:** 2h breakout/breakdown tags aligned with title semantics (🟪 up, 🟥 down). 2h compress tag set to 🟨 (neutral). Auto-anchor notification tag set to 🟫. Other alert tags unchanged.
- **Settings export:** `/settings.txt` export page; header uses project version (`VERSION_STRING`) so exported file shows current version (e.g. `# ESP32-Crypto-Alert 5.10 settings export`).
- **Notifications page:** Web UI includes a notifications section (recent notifications, tags, etc.) as in the rebuild.
- **OTA web update:** Optional OTA firmware update via web UI is supported when enabled in the build (`OTA_ENABLED` in `platform_config.h`). Default in this rebuild is **off**; use a partition scheme with OTA slots and set `OTA_ENABLED 1` to use `/update`. See `docs/OTA_UPDATE.md`.

---

## Intentional exclusions (not in v5.10)

- **No ArduinoOTA:** OTA in this build is the chunked web-upload flow only (when enabled), not ArduinoOTA.
- **No auto-anchor runtime:** Auto-anchor is not reintroduced; anchor source modes AUTO/AUTO_FALLBACK are not used at runtime. Manual and OFF only.
- **No Phase 1C:** No `confluenceSentThisRound` flag and no 5m cooldown exception in the same round as confluence.
- **No notification log in code** beyond what the Web UI displays (no extra mutex/log buffer from mainline).
- **No move30mHardOverride / force-allow 30m** logic from mainline.
- **No webPassword / export auth** from mainline in this release.

---

## Docs and references

- Baseline notification spec: `docs/V509_NOTIFICATION_BASELINE.md`.
- Migration risk analysis: `docs/V509_MAINLINE_MIGRATION_RISK_ANALYSIS.md`.
- Phase 1 implementation: `docs/PHASE1_IMPLEMENTATION_SUMMARY.md`.
- Short-term proposal (Phase 1): `docs/PHASE1_SHORTTERM_NOTIFICATION_PROPOSAL.md`.
- Title/tag audit: `docs/TITLE_COLORTAG_CONGRUENCE_AUDIT.md`.
- OTA: `docs/OTA_UPDATE.md`.
