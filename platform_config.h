// platform_config.h
// Platform-specifieke configuratie
// Selecteer je platform door een van de onderstaande defines te activeren:

//#define PLATFORM_CYD28
#define PLATFORM_TTGO

// Standaard taal instelling (0 = Nederlands, 1 = English)
// Deze waarde wordt gebruikt als fallback als er nog geen taal is opgeslagen in Preferences
// Je kunt de taal altijd wijzigen via de web interface
#ifndef DEFAULT_LANGUAGE
#define DEFAULT_LANGUAGE 0  // 0 = Nederlands, 1 = English
#endif

// Platform-specifieke instellingen
#ifdef PLATFORM_TTGO
    #include "PINS_TTGO_T_Display.h"
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
#elif defined(PLATFORM_CYD28)
    #include "PINS_CYD-ESP32-2432S028-2USB.h"
    #define MQTT_TOPIC_PREFIX "cyd28_crypto"
    #define DEVICE_NAME "CYD 2.8 Crypto Monitor"
    #define DEVICE_MODEL "ESP32 CYD 2.8"
    #define HAS_TOUCHSCREEN true
    #define HAS_PHYSICAL_BUTTON false
    #define SYMBOL_1MIN_LABEL "1 min"
    #define SYMBOL_30MIN_LABEL "30 min"
    #define CHART_WIDTH 240
    #define CHART_HEIGHT 80
    #define CHART_ALIGN_Y 24
    #define PRICE_BOX_Y_START 112  // Terug naar originele waarde
    #define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_18
    #define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_18
    #define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_16
    #define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_16
    #define FONT_SIZE_ANCHOR &lv_font_montserrat_14  // 1 stapje kleiner (was 16)
    #define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_14
    #define FONT_SIZE_FOOTER &lv_font_montserrat_12
    #define FONT_SIZE_IP_PREFIX &lv_font_montserrat_14
    #define FONT_SIZE_IP &lv_font_montserrat_12
    #define FONT_SIZE_CHART_DATE_TIME &lv_font_montserrat_12
    #define FONT_SIZE_CHART_VERSION &lv_font_montserrat_12
    #define FONT_SIZE_CHART_MAX_LABEL &lv_font_montserrat_10
    #define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_14
#else
    #error "Please define PLATFORM_TTGO or PLATFORM_CYD28 in platform_config.h"
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

