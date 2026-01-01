// platform_config.h
// Platform-specifieke configuratie
// Selecteer je platform door een van de onderstaande defines te activeren:

//#define PLATFORM_CYD24
//#define PLATFORM_CYD28
//#define PLATFORM_TTGO
//#define PLATFORM_ESP32S3_SUPERMINI
#define PLATFORM_ESP32S3_GEEK 

// --- Version Configuration ---
// Versie wordt hier gedefinieerd zodat het beschikbaar is voor alle modules
#ifndef VERSION_STRING
#define VERSION_MAJOR 4
#define VERSION_MINOR 15
#define VERSION_STRING "4.15"
#endif

// --- Debug Configuration ---
// Zet op 1 om alleen knop-acties te loggen, 0 voor alle logging
#ifndef DEBUG_BUTTON_ONLY
#define DEBUG_BUTTON_ONLY 1
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
#ifdef PLATFORM_TTGO
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_TTGO_T_Display.h"
    #endif
    #define MQTT_TOPIC_PREFIX "ttgo_crypto"
    #define DEVICE_NAME "TTGO T-Display Crypto Monitor"
    #define DEVICE_MODEL "ESP32 TTGO T-Display"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1m"
    #define SYMBOL_30MIN_LABEL "30m"
    #define STATUS_LED_PIN 2
    #define CHART_WIDTH 135
    #define CHART_HEIGHT 60
    #define CHART_ALIGN_Y 26
    #define PRICE_BOX_Y_START 85  // Terug naar originele waarde
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_10  // 1 stapje kleiner (was 12)
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_10
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_10
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12
#elif defined(PLATFORM_CYD24)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_CYD-ESP32-2432S024.h"
    #endif
    #define MQTT_TOPIC_PREFIX "cyd24_crypto"
    #define DEVICE_NAME "CYD 2.4 Crypto Monitor"
    #define DEVICE_MODEL "ESP32 CYD 2.4"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1 min"
    #define SYMBOL_30MIN_LABEL "30 min"
    #define SYMBOL_2H_LABEL "2h"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 72  // Verkleind van 80 naar 72 (8px kleiner)
    #define CHART_ALIGN_Y 24
    #define PRICE_BOX_Y_START 99  // Aangepast: 24 (chart top) + 72 (chart height) + 3 (spacing) = 99
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
    #define SYMBOL_COUNT 4  // CYD: BTCEUR, 1m, 30m, 2h
#elif defined(PLATFORM_CYD28)
    #if !defined(UICONTROLLER_INCLUDE) && !defined(MODULE_INCLUDE)
    #include "PINS_CYD-ESP32-2432S028-2USB.h"
    #endif
    #define MQTT_TOPIC_PREFIX "cyd28_crypto"
    #define DEVICE_NAME "CYD 2.8 Crypto Monitor"
    #define DEVICE_MODEL "ESP32 CYD 2.8"
    #define HAS_TOUCHSCREEN false
    #define HAS_PHYSICAL_BUTTON true
    #define BUTTON_PIN 0
    #define SYMBOL_1MIN_LABEL "1 min"
    #define SYMBOL_30MIN_LABEL "30 min"
    #define SYMBOL_2H_LABEL "2h"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 72  // Verkleind van 80 naar 72 (8px kleiner)
    #define CHART_ALIGN_Y 24
    #define PRICE_BOX_Y_START 99  // Aangepast: 24 (chart top) + 72 (chart height) + 3 (spacing) = 99
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
    #define SYMBOL_COUNT 4  // CYD: BTCEUR, 1m, 30m, 2h
#elif defined(PLATFORM_ESP32S3_SUPERMINI)
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
    #define SYMBOL_COUNT 3  // TTGO/ESP32-S3: BTCEUR, 1m, 30m
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
#else
    #error "Please define PLATFORM_TTGO, PLATFORM_CYD24, PLATFORM_CYD28, PLATFORM_ESP32S3_SUPERMINI or PLATFORM_ESP32S3_GEEK in platform_config.h"
#endif

// Fallback: als SYMBOL_COUNT nog niet gedefinieerd is, gebruik default 3
#ifndef SYMBOL_COUNT
#define SYMBOL_COUNT 3
#endif

// Helper macros voor conditional compilation
#if HAS_TOUCHSCREEN
    #define TOUCHSCREEN_CODE(code) code
#else
    #define TOUCHSCREEN_CODE(code)
#endif

#if HAS_PHYSICAL_BUTTON
    #define PHYSICAL_BUTTON_CODE(code) code
#else
    #define PHYSICAL_BUTTON_CODE(code)
#endif

