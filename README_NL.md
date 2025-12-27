# Unified LVGL9 Crypto Monitor

Een unificatie van de Crypto Monitor voor verschillende ESP32 display platforms: TTGO T-Display, CYD 2.4" en CYD 2.8".

## Ondersteunde Platforms

- **TTGO T-Display**: 1.14" 135x240 TFT display (ST7789)
- **CYD 2.4"**: 2.4" 240x320 TFT display met touchscreen (XPT2046)
- **CYD 2.8"**: 2.8" 240x320 TFT display met touchscreen (XPT2046)
- **ESP32-S3 Super Mini**: ESP32-S3 Super Mini HW-747 v0.0.2i + 1.54" 240x240 TFT display (ST7789)

## Features

- Real-time Bitcoin (BTCEUR) prijs monitoring via Binance API
- Live grafiek met 60 datapunten (~1.5 minuten historie bij 1500ms API interval)
- 1 minuut return berekening gebruikt 40 datapunten (exact 1 minuut bij 1500ms API interval)
- Trend detectie (2-uur trend analyse)
- Volatiliteit monitoring (laag/gemiddeld/hoog)
- Anchor price tracking met take profit en stop loss alerts
- 1 minuut en 30 minuut gemiddelde prijs tracking
- Min/Max/Diff weergave voor 1m en 30m periodes
- MQTT integratie voor home automation
- NTFY.sh notificaties voor alerts
- Web interface voor configuratie
- WiFi Manager voor eenvoudige WiFi setup
- **Trend-Adaptive Anchors**: Dynamische anchor thresholds op basis van trend
- **Smart Confluence Mode**: Gecombineerde alerts wanneer meerdere timeframes samenvallen
- **Auto-Volatility Mode**: Automatische threshold aanpassing op basis van volatiliteit
- **Warm-Start met Binance historische data**: Vul buffers bij opstarten met historische data voor direct bruikbare trend- en volatiliteitsindicaties
- **2-uur Alert Systeem**: Instelbare notificaties voor 2-uur timeframe (breakout, breakdown, compressie, mean reversion, anchor context)

## TL;DR – Alerts lezen in één oogopslag

**Confluence Alert** = hoogste prioriteit
→ meerdere timeframes wijzen dezelfde kant op

**1m Spike** = snelle impuls
→ vooral een opletmoment, geen direct handelssignaal

**5m Move** = momentum-opbouw
→ belangrijker dan 1m, geeft richting

**30m Move** = structurele verplaatsing
→ zeldzaam, maar zwaarwegend

**TrendState**
- **UP** → UP-signalen zijn belangrijker
- **DOWN** → DOWN-signalen zijn belangrijker
- **SIDEWAYS** → alleen confluence echt interessant

**Volatiliteit**
- **LOW** → kleine bewegingen tellen
- **HIGH** → alleen grote bewegingen tellen

**Anchor alerts** (Max Loss / Take Profit)
→ beslissingsmomenten, grenzen passen zich aan de trend aan

**Geen alerts** = geen duidelijke kans
→ systeem filtert ruis bewust weg

**Minder meldingen = hogere kwaliteit.** Het systeem helpt bepalen wanneer kijken zinvol is, niet wat je moet doen.

## Alert Systeem Decision Tree

Het alertsysteem gebruikt een gestructureerde decision tree om alerts te genereren. Hieronder staat de volledige logica:

### 1. Nieuwe candle / tick binnen

1.1. Update prijs, 1m/5m/30m returns en historie

### 2. Bepaal volatiliteit (Auto-Volatility Mode)

2.1. Als `autoVolatilityEnabled == true`:
    - Bereken standaarddeviatie (σ) van laatste N 1m-returns (sliding window)
    - Bereken `volatilityFactor = clamp(σ / σ_baseline, minMultiplier, maxMultiplier)`
    - Schaal alle thresholds:
      - `1mSpikeThreshold = basis_1m * volatilityFactor`
      - `5mMoveThreshold = basis_5m * sqrt(volatilityFactor)` (sqrt voor langere timeframes)
      - `30mMoveThreshold = basis_30m * sqrt(volatilityFactor)` (sqrt voor langere timeframes)
2.2. Als `autoVolatilityEnabled == false`:
    - Gebruik basis thresholds zonder aanpassing

### 3. Bepaal trend

3.1. Bereken 2-uur return (`ret_2h`) en 30-minuut return (`ret_30m`)
3.2. Als `ret_2h >= trendThreshold` EN `ret_30m >= 0.0` → Trend = **UP**
3.3. Als `ret_2h <= -trendThreshold` EN `ret_30m <= 0.0` → Trend = **DOWN**
3.4. Anders → Trend = **SIDEWAYS**

### 4. Pas Anchor-risico aan (Trend-Adaptive Anchors)

4.1. Als `trendAdaptiveAnchorsEnabled == true`:
    - Start vanuit basiswaarden: `anchorMaxLossBase`, `anchorTakeProfitBase`
    - Kies multipliers per Trend:
      - bij **UP**: `lossMul_up`, `tpMul_up`
      - bij **DOWN**: `lossMul_down`, `tpMul_down`
      - bij **SIDEWAYS**: 1.0 (geen aanpassing)
    - Bereken:
      - `anchorMaxLoss = anchorMaxLossBase * lossMul_trend`
      - `anchorTakeProfit = anchorTakeProfitBase * tpMul_trend`
    - Clamp waarden voor veiligheid (max loss: -6% tot -1%, take profit: 2% tot 10%)
4.2. Als `trendAdaptiveAnchorsEnabled == false`:
    - Gebruik basiswaarden zonder aanpassing

### 5. Detecteer spikes & moves op elk timeframe

5.1. **1m Spike**: 
    - `|ret_1m| >= effective1mSpikeThreshold` **EN**
    - `|ret_5m| >= spike5mThreshold` (filter) **EN**
    - Beide in dezelfde richting → kandidaat 1m spike

5.2. **5m Move**: 
    - `|ret_5m| >= effective5mMoveThreshold` → kandidaat 5m move

5.3. **30m Move**: 
    - `|ret_30m| >= effective30mMoveThreshold` **EN**
    - `|ret_5m| >= move5mThreshold` (filter) **EN**
    - Beide in dezelfde richting → kandidaat 30m move

### 6. Smart Confluence logica

6.1. Als `smartConfluenceEnabled == true`:
    - Verzamel recente 1m en 5m events (richting UP of DOWN)
    - Check of:
      - 1m event en 5m event binnen tijdvenster liggen (±5 minuten)
      - Beide events in dezelfde richting zijn
      - 30m trend de richting ondersteunt (UP trend ondersteunt UP events, DOWN trend ondersteunt DOWN events, SIDEWAYS ondersteunt beide)
      - Cooldown voor confluence alerts is verlopen
    - Als alle voorwaarden voldaan:
      - Stuur één "Confluence Alert" met samenvatting (1m magnitude, 5m magnitude, 30m trend)
      - Markeer 1m en 5m events als "gebruikt in confluence"
      - Onderdruk individuele alerts voor deze events
    - Als voorwaarden niet voldaan:
      - Verzend alleen losse alerts die voldoen aan cooldown/debouncing eisen
      - Onderdruk alerts die al gebruikt zijn in confluence

6.2. Als `smartConfluenceEnabled == false`:
    - Verstuur alle alerts die thresholds overschrijden (onder respecteren van cooldowns)
    - Geen confluence checks

### 7. Anchor Max Loss / Take Profit bewaken

7.1. Bereken PnL t.o.v. Anchor:
    - `deltaPct = (prijs - anchorPrice) / anchorPrice * 100%`

7.2. Als `deltaPct <= effectiveAnchorMaxLoss`:
    - Stuur "Max Loss" alert
    - Vermeld trend en effective threshold (als trend-adaptive aan staat)

7.3. Als `deltaPct >= effectiveAnchorTakeProfit`:
    - Stuur "Take Profit" alert
    - Vermeld trend en effective threshold (als trend-adaptive aan staat)

### 8. Cooldown & housekeeping

8.1. Per signaaltype (1m, 5m, 30m, Confluence, Anchor):
    - Check of cooldown verlopen is
    - Werk laatste verzendtijd bij
    - Check maximum aantal alerts per uur

8.2. Log alle events (signalen, trend, volatiliteit, anchors) voor analyse

### Flow Chart

```
[ START: nieuwe candle / tick ]
            |
            v
[ Update prijs & historie ]
- update price
- bereken ret_1m, ret_5m, ret_30m, ret_2h
- update sliding windows
            |
            v
[ Auto-Volatility Mode ? ]
      |                         |
     ja                        nee
      |                         |
      v                         v
[ Bereken σ (1m returns) ]   [ Gebruik basis thresholds ]
[ volFactor = clamp(σ/σ₀) ]
[ eff thresholds:
  - 1m  = base1m * volFactor
  - 5m  = base5m * sqrt(volFactor)
  - 30m = base30m * sqrt(volFactor) ]
            |
            v
[ Bepaal TrendState ]
- if ret_2h ≥ trendThreshold AND ret_30m ≥ 0 → TREND_UP
- if ret_2h ≤ -trendThreshold AND ret_30m ≤ 0 → TREND_DOWN
- else → TREND_SIDEWAYS
            |
            v
[ Trend-Adaptive Anchors ? ]
      |                         |
     ja                        nee
      |                         |
      v                         v
[ Bereken effective anchors ] [ Gebruik basis anchors ]
- start from base MaxLoss / TakeProfit
- apply multipliers per TrendState
- clamp (veiligheidsgrenzen)
            |
            v
[ Detecteer events per timeframe ]
            |
            +--> [ 1m Spike check ]
            |     - |ret_1m| ≥ effSpike1m
            |     - |ret_5m| ≥ spike5mThreshold
            |     - zelfde richting
            |     → kandidaat 1m event
            |
            +--> [ 5m Move check ]
            |     - |ret_5m| ≥ effMove5m
            |     → kandidaat 5m event
            |
            +--> [ 30m Move check ]
                  - |ret_30m| ≥ effMove30m
                  - |ret_5m| ≥ move5mThreshold
                  - zelfde richting
                  → kandidaat 30m event
            |
            v
[ Smart Confluence Mode ? ]
      |                         |
     ja                        nee
      |                         |
      v                         v
[ Verzamel recente events ]  [ Verstuur losse alerts ]
- 1m + 5m events
- richting (UP/DOWN)
- timestamps
            |
            v
[ Confluence voorwaarden check ]
- 1m & 5m binnen tijdvenster (±5 min)
- zelfde richting
- TrendState ondersteunt richting
  * UP → alleen UP
  * DOWN → alleen DOWN
  * SIDEWAYS → beide
- confluence cooldown verlopen?
            |
      +-----+-----+
      |           |
     ja          nee
      |           |
      v           v
[ Confluence Alert ]   [ Losse alerts ]
- stuur 1 alert     - alleen als cooldown ok
- markeer events    - events niet dubbel sturen
  als "gebruikt"
            |
            v
[ Anchor bewaking ]
- deltaPct = (price - anchorPrice) / anchorPrice * 100
- if deltaPct ≤ effAnchorMaxLoss → Max Loss alert
- if deltaPct ≥ effAnchorTakeProfit → Take Profit alert
            |
            v
[ Cooldowns & housekeeping ]
- update lastSent times
- max alerts / uur check
- log: trend, volatility, events, anchors
            |
            v
[ EINDE → wachten op volgende candle ]
```

### Belangrijke Notities

- **5m-Confirmatiefilter**: Let op: 1m spike en 30m move alerts zijn 'gated' door een 5m-confirmatiefilter (`spike5mThreshold` / `move5mThreshold`). Dit betekent dat deze alerts alleen worden verzonden wanneer zowel de 1m/30m threshold als de 5m filter threshold worden overschreden, en beide in dezelfde richting zijn.
- **Effective Thresholds**: Wanneer Auto-Volatility Mode aan staat, worden alle threshold vergelijkingen gedaan met de effective thresholds (aangepast op basis van volatiliteit)
- **Feature Uitschakeling**: Wanneer alle drie features uit staan, is het gedrag identiek aan de basisversie (geen aanpassingen)
- **Thread Safety**: Alle berekeningen zijn thread-safe met mutex bescherming waar nodig
- **Edge Cases**: Validatie voorkomt deling door nul, negatieve thresholds, en onvoldoende data

## Hardware Vereisten

### TTGO T-Display
- ESP32 met TTGO T-Display module
- 1.14" 135x240 TFT display (ST7789)
- Fysieke reset button (GPIO 0)

### CYD 2.4" / 2.8"
- ESP32 met CYD display module
- 2.4" of 2.8" 240x320 TFT display
- Touchscreen (XPT2046)

### ESP32-S3 Super Mini
- ESP32-S3 Super Mini HW-747 v0.0.2i
- 1.54" 240x240 TFT display (ST7789)
- Fysieke reset button (GPIO 0)

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
// #define PLATFORM_ESP32S3_SUPERMINI
```

**Voor ESP32-S3 Super Mini:**
```cpp
//#define PLATFORM_CYD28
// #define PLATFORM_TTGO
// #define PLATFORM_CYD24
#define PLATFORM_ESP32S3_SUPERMINI
```

**⚠️ Let op**: Er mag maar ÉÉN platform actief zijn! Zorg dat de andere drie regels met `//` zijn uitgecommentarieerd.

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

## Schermafbeeldingen

De volgende schermafbeeldingen tonen de verschillende schermen van de applicatie:

### Startscherm
![Startscherm](images/startup.png)

### WiFi Instel Scherm
![WiFi Instel Scherm](images/wifi_config.png)

### WiFi Verbonden Scherm
![WiFi Verbonden Scherm](images/wifi_connected.png)

### Hoofdscherm
![Hoofdscherm](images/main_screen.png)

**Let op**: Om schermafbeeldingen toe te voegen aan je repository:
1. Maak een `images` map aan in de root van het project
2. Plaats je schermafbeeldingen in deze map met de namen zoals hierboven getoond
3. Ondersteunde formaten: PNG, JPG, of GIF
4. Aanbevolen grootte: 800-1200px breedte voor beste weergave op GitHub

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
- **Anchor Waarde (EUR)**: Stel de anchor prijs waarde in
  - Voer een aangepaste waarde in (standaard: huidige prijs is vooringevuld)
  - Laat leeg om de huidige prijs te gebruiken
  - Klik op "Stel Anchor In" knop om direct toe te passen (los van het opslaan van andere instellingen)
- **Anchor Take Profit (%)**: Percentage boven anchor voor winstmelding (standaard: 5.0%)
- **Anchor Max Loss (%)**: Percentage onder anchor voor verliesmelding (standaard: -3.0%)

**Let op**: De anchor waarde kan worden ingesteld via:
- **Web Interface**: Voer waarde in en klik op "Stel Anchor In" knop
- **Fysieke Knop** (alleen TTGO): Druk op reset button (GPIO 0)
- **Touchscreen** (alleen CYD): Tik op de BTCEUR prijs card
- **MQTT**: Stuur waarde naar `{prefix}/config/anchorValue/set` topic (zie MQTT sectie)

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
- **Standaard**: Automatisch gegenereerd als `[ESP32-ID]-alert` (bijv. `9MK28H3Q-alert`)
  - De ESP32-ID is uniek per device (8 karakters met Crockford Base32 encoding)
  - Gebruikt veilige karakterset zonder verwarrende tekens (geen 0/O, 1/I/L, U)
  - Karakterset: `0123456789ABCDEFGHJKMNPQRSTVWXYZ`
  - De ESP32-ID wordt getoond op het device scherm voor eenvoudige referentie
- **Gebruik**: Het standaard topic is al uniek per device, maar je kunt het indien nodig wijzigen in de web interface
- **Belangrijk**: 
  - Elk device krijgt automatisch een uniek topic, waardoor conflicten tussen meerdere devices worden voorkomen
  - **Dit is de topic waarop je je moet abonneren in de NTFY app om notificaties te ontvangen op je mobiel**

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
  - **CYD**: Formaat `dd-mm-yyyy` (bijv. "26-01-2025")
  - **TTGO**: Formaat `dd-mm-yy` (bijv. "26-01-25") - compact formaat voor lagere resolutie
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

**4. 2 Uur Card (alleen CYD 2.4" en 2.8")**
- **Titel**: "2h"
- **Percentage**: 2-uur return percentage (prijsverandering t.o.v. 2 uur geleden)
- **Gemiddelde prijs**: Linksonder in de box (gemiddelde van laatste 2 uur)
- **Rechts uitgelijnd**:
  - Boven (groen): Max prijs in laatste 2 uur
  - Midden (grijs): Verschil tussen max en min
  - Onder (rood): Min prijs in laatste 2 uur

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
- Touchscreen interactie via dedicated "Klik Vast" knop
- Twee-regel footer: WiFi signaal/RAM (regel 1), IP/versie (regel 2)

### ESP32-S3 Super Mini
- Vierkant 240x240 display layout (240px verticaal, zelfde als TTGO)
- Fysieke reset button voor anchor price (GPIO 0)
- Compacte layout vergelijkbaar met TTGO (geoptimaliseerd voor 240px verticale resolutie)
- Datum formaat: dd-mm-yy (compact formaat zoals TTGO)

## NTFY.sh Setup en Gebruik

### Wat is NTFY.sh?

NTFY.sh is een gratis, open-source push notification service. Het stelt je in staat om notificaties te ontvangen op je telefoon, tablet of computer zonder dat je een eigen server hoeft te draaien.

### NTFY App Installeren

1. **Android/iOS**: Installeer de officiële NTFY app uit de Play Store of App Store
2. **Desktop**: Download de desktop app van [ntfy.sh/apps](https://ntfy.sh/apps)

### NTFY Topic Instellen

**Automatische Unieke Topic Generatie**:
- Standaard genereert het device automatisch een uniek NTFY topic met je ESP32's unieke ID
- Format: `[ESP32-ID]-alert` (bijv. `9MK28H3Q-alert`)
- De ESP32-ID wordt afgeleid van het MAC adres van het device met Crockford Base32 encoding (8 karakters)
- Gebruikt veilige karakterset: `0123456789ABCDEFGHJKMNPQRSTVWXYZ` (geen verwarrende 0/O, 1/I/L, U)
- Biedt 2^40 = 1,1 biljoen mogelijke combinaties, waardoor uniekheid gegarandeerd is
- De ESP32-ID wordt getoond op het device scherm (in het chart title gebied voor CYD, of op regel 2 voor TTGO)

**Handmatige Configuratie**:
1. **Via Web Interface** (Aanbevolen):
   - Ga naar de web interface van je device
   - Het standaard topic is al ingesteld met je unieke ESP32 ID
   - Je kunt het indien nodig wijzigen bij "NTFY Topic"
   - **Belangrijk**: Dit is de NTFY topic waarop je je moet abonneren in de NTFY app om notificaties te ontvangen op je mobiel
   - Sla de instellingen op

2. **Abonneer op het topic in de NTFY app**:
   - Open de NTFY app
   - Klik op "Subscribe to topic"
   - Voer je topic naam in (getoond op het device display of in web interface)
   - Voorbeeld: Als je ESP32-ID `9MK28H3Q` is, abonneer je op `9MK28H3Q-alert`
   - Klik op "Subscribe"

**Let op**: De ESP32-ID wordt getoond op het device scherm, waardoor het eenvoudig is om te zien op welk topic je je moet abonneren in de NTFY app.

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

**Let op**: De standaard NTFY.sh service is publiek - iedereen met je topic naam kan je notificaties zien.
- **Goed nieuws**: Elk device krijgt automatisch een uniek topic op basis van zijn ESP32 ID (bijv. `a1b2c3-alert`), waardoor conflicten zeer onwaarschijnlijk zijn
- Voor extra beveiliging kun je je topic nog steeds beveiligen met een wachtwoord (zie hierboven)

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

Het device publiceert naar de volgende topics (prefix is platform-specifiek: `ttgo_crypto`, `cyd24_crypto`, `cyd28_crypto`, of `esp32s3_crypto`):

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
- `{prefix}/config/anchorValue` - Anchor prijs waarde in EUR (float)
- `{prefix}/config/trendThreshold` - Trend threshold % (float)
- `{prefix}/config/volatilityLowThreshold` - Volatiliteit low threshold % (float)
- `{prefix}/config/volatilityHighThreshold` - Volatiliteit high threshold % (float)

**Om een instelling te wijzigen**: Publiceer de nieuwe waarde naar `{prefix}/config/{setting}/set`

**Voorbeeld**: Om de 1m spike threshold te wijzigen naar 0.5%:
```bash
mosquitto_pub -h 192.168.1.100 -t "ttgo_crypto/config/spike1m/set" -m "0.5"
```

**Voorbeeld**: Om anchor waarde in te stellen op 78650.00 EUR:
```bash
mosquitto_pub -h 192.168.1.100 -t "ttgo_crypto/config/anchorValue/set" -m "78650.00"
```

**Voorbeeld**: Om anchor in te stellen op huidige prijs:
```bash
mosquitto_pub -h 192.168.1.100 -t "ttgo_crypto/config/anchorValue/set" -m "current"
```

#### Control Topics
- `{prefix}/button/reset/set` - Publiceer "PRESS" om anchor price in te stellen op huidige prijs (string)

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
- `number.{device_id}_anchorValue` - Anchor prijs waarde in EUR
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

## Versie Geschiedenis

### Versie 4.03
- **2-uur Alert Thresholds**: Nieuwe instelbare thresholds voor 2-uur alerts via web-interface en MQTT
  - Breakout/Breakdown: margin, reset margin en cooldown instelbaar
  - Mean Reversion: min distance, touch band en cooldown instelbaar
  - Range Compression: threshold, reset en cooldown instelbaar
  - Anchor Context: margin en cooldown instelbaar
- **2-uur Alert Notificaties**: Vijf nieuwe notificatietypes voor 2-uur timeframe
  - 2h Breakout Up: Prijs breekt boven 2h high met configurable margin
  - 2h Breakdown Down: Prijs breekt onder 2h low met configurable margin
  - Range Compression: 2h range wordt zeer klein (< threshold%)
  - Mean Reversion Touch: Prijs keert terug naar 2h gemiddelde na significante afwijking
  - Anchor Context: Anchor prijs ligt buiten 2h range
- **Debug Logging**: Optionele compile-time debug logging voor 2h alerts (DEBUG_2H_ALERTS flag)
- **Memory Optimalisaties**: Verschillende buffers verkleind om DRAM overflow te voorkomen
- **Code Optimalisaties**: Alert2HState struct geoptimaliseerd met bitfields (24 bytes i.p.v. 32 bytes)

### Versie 4.02
- **2-uur Box**: Nieuwe prijsbox voor 2-uur timeframe (alleen CYD 2.4" en 2.8")
  - Toont min, max, diff en gemiddelde prijs over laatste 2 uur
  - Percentage return berekening op basis van gemiddelde minuut-waarden
  - Kleurcodering op basis van prijsbeweging
  - UI layout geoptimaliseerd voor 320px schermbreedte

### Versie 4.01
- **2-uur Box voor CYD Platforms**: Toegevoegd 2-uur (2h) prijs box voor CYD 2.4" en CYD 2.8" platforms
  - **Alleen voor CYD**: 2h box wordt alleen getoond op CYD 2.4" en CYD 2.8" (320px schermbreedte)
  - **Vier Boxen**: BTCEUR, 1m, 30m en 2h boxen worden nu getoond op CYD platforms
  - **2h Box Features**:
    - Percentage return over laatste 2 uur in de title
    - Gemiddelde prijs van laatste 2 uur linksonder
    - Min/Max/Diff waarden rechts in de box (zoals bij 1m en 30m)
  - **UI Optimalisaties**:
    - Grafiek hoogte verkleind van 80px naar 72px (8px kleiner)
    - Spacing tussen grafiek en BTCEUR box aangepast naar 3px (consistent met andere boxen)
    - Font groottes voor CYD platforms aangepast naar ESP32-S3 waarden voor betere ruimtebenutting
    - Alle vier boxen en grafiek passen nu perfect binnen 320px schermbreedte
  - **Data Management**:
    - `averagePrices[3]` wordt berekend op basis van beschikbare minuten (max 120)
    - `prices[3]` bevat 2-uur return percentage (ret_2h)
    - Min/Max/Diff berekening via `findMinMaxInLast2Hours()` functie
- **Code Verbeteringen**:
  - Debug Serial.printf statements verwijderd
  - Betere error handling voor 2h data berekeningen
  - Geoptimaliseerde memory usage voor CYD platforms

### Versie 4.00
- **Huidige Versie**: Laatste stabiele release

### Versie 3.62
- **Anchor Waarde Instellen via Web Interface en MQTT**:
  - **Web Interface**: Toegevoegd "Anchor Waarde (EUR)" invoerveld met "Stel Anchor In" knop in "Anchor Instellingen" sectie
    - Standaard waarde is vooringevuld met huidige prijs
    - Kan aangepaste waarde invoeren of leeg laten om huidige prijs te gebruiken
    - Knop past anchor direct toe (los van het opslaan van andere instellingen)
  - **MQTT Ondersteuning**: Toegevoegd `{prefix}/config/anchorValue/set` topic voor het instellen van anchor waarde
    - Accepteert numerieke waarde (bijv. "78650.00") of "current" om huidige prijs te gebruiken
    - Home Assistant auto-discovery: `number.{device_id}_anchorValue` entity
  - **Asynchrone Verwerking**: Alle anchor instellingen (web, MQTT, fysieke knop) gebruiken queue voor thread-safe verwerking
  - **Verbeterde Thread Safety**: Gecentraliseerde `queueAnchorSetting()` helper functie voor alle input methoden
  - **Betere Error Handling**: Verbeterde validatie en error recovery
- **Web Interface Verbeteringen**:
  - Secties herorganiseerd: "Algemene instellingen" (Taal, NTFY Topic, Binance Symbool) gevolgd door "Anchor Instellingen"
  - Anchor instellingen verplaatst direct onder Algemene instellingen voor betere organisatie
  - Volgorde gewisseld: "Trend & Volatiliteit Instellingen" komt nu voor "MQTT Instellingen"
- **Display Verbeteringen**:
  - **CYD**: Datum formaat gewijzigd naar `dd-mm-yyyy` (bijv. "26-01-2025") en positie 2 pixels naar links aangepast
  - **TTGO**: Datum formaat blijft `dd-mm-yy` (bijv. "26-01-25") voor lagere resolutie compatibiliteit

### Versie 3.61
- **Web Interface Anchor Reset**: Toegevoegd anchor waarde reset functionaliteit via web interface
  - Invoerveld met huidige prijs als standaard
  - Aparte knop voor directe anchor instelling
  - Asynchrone verwerking om crashes te voorkomen

### Versie 3.50
- **Code Kwaliteit & Betrouwbaarheid Verbeteringen (Sprint 1)**:
  - **Input Validatie**: Toegevoegd `safeAtof()` helper functie met NaN/Inf validatie voor alle float conversies (20 locaties geüpdatet)
  - **Range Checks**: Toegevoegd range validatie voor alle numerieke MQTT en web inputs (spike/move thresholds, cooldowns)
  - **Memory Optimalisatie**: Refactored `httpGET()` en `parsePrice()` om char arrays te gebruiken i.p.v. String objecten, vermindert geheugenfragmentatie
  - **Code Vereenvoudiging**: Refactored MQTT callback van geneste if-else chain naar lookup table structuur (~140 regels → ~80 regels, veel leesbaarder)
  - **HTTP Retry Logic**: Toegevoegd automatisch retry mechanisme (max 2 retries) voor tijdelijke HTTP failures (timeouts, connectie problemen)
  - Verbeterde error handling en logging door de hele codebase
  - Betere betrouwbaarheid en robuustheid voor network operaties

### Versie 3.49
- **Huidige Versie**: Laatste stabiele release
- **1m en 5m Return Berekeningen Opgelost**: Probleem opgelost waarbij 1m en 5m returns op 0.00% bleven
  - Berekeningen aangepast voor 1500ms API update interval
  - 1m return gebruikt nu correct 40 waarden (in plaats van 60) voor 1 minuut periode
  - 5m return gebruikt nu correct 200 waarden (in plaats van 300) voor 5 minuten periode
  - Toegevoegd `VALUES_FOR_1MIN_RETURN` en `VALUES_FOR_5MIN_RETURN` constanten gebaseerd op `UPDATE_API_INTERVAL`

### Versie 3.24
- **TTGO Partition Scheme Fix**: Flash size detectie probleem opgelost voor TTGO T-Display
  - TTGO gebruikt nu `huge_app` partition scheme met expliciete `FlashSize=4M` instelling
  - Oplost "Detected size(4096k) smaller than the size in the binary image header(16384k)" fout
  - Upload script configureert nu correct partition scheme per platform

### Versie 3.23
- **SPI Frequentie Configuratie**: Expliciet SPI frequenties ingesteld in platform-specifieke header bestanden
  - TTGO T-Display: 27 MHz (PINS_TTGO_T_Display.h)
  - CYD 2.8": 55 MHz (PINS_CYD-ESP32-2432S028-2USB.h)
  - CYD 2.4": 40 MHz (PINS_CYD-ESP32-2432S024.h)

### Versie 3.22
- **CYD Footer Redesign**: Twee-regel footer layout
  - Regel 1: WiFi signaalsterkte (dBm) links, RAM (kB) rechts
  - Regel 2: IP-adres links, versienummer rechts
- **Anchor Knop**: Blauwe "Klik Vast" knop onder 30min box (80px breed, 0.66x van origineel)
- **BTCEUR Box**: Touch-functionaliteit verwijderd (nu via dedicated knop)
- **Prestatie Verbeteringen voor CYD**:
  - Verhoogde UI task mutex timeout (50ms → 100ms) voor betere grafiek updates
  - Verhoogde LVGL handler frequentie (5ms → 3ms) voor vloeiendere rendering
  - Verlaagde API task mutex timeout (300ms → 200ms) voor snellere UI updates
  - Vermindert grafiek haperingen op CYD devices

### Versie 3.21
- Touchscreen responsiviteit verbeteringen (5ms polling, PRESSED event support)
- Touchscreen notificatie formaat gelijkgetrokken met fysieke knop
- LVGL deprecated define fix (LV_FS_DEFAULT_DRIVER_LETTER)

## Licentie

MIT License - Zie `LICENSE` bestand voor details.

## Auteur

Jan Pieter Duhen

## Credits

- **LVGL** - Graphics library voor embedded systems
- **Binance** - Cryptocurrency API
- **Arduino_GFX** - Display drivers voor ESP32
- **WiFiManager** - WiFi configuration library
- **PubSubClient3** - MQTT client library

