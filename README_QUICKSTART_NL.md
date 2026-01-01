# ESP32 Crypto Alert – Quick Start

## Wat heb ik nodig?

- ESP32 met display (CYD, ESP32-S3 SuperMini, ESP32-S3 GEEK, TTGO T-Display, of vergelijkbaar)
- WiFi verbinding
- NTFY.sh account (of publiek topic)
- Binance API (publieke endpoints, geen key vereist)

---

## Stappen

1. **Flash de firmware** naar je ESP32
2. **Zet het apparaat aan**
3. **Verbind met WiFi** (bij eerste opstarten verschijnt WiFi manager)
4. **Open de web-interface** (IP-adres staat op het scherm)
5. **Configureer instellingen:**
   - Binance symbol (bijv. BTCEUR, ETHUSDT)
   - NTFY topic (waar alerts worden verstuurd)
   - Anchor prijs (jouw referentieniveau)
   - Alert drempels (gevoeligheid)
6. **Sla instellingen op**
7. **Begin alerts te ontvangen!**

---

## Eerste tips

- **Begin met standaard drempels** – deze zijn gebalanceerd voor de meeste gebruikers
- **Stel je anchor in** op een betekenisvolle prijs (instap, support, resistance)
- **Activeer eerst maar een paar alerts** om het gedrag te begrijpen
- **Observeer een dag** voordat je instellingen aanscherpt of versoepelt
- **Gebruik cooldowns** om alert spam te voorkomen

---

## Aanbevolen eerste setup

### Conservatief (weinig alerts)
- Hogere drempels (2-3% voor spikes)
- Langere cooldowns (5-10 minuten)
- Confluence modus aan
- Focus alleen op 2h alerts

### Gebalanceerd (standaard)
- Gebruik standaard drempels
- Gemengde tijdschalen
- Goede signaal/ruis verhouding

### Agressief (veel alerts)
- Lagere drempels (0,5-1% voor spikes)
- Korte cooldowns (1-2 minuten)
- Alle tijdschalen ingeschakeld

---

## Veelgemaakte fouten

- ❌ **Drempels te laag instellen** → alert spam
- ❌ **Cooldowns vergeten** → dubbele notificaties
- ❌ **Alerts behandelen als koop/verkoop commando's** → dit is een context tool, geen trading bot
- ❌ **Anchor prijs niet instellen** → veel alerts werken niet goed
- ❌ **2h context negeren** → belangrijke structurele veranderingen missen

---

## Alerts begrijpen

**1m / 5m alerts** = Snelle, reactieve signalen  
→ "Er gebeurt nu iets"

**30m alerts** = Bevestigd momentum  
→ "Deze move heeft kracht"

**2h alerts** = Structurele veranderingen  
→ "Marktregime verschuift"

**Meerdere alerts samen** = Meestal significant  
→ "Let op, er gebeurt iets groots"

---

## Dit systeem gaat over **context, niet commando's**

Gebruik het om:
- ✅ Geïnformeerd te blijven zonder naar grafieken te staren
- ✅ Op de hoogte te worden gebracht wanneer iets betekenisvols gebeurt
- ✅ Marktstructuur en context te begrijpen

Gebruik het niet om:
- ❌ Automatisch trades uit te voeren
- ❌ Je eigen analyse te vervangen
- ❌ Financiële beslissingen te nemen zonder na te denken

---

## Hulp nodig?

- Bekijk de hoofd `README_NL.md` voor gedetailleerde uitleg
- Review `README.md` voor Engelse documentatie
- Alle instellingen worden uitgelegd in de web-interface tooltips

