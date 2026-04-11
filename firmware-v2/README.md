# Firmware V2 (voorbereiding)

Deze map is de geplande plek voor de **nieuwe firmware** tijdens de gecontroleerde V2-herbouw. V1 blijft in de repo-root (`ESP32-Crypto-Alert.ino`, `src/`, `platform_config.h`) de functionele referentie.

## Richting

- **Voorkeur:** ESP-IDF als bouwsysteem voor productierijpheid, componenten en tooling.
- **Status:** nog geen volledig ESP-IDF-project; deze stap levert alleen structuur en documentatie.

## Beoogde lay-out (ESP-IDF-conventie)

Wanneer de migratie start, kan de inhoud er ongeveer zo uitzien:

```
firmware-v2/
├── CMakeLists.txt          # project() — nog toe te voegen
├── sdkconfig.defaults      # board defaults — nog toe te voegen
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild   # optioneel
│   └── …                   # broncode, entry `app_main`
├── components/             # optioneel: eigen herbruikbare componenten
└── README.md               # dit bestand
```

De map `main/` is nu voorbereid met een placeholder (`.gitkeep`) zodat de structuur in Git zichtbaar blijft.

## Relatie met V1

- Domeinlogica (alerts, metrics, warm start, enz.) wordt **conceptueel** hergebruikt of opnieuw gemodelleerd; **geen copy-paste-migratie** zonder review.
- Zie ook: `docs/migration/` en `docs/architecture/`.
