// platform_config.h
// Platform-specifieke configuratie
// Selecteer je platform door een van de onderstaande defines te activeren:

// Actief ondersteunde boards
//#define PLATFORM_ESP32S3_SUPERMINI
//#define PLATFORM_ESP32S3_GEEK
#define PLATFORM_ESP32S3_LCDWIKI_28
//#define PLATFORM_ESP32S3_JC3248W535  // JC3248W535CIY 3.5" QSPI (AXS15231B), 320x480
//#define PLATFORM_ESP32S3_AMOLED_206

// --- Version Configuration ---
// Versie wordt hier gedefinieerd zodat het beschikbaar is voor alle modules
#ifndef VERSION_STRING
#define VERSION_MAJOR 5
#define VERSION_MINOR 115
#define VERSION_STRING "5.115"
#endif

// TF min/max bronstatus op kaarttitels (LIVE/WARM/MIX) — JC3248, GEEK, LCDwiki 2.8
#if defined(PLATFORM_ESP32S3_JC3248W535) || defined(PLATFORM_ESP32S3_GEEK) || defined(PLATFORM_ESP32S3_LCDWIKI_28)
#define UI_HAS_TF_MINMAX_STATUS_UI 1
#else
#define UI_HAS_TF_MINMAX_STATUS_UI 0
#endif

// --- Debug Configuration ---
// Zet op 1 om alleen knop-acties te loggen, 0 voor alle logging
#ifndef DEBUG_BUTTON_ONLY
#define DEBUG_BUTTON_ONLY 0
#endif

// --- NTFY runtime-diagnostiek (Fase 1 tracker) ---
// Master-vlag: op 1 = o.a. NTFY “Reboot / setup compleet” na boot + optionele WS-live health ping (zie .ino).
// Periodic test blijft uit tenzij je CRYPTO_ALERT_NTFY_PERIODIC_TEST op 1 zet in ESP32-Crypto-Alert.ino.
// Sub-vlaggen staan in ESP32-Crypto-Alert.ino (CRYPTO_ALERT_NTFY_STARTUP_TEST / PERIODIC_TEST).
#ifndef CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME
#define CRYPTO_ALERT_NTFY_DIAGNOSTICS_RUNTIME 1
#endif

// --- WebSocket Configuration ---
#ifndef WS_ENABLED
#define WS_ENABLED 1  // 0 = uit, 1 = aan (standaard aan)
#endif

// --- OTA (Over-The-Air) updates via web UI ---
// Vereist een partitieschema met OTA-ondersteuning (twee app-partities, bijv. "Minimal SPIFFS (1.9MB APP with OTA)").
// Standaard uitgeschakeld; zet OTA_ENABLED op 1 alleen als je een geschikt schema gebruikt.
#ifndef OTA_ENABLED
#define OTA_ENABLED 1
#endif

// Zet op 1 om uitgebreide debug logging toe te voegen voor berekeningen verificatie
// WAARSCHUWING: DEBUG_CALCULATIONS gebruikt ~2808 bytes DRAM voor debug strings
#ifndef DEBUG_CALCULATIONS
#define DEBUG_CALCULATIONS 0  // Standaard uit (productie)
#endif

// Alertketen: compacte trace (grep op [ALERT_TRACE]); alleen logging, geen gedrag
#ifndef DEBUG_ALERT_TRACE
#define DEBUG_ALERT_TRACE 1
#endif

// UI: compacte Serial-log per timeframe-min/max (raw vs live-merge); los van DEBUG_CALCULATIONS om DRAM te sparen
#ifndef DEBUG_UI_TIMEFRAME_MINMAX
#define DEBUG_UI_TIMEFRAME_MINMAX 0  // Standaard uit (productie)
#endif

// applyLiveMinMax: skip merge als laatste live-prijs ouder is dan dit (ms). 0 = geen guard (alleen logging mogelijk).
// Typisch: 2–3× UPDATE_API_INTERVAL (4s) is "vers"; default ruim voor netwerk-stops.
#ifndef UI_APPLY_LIVE_MINMAX_MAX_STALE_MS
#define UI_APPLY_LIVE_MINMAX_MAX_STALE_MS 120000UL
#endif

// Standaard taal instelling (0 = Nederlands, 1 = English)
// Deze waarde wordt gebruikt als fallback als er nog geen taal is opgeslagen in Preferences
// Je kunt de taal altijd wijzigen via de web interface
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // 0 = Nederlands, 1 = English
#endif

// Platform-specifieke instellingen
// PINS files worden alleen geïncludeerd vanuit de main .ino file, niet vanuit modules
// Dit voorkomt multiple definition errors voor bus en gfx
#if defined(PLATFORM_ESP32S3_SUPERMINI)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_ESP32S3_SuperMini_ST7789_154.h"
    #endif
    #define MQTT_TOPIC_PREFIX "esp32s3_crypto"
    #define DEVICE_NAME "ESP32-S3 Super Mini Crypto Monitor"
    #define DEVICE_MODEL "ESP32-S3 Super Mini"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1m"
    #define SYMBOL_30MIN_LABEL "30m"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 60
    #define CHART_ALIGN_Y 26
    #define PRICE_BOX_Y_START 85
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
    #define SYMBOL_COUNT 3  // ESP32-S3 Super Mini: BTCEUR, 1m, 30m
#elif defined(PLATFORM_ESP32S3_GEEK)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_ESP32S3_GEEK_ST7789_114.h"
    #endif
    #define MQTT_TOPIC_PREFIX "esp32s3geek_crypto"
    #define DEVICE_NAME "ESP32-S3 GEEK Crypto Monitor"
    #define DEVICE_MODEL "ESP32-S3 GEEK"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1m"
    #define SYMBOL_30MIN_LABEL "30m"
    #define CHART_WIDTH 135
    #define CHART_HEIGHT 60
    #define CHART_ALIGN_Y 26
    #define PRICE_BOX_Y_START 85
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
    #define SYMBOL_COUNT 3  // GEEK: BTCEUR, 1m, 30m
#elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_ESP32S3_LCDWIKI_28_ILI9341.h"
    #endif
    #define MQTT_TOPIC_PREFIX "esp32s3_lcdwiki28_crypto"
    #define DEVICE_NAME "ESP32-S3 LCDwiki 2.8 Crypto Monitor"
    #define DEVICE_MODEL "ESP32-S3 LCDwiki 2.8"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1 min"
    #define SYMBOL_30MIN_LABEL "30 min"
    #define SYMBOL_2H_LABEL "2h"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 72
    #define CHART_ALIGN_Y 24
    #define PRICE_BOX_Y_START 99
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
    #define SYMBOL_COUNT 4  // LCDwiki 2.8: BTCEUR, 1m, 30m, 2h
#elif defined(PLATFORM_ESP32S3_JC3248W535)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_ESP32S3_JC3248W535_AXS15231B.h"
    #endif
    // -------------------------------------------------------------------------
    // JC3248W535CIY + AXS15231B — bewezen stabiele defaults (esp_lcd backend)
    // Hoofdroute: DisplayBackend_Axs15231bEspLcd + vendor QSPI pinset (geen Arduino_GFX).
    // Onderstaande waarden zijn de laatst geverifieerde combinatie voor correct beeld.
    // Aanpassen alleen bij bewuste A/B-test; andere platforms blijven ongewijzigd.
    // -------------------------------------------------------------------------
    // RGB565: software byte-swap in DMA transportbuffer vóór draw_bitmap.
    #ifndef CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES
    #define CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES 1
    #endif
    // QSPI panel-IO pixelklok (Hz).
    #ifndef CRYPTO_ALERT_AXS15231B_PCLK_HZ
    #define CRYPTO_ALERT_AXS15231B_PCLK_HZ 20000000
    #endif
    // SPI transactie-queue diepte (1 = conservatief, stabiel met stripe-flush).
    #ifndef CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH
    #define CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH 1
    #endif
    // 1 = full-screen kleur self-test in backend begin(); 0 = normale app/LVGL.
    #ifndef CRYPTO_ALERT_AXS15231B_SELFTEST
    #define CRYPTO_ALERT_AXS15231B_SELFTEST 0
    #endif
    // 1 = pinset zoals Espressif DEMO_LVGL esp_bsp (niet schema-PINS header).
    #ifndef CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS
    #define CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS 1
    #endif
    // TE-sync (GPIO TE, typ. GPIO38 op dit board): wacht op tearing-edge vóór draw_bitmap,
    // vergelijkbaar met vendor draw_wait_cb / bsp_display_sync_cb — helpt tegen tearing/grijze sluier.
    // Zet op 0 voor A/B-test zonder TE-wacht.
    #ifndef CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
    #define CRYPTO_ALERT_AXS15231B_USE_TE_SYNC 1
    #endif
    #ifndef CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS
    #define CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS 35
    #endif
    // 0 = RGB element order, 1 = BGR (panel_config.rgb_ele_order).
    #ifndef CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR
    #define CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR 0
    #endif
    // Panel kleurinversie na init (esp_lcd_panel_invert_color).
    #ifndef CRYPTO_ALERT_AXS15231B_INVERT_COLORS
    #define CRYPTO_ALERT_AXS15231B_INVERT_COLORS 1
    #endif
    // Debugoptie:
    // Zet deze define aan als je expliciet de AXS15231B esp_lcd-backend wil gebruiken
    // en NIET wilt dat er stilletjes wordt teruggevallen op Arduino_GFX.
    // #define CRYPTO_ALERT_FORCE_AXS15231B_BACKEND 1
    #define MQTT_TOPIC_PREFIX "jc3248w535_crypto"
    #define DEVICE_NAME "JC3248W535 Crypto Monitor"
    #define DEVICE_MODEL "ESP32-S3 JC3248W535 3.5\""
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1 min"
    #define SYMBOL_30MIN_LABEL "30 min"
    #define SYMBOL_2H_LABEL "2h"
    #define SYMBOL_5M_LABEL "5 min"
    #define SYMBOL_1D_LABEL "1d"
    #define SYMBOL_7D_LABEL "7d"
    // 320px scherm: zelfde contentbreedte als prijskaarten (LV_PCT(100)), niet de smalle 240px-fallback.
    #define CHART_WIDTH 320
    #define CHART_HEIGHT 102  // was 72: +30 px grafiekhoogte
    #define CHART_ALIGN_Y 24
    #define PRICE_BOX_Y_START 129  // was 99: prijskaarten +30 px naar beneden (Y)
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
    #define SYMBOL_COUNT 7  // JC3248W535: data-index 0 spot, 1=1m, 2=30m, 3=2h, 4=5m, 5=1d, 6=7d (visuele volgorde via display-order mapping)
    #define LVGL_SCREEN_WIDTH 320
    #define LVGL_SCREEN_HEIGHT 480
#elif defined(PLATFORM_ESP32S3_AMOLED_206)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_ESP32S3_AMOLED_206_CO5300.h"
    #endif
    #define MQTT_TOPIC_PREFIX "esp32s3_amoled206_crypto"
    #define DEVICE_NAME "ESP32-S3 AMOLED 2.06 Crypto Monitor"
    #define DEVICE_MODEL "ESP32-S3 AMOLED 2.06"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1m"
    #define SYMBOL_30MIN_LABEL "30m"
    #define SYMBOL_2H_LABEL "2h"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 60
    #define CHART_ALIGN_Y 26
    #define PRICE_BOX_Y_START 85
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
    #define SYMBOL_COUNT 3  // AMOLED: BTCEUR, 1m, 30m
    #define LVGL_SCREEN_WIDTH 410
    #define LVGL_SCREEN_HEIGHT 502
#else
    #error "Please define PLATFORM_ESP32S3_SUPERMINI, PLATFORM_ESP32S3_GEEK, PLATFORM_ESP32S3_LCDWIKI_28, PLATFORM_ESP32S3_JC3248W535 or PLATFORM_ESP32S3_AMOLED_206 in platform_config.h"
#endif

// Fallback: als SYMBOL_COUNT nog niet gedefinieerd is, gebruik default 3
#ifndef SYMBOL_COUNT
#define SYMBOL_COUNT 3
#endif
