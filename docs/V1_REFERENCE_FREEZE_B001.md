# B-001 — V1 als formele referentielijn (bevroren)

**Status:** vastgelegd  
**Datum:** 2026-04-11  
**Repo:** [ESP32-Crypto-Alert](https://github.com/jpduhen/ESP32-Crypto-Alert)

## Wat is “V1”?

- **Codepad:** Arduino-IDE / PlatformIO-stijl firmware in de **repository root**: `ESP32-Crypto-Alert.ino`, `src/`, `platform_config.h`, enz.
- **Branch (canoniek):** **`main`** — dit is de referentielijn voor de bestaande productie-/veldconfiguratie zolang V2 nog niet feature-pariteit heeft.

## Formele referentie (tag)

- **Aanbevolen git-tag** (door maintainer op te zetten wanneer de tree stabiel is):  
  `v1-reference-frozen`  
  op de commit die op dat moment **`main`** representeert.
- **Tot die tag bestaat:** de referentie is **de tip van `main`** op het moment van een release- of onderhoudsbesluit; documenteer desnoods het commit-SHA in release notes.

```bash
# Eenmalig (voorbeeld):
git checkout main
git pull
git tag -a v1-reference-frozen -m "B-001: V1 referentie freeze"
git push origin v1-reference-frozen
```

## Wat is bevroren?

- **Geen** nieuwe grote functionele lagen of brede architectuur-herslagen op V1 zonder expliciet besluit (werkdocument / issue).
- **Geen** verplaatsing van de primaire ontwikkelrichting terug naar V1 terwijl V2 actief wordt doorontwikkeld.

## Wat mag nog wél op V1?

- **Onderhoud:** kritieke bugfixes, securitypatches, kleine correcties.
- **Documentatie** en gebruikershandleiding.
- **Board-/pin-aanpassingen** die de bestaande structuur respecteren.
- **Compatibiliteit** met bestaande gebruikers (geen breaking wijzigingen zonder versie/communicatie).

## Relatie tot V2

| | V1 (`main`, repo-root Arduino) | V2 (`v2/foundation`, `firmware-v2/`, ESP-IDF) |
|---|-------------------------------|-----------------------------------------------|
| **Rol** | Referentie, veldproven gedrag, fallback | Actieve herbouw, nieuwe architectuur |
| **Toolchain** | Arduino / bestaande workflow | ESP-IDF v5.4.2 (zie `firmware-v2/ESP_IDF_VERSION`) |
| **Primaire status** | [README.md](../README.md), release notes | [firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md](../firmware-v2/v_2_herbouw_werkdocument_esp_32_crypto_alert.md) |

Nieuwe features die de complexiteit van V1 verder opjagen horen in principe **niet** op `main`, tenzij er een bewuste uitzondering is (bijv. productiestop op V2).

## Zie ook

- [M-002 netwerkgrenzen (V2)](architecture/M002_NETWORK_BOUNDARIES.md) — zelfde repo; V2-ontwikkeling.
