# Unified LVGL9 Crypto Monitor

Een unificatie van de Crypto Monitor voor verschillende ESP32 display platforms: TTGO T-Display, CYD 2.4" en CYD 2.8".

## Ondersteunde Platforms

- **TTGO T-Display**: 1.14" 135x240 TFT display (ST7789)
- **CYD 2.4"**: 2.4" 240x320 TFT display met touchscreen (XPT2046)
- **CYD 2.8"**: 2.8" 240x320 TFT display met touchscreen (XPT2046)

## Features

- Real-time Bitcoin (BTCEUR) prijs monitoring via Binance API
- Live grafiek met 60 datapunten (1 minuut historie)
- Trend detectie (2-uur trend analyse)
- Volatiliteit monitoring (laag/gemiddeld/hoog)
- Anchor price tracking met take profit en stop loss alerts
- 1 minuut en 30 minuut gemiddelde prijs tracking
- Min/Max/Diff weergave voor 1m en 30m periodes
- MQTT integratie voor home automation
- NTFY.sh notificaties voor alerts
- Web interface voor configuratie
- WiFi Manager voor eenvoudige WiFi setup

## Hardware Vereisten

### TTGO T-Display
- ESP32 met TTGO T-Display module
- 1.14" 135x240 TFT display (ST7789)
- Fysieke reset button (GPIO 0)

### CYD 2.4" / 2.8"
- ESP32 met CYD display module
- 2.4" of 2.8" 240x320 TFT display
- Touchscreen (XPT2046)

## Software Vereisten

- Arduino IDE 1.8.x of 2.x
- ESP32 Board Support Package
- LVGL library v9.2.2 of hoger
- Arduino_GFX library
- WiFiManager library
- PubSubClient3 library
- XPT2046_Touchscreen library (alleen voor CYD varianten)

## Installatie

> **⚠️ BELANGRIJK**: Lees eerst de hele installatie sectie voordat je begint!

### Stap 1: Clone de Repository
```bash
git clone https://github.com/<jouw-username>/<repository-naam>.git
cd UNIFIED-LVGL9-Crypto_Monitor
```

**Of download als ZIP**: Klik op de groene "Code" knop op GitHub en selecteer "Download ZIP"

### Stap 2: Selecteer je Platform ⚠️ BELANGRIJK - DIT MOET JE EERST DOEN!

**Je MOET eerst aangeven welk bordje je gebruikt voordat je de code compileert!**

**Locatie**: Open het bestand **`platform_config.h`** in de root van het project (naast `UNIFIED-LVGL9-Crypto_Monitor.ino`)

**Wat te doen**:
1. Open `platform_config.h` in een teksteditor of Arduino IDE
2. Zoek naar de volgende regels (ongeveer regel 5-7):
```cpp
//#define PLATFORM_CYD28
#define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

3. **Activeer het juiste platform** door de `//` (commentaar) te verwijderen en de andere regels uit te commentariëren:

**Voor TTGO T-Display:**
```cpp
//#define PLATFORM_CYD28
#define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

**Voor CYD 2.4":**
```cpp
//#define PLATFORM_CYD28
// #define PLATFORM_TTGO
#define PLATFORM_CYD24
```

**Voor CYD 2.8":**
```cpp
#define PLATFORM_CYD28
// #define PLATFORM_TTGO
// #define PLATFORM_CYD24
```

**⚠️ Let op**: Er mag maar ÉÉN platform actief zijn! Zorg dat de andere twee regels met `//` zijn uitgecommentarieerd.

#### Optioneel: Stel Standaard Taal In

Je kunt optioneel de standaard taal instellen in `platform_config.h`. Deze wordt gebruikt als fallback als er nog geen taal is opgeslagen in Preferences (bijvoorbeeld bij eerste opstarten).

**Locatie**: In `platform_config.h`, zoek naar de taal instelling (ongeveer regel 10-12):

```cpp
// Standaard taal instelling (0 = Nederlands, 1 = English)
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // 0 = Nederlands, 1 = English
#endif
```

**Opties**:
- `0` = Nederlands - Standaard
- `1` = English

**Let op**: Je kunt de taal altijd later wijzigen via de web interface. Deze instelling wordt alleen gebruikt als fallback bij eerste opstarten.

### Stap 3: Installeer Libraries

Installeer de vereiste libraries via Arduino Library Manager:
   - **LVGL** (v9.2.2 of hoger) - Vereist voor alle platforms
   - **WiFiManager** - Vereist voor alle platforms
   - **PubSubClient3** - Vereist voor alle platforms
   - **Arduino_GFX** - Vereist voor alle platforms
   - **XPT2046_Touchscreen** - Alleen vereist voor CYD 2.4" en 2.8" (niet nodig voor TTGO)

### Stap 4: Upload naar ESP32

1. Open `UNIFIED-LVGL9-Crypto_Monitor.ino` in Arduino IDE
2. Selecteer je ESP32 board in Tools → Board
3. Selecteer de juiste poort in Tools → Port
4. Klik op Upload

### Stap 5: Eerste Opstarten

Bij eerste opstarten:
   - Verbind met het WiFi Access Point dat wordt aangemaakt
   - Configureer je WiFi credentials via de web interface
   - Configureer MQTT en NTFY settings (optioneel)

## Configuratie

### Web Interface

Na de eerste WiFi setup kun je de web interface bereiken op het IP-adres dat op het scherm wordt getoond.

**Toegang**: Open je browser en ga naar `http://<IP-adres>` (bijv. `http://192.168.1.50`)

De web interface biedt een overzichtelijke, donkere interface met alle instellingen gegroepeerd in secties:

#### Taal Selectie
- **Taal / Language**: Kies tussen Nederlands of English
  - Deze instelling beïnvloedt alle teksten op het display en in de web interface
  - De taal wordt opgeslagen in Preferences en blijft behouden na herstart
  - Je kunt ook een standaard taal instellen in `platform_config.h` (zie Installatie sectie)

#### Basis Instellingen
- **NTFY Topic**: Je NTFY.sh topic naam voor notificaties
- **Binance Symbool**: Het trading pair dat je wilt monitoren (bijv. BTCEUR, BTCUSDT, ETHUSDT)

#### Spike & Move Alerts
Configureer wanneer je alerts wilt ontvangen bij snelle prijsbewegingen:

- **1m Spike - ret_1m threshold (%)**: Drempelwaarde voor 1-minuut spike alerts (standaard: 0.30%)
- **1m Spike - ret_5m filter (%)**: Filter om valse meldingen te voorkomen (standaard: 0.60%)
  - *Uitleg*: Beide voorwaarden moeten waar zijn voor een alert
- **30m Move - ret_30m threshold (%)**: Drempelwaarde voor 30-minuut move alerts (standaard: 2.0%)
- **30m Move - ret_5m filter (%)**: Filter voor 30-minuut moves (standaard: 0.5%)
- **5m Move Alert - threshold (%)**: Drempelwaarde voor 5-minuut move alerts (standaard: 1.0%)

#### Cooldowns
Tijd tussen notificaties om spam te voorkomen:

- **1-minuut spike cooldown (seconden)**: Tijd tussen 1m spike alerts (standaard: 600 = 10 minuten)
- **30-minuten move cooldown (seconden)**: Tijd tussen 30m move alerts (standaard: 600 = 10 minuten)
- **5-minuten move cooldown (seconden)**: Tijd tussen 5m move alerts (standaard: 600 = 10 minuten)

#### MQTT Instellingen
Configureer je MQTT broker voor integratie met Home Assistant of andere systemen:

- **MQTT Host (IP)**: IP-adres van je MQTT broker (bijv. `192.168.1.100`)
- **MQTT Poort**: Poort van je MQTT broker (standaard: `1883` voor niet-versleuteld, `8883` voor SSL)
- **MQTT Gebruiker**: Gebruikersnaam voor MQTT authenticatie (optioneel)
- **MQTT Wachtwoord**: Wachtwoord voor MQTT authenticatie (optioneel)

**Let op**: Laat gebruiker en wachtwoord leeg als je MQTT broker geen authenticatie vereist.

#### Trend & Volatiliteit Instellingen
- **Trend Threshold (%)**: Percentage verschil voor trend detectie (standaard: 1.0%)
  - Boven deze waarde = OMHOOG, onder = OMLAAG, anders = ZIJWAARTS
- **Volatiliteit Low Threshold (%)**: Onder deze waarde is markt RUSTIG (standaard: 0.06%)
- **Volatiliteit High Threshold (%)**: Boven deze waarde is markt VOLATIEL (standaard: 0.12%)

#### Anchor Instellingen
- **Anchor Take Profit (%)**: Percentage boven anchor voor winstmelding (standaard: 5.0%)
- **Anchor Max Loss (%)**: Percentage onder anchor voor verliesmelding (standaard: -3.0%)

**Opslaan**: Klik op "Opslaan" om alle instellingen op te slaan. Het device zal automatisch opnieuw verbinden met MQTT als de instellingen zijn gewijzigd.

### Taal Instellingen

Het device ondersteunt twee talen: **Nederlands** en **English**. Alle teksten zijn vertaald, inclusief:

**Display Teksten**:
- WiFi setup schermen ("Verbinden met WiFi", "Stel de WiFi in", etc.)
- Trend indicatoren (OMHOOG/OMLAAG/ZIJWAARTS of UP/DOWN/SIDEWAYS)
- Volatiliteit indicatoren (RUSTIG/GEMIDDELD/VOLATIEL of CALM/MEDIUM/VOLATILE)
- "Wacht Xm" berichten

**Web Interface**:
- Alle labels en veldnamen
- Alle uitlegteksten en help-teksten
- Alle sectie headers
- Alle knoppen en berichten

**Hoe Taal Wijzigen**:
1. **Via Web Interface** (Aanbevolen):
   - Ga naar de web interface
   - Selecteer je gewenste taal uit de dropdown bovenaan
   - Klik op "Opslaan"
   - De taal wordt opgeslagen en blijft behouden na herstart

2. **Via platform_config.h** (Alleen standaard):
   - Bewerk `platform_config.h`
   - Stel `DEFAULT_LANGUAGE` in op `0` (Nederlands) of `1` (English)
   - Dit beïnvloedt alleen de initiële taal bij eerste opstarten
   - Na eerste opstarten heeft de taal uit Preferences voorrang

**Let op**: De taal instelling wordt opgeslagen in Preferences, dus blijft behouden na herstart. De `DEFAULT_LANGUAGE` in `platform_config.h` wordt alleen gebruikt als fallback als er nog geen taal is opgeslagen.

### Instellingen Uitleg

#### Binance Symbol
- **Wat het doet**: Bepaalt welke cryptocurrency wordt gemonitord
- **Standaard**: `BTCEUR` (Bitcoin in Euro)
- **Voorbeelden**: `BTCUSDT`, `ETHUSDT`, `ADAUSDT`, etc.
- **Gebruik**: Voer het symbool in zoals het op Binance wordt gebruikt

#### NTFY Topic
- **Wat het doet**: Het topic waarop notificaties worden verzonden via NTFY.sh
- **Standaard**: `crypto-monitor-alerts`
- **Gebruik**: Kies een unieke naam voor jouw device (bijv. `mijn-crypto-monitor` of `btc-alerts-keuken`)
- **Belangrijk**: Dit topic moet uniek zijn als je meerdere devices gebruikt

#### MQTT Settings
- **MQTT Host**: IP-adres van je MQTT broker (bijv. `192.168.1.100` of `mqtt.example.com`)
- **MQTT Port**: Poort van je MQTT broker (standaard: `1883` voor niet-versleuteld, `8883` voor SSL)
- **MQTT User**: Gebruikersnaam voor MQTT authenticatie
- **MQTT Password**: Wachtwoord voor MQTT authenticatie
- **Gebruik**: Laat leeg als je MQTT niet gebruikt (optioneel)

#### Thresholds (Drempelwaarden)
Deze bepalen wanneer je notificaties ontvangt bij snelle prijsbewegingen:

- **1 Min Up**: Notificatie bij stijgende trend > X% per minuut (standaard: `0.5%`)
  - *Voorbeeld*: Bij 0.5% krijg je een melding als de prijs in 1 minuut met meer dan 0.5% stijgt
  
- **1 Min Down**: Notificatie bij dalende trend < -X% per minuut (standaard: `-0.5%`)
  - *Voorbeeld*: Bij -0.5% krijg je een melding als de prijs in 1 minuut met meer dan 0.5% daalt

- **30 Min Up**: Notificatie bij stijgende trend > X% per 30 minuten (standaard: `2.0%`)
  - *Voorbeeld*: Bij 2.0% krijg je een melding als de prijs in 30 minuten met meer dan 2% stijgt

- **30 Min Down**: Notificatie bij dalende trend < -X% per 30 minuten (standaard: `-2.0%`)
  - *Voorbeeld*: Bij -2.0% krijg je een melding als de prijs in 30 minuten met meer dan 2% daalt

#### Anchor Settings
Instellingen voor de anchor price functionaliteit:

- **Take Profit**: Percentage boven anchor price voor winstmelding (standaard: `5.0%`)
  - *Voorbeeld*: Als je anchor op €50.000 zet en take profit op 5%, krijg je een melding bij €52.500
  
- **Max Loss**: Percentage onder anchor price voor verliesmelding (standaard: `-3.0%`)
  - *Voorbeeld*: Als je anchor op €50.000 zet en max loss op -3%, krijg je een melding bij €48.500

- **Trend Threshold**: Percentage verschil voor trend detectie (standaard: `1.0%`)
  - Bepaalt wanneer een trend als "OMHOOG" of "OMLAAG" wordt beschouwd (vs "ZIJWAARTS")

#### Volatiliteit Thresholds
Bepalen wanneer de markt als rustig, gemiddeld of volatiel wordt beschouwd:

- **Low Threshold**: Onder deze waarde is de markt "RUSTIG" (standaard: `0.06%`)
- **High Threshold**: Boven deze waarde is de markt "VOLATIEL" (standaard: `0.12%`)
- Tussen deze waarden is de markt "GEMIDDELD"

## Display Overzicht

### Wat wordt er getoond?

Het display toont real-time cryptocurrency informatie in een overzichtelijke layout:

#### Bovenste Sectie (Header)
- **Datum en Tijd**: Huidige datum en tijd (rechts uitgelijnd)
- **Versienummer**: Software versie (midden)
- **Chart Title**: Beginletters van je NTFY topic (CYD) of beginletters op regel 2 (TTGO)

#### Grafiek Sectie
- **Live Prijs Grafiek**: 
  - 60 datapunten (1 minuut historie)
  - Blauwe lijn toont prijsverloop
  - Automatische schaal aanpassing
- **Trend Indicator**: Linksboven in de grafiek
  - 🟢 **OMHOOG** (groen): Stijgende trend (>1% over 2 uur)
  - 🔴 **OMLAAG** (rood): Dalende trend (<-1% over 2 uur)
  - ⚪ **ZIJWAARTS** (grijs): Geen duidelijke trend
  - Toont "Wacht Xm" als er nog niet genoeg data is (minimaal 2 uur nodig)
- **Volatiliteit Indicator**: Linksonder in de grafiek
  - 🟢 **RUSTIG** (groen): <0.06% gemiddelde beweging
  - 🟠 **GEMIDDELD** (oranje): 0.06% - 0.12% gemiddelde beweging
  - 🔴 **VOLATIEL** (rood): >0.12% gemiddelde beweging
  - Direct beschikbaar vanaf eerste minuut

#### Prijs Cards (3 blokken)

**1. BTCEUR Card (Hoofdprijs)**
- **Titel**: Cryptocurrency symbool (bijv. BTCEUR)
- **Huidige Prijs**: Real-time prijs (blauw)
- **Anchor Price Info**:
  - **CYD**: Rechts verticaal gecentreerd:
    - Boven (groen): Take profit prijs met percentage (bijv. "+5.00% 52500.00")
    - Midden (oranje): Anchor prijs met percentage verschil (bijv. "+2.50% 51250.00")
    - Onder (rood): Stop loss prijs met percentage (bijv. "-3.00% 48500.00")
  - **TTGO**: Rechts verticaal gecentreerd (zonder percentages):
    - Boven (groen): Take profit prijs (bijv. "52500.00")
    - Midden (oranje): Anchor prijs (bijv. "50000.00")
    - Onder (rood): Stop loss prijs (bijv. "48500.00")
- **Interactie**: 
  - **CYD**: Tik op het blok om anchor price te zetten
  - **TTGO**: Druk op reset button (GPIO 0) om anchor price te zetten

**2. 1 Minuut Card**
- **Titel**: "1m" (TTGO) of "1 min" (CYD)
- **Percentage**: 1-minuut return percentage (prijsverandering t.o.v. 1 minuut geleden)
- **Rechts uitgelijnd**:
  - Boven (groen): Max prijs in laatste minuut
  - Midden (grijs): Verschil tussen max en min
  - Onder (rood): Min prijs in laatste minuut

**3. 30 Minuten Card**
- **Titel**: "30m" (TTGO) of "30 min" (CYD)
- **Percentage**: 30-minuut return percentage (prijsverandering t.o.v. 30 minuten geleden)
- **Rechts uitgelijnd**:
  - Boven (groen): Max prijs in laatste 30 minuten
  - Midden (grijs): Verschil tussen max en min
  - Onder (rood): Min prijs in laatste 30 minuten

#### Footer Sectie
- **CYD**: Toont IP-adres, WiFi signaalsterkte (dBm) en beschikbaar RAM (kB)
  - Voorbeeld: `IP: 192.168.1.50   -45dBm   RAM: 125kB`
- **TTGO**: Toont alleen IP-adres (vanwege beperkte ruimte)
  - Voorbeeld: `192.168.1.50`

## Verschil tussen CYD en TTGO Display

### CYD 2.4" / 2.8" Display (240x320 pixels)

**Layout Kenmerken:**
- **Ruimere layout** met meer details
- **Chart Title**: Grote titel boven de grafiek met beginletters van NTFY topic
- **Datum/Tijd/Versie**: Alle drie op dezelfde regel rechts uitgelijnd (versie op 120px, datum op 180px, tijd op 240px)
- **Grafiek**: 240px breed, 80px hoog
- **Font Sizes**: Groter voor betere leesbaarheid
  - BTCEUR titel: 18px
  - BTCEUR prijs: 16px
  - Anchor labels: 14px
- **Anchor Weergave**: Met percentages (bijv. "+5.00% 52500.00")
- **Footer**: Uitgebreid met IP, WiFi signaalsterkte en RAM gebruik
- **Touchscreen**: Interactie via touch (tik op BTCEUR blok voor anchor)

### TTGO T-Display (135x240 pixels)

**Layout Kenmerken:**
- **Compacte layout** geoptimaliseerd voor klein scherm
- **Geen Chart Title**: Beginletters staan op regel 2 links (in plaats van boven grafiek)
- **Datum/Tijd/Versie**: Compact op 2 regels
  - Regel 1: Datum rechts, Trend indicator links
  - Regel 2: Beginletters links, Versie midden, Tijd rechts, Volatiliteit links
- **Grafiek**: 135px breed, 60px hoog (kleiner maar nog steeds duidelijk)
- **Font Sizes**: Kleiner voor compacte weergave
  - BTCEUR titel: 14px
  - BTCEUR prijs: 12px
  - Anchor labels: 10px
- **Anchor Weergave**: Alleen prijzen zonder percentages (bijv. "52500.00")
- **Footer**: Alleen IP-adres (geen ruimte voor extra info)
- **Fysieke Button**: Reset button (GPIO 0) voor anchor functionaliteit

### Belangrijkste Verschillen Samengevat

| Feature | CYD | TTGO |
|---------|-----|------|
| Scherm grootte | 240x320 | 135x240 |
| Chart title | Boven grafiek | Op regel 2 links |
| Datum/tijd layout | 1 regel, 3 items | 2 regels, compact |
| Font sizes | Groter (14-18px) | Kleiner (10-14px) |
| Anchor weergave | Met percentages | Alleen prijzen |
| Footer | IP + RSSI + RAM | Alleen IP |
| Interactie | Touchscreen | Fysieke button |
| Grafiek grootte | 240x80 | 135x60 |

Beide layouts tonen dezelfde informatie, maar de TTGO versie is geoptimaliseerd voor het kleinere scherm met compactere weergave en kleinere fonts.

## Platform-specifieke Features

### TTGO T-Display
- Compacte layout aangepast voor klein scherm (135x240)
- Fysieke reset button voor anchor price
- Alleen IP-adres in footer

### CYD 2.4" / 2.8"
- Ruimere layout met meer details
- Touchscreen interactie
- Footer met IP, WiFi signaalsterkte en RAM gebruik

## NTFY.sh Setup en Gebruik

### Wat is NTFY.sh?

NTFY.sh is een gratis, open-source push notification service. Het stelt je in staat om notificaties te ontvangen op je telefoon, tablet of computer zonder dat je een eigen server hoeft te draaien.

### NTFY App Installeren

1. **Android/iOS**: Installeer de officiële NTFY app uit de Play Store of App Store
2. **Desktop**: Download de desktop app van [ntfy.sh/apps](https://ntfy.sh/apps)

### NTFY Topic Instellen

1. **Kies een uniek topic naam**:
   - Gebruik alleen kleine letters, cijfers en streepjes
   - Bijvoorbeeld: `mijn-crypto-alerts` of `btc-monitor-keuken`
   - **Belangrijk**: Maak het uniek om conflicten te voorkomen

2. **Configureer in de web interface**:
   - Ga naar de web interface van je device
   - Voer je gekozen topic naam in bij "NTFY Topic"
   - Sla de instellingen op

3. **Abonneer op het topic in de NTFY app**:
   - Open de NTFY app
   - Klik op "Subscribe to topic"
   - Voer je topic naam in (bijv. `mijn-crypto-alerts`)
   - Klik op "Subscribe"

### Notificatie Types

Het device verstuurt de volgende soorten notificaties:

#### 1. Trend Change Notificaties
- **Wanneer**: Bij verandering van trend (OMHOOG ↔ OMLAAG ↔ ZIJWAARTS)
- **Cooldown**: Maximaal 1x per 10 minuten (om spam te voorkomen)
- **Inhoud**: 
  - Oude en nieuwe trend
  - 2-uur return percentage
  - 30-minuut return percentage
  - Huidige volatiliteit status
- **Voorbeeld**: 
  ```
  Titel: "Trend Change: ZIJWAARTS → OMHOOG"
  Bericht: "2h: +1.5% | 30m: +0.8% | Vol: GEMIDDELD"
  ```
- **Kleur**: Groen voor OMHOOG, rood voor OMLAAG, grijs voor ZIJWAARTS

#### 2. 1-Minuut Spike Notificaties
- **Wanneer**: Snelle prijsbeweging in 1 minuut
- **Voorwaarden**:
  - 1-minuut return > threshold (stijging) OF < threshold (daling)
  - 5-minuut return als filter (om valse meldingen te voorkomen)
- **Cooldown**: Maximaal 1x per 10 minuten
- **Limiet**: Maximaal 6 meldingen per uur
- **Voorbeeld**:
  ```
  Titel: "1m Spike: +0.8%"
  Bericht: "Prijs: €52,450.00 (was €52,030.00)"
  ```
- **Kleur**: Groen voor stijging, rood voor daling

#### 3. 30-Minuut Move Notificaties
- **Wanneer**: Significante prijsbeweging over 30 minuten
- **Voorwaarden**:
  - 30-minuut return > threshold (stijging) OF < threshold (daling)
  - 5-minuut return als filter
- **Cooldown**: Maximaal 1x per 10 minuten
- **Limiet**: Maximaal 6 meldingen per uur
- **Voorbeeld**:
  ```
  Titel: "30m Move: +2.5%"
  Bericht: "Prijs: €53,125.00 (was €51,840.00)"
  ```
- **Kleur**: Groen voor stijging, rood voor daling

#### 4. 5-Minuut Move Notificaties
- **Wanneer**: Significante prijsbeweging over 5 minuten
- **Voorwaarden**: 5-minuut return > threshold
- **Cooldown**: Maximaal 1x per 10 minuten
- **Limiet**: Maximaal 6 meldingen per uur
- **Voorbeeld**:
  ```
  Titel: "5m Move: +1.2%"
  Bericht: "Prijs: €52,622.40"
  ```
- **Kleur**: Groen voor stijging, rood voor daling

#### 5. Anchor Price Notificaties
- **Take Profit**: 
  - **Wanneer**: Prijs bereikt take profit percentage boven anchor price
  - **Voorbeeld**: Anchor €50.000, take profit 5% → melding bij €52.500
  - **Inhoud**: Anchor prijs, huidige prijs, percentage winst
  - **Kleur**: Groen met 💰 emoji
  - **Eenmalig**: Wordt maar 1x verzonden per anchor
  
- **Max Loss (Stop Loss)**: 
  - **Wanneer**: Prijs bereikt max loss percentage onder anchor price
  - **Voorbeeld**: Anchor €50.000, max loss -3% → melding bij €48.500
  - **Inhoud**: Anchor prijs, huidige prijs, percentage verlies
  - **Kleur**: Rood met ⚠️ emoji
  - **Eenmalig**: Wordt maar 1x verzonden per anchor

### Notificatie Instellingen Tips

- **Minder notificaties**: Verhoog de threshold waarden (bijv. 1 Min Up van 0.5% naar 1.0%)
- **Meer notificaties**: Verlaag de threshold waarden (bijv. 1 Min Up van 0.5% naar 0.3%)
- **Alleen belangrijke bewegingen**: Gebruik alleen 30-minuut notificaties
- **Snelle alerts**: Gebruik 1-minuut notificaties voor snelle reacties
- **Anchor tracking**: Zet anchor price voor belangrijke prijsniveaus die je wilt monitoren

### NTFY Topic Beveiliging (Optioneel)

Voor extra beveiliging kun je je topic beveiligen met een wachtwoord:

1. Ga naar [ntfy.sh](https://ntfy.sh) en maak een account
2. Maak een beveiligd topic met wachtwoord
3. In de NTFY app: Voeg het wachtwoord toe bij het abonneren

**Let op**: De standaard NTFY.sh service is publiek - iedereen met je topic naam kan je notificaties zien. Gebruik een unieke naam of beveilig je topic.

### Troubleshooting NTFY

- **Geen notificaties ontvangen?**
  - Controleer of je correct geabonneerd bent op het juiste topic
  - Controleer of de topic naam exact overeenkomt (hoofdlettergevoelig)
  - Controleer je internetverbinding op het device
  
- **Notificaties komen te laat?**
  - NTFY.sh gebruikt gratis servers die soms vertraging kunnen hebben
  - Voor betere performance kun je je eigen NTFY server draaien

## MQTT Integratie

### MQTT Topics

Het device publiceert naar de volgende topics (prefix is platform-specifiek: `ttgo_crypto`, `cyd24_crypto`, of `cyd28_crypto`):

#### Data Topics (Read-only)
- `{prefix}/values/price` - Huidige prijs (float, bijv. `52345.67`)
- `{prefix}/values/return_1m` - 1 minuut return percentage (float, bijv. `0.25`)
- `{prefix}/values/return_5m` - 5 minuut return percentage (float, bijv. `0.50`)
- `{prefix}/values/return_30m` - 30 minuut return percentage (float, bijv. `1.25`)
- `{prefix}/values/timestamp` - Unix timestamp in milliseconden

#### Status Topics
- `{prefix}/trend` - Trend state (string: "UP", "DOWN", of "SIDEWAYS")
- `{prefix}/volatility` - Volatiliteit state (string: "LOW", "MEDIUM", of "HIGH")
- `{prefix}/anchor/event` - Anchor events (JSON met event type, prijs en timestamp)

#### Config Topics (Read/Write)
Deze topics kunnen worden gelezen (huidige waarde) en geschreven (om te wijzigen):

- `{prefix}/config/spike1m` - 1m spike threshold (float)
- `{prefix}/config/spike5m` - 5m spike filter (float)
- `{prefix}/config/move30m` - 30m move threshold (float)
- `{prefix}/config/move5m` - 5m move filter (float)
- `{prefix}/config/move5mAlert` - 5m move alert threshold (float)
- `{prefix}/config/cooldown1min` - 1m cooldown in seconden (integer)
- `{prefix}/config/cooldown30min` - 30m cooldown in seconden (integer)
- `{prefix}/config/cooldown5min` - 5m cooldown in seconden (integer)
- `{prefix}/config/binanceSymbol` - Binance symbool (string)
- `{prefix}/config/ntfyTopic` - NTFY topic (string)
- `{prefix}/config/anchorTakeProfit` - Anchor take profit % (float)
- `{prefix}/config/anchorMaxLoss` - Anchor max loss % (float)
- `{prefix}/config/trendThreshold` - Trend threshold % (float)
- `{prefix}/config/volatilityLowThreshold` - Volatiliteit low threshold % (float)
- `{prefix}/config/volatilityHighThreshold` - Volatiliteit high threshold % (float)

**Om een instelling te wijzigen**: Publiceer de nieuwe waarde naar `{prefix}/config/{setting}/set`

**Voorbeeld**: Om de 1m spike threshold te wijzigen naar 0.5%:
```bash
mosquitto_pub -h 192.168.1.100 -t "ttgo_crypto/config/spike1m/set" -m "0.5"
```

#### Control Topics
- `{prefix}/button/reset/set` - Publiceer "PRESSED" om anchor price te zetten (string)

### Home Assistant Integratie

Het device ondersteunt **MQTT Auto Discovery** voor Home Assistant. Dit betekent dat je device automatisch wordt gedetecteerd en toegevoegd aan Home Assistant!

#### Automatische Detectie

1. **Zorg dat MQTT is geconfigureerd** in de web interface
2. **Zorg dat MQTT Broker is geconfigureerd** in Home Assistant
3. **Start het device** - het publiceert automatisch discovery berichten
4. **Ga naar Home Assistant** → Settings → Devices & Services → MQTT
5. **Klik op "Configure"** bij je MQTT integratie
6. Je device zou automatisch moeten verschijnen onder "Discovered MQTT devices"

#### Beschikbare Entities in Home Assistant

Na detectie krijg je de volgende entities:

**Sensors (Read-only)**:
- `sensor.{device_id}_price` - Huidige cryptocurrency prijs
- `sensor.{device_id}_return_1m` - 1 minuut return percentage
- `sensor.{device_id}_return_5m` - 5 minuut return percentage
- `sensor.{device_id}_return_30m` - 30 minuut return percentage
- `sensor.{device_id}_anchor_event` - Anchor events (JSON)

**Numbers (Read/Write)**:
- `number.{device_id}_spike1m` - 1m spike threshold
- `number.{device_id}_spike5m` - 5m spike filter
- `number.{device_id}_move30m` - 30m move threshold
- `number.{device_id}_move5m` - 5m move filter
- `number.{device_id}_move5mAlert` - 5m move alert threshold
- `number.{device_id}_cooldown1min` - 1m cooldown (seconden)
- `number.{device_id}_cooldown30min` - 30m cooldown (seconden)
- `number.{device_id}_cooldown5min` - 5m cooldown (seconden)
- `number.{device_id}_anchorTakeProfit` - Anchor take profit %
- `number.{device_id}_anchorMaxLoss` - Anchor max loss %
- `number.{device_id}_trendThreshold` - Trend threshold %
- `number.{device_id}_volatilityLowThreshold` - Volatiliteit low threshold %
- `number.{device_id}_volatilityHighThreshold` - Volatiliteit high threshold %

**Text (Read/Write)**:
- `text.{device_id}_binanceSymbol` - Binance symbool
- `text.{device_id}_ntfyTopic` - NTFY topic

**Button**:
- `button.{device_id}_reset` - Reset anchor price (klik om anchor te zetten)

#### Home Assistant Automations

Je kunt automations maken op basis van de MQTT data:

**Voorbeeld 1: Notificatie bij hoge prijs**
```yaml
automation:
  - alias: "Crypto Price Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.ttgo_crypto_xxxxx_price
        above: 55000
    action:
      - service: notify.mobile_app
        data:
          message: "Bitcoin prijs is boven €55.000!"
```

**Voorbeeld 2: Notificatie bij snelle daling**
```yaml
automation:
  - alias: "Crypto Crash Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.ttgo_crypto_xxxxx_return_1m
        below: -1.0
    action:
      - service: notify.mobile_app
        data:
          message: "Waarschuwing: Snelle daling gedetecteerd!"
```

**Voorbeeld 3: Dashboard Card**
Voeg een card toe aan je dashboard:
```yaml
type: entities
entities:
  - entity: sensor.ttgo_crypto_xxxxx_price
    name: Bitcoin Prijs
  - entity: sensor.ttgo_crypto_xxxxx_return_30m
    name: 30 Min Return
  - entity: sensor.ttgo_crypto_xxxxx_trend
    name: Trend
```

#### Handmatige MQTT Configuratie (zonder Auto Discovery)

Als Auto Discovery niet werkt, kun je handmatig sensors toevoegen in Home Assistant:

1. Ga naar Configuration → Integrations → MQTT → Configure
2. Klik op "Add Entry"
3. Voeg een sensor toe met:
   - **Topic**: `ttgo_crypto/values/price` (of je prefix)
   - **Name**: `Crypto Price`
   - **State Topic**: `ttgo_crypto/values/price`
   - **Unit of Measurement**: `EUR`

### MQTT Gebruik zonder Home Assistant

MQTT is optioneel en kan ook gebruikt worden met andere systemen zoals:
- **Node-RED**: Voor geavanceerde automations
- **OpenHAB**: Home automation platform
- **Grafana**: Voor data visualisatie
- **Custom scripts**: Python, Node.js, etc.

Als je MQTT niet gebruikt, kun je de MQTT instellingen leeg laten in de web interface.

## Publiceren naar GitHub

### Stap 1: Maak een GitHub Repository

1. Ga naar [GitHub.com](https://github.com) en log in
2. Klik op het **+** icoon rechtsboven → **New repository**
3. Kies een repository naam (bijv. `unified-lvgl9-crypto-monitor`)
4. Kies **Public** of **Private**
5. **NIET** "Initialize with README" aanvinken (we hebben al een README)
6. Klik op **Create repository**

### Stap 2: Initialiseer Git en Push Code

Open een terminal in de project directory en voer de volgende commando's uit:

```bash
# Navigeer naar de project directory
cd /Users/janpieterduhen/MEGA/@HOKUSAI/Arduino_nieuw/UNIFIED-LVGL9-Crypto_Monitor

# Initialiseer git repository (als nog niet gedaan)
git init

# Voeg alle bestanden toe
git add .

# Maak eerste commit
git commit -m "Initial commit: Unified LVGL9 Crypto Monitor"

# Voeg remote repository toe (vervang <jouw-username> en <repository-naam>)
git remote add origin https://github.com/<jouw-username>/<repository-naam>.git

# Push naar GitHub
git branch -M main
git push -u origin main
```

**Let op**: Als je GitHub authenticatie gebruikt, moet je mogelijk een Personal Access Token gebruiken in plaats van je wachtwoord.

### Stap 3: Update README (Optioneel)

Vergeet niet om in de README de volgende regels aan te passen:
- Regel 51: Vervang `<repository-url>` met je daadwerkelijke GitHub URL
- Regel 133: Voeg je licentie toe (MIT licentie is al toegevoegd in `LICENSE` bestand)
- Regel 137: Voeg je naam/informatie toe

### Stap 4: Maak een Release (Optioneel)

Voor belangrijke versies kun je een GitHub Release maken:

1. Ga naar je repository op GitHub
2. Klik op **Releases** → **Create a new release**
3. Kies een tag (bijv. `v3.14`)
4. Voeg release notes toe
5. Klik op **Publish release**

## Licentie

MIT License - Zie `LICENSE` bestand voor details.

## Auteur

[Voeg hier je naam/informatie toe]

## Credits

- **LVGL** - Graphics library voor embedded systems
- **Binance** - Cryptocurrency API
- **Arduino_GFX** - Display drivers voor ESP32
- **WiFiManager** - WiFi configuration library
- **PubSubClient3** - MQTT client library

