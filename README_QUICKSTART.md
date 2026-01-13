# ESP32 Crypto Alert – Quick Start

## What do I need?

- ESP32 with display (CYD, ESP32-S3 SuperMini, ESP32-S3 GEEK, TTGO T-Display, or similar)
- WiFi connection
- NTFY.sh account (or public topic)
- Bitvavo API (public endpoints, no key required)

---

## Steps

1. **Flash the firmware** to your ESP32
2. **Power on** the device
3. **Connect to WiFi** (first boot will show WiFi manager)
4. **Open the web interface** (IP address shown on display)
5. **Configure settings:**
   - Bitvavo market (e.g. BTC-EUR, ETH-EUR)
   - NTFY topic (where alerts are sent)
   - Anchor price (your reference level)
   - Alert thresholds (sensitivity)
6. **Save settings**
7. **Start receiving alerts!**

---

## First tips

- **Start with default thresholds** – they're balanced for most use cases
- **Set your anchor** to a meaningful price (entry, support, resistance)
- **Enable only a few alerts** at first to understand behavior
- **Observe for a day** before tightening or loosening settings
- **Use cooldowns** to prevent alert spam

---

## Recommended initial setup

### Conservative (few alerts)
- Higher thresholds (2-3% for spikes)
- Longer cooldowns (5-10 minutes)
- Enable confluence mode
- Focus on 2h alerts only

### Balanced (default)
- Use default thresholds
- Mixed timeframes
- Good signal/noise ratio

### Aggressive (many alerts)
- Lower thresholds (0.5-1% for spikes)
- Short cooldowns (1-2 minutes)
- All timeframes enabled

---

## Common mistakes

- ❌ **Setting thresholds too low** → alert spam
- ❌ **Forgetting cooldowns** → duplicate notifications
- ❌ **Treating alerts as buy/sell commands** → this is a context tool, not a trading bot
- ❌ **Not setting anchor price** → many alerts won't work properly
- ❌ **Ignoring 2h context** → missing important structural changes

---

## Understanding alerts

**1m / 5m alerts** = Fast, reactive signals  
→ "Something is happening right now"

**30m alerts** = Confirmed momentum  
→ "This move has legs"

**2h alerts** = Structural changes  
→ "Market regime is shifting"

**Multiple alerts together** = Usually significant  
→ "Pay attention, something big is happening"

---

## This system is about **context, not commands**

Use it to:
- ✅ Stay informed without staring at charts
- ✅ Get notified when something meaningful happens
- ✅ Understand market structure and context

Do not use it to:
- ❌ Automatically execute trades
- ❌ Replace your own analysis
- ❌ Make financial decisions without thinking

---

## Need help?

- Check the main `README.md` for detailed explanations
- Review `README_NL.md` for Dutch documentation
- All settings are explained in the web interface tooltips
