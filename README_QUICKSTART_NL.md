# ESP32 Crypto Alert – Quick Start

## Wat heb ik nodig?

- ESP32 met display (CYD, ESP32-S3 SuperMini, ESP32-S3 GEEK, TTGO T-Display, of vergelijkbaar)
- WiFi verbinding
- NTFY.sh account (of publiek topic)
- Bitvavo API (publieke endpoints, geen key vereist)

---

## Stappen

1. **Flash de firmware** naar je ESP32
2. **Zet het apparaat aan**
3. **Verbind met WiFi** (bij eerste opstarten verschijnt WiFi manager)
4. **Open de web-interface** (IP-adres staat op het scherm)
5. **Configureer instellingen:**
   - Bitvavo market (bijv. BTC-EUR, ETH-EUR)
   - NTFY topic (waar alerts worden verstuurd)
   - Anchor prijs (jouw referentieniveau)
   - Alert drempels en cooldowns
   - Nachtstand (optioneel tijdvenster en filters)
6. **Sla instellingen op**
7. **Begin alerts te ontvangen!**

---

## Eerste tips

- **Begin met standaard drempels** – deze zijn gebalanceerd voor de meeste gebruikers
- **Stel je anchor in** op een betekenisvolle prijs (instap, support, resistance)
- **Activeer eerst maar een paar alerts** om het gedrag te begrijpen
- **Observeer een dag** voordat je instellingen aanscherpt of versoepelt
- **Gebruik cooldowns** om alert spam te voorkomen
- **PSRAM**: Uitschakelen op CYD/TTGO; alleen inschakelen op boards met PSRAM

---

## Aanbevolen eerste setup

- Gebruik de standaard drempels en cooldowns
- Zet nachtstand aan als je ’s nachts minder meldingen wilt
- Pas alleen aan na minimaal 1 dag observatie

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

**1d/7d trend labels** = Langetermijncontext  
→ "Grotere trendrichting"

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

