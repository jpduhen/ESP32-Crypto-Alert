# Uitgangspunten V2-herbouw

**Status:** concept — vastgelegd in de voorbereidingsfase van V2.  
**Referentie V1:** huidige Arduino/ESP32-build in repo-root en `src/`.

## Doelen

1. **Modulariteit:** duidelijke grenzen tussen netwerk, domein (prijs/alerts), presentatie (display/WebUI), en platform (board/HAL).
2. **Onderhoudbaarheid:** minder conditionele compilatie-spaghetti; configuratie en board-support expliciet en testbaar.
3. **Productierijpheid:** reproduceerbare builds, duidelijke update-/OTA-story, logging en diagnostiek op één manier gemodelleerd.

## Technische voorkeur

- **Build:** voorkeur **ESP-IDF** (versie en migratiepad worden vastgelegd bij start van de echte port).
- **V1:** blijft de functionele referentie tot V2 feature-pariteit bereikt is (per besluit).

## Doelboards (prioriteit)

| Prioriteit | Board        | Opmerking                          |
|-----------|--------------|-------------------------------------|
| 1         | ESP32-S3 GEEK | Eerste referentiehardware voor V2   |
| 2         | LCDWIKI       | Zoals in V1 ondersteund             |
| 3         | JC3248W535    | QSPI / AXS15231B — complexer displaypad |

**Geen prioriteit meer:** CYD, TTGO (tenzij later expliciet heropend).

## Te behouden waarde uit V1 (kandidaten)

Zonder voorafbepalen van implementatie in V2:

- WebUI en configuratieflow (conceptueel).
- OTA-updates.
- WebSocket- en REST-paden naar prijsdata (waar van toepassing).
- NTFY- en MQTT-integratie (notificaties en domoticakoppeling).

## Expliciete randvoorwaarden

- Geen “big bang”-verwijdering van V1-code in deze repo; V2 groeit naast of in `firmware-v2/` volgens teamafspraken.
- Aannames over Bitvavo API, TLS, en flash-layout blijven **nader te valideren** tijdens de migratie.

## Open punten (bewust niet ingevuld)

- Exacte ESP-IDF-versie en componentkeuze (LVGL, netwerkstack).
- Of delen van V1 als Arduino-component binnen IDF tijdelijk blijven bestaan.
