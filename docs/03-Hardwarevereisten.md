# Hoofdstuk 3: Hardwarevereisten

## 3.1 Inleiding
Het ESP32-Crypto-Alert project is ontworpen om te draaien op verschillende ESP32-development boards, bij voorkeur met een ingebouwd TFT-display. Dit maakt het apparaat volledig standalone en visueel aantrekkelijk zonder extra componenten.

Het project ondersteunt zowel **kant-en-klare boards met display** als **custom builds** met een losse ESP32 en TFT-scherm. De code bevat specifieke configuraties voor populaire boards (via defines bovenaan de sketch).

**Minimale eisen:**
- ESP32-module (dual-core, WiFi/BLE)
- Minimaal 4 MB Flash (voor OTA en web-interface)
- Stabiele 5V-voeding (via USB of adapter)
- WiFi-verbinding voor Binance-data en notificaties

Een display is sterk aanbevolen, maar niet verplicht (headless gebruik met alleen notificaties is mogelijk).

## 3.2 Aanbevolen Kant-en-Klare Boards

### 3.2.1 ESP32-2432S028R "Cheap Yellow Display" (CYD)
De meest gebruikte en goedkoopste optie (€10-15). 2.8" resistive touchscreen (ILI9341), SD-kaartslot en veel GPIO.

![Cheap Yellow Display voorkant](img/cyd-front.webp)  
*Voorkant van de ESP32-2432S028R "Cheap Yellow Display".*

![Cheap Yellow Display achterkant](img/cyd-back.webp)  
*Achterkant met pin-labels.*

### 3.2.2 LilyGO TTGO T-Display
Compact board met 1.14" IPS-display (ST7789). Ideaal voor kleine bureau-opstelling.

![TTGO T-Display](img/ttgo-t-display.webp)  
*LilyGO TTGO T-Display.*

### 3.2.3 Waveshare ESP32-S3-GEEK
Compact board met ESP32-S3, 1.14" IPS LCD (ST7789, 240×135), 16MB Flash, 2MB PSRAM en TF-kaartslot.

![Waveshare ESP32-S3-GEEK](img/waveshare-s3-geek.jpg)  
*Waveshare ESP32-S3-GEEK – uitstekende prestaties en veel interfaces.*

## 3.3 Custom Builds
Bij gebruik van een generieke ESP32 (bijv. DevKit) kun je een los TFT-display aansluiten via SPI.

![Generieke ESP32 pinout](img/esp32-s3-supermini-pinout.webp)  
*Pinout van een ESP32-S3-Supermini board.*

![TFT SPI bedrading](img/tft-spi-wiring.webp)  
*Typische SPI-bedrading voor een ILI9341 of ST7789 TFT-display.*

## 3.4 Voeding en Accessoires
- **Voeding**: USB-kabel (min. 1A). Voor permanent gebruik een 5V/2A adapter.
- **Behuizing**: Veel 3D-printbare cases beschikbaar op Thingiverse/Printables.
- **Optioneel**: MicroSD-kaart (voor logging), externe antenne voor betere WiFi.

## 3.5 Compatibiliteitsoverzicht

| Board/Model                  | Display Type          | Resolutie     | Touch         | Direct Ondersteund | Opmerkingen                          |
|------------------------------|-----------------------|---------------|---------------|---------------------|--------------------------------------|
| ESP32-2432S028R (CYD)        | 2.8" ILI9341         | 320×240      | Resistive    | Ja                 | Beste prijs/kwaliteit                |
| LilyGO TTGO T-Display        | 1.14" ST7789         | 240×135     | Nee          | Ja                 | Compact                              |
| Waveshare ESP32-S3-GEEK      | 1.14" IPS ST7789     | 240×135      | Nee          | Ja (goed compatibel)| Krachtig, TF-slot                    |
| Generieke ESP32 + TFT        | Variabel             | Variabel     | Optioneel    | Ja (custom config) | Flexibel, meer werk                  |

---

*Ga naar [Hoofdstuk 2: Functies en Mogelijkheden](02-Functies-en-Mogelijkheden.md) | [Hoofdstuk 4: Installatie](04-Installatie.md)*