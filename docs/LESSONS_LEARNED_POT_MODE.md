# Lessen geleerd: Pot Mode implementatie

## Probleem
Bij het implementeren van Pot Mode MQTT callbacks ontstonden er meerdere compile errors door structuurproblemen in `mqttCallback`.

## Wat ging er fout?

### 1. **SettingsStore API verkeerd gebruikt**
- **Fout**: `CryptoMonitorSettings& settings = settingsStore.settings;` - SettingsStore heeft geen `settings` member
- **Oplossing**: Gebruik `settingsStore.load()` om settings te laden en `settingsStore.save(settings)` om op te slaan

### 2. **Structuurproblemen in mqttCallback**
- **Fout**: `displayInversion` callback stond binnen de `else` van `language`, wat de nesting verstoorde
- **Fout**: Pot mode callbacks werden verkeerd genest binnen bestaande `if (!handled)` blokken
- **Oplossing**: Elke nieuwe callback moet een eigen `if (!handled)` blok hebben op hetzelfde niveau

### 3. **Ontbrekende sluitende accolades**
- **Fout**: Door verkeerde nesting ontbraken er sluitende accolades, waardoor de compiler dacht dat `mqttCallback` nooit werd afgesloten
- **Oplossing**: Altijd de structuur controleren door accolades te tellen

## Best practices voor toekomstige MQTT callback toevoegingen

### 1. **Gebruik altijd hetzelfde patroon**
```cpp
// Nieuwe callback - altijd op hetzelfde niveau als andere callbacks
if (!handled) {
    snprintf(topicBufferFull, sizeof(topicBufferFull), "%s/config/nieuweSetting/set", prefixBuffer);
    if (strcmp(topicBuffer, topicBufferFull) == 0) {
        // Verwerk waarde
        // ...
        handled = true;
    }
}
```

### 2. **SettingsStore correct gebruiken**
```cpp
// FOUT:
CryptoMonitorSettings& settings = settingsStore.settings;  // SettingsStore heeft geen settings member!

// GOED:
CryptoMonitorSettings settings = settingsStore.load();  // Laad settings
// ... pas settings aan ...
settingsStore.save(settings);  // Sla op
```

### 3. **Structuur controleren**
- Elke `if (!handled)` moet een eigen blok zijn
- Niet nesten binnen bestaande `else` blokken
- Altijd `handled = true;` zetten na verwerking
- Controleren dat alle accolades kloppen

### 4. **Testen tijdens ontwikkeling**
- Compileer regelmatig tijdens het toevoegen van code
- Controleer structuur voordat je verder gaat
- Gebruik een code editor met syntax highlighting

## Wat moeten we nu doen?

1. **Pot Mode MQTT callbacks opnieuw implementeren** met de juiste structuur
2. **SettingsStore API correct gebruiken** - altijd `load()` en `save()` gebruiken
3. **Elke callback in een eigen `if (!handled)` blok** plaatsen
4. **Tussentijds compileren** om structuurproblemen vroeg te detecteren

## Volgende stappen

1. Pot Mode MQTT callbacks opnieuw implementeren met correcte structuur
2. Testen dat de code compileert na elke wijziging
3. Verifiëren dat alle accolades kloppen
