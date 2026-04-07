// Definieer SYMBOL_COUNT VOORDAT UIController.h wordt geïncludeerd
// We kunnen platform_config.h niet volledig includen omdat dat multiple definition errors veroorzaakt (gfx/bus)
// Oplossing: lees alleen de platform define uit platform_config.h door het bestand te parsen
// of definieer SYMBOL_COUNT direct hier op basis van de platform define
// 
// Eenvoudigste oplossing: include platform_config.h maar voorkom dat PINS files worden geïncludeerd
// door de include statements te conditioneel te maken in platform_config.h zelf
// OF: maak een kleine header die alleen de platform defines bevat

// Voor nu: probeer platform_config.h te includen maar voorkom de PINS includes
// door een define te gebruiken die de PINS includes overslaat
#define UICONTROLLER_INCLUDE  // Flag om aan te geven dat we vanuit UIController includen
#include "../../platform_config.h"
#undef UICONTROLLER_INCLUDE

#include "../PriceFormat/QuotePriceFormat.h"
#include "ChartPriceScale.h"

#include "UIController.h"
#include <cstdint>  // int32_t
#include <cstring>   // strcmp, strncpy
#include <math.h>  // fabsf — vlakke return-kleur in updatePriceCardColor()
#include <lvgl.h>
#include "../display/DisplayBackend.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
// Fase 8.5.2: updateTrendLabel() dependencies
#include "../TrendDetector/TrendDetector.h"  // Voor TrendState enum
// Fase 8.5.3: updateVolatilityLabel() dependencies
#include "../VolatilityTracker/VolatilityTracker.h"  // Voor VolatilityState enum
// Fase 8.6.x: volume/range UI indicators dependencies
#include "../AlertEngine/AlertEngine.h"  // Voor VolumeRangeStatus, TwoHMetrics
#include "../RegimeEngine/RegimeEngine.h"
// Fase 8.6.1: updateBTCEURCard() dependencies
#include "../AnchorSystem/AnchorSystem.h"  // Voor AnchorConfigEffective struct

extern TwoHMetrics computeTwoHMetrics();  // Definitie in ESP32-Crypto-Alert.ino (zelfde bron als /status)

// Platform-specifieke constants (gedefinieerd in platform_config.h, maar we includen niet om dubbele definitie van gfx/bus te voorkomen)
// Fase 8.3: createChart() dependencies - constants worden extern gebruikt
// Note: CHART_WIDTH, CHART_HEIGHT, CHART_ALIGN_Y, FONT_SIZE_* zijn gedefinieerd in platform_config.h
// We gebruiken ze via extern of via de .ino file die ze al heeft geïncludeerd
// Als fallback definiëren we default waarden
#ifndef CHART_WIDTH
#define CHART_WIDTH 240  // Fallback (wordt door platform_config.h overschreven)
#endif
#ifndef CHART_HEIGHT
#define CHART_HEIGHT 60  // Default (wordt overschreven door platform_config.h als het later wordt gedefinieerd)
#endif
#ifndef CHART_ALIGN_Y
#define CHART_ALIGN_Y 26  // Default (wordt overschreven door platform_config.h als het later wordt gedefinieerd)
#endif
// Font constants (platform-specifiek, gedefinieerd in platform_config.h)
// Note: Font constants zijn pointers naar LVGL fonts (extern symbols)
// We kunnen ze niet zomaar definiëren zonder de font definitie
// De .ino file definieert deze via platform_config.h, dus we gebruiken extern
// Als fallback gebruiken we direct de font pointer
#ifndef FONT_SIZE_TREND_VOLATILITY
// Forward declaration: font wordt gedefinieerd in LVGL library
extern const lv_font_t lv_font_montserrat_12;
#define FONT_SIZE_TREND_VOLATILITY &lv_font_montserrat_12  // Default (wordt overschreven door platform_config.h)
#endif
// Additional font constants voor createPriceBoxes()
#ifndef FONT_SIZE_TITLE_BTCEUR
extern const lv_font_t lv_font_montserrat_14;
#define FONT_SIZE_TITLE_BTCEUR &lv_font_montserrat_14  // Default
#endif
#ifndef FONT_SIZE_TITLE_OTHER
#define FONT_SIZE_TITLE_OTHER &lv_font_montserrat_12  // Default (gebruikt al gedefinieerde font)
#endif
#ifndef FONT_SIZE_PRICE_BTCEUR
#define FONT_SIZE_PRICE_BTCEUR &lv_font_montserrat_12  // Default (gebruikt al gedefinieerde font)
#endif
#ifndef FONT_SIZE_PRICE_OTHER
#define FONT_SIZE_PRICE_OTHER &lv_font_montserrat_12  // Default (gebruikt al gedefinieerde font)
#endif
#ifndef FONT_SIZE_PRICE_MIN_MAX_DIFF
#define FONT_SIZE_PRICE_MIN_MAX_DIFF &lv_font_montserrat_12  // Default (gebruikt al gedefinieerde font)
#endif
// PRICE_BOX_Y_START constant
#ifndef PRICE_BOX_Y_START
#define PRICE_BOX_Y_START 85  // Default (wordt overschreven door platform_config.h)
#endif
// FONT_SIZE_FOOTER constant
#ifndef FONT_SIZE_FOOTER
#define FONT_SIZE_FOOTER &lv_font_montserrat_12  // Default (gebruikt al gedefinieerde font)
#endif

// Volume/range UI thresholds
#ifndef VOLUME_BADGE_THRESHOLD_PCT
#define VOLUME_BADGE_THRESHOLD_PCT 50.0f
#endif

// Constants die gedefinieerd zijn in .ino (moeten beschikbaar zijn voor module)
// Fase 8.3: createChart() dependencies
#ifndef POINTS_TO_CHART
#define POINTS_TO_CHART 60  // Default value (wordt overschreven door .ino als het later wordt gedefinieerd)
#endif

// Forward declarations voor globale variabelen (worden gebruikt door callbacks)
extern void Serial_println(const char*);
extern void Serial_println(const __FlashStringHelper*);
// display backend global wordt gedeclareerd in DisplayBackend.h
// Fase 8.10.1: setupLVGL() dependencies
extern lv_display_t *disp;
extern lv_color_t *disp_draw_buf;
extern size_t disp_draw_buf_size;
extern bool hasPSRAM();
extern void setDisplayBrigthness();

// Forward declarations voor globale variabelen (worden gebruikt door create functies)
// Fase 8.3: createChart() dependencies
extern float openPrices[];
extern uint8_t symbolIndexToChart;
extern int32_t maxRange;
extern int32_t minRange;
extern char ntfyTopic[];
extern void getDeviceIdFromTopic(const char* topic, char* buffer, size_t bufferSize);
// Fase 8.11.1: createFooter() dependencies (2-regel footer: lblFooterLine*, ramLabel)
extern lv_obj_t *lblFooterLine1;
extern lv_obj_t *lblFooterLine2;
extern lv_obj_t *ramLabel;
extern void disableScroll(lv_obj_t *obj);
extern const char* const symbols[];
// Phase 1: Anchor queue (uitgevoerd in apiTask)
extern bool queueAnchorSetting(float value, bool useCurrentPrice);
// VERSION_STRING wordt gedefinieerd in platform_config.h (beschikbaar voor alle modules)
// Geen fallback nodig omdat platform_config.h altijd wordt geïncludeerd
extern void formatIPAddress(IPAddress ip, char* buffer, size_t bufferSize);

// Fase 8.5.2: updateTrendLabel() dependencies
extern TrendDetector trendDetector;
extern bool hasRet2h;
extern bool hasRet30m;
extern bool hasRet1d;
extern bool hasRet7d;
extern bool warmStart1dValid;
extern bool warmStart7dValid;
extern bool hasRet2hWarm;
extern bool hasRet30mWarm;
extern bool hasRet2hLive;
extern bool hasRet30mLive;
#if defined(PLATFORM_ESP32S3_JC3248W535)
extern bool hasRet7dLive;
#endif
extern bool regimeEngineEnabled;
#if UI_HAS_TF_MINMAX_STATUS_UI
extern float g_uiTfRawMin[7];
extern float g_uiTfRawMax[7];
extern bool g_uiTfRawValid[7];
extern uint8_t g_uiTfMinMaxSrc[7];
extern void uiResetTfMinMaxSnapshot(void);
extern uint8_t calcLivePctSecondWindow(void);
#endif
#if defined(PLATFORM_ESP32S3_JC3248W535)
extern uint8_t g_uiLastMinMaxSource1d;
extern uint8_t g_uiLastMinMaxSource7d;
extern bool hasRet1dWarm;
extern bool hasRet7dWarm;
extern uint8_t livePct5m;
#endif
extern uint8_t language;
extern bool minuteArrayFilled;
extern uint8_t minuteIndex;
extern float lastFetchedPrice;
extern float latestKnownPrice;
extern unsigned long latestKnownPriceMs;
extern uint32_t lastApiMs;
extern uint8_t latestKnownPriceSource;
extern uint8_t calcLivePctMinuteAverages(uint16_t windowMinutes);
extern uint8_t calcLivePctHourlyLastN(uint16_t windowHours);
extern const char* getText(const char* nlText, const char* enText);
// MINUTES_FOR_30MIN_CALC is een #define, niet een variabele
#ifndef MINUTES_FOR_30MIN_CALC
#define MINUTES_FOR_30MIN_CALC 120  // Default (wordt overschreven door .ino)
#endif
// Fase 8.5.3: updateVolatilityLabel() dependencies
extern VolatilityTracker volatilityTracker;
// Fase 8.5.4: updateWarmStartStatusLabel() — grafiekbadge (zie forward decl. warmStartStatusLabel)
// Fase 8.6.1: updateBTCEURCard() dependencies
#include "../AnchorSystem/AnchorSystem.h"  // Voor AnchorConfigEffective struct
extern AnchorSystem anchorSystem;
extern float prices[];
extern bool anchorActive;
extern float anchorPrice;
extern float anchorMaxLoss;
extern float anchorTakeProfit;
extern lv_obj_t *priceTitle[];
extern lv_obj_t *priceLbl[];
extern lv_obj_t *anchorMaxLabel;
extern lv_obj_t *anchorLabel;
extern lv_obj_t *anchorMinLabel;
// Cache variabelen en buffers (moeten extern zijn)
extern float lastPriceLblValue;
extern float lastAnchorMaxValue;
extern float lastAnchorValue;
extern float lastAnchorMinValue;
// Buffer sizes (gedefinieerd in .ino) — voldoende voor dynamische EUR-format (tot 5 decimalen < €1)
#define PRICE_LBL_BUFFER_SIZE 32
#define ANCHOR_LABEL_BUFFER_SIZE 48
#define ANCHOR_MAX_LABEL_BUFFER_SIZE 48
extern char priceLblBuffer[PRICE_LBL_BUFFER_SIZE];
extern char anchorMaxLabelBuffer[ANCHOR_MAX_LABEL_BUFFER_SIZE];
extern char anchorLabelBuffer[ANCHOR_LABEL_BUFFER_SIZE];
extern char anchorMinLabelBuffer[ANCHOR_LABEL_BUFFER_SIZE];
// Fase 8.6.2: updateAveragePriceCard() dependencies
extern bool secondArrayFilled;
extern uint8_t secondIndex;
extern float averagePrices[];
extern void findMinMaxInSecondPrices(float &minVal, float &maxVal);
extern void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
extern lv_obj_t *price1MinMaxLabel;
extern lv_obj_t *price1MinMinLabel;
extern lv_obj_t *price1MinDiffLabel;
extern lv_obj_t *price30MinMaxLabel;
extern lv_obj_t *price30MinMinLabel;
extern lv_obj_t *price30MinDiffLabel;
extern lv_obj_t *price2HMaxLabel;
extern lv_obj_t *price2HMinLabel;
extern lv_obj_t *price2HDiffLabel;
extern char priceTitleBuffer[SYMBOL_COUNT][40];  // Verkleind van 48 naar 40 bytes
extern char price1MinMaxLabelBuffer[24];
extern char price1MinMinLabelBuffer[24];
extern char price1MinDiffLabelBuffer[24];
extern char price30MinMaxLabelBuffer[24];
extern char price2HMaxLabelBuffer[24];
extern char price2HMinLabelBuffer[24];
extern char price2HDiffLabelBuffer[24];
extern char price30MinMinLabelBuffer[24];
extern char price30MinDiffLabelBuffer[32];
extern char bitvavoSymbol[16];
extern char chartColorMode[8];
extern char chartColorManual[16];
extern float lastPrice1MinMaxValue;
extern float lastPrice1MinMinValue;
extern float lastPrice1MinDiffValue;
extern float lastPrice30MinMaxValue;
extern float lastPrice30MinMinValue;
extern float lastPrice30MinDiffValue;
extern float lastPrice2HMaxValue;
extern float lastPrice2HMinValue;
extern float lastPrice2HDiffValue;
#if defined(PLATFORM_ESP32S3_JC3248W535)
extern void findMinMaxInFiveMinutePrices(float &minVal, float &maxVal);
extern void findMinMaxInLast24Hours(float &minVal, float &maxVal);
extern bool uiFiveMinuteHasMinimalData(void);
extern lv_obj_t *price5mMaxLabel;
extern lv_obj_t *price5mMinLabel;
extern lv_obj_t *price5mDiffLabel;
extern char price5mMaxLabelBuffer[24];
extern char price5mMinLabelBuffer[24];
extern char price5mDiffLabelBuffer[24];
extern float lastPrice5mMaxValue;
extern float lastPrice5mMinValue;
extern float lastPrice5mDiffValue;
extern lv_obj_t *price1dMaxLabel;
extern lv_obj_t *price1dMinLabel;
extern lv_obj_t *price1dDiffLabel;
extern char price1dMaxLabelBuffer[24];
extern char price1dMinLabelBuffer[24];
extern char price1dDiffLabelBuffer[24];
extern float lastPrice1dMaxValue;
extern float lastPrice1dMinValue;
extern float lastPrice1dDiffValue;
extern void findMinMaxInLast7Days(float &minVal, float &maxVal);
extern lv_obj_t *price7dMaxLabel;
extern lv_obj_t *price7dMinLabel;
extern lv_obj_t *price7dDiffLabel;
extern char price7dMaxLabelBuffer[24];
extern char price7dMinLabelBuffer[24];
extern char price7dDiffLabelBuffer[24];
extern float lastPrice7dMaxValue;
extern float lastPrice7dMinValue;
extern float lastPrice7dDiffValue;
#endif
extern char lastPriceTitleText[SYMBOL_COUNT][32];  // Verkleind van 48 naar 32 bytes
extern char priceLblBufferArray[SYMBOL_COUNT][32];
extern float lastPriceLblValueArray[SYMBOL_COUNT];

// Forward declarations voor globale UI pointers (parallel implementatie - moeten ook geïnitialiseerd worden)
// Fase 8.4.3: Synchroniseer module pointers met globale pointers voor backward compatibility
extern lv_obj_t *chart;
extern lv_chart_series_t *dataSeries;
extern lv_obj_t *trendLabel;
extern lv_obj_t *volatilityLabel;
extern lv_obj_t *volumeConfirmLabel;
extern lv_obj_t *mediumTrendLabel;
extern lv_obj_t *longTermTrendLabel;
extern lv_obj_t *warmStartStatusLabel;
extern lv_obj_t *chartVersionLabel;
extern lv_obj_t *priceBox[];
extern lv_obj_t *priceTitle[];
extern lv_obj_t *priceLbl[];
extern lv_obj_t *chartTitle;
extern lv_obj_t *chartDateLabel;
extern lv_obj_t *chartTimeLabel;
extern lv_obj_t *chartBeginLettersLabel;
// Fase 8.7.1: updateChartSection() dependencies
extern bool newPriceDataAvailable;
extern void safeStrncpy(char *dest, const char *src, size_t destSize);
// Fase 8.11.3: updateChartRange() is verplaatst naar UIController module (private method)
// Fase 8.8.1: updateUI() dependencies
extern uint32_t lastApiMs;
extern void updateFooter();
// Fase 8.9.1: checkButton() dependencies
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
extern SemaphoreHandle_t dataMutex;
extern void fetchPrice();
extern bool safeMutexTake(SemaphoreHandle_t mutex, TickType_t timeout, const char* context);
extern void safeMutexGive(SemaphoreHandle_t mutex, const char* context);
extern unsigned long lastButtonPress;
extern int lastButtonState;
// BUTTON_DEBOUNCE_MS wordt nu als #define gebruikt i.p.v. const variabele
// BUTTON_PIN is een #define, moet via platform_config.h of pins header
#ifndef BUTTON_PIN
#define BUTTON_PIN 0  // Default voor ESP32 (GPIO 0)
#endif
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 500  // 500ms debounce
#endif
extern lv_obj_t *ipLabel;
extern lv_obj_t *price1MinMaxLabel;
extern lv_obj_t *price1MinMinLabel;
extern lv_obj_t *price1MinDiffLabel;
extern lv_obj_t *price30MinMaxLabel;
extern lv_obj_t *price30MinMinLabel;
extern lv_obj_t *price30MinDiffLabel;
extern lv_obj_t *anchorLabel;
extern lv_obj_t *anchorMaxLabel;
extern lv_obj_t *anchorMinLabel;
extern VolumeRangeStatus lastVolumeRange1m;
extern VolumeRangeStatus lastVolumeRange5m;

// UIController implementation
// Fase 8: UI Module refactoring

UIController::UIController() {
    // Fase 8.2.1: Initialiseer UI object pointers naar nullptr
    chart = nullptr;
    dataSeries = nullptr;
    trendLabel = nullptr;
    volatilityLabel = nullptr;
    mediumTrendLabel = nullptr;
    longTermTrendLabel = nullptr;
    warmStartStatusLabel = nullptr;
    lblFooterLine1 = nullptr;
    lblFooterLine2 = nullptr;
    ramLabel = nullptr;
    chartVersionLabel = nullptr;
    
    for (uint8_t i = 0; i < SYMBOL_COUNT; i++) {
        priceBox[i] = nullptr;
        priceTitle[i] = nullptr;
        priceLbl[i] = nullptr;
    }
    
    chartTitle = nullptr;
    chartDateLabel = nullptr;
    chartTimeLabel = nullptr;
    chartBeginLettersLabel = nullptr;
    ipLabel = nullptr;
    
    price1MinMaxLabel = nullptr;
    price1MinMinLabel = nullptr;
    price1MinDiffLabel = nullptr;
    price30MinMaxLabel = nullptr;
    price30MinMinLabel = nullptr;
    price30MinDiffLabel = nullptr;
    
    anchorLabel = nullptr;
    anchorMaxLabel = nullptr;
    anchorMinLabel = nullptr;
    volumeConfirmLabel = nullptr;
}

void UIController::begin() {
    // Fase 8.1.2: Basis initialisatie
    // Fase 8.10: LVGL initialisatie wordt hier later toegevoegd
}

// LVGL callback: print log information
// Fase 8.1.3: Verplaatst naar UIController module
// Note: Deze functie moet extern blijven voor LVGL (niet static in header)
void UIController::my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial_println(buf);
    Serial.flush();
}

// LVGL callback: retrieve elapsed time
// Fase 8.1.3: Verplaatst naar UIController module
// Note: Deze functie moet extern blijven voor LVGL (niet static in header)
uint32_t UIController::millis_cb(void) {
    return millis();
}

// LVGL callback: flush display buffer
// Fase 8.1.3: Verplaatst naar UIController module
// Note: Deze functie moet extern blijven voor LVGL (niet static in header)
void UIController::my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    static bool s_loggedFirstFlush = false;
    if (!s_loggedFirstFlush) {
        if (area) {
            Serial.printf("[LVGL] First flush begin: area=(%d,%d)-(%d,%d)\n", area->x1, area->y1, area->x2, area->y2);
        } else {
            Serial.println("[LVGL] First flush begin: area=<null>");
        }
    }

    if (g_displayBackend && area && px_map) {
        g_displayBackend->flush(area, px_map);
    }

    if (!s_loggedFirstFlush) {
        Serial.println("[LVGL] Before lv_display_flush_ready()");
    }
    lv_disp_flush_ready(disp);
    if (!s_loggedFirstFlush) {
        Serial.println("[LVGL] After lv_display_flush_ready()");
        s_loggedFirstFlush = true;
    }
}

static bool isUsdcQuoteSymbol(const char* symbol)
{
    if (symbol == nullptr) {
        return false;
    }
    const char* suffix = "-USDC";
    size_t len = strlen(symbol);
    size_t suffixLen = strlen(suffix);
    if (len < suffixLen) {
        return false;
    }
    return strcmp(symbol + (len - suffixLen), suffix) == 0;
}

// Base-deel vóór '-' (Bitvavo "BASE-QUOTE"); leeg bij ontbrekende '-'
static void getBaseAssetPrefix(const char* symbol, char* out, size_t outSz)
{
    if (out == nullptr || outSz == 0) {
        return;
    }
    out[0] = '\0';
    if (symbol == nullptr) {
        return;
    }
    const char* dash = strchr(symbol, '-');
    if (dash == nullptr) {
        safeStrncpy(out, symbol, outSz);
        return;
    }
    size_t n = static_cast<size_t>(dash - symbol);
    if (n >= outSz) {
        n = outSz - 1;
    }
    memcpy(out, symbol, n);
    out[n] = '\0';
}

static bool baseAssetEquals(const char* base, const char* expect)
{
    if (base == nullptr || expect == nullptr) {
        return false;
    }
    for (; *base && *expect; base++, expect++) {
        char a = *base;
        char b = *expect;
        if (a >= 'a' && a <= 'z') {
            a = static_cast<char>(a - 32);
        }
        if (b >= 'a' && b <= 'z') {
            b = static_cast<char>(b - 32);
        }
        if (a != b) {
            return false;
        }
    }
    return *base == '\0' && *expect == '\0';
}

// Quote-accent: EUR-blauw / USDC-groen (header, hoofdprijs, footer — niet de chartlijn)
static lv_color_t getQuoteAccentColor()
{
    return isUsdcQuoteSymbol(bitvavoSymbol)
        ? lv_palette_main(LV_PALETTE_GREEN)
        : lv_palette_main(LV_PALETTE_BLUE);
}

// Chart lijn + punten: op BASE-asset (BTC oranje, ETH paars, …)
static lv_color_t getChartAssetColor()
{
    char base[12];
    getBaseAssetPrefix(bitvavoSymbol, base, sizeof(base));
    if (baseAssetEquals(base, "BTC")) {
        return lv_palette_main(LV_PALETTE_ORANGE);
    }
    if (baseAssetEquals(base, "ETH")) {
        return lv_palette_main(LV_PALETTE_PURPLE);
    }
    if (baseAssetEquals(base, "SOL")) {
        return lv_palette_main(LV_PALETTE_YELLOW);
    }
    if (baseAssetEquals(base, "ADA")) {
        return lv_palette_main(LV_PALETTE_RED);
    }
    if (baseAssetEquals(base, "XRP")) {
        return lv_palette_main(LV_PALETTE_CYAN);
    }
    return lv_palette_lighten(LV_PALETTE_GREY, 2);
}

// Alleen voor grafieklijn/punten bij chartColorMode == manual
static lv_color_t lvColorFromManualChartKey(const char* key)
{
    if (key == nullptr || key[0] == '\0') {
        return lv_palette_main(LV_PALETTE_ORANGE);
    }
    if (strcmp(key, "orange") == 0) {
        return lv_palette_main(LV_PALETTE_ORANGE);
    }
    if (strcmp(key, "purple") == 0) {
        return lv_palette_main(LV_PALETTE_PURPLE);
    }
    if (strcmp(key, "yellow") == 0) {
        return lv_palette_main(LV_PALETTE_YELLOW);
    }
    if (strcmp(key, "red") == 0) {
        return lv_palette_main(LV_PALETTE_RED);
    }
    if (strcmp(key, "cyan") == 0) {
        return lv_palette_main(LV_PALETTE_CYAN);
    }
    if (strcmp(key, "blue") == 0) {
        return lv_palette_main(LV_PALETTE_BLUE);
    }
    if (strcmp(key, "green") == 0) {
        return lv_palette_main(LV_PALETTE_GREEN);
    }
    if (strcmp(key, "white") == 0) {
        return lv_color_white();
    }
    return lv_palette_main(LV_PALETTE_ORANGE);
}

static lv_color_t getEffectiveChartSeriesColor()
{
    if (chartColorMode[0] != '\0' && strcmp(chartColorMode, "manual") == 0) {
        return lvColorFromManualChartKey(chartColorManual);
    }
    return getChartAssetColor();
}

static lv_color_t getChartSeriesLineColor()
{
    return getEffectiveChartSeriesColor();
}

static const char* getQuoteHexColor()
{
    return isUsdcQuoteSymbol(bitvavoSymbol) ? "4CAF50" : "2196F3";
}

static const char* getBaseHexColor()
{
    char base[12];
    getBaseAssetPrefix(bitvavoSymbol, base, sizeof(base));
    if (baseAssetEquals(base, "BTC")) {
        return "FF8C00";
    }
    if (baseAssetEquals(base, "ETH")) {
        return "9C27B0";
    }
    if (baseAssetEquals(base, "SOL")) {
        return "FDD835";
    }
    if (baseAssetEquals(base, "ADA")) {
        return "F44336";
    }
    if (baseAssetEquals(base, "XRP")) {
        return "00BCD4";
    }
    return "BDBDBD";
}

static void setBtcTitleLabel()
{
    if (priceTitle[0] == nullptr) {
        return;
    }
    lv_label_set_recolor(priceTitle[0], true);
    const char* symbol = bitvavoSymbol;
    const char* dash = (symbol != nullptr) ? strchr(symbol, '-') : nullptr;
    char base[8] = {0};
    char quote[8] = {0};
    if (dash != nullptr) {
        size_t baseLen = dash - symbol;
        if (baseLen >= sizeof(base)) baseLen = sizeof(base) - 1;
        strncpy(base, symbol, baseLen);
        base[baseLen] = '\0';
        safeStrncpy(quote, dash + 1, sizeof(quote));
    } else if (symbol != nullptr) {
        safeStrncpy(base, symbol, sizeof(base));
    }
    const char* quoteHex = getQuoteHexColor();
    const char* baseHex = getBaseHexColor();
    char titleBuf[32];
    if (quote[0] != '\0') {
        snprintf(titleBuf, sizeof(titleBuf), "#%s %s#-#%s %s#", baseHex, base, quoteHex, quote);
    } else {
        snprintf(titleBuf, sizeof(titleBuf), "#%s %s#", baseHex, base);
    }
    lv_label_set_text(priceTitle[0], titleBuf);
}

static void applyChartHeaderFooterColors(lv_color_t color)
{
    if (chartTitle != nullptr) {
        lv_obj_set_style_text_color(chartTitle, color, 0);
    }
    if (chartDateLabel != nullptr) {
        lv_obj_set_style_text_color(chartDateLabel, color, 0);
    }
    if (chartTimeLabel != nullptr) {
        lv_obj_set_style_text_color(chartTimeLabel, color, 0);
    }
    if (chartBeginLettersLabel != nullptr) {
        lv_obj_set_style_text_color(chartBeginLettersLabel, color, 0);
    }
    if (chartVersionLabel != nullptr) {
        lv_obj_set_style_text_color(chartVersionLabel, color, 0);
    }
    if (lblFooterLine1 != nullptr) {
        lv_obj_set_style_text_color(lblFooterLine1, color, 0);
    }
    if (lblFooterLine2 != nullptr) {
        lv_obj_set_style_text_color(lblFooterLine2, color, 0);
    }
    if (ramLabel != nullptr) {
        lv_obj_set_style_text_color(ramLabel, color, 0);
    }
    if (ipLabel != nullptr) {
        lv_obj_set_style_text_color(ipLabel, color, 0);
    }
}

static void applyBtcEurBoxColors(lv_color_t color)
{
    if (priceLbl[0] != nullptr) {
        lv_obj_set_style_text_color(priceLbl[0], color, 0);
    }
}

// Fase 8.3.1: createChart() verplaatst naar UIController module (parallel implementatie)
void UIController::createChart() {
    // Chart - gebruik platform-specifieke afmetingen
    chart = lv_chart_create(lv_scr_act());
    ::chart = chart;  // Fase 8.4.3: Synchroniseer met globale pointer voor backward compatibility
    lv_chart_set_point_count(chart, POINTS_TO_CHART);
    lv_obj_set_size(chart, CHART_WIDTH, CHART_HEIGHT);
    #if defined(PLATFORM_ESP32S3_JC3248W535)
    // Lijn uit met prijskaarten (links uitgelijnd, volle CHART_WIDTH).
    lv_obj_align(chart, LV_ALIGN_TOP_LEFT, 0, CHART_ALIGN_Y);
    #else
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, CHART_ALIGN_Y);
    #endif
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    disableScroll(chart);
    
    // Initiële Y-range: zelfde EUR→chart-eenheden en dynamische half-range als updateChartRange()
    // (ChartPriceScale.h). Gebruik geen oude cent-schaal open*100 of vaste PRICE_RANGE.
    float refOpen = openPrices[symbolIndexToChart];
    if (!(refOpen > 0.0f)) {
        refOpen = (prices[0] > 0.0f) ? prices[0] : 100.0f;
    }
    const float chartScaleInit = getChartPriceScale(refOpen);
    const int32_t halfRangeInit = chartHalfRangeY(refOpen, chartScaleInit);
    int32_t p = chartPriceEurToY(refOpen);
    maxRange = p + halfRangeInit;
    minRange = p - halfRangeInit;
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);

    // Maak één serie aan (kleur op BASE-asset; quote-accent blijft voor header/hoofdprijs)
    dataSeries = lv_chart_add_series(chart, getChartSeriesLineColor(), LV_CHART_AXIS_PRIMARY_Y);
    ::dataSeries = dataSeries;  // Fase 8.4.3: Synchroniseer met globale pointer

    // Trend/volatiliteit labels in de chart, links uitgelijnd binnen de chart
    trendLabel = lv_label_create(chart);
    ::trendLabel = trendLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(trendLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(trendLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(trendLabel, LV_ALIGN_TOP_LEFT, -4, -6);
    lv_label_set_text(trendLabel, "--");
    
    // Warm-start status label (rechts bovenin chart, zelfde hoogte als trend)
    warmStartStatusLabel = lv_label_create(chart);
    ::warmStartStatusLabel = warmStartStatusLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(warmStartStatusLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(warmStartStatusLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(warmStartStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(warmStartStatusLabel, LV_ALIGN_TOP_RIGHT, 4, -6);
    lv_label_set_text(warmStartStatusLabel, "--");
    
    volumeConfirmLabel = lv_label_create(chart);
    ::volumeConfirmLabel = volumeConfirmLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(volumeConfirmLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(volumeConfirmLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(volumeConfirmLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(volumeConfirmLabel, LV_ALIGN_RIGHT_MID, 4, 0);
    lv_label_set_text(volumeConfirmLabel, "");
    
    volatilityLabel = lv_label_create(chart);
    ::volatilityLabel = volatilityLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(volatilityLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(volatilityLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(volatilityLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(volatilityLabel, LV_ALIGN_BOTTOM_RIGHT, 4, 6);
    lv_label_set_text(volatilityLabel, "--");
    
    // Medium trend label (links midden)
    mediumTrendLabel = lv_label_create(chart);
    ::mediumTrendLabel = mediumTrendLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(mediumTrendLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(mediumTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(mediumTrendLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(mediumTrendLabel, LV_ALIGN_LEFT_MID, -4, 0);
    lv_label_set_text(mediumTrendLabel, "--");
    
    // Lange termijn trend label (links onderin)
    longTermTrendLabel = lv_label_create(chart);
    ::longTermTrendLabel = longTermTrendLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(longTermTrendLabel, FONT_SIZE_TREND_VOLATILITY, 0);
    lv_obj_set_style_text_color(longTermTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(longTermTrendLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(longTermTrendLabel, LV_ALIGN_BOTTOM_LEFT, -4, 6);
    lv_label_set_text(longTermTrendLabel, "--");
    
    // Platform-specifieke layout voor chart title
    #if !defined(PLATFORM_ESP32S3_SUPERMINI) && !defined(PLATFORM_ESP32S3_GEEK)
    chartTitle = lv_label_create(lv_scr_act());
    ::chartTitle = chartTitle;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(chartTitle, &lv_font_montserrat_16, 0);
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartTitle, deviceIdBuffer);
    lv_obj_set_style_text_color(chartTitle, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align_to(chartTitle, chart, LV_ALIGN_OUT_TOP_LEFT, 0, -4);
    #endif
}

// Fase 8.3.2: createHeaderLabels() verplaatst naar UIController module (parallel implementatie)
void UIController::createHeaderLabels() {
    #if defined(PLATFORM_ESP32S3_GEEK)
    // GEEK: Compacte layout met datum op regel 1, beginletters/versie/tijd op regel 2
    chartDateLabel = lv_label_create(lv_scr_act());
    ::chartDateLabel = chartDateLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, CHART_WIDTH);
    lv_obj_set_pos(chartDateLabel, 0, 0); // GEEK: originele positie (geen aanpassing nodig)
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    ::chartBeginLettersLabel = chartBeginLettersLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(chartBeginLettersLabel, ntfyTopic);
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    ::chartTimeLabel = chartTimeLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, CHART_WIDTH);
    lv_obj_set_pos(chartTimeLabel, 0, 10);
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
    // Super Mini: ruimere header met datum/tijd en device-id (beginletters)
    chartDateLabel = lv_label_create(lv_scr_act());
    ::chartDateLabel = chartDateLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, 180);
    lv_obj_set_pos(chartDateLabel, -2, 4); // 2 pixels naar links
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    ::chartBeginLettersLabel = chartBeginLettersLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(chartBeginLettersLabel, ntfyTopic);
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    ::chartTimeLabel = chartTimeLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, 240);
    lv_obj_set_pos(chartTimeLabel, 0, 4);
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
    // JC3248 320×480: zelfde effectieve breedte als prijskaarten (geen smalle 240px-fallback).
    chartDateLabel = lv_label_create(lv_scr_act());
    ::chartDateLabel = chartDateLabel;
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, LV_SIZE_CONTENT);
    lv_obj_align(chartDateLabel, LV_ALIGN_TOP_MID, 0, 4);

    chartTimeLabel = lv_label_create(lv_scr_act());
    ::chartTimeLabel = chartTimeLabel;
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, LV_SIZE_CONTENT);
    lv_obj_align(chartTimeLabel, LV_ALIGN_TOP_RIGHT, -4, 4);
    #else
    // Standaard header: datum en tijd (o.a. LCDWIKI 2.8, AMOLED — geen GEEK/SuperMini/JC3248-tak)
    chartDateLabel = lv_label_create(lv_scr_act());
    ::chartDateLabel = chartDateLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, 180);
    lv_obj_set_pos(chartDateLabel, -2, 4); // 2 pixels naar links
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    ::chartTimeLabel = chartTimeLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, 240);
    lv_obj_set_pos(chartTimeLabel, 0, 4);
    #endif
}

// Fase 8.3.3: createPriceBoxes() verplaatst naar UIController module (parallel implementatie)
void UIController::createPriceBoxes() {
    // JC3248: visuele volgorde spot → 1m → 5m → 30m → 2h → 1d → 7d; data-indexen 0..6
#if defined(PLATFORM_ESP32S3_JC3248W535)
    static const uint8_t kJcDisplayOrder[] = {0, 1, 4, 2, 3, 5, 6};  // moet SYMBOL_COUNT entries hebben
    lv_obj_t *prevVisBox = nullptr;
#endif
    for (uint8_t slot = 0; slot < SYMBOL_COUNT; ++slot)
    {
#if defined(PLATFORM_ESP32S3_JC3248W535)
        const uint8_t dataIndex = kJcDisplayOrder[slot];
#else
        const uint8_t dataIndex = slot;
#endif
        priceBox[dataIndex] = lv_obj_create(lv_scr_act());
        ::priceBox[dataIndex] = priceBox[dataIndex];  // Fase 8.4.3: Synchroniseer met globale pointer
        lv_obj_set_size(priceBox[dataIndex], LV_PCT(100), LV_SIZE_CONTENT);

#if defined(PLATFORM_ESP32S3_JC3248W535)
        if (slot == 0) {
            lv_obj_align(priceBox[dataIndex], LV_ALIGN_TOP_LEFT, 0, PRICE_BOX_Y_START);
        } else {
            lv_obj_align_to(priceBox[dataIndex], prevVisBox, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        }
        prevVisBox = priceBox[dataIndex];
#else
        if (dataIndex == 0) {
            lv_obj_align(priceBox[dataIndex], LV_ALIGN_TOP_LEFT, 0, PRICE_BOX_Y_START);
        }
        else {
            lv_obj_align_to(priceBox[dataIndex], priceBox[dataIndex - 1], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        }
#endif

        lv_obj_set_style_radius(priceBox[dataIndex], 6, 0);
        lv_obj_set_style_pad_all(priceBox[dataIndex], 4, 0);
        lv_obj_set_style_border_width(priceBox[dataIndex], 1, 0);
        lv_obj_set_style_border_color(priceBox[dataIndex], lv_palette_main(LV_PALETTE_GREY), 0);
        disableScroll(priceBox[dataIndex]);

        // Symbol caption
        priceTitle[dataIndex] = lv_label_create(priceBox[dataIndex]);
        ::priceTitle[dataIndex] = priceTitle[dataIndex];  // Fase 8.4.3: Synchroniseer
        if (dataIndex == 0) {
            lv_obj_set_style_text_font(priceTitle[dataIndex], FONT_SIZE_TITLE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceTitle[dataIndex], FONT_SIZE_TITLE_OTHER, 0);
        }
        if (dataIndex == 0) {
            lv_obj_set_style_text_color(priceTitle[dataIndex], lv_color_white(), 0);
        } else {
            lv_obj_set_style_text_color(priceTitle[dataIndex], lv_color_white(), 0);
        }
        lv_label_set_text(priceTitle[dataIndex], symbols[dataIndex]);
        lv_obj_align(priceTitle[dataIndex], LV_ALIGN_TOP_LEFT, 0, 0);
        if (dataIndex == 0) {
            setBtcTitleLabel();
        }
        
        // Live price - platform-specifieke layout
        priceLbl[dataIndex] = lv_label_create(priceBox[dataIndex]);
        ::priceLbl[dataIndex] = priceLbl[dataIndex];  // Fase 8.4.3: Synchroniseer
        if (dataIndex == 0) {
            lv_obj_set_style_text_font(priceLbl[dataIndex], FONT_SIZE_PRICE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceLbl[dataIndex], FONT_SIZE_PRICE_OTHER, 0);
        }
        
        #if defined(PLATFORM_ESP32S3_GEEK)
        if (dataIndex == 0) {
            lv_obj_set_style_text_align(priceLbl[dataIndex], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[dataIndex], getQuoteAccentColor(), 0);
            lv_obj_align_to(priceLbl[dataIndex], priceTitle[dataIndex], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[dataIndex], priceTitle[dataIndex], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (dataIndex == 0) - compacte GEEK-layout
        if (dataIndex == 0) {
            anchorMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorMaxLabel = anchorMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorLabel = anchorLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorMinLabel = anchorMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
            
            // Forceer refresh van anchor-labels na UI rebuild
            lastAnchorMaxValue = -1.0f;
            lastAnchorValue = -1.0f;
            lastAnchorMinValue = -1.0f;
            anchorMaxLabelBuffer[0] = '\0';
            anchorLabelBuffer[0] = '\0';
            anchorMinLabelBuffer[0] = '\0';
        }
        #else
        if (dataIndex == 0) {
            lv_obj_set_style_text_align(priceLbl[dataIndex], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[dataIndex], getQuoteAccentColor(), 0);
            lv_obj_align_to(priceLbl[dataIndex], priceTitle[dataIndex], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[dataIndex], priceTitle[dataIndex], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (dataIndex == 0) — standaard kaartlayout met percentages
        if (dataIndex == 0) {
            anchorLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorLabel = anchorLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorMaxLabel = anchorMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[dataIndex]);
            ::anchorMinLabel = anchorMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
            
            // Forceer refresh van anchor-labels na UI rebuild
            lastAnchorMaxValue = -1.0f;
            lastAnchorValue = -1.0f;
            lastAnchorMinValue = -1.0f;
            anchorMaxLabelBuffer[0] = '\0';
            anchorLabelBuffer[0] = '\0';
            anchorMinLabelBuffer[0] = '\0';
        }
        #endif
        
        lv_label_set_text(priceLbl[dataIndex], "--");
        
        // Min/Max/Diff labels voor 1 min blok
        if (dataIndex == 1)
        {
            price1MinMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price1MinMaxLabel = price1MinMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price1MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMaxLabel, "--");
            lv_obj_align(price1MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price1MinDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price1MinDiffLabel = price1MinDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price1MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinDiffLabel, "--");
            lv_obj_align(price1MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price1MinMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price1MinMinLabel = price1MinMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price1MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMinLabel, "--");
            lv_obj_align(price1MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        
        // Min/Max/Diff labels voor 30 min blok (index 2)
        if (dataIndex == 2)
        {
            price30MinMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price30MinMaxLabel = price30MinMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price30MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMaxLabel, "--");
            lv_obj_align(price30MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price30MinDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price30MinDiffLabel = price30MinDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price30MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinDiffLabel, "--");
            lv_obj_align(price30MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price30MinMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price30MinMinLabel = price30MinMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price30MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMinLabel, "--");
            lv_obj_align(price30MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        
        // Min/Max/Diff labels voor 2h blok (index 3) — LCDWIKI_28 / JC3248W535
        #if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
        if (dataIndex == 3)
        {
            // Initialiseer buffers
            strcpy(price2HMaxLabelBuffer, "--");
            strcpy(price2HDiffLabelBuffer, "--");
            strcpy(price2HMinLabelBuffer, "--");
            
            price2HMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price2HMaxLabel = price2HMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price2HMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HMaxLabel, price2HMaxLabelBuffer);
            lv_obj_align(price2HMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price2HDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price2HDiffLabel = price2HDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price2HDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HDiffLabel, price2HDiffLabelBuffer);
            lv_obj_align(price2HDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price2HMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price2HMinLabel = price2HMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price2HMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HMinLabel, price2HMinLabelBuffer);
            lv_obj_align(price2HMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        #endif
        #if defined(PLATFORM_ESP32S3_JC3248W535)
        // Min/Max/Diff labels voor 5m blok (index 4) — alleen JC3248 5-kaart UI
        if (dataIndex == 4)
        {
            strcpy(price5mMaxLabelBuffer, "--");
            strcpy(price5mDiffLabelBuffer, "--");
            strcpy(price5mMinLabelBuffer, "--");
            
            price5mMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price5mMaxLabel = price5mMaxLabel;
            lv_obj_set_style_text_font(price5mMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price5mMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price5mMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price5mMaxLabel, price5mMaxLabelBuffer);
            lv_obj_align(price5mMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price5mDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price5mDiffLabel = price5mDiffLabel;
            lv_obj_set_style_text_font(price5mDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price5mDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price5mDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price5mDiffLabel, price5mDiffLabelBuffer);
            lv_obj_align(price5mDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price5mMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price5mMinLabel = price5mMinLabel;
            lv_obj_set_style_text_font(price5mMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price5mMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price5mMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price5mMinLabel, price5mMinLabelBuffer);
            lv_obj_align(price5mMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        // Min/Max/Diff labels voor 1d blok (data-index 5) — alleen JC3248
        if (dataIndex == 5)
        {
            strcpy(price1dMaxLabelBuffer, "--");
            strcpy(price1dDiffLabelBuffer, "--");
            strcpy(price1dMinLabelBuffer, "--");

            price1dMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price1dMaxLabel = price1dMaxLabel;
            lv_obj_set_style_text_font(price1dMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1dMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price1dMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1dMaxLabel, price1dMaxLabelBuffer);
            lv_obj_align(price1dMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);

            price1dDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price1dDiffLabel = price1dDiffLabel;
            lv_obj_set_style_text_font(price1dDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1dDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price1dDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1dDiffLabel, price1dDiffLabelBuffer);
            lv_obj_align(price1dDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);

            price1dMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price1dMinLabel = price1dMinLabel;
            lv_obj_set_style_text_font(price1dMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1dMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price1dMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1dMinLabel, price1dMinLabelBuffer);
            lv_obj_align(price1dMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        if (dataIndex == 6)
        {
            strcpy(price7dMaxLabelBuffer, "--");
            strcpy(price7dDiffLabelBuffer, "--");
            strcpy(price7dMinLabelBuffer, "--");

            price7dMaxLabel = lv_label_create(priceBox[dataIndex]);
            ::price7dMaxLabel = price7dMaxLabel;
            lv_obj_set_style_text_font(price7dMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price7dMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price7dMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price7dMaxLabel, price7dMaxLabelBuffer);
            lv_obj_align(price7dMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);

            price7dDiffLabel = lv_label_create(priceBox[dataIndex]);
            ::price7dDiffLabel = price7dDiffLabel;
            lv_obj_set_style_text_font(price7dDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price7dDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price7dDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price7dDiffLabel, price7dDiffLabelBuffer);
            lv_obj_align(price7dDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);

            price7dMinLabel = lv_label_create(priceBox[dataIndex]);
            ::price7dMinLabel = price7dMinLabel;
            lv_obj_set_style_text_font(price7dMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price7dMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price7dMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price7dMinLabel, price7dMinLabelBuffer);
            lv_obj_align(price7dMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        #endif
    }
}

// Fase 8.5.1: updateDateTimeLabels() naar Module
// Helper functie om datum/tijd labels bij te werken
void UIController::updateDateTimeLabels()
{
    // Cache variabelen voor datum/tijd labels (lokaal voor deze functie)
    static char lastDateText[11] = {0};  // Cache voor date label
    static char lastTimeText[9] = {0};   // Cache voor time label
    
    // Fase 8.5.1: Gebruik globale pointer (synchroniseert met module pointer)
    if (::chartDateLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            #if defined(PLATFORM_ESP32S3_GEEK)
            // GEEK: compact formaat dd-mm-yy voor lagere resolutie
            char dateStr[9]; // dd-mm-yy + null terminator = 9 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
            #else
            // Niet-GEEK: volledig formaat dd-mm-yyyy
            char dateStr[11]; // dd-mm-yyyy + null terminator = 11 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
            #endif
            // Update alleen als datum veranderd is
            if (strcmp(lastDateText, dateStr) != 0) {
                strncpy(lastDateText, dateStr, sizeof(lastDateText) - 1);
                lastDateText[sizeof(lastDateText) - 1] = '\0';
                lv_label_set_text(::chartDateLabel, dateStr);
            }
        }
    }
    
    // Fase 8.5.1: Gebruik globale pointer (synchroniseert met module pointer)
    if (::chartTimeLabel != nullptr)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char timeStr[9];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            // Update alleen als tijd veranderd is
            if (strcmp(lastTimeText, timeStr) != 0) {
                strncpy(lastTimeText, timeStr, sizeof(lastTimeText) - 1);
                lastTimeText[sizeof(lastTimeText) - 1] = '\0';
                lv_label_set_text(::chartTimeLabel, timeStr);
            }
        }
    }
}

// Fase 8.3.4: createFooter() verplaatst naar UIController module (parallel implementatie)
void UIController::createFooter() {
    #if defined(PLATFORM_ESP32S3_GEEK)
    // GEEK: IP-adres links, versienummer rechts
    ipLabel = lv_label_create(lv_scr_act());
    ::ipLabel = ipLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(ipLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ipLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    ::chartVersionLabel = chartVersionLabel;  // Sync globale pointer voor updateFooter()
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    
    if (WiFi.status() == WL_CONNECTED) {
        // Geoptimaliseerd: gebruik char array i.p.v. String
        static char ipBuffer[16];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        lv_label_set_text(ipLabel, ipBuffer);
    } else {
        lv_label_set_text(ipLabel, "--");
    }
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
    // ESP32-S3 Super Mini: IP + dBm links, RAM + versie rechts (één regel, meer horizontale ruimte)
    ipLabel = lv_label_create(lv_scr_act());
    ::ipLabel = ipLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(ipLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ipLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    ::chartVersionLabel = chartVersionLabel;  // Sync globale pointer voor updateFooter()
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
    
    if (WiFi.status() == WL_CONNECTED) {
        // ESP32-S3: IP + dBm op één regel (5 spaties tussen IP en dBm)
        static char ipBuffer[32];
        formatIPAddress(WiFi.localIP(), ipBuffer, sizeof(ipBuffer));
        int rssi = WiFi.RSSI();
        char *ipEnd = ipBuffer + strlen(ipBuffer);
        snprintf(ipEnd, sizeof(ipBuffer) - strlen(ipBuffer), "     %ddBm", rssi); // 5 spaties
        lv_label_set_text(ipLabel, ipBuffer);
    } else {
        lv_label_set_text(ipLabel, "--     --dBm");
    }
    
    // Update versie label met RAM info (5 spaties tussen kB en versie)
    uint32_t freeRAM = heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024;
    static char versionBuffer[16];
    snprintf(versionBuffer, sizeof(versionBuffer), "%ukB     %s", freeRAM, VERSION_STRING); // 5 spaties
    lv_label_set_text(chartVersionLabel, versionBuffer);
    #else
    // 2-regel footer (RSSI/RAM regel 1, IP regel 2; versie via chartVersionLabel)
    lblFooterLine1 = lv_label_create(lv_scr_act());
    ::lblFooterLine1 = lblFooterLine1;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(lblFooterLine1, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(lblFooterLine1, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(lblFooterLine1, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lblFooterLine1, LV_ALIGN_BOTTOM_LEFT, 0, -18);
    lv_label_set_text(lblFooterLine1, "--dBm");
    
    ramLabel = lv_label_create(lv_scr_act());
    ::ramLabel = ramLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(ramLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ramLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ramLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(ramLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -18);
    lv_label_set_text(ramLabel, "--kB");
    
    lblFooterLine2 = lv_label_create(lv_scr_act());
    ::lblFooterLine2 = lblFooterLine2;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(lblFooterLine2, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(lblFooterLine2, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(lblFooterLine2, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lblFooterLine2, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    lv_label_set_text(lblFooterLine2, "--.--.--.--");
    
    chartVersionLabel = lv_label_create(lv_scr_act());
    ::chartVersionLabel = chartVersionLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartVersionLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(chartVersionLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartVersionLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartVersionLabel, VERSION_STRING);
    lv_obj_align(chartVersionLabel, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
#endif
}

// Helper: reset UI pointers before rebuild (voorkomt stale pointers na lv_obj_clean)
static void resetUiPointers() {
    chart = nullptr;
    dataSeries = nullptr;
    trendLabel = nullptr;
    warmStartStatusLabel = nullptr;
    volumeConfirmLabel = nullptr;
    volatilityLabel = nullptr;
    mediumTrendLabel = nullptr;
    longTermTrendLabel = nullptr;
    chartTitle = nullptr;
    chartDateLabel = nullptr;
    chartBeginLettersLabel = nullptr;
    chartTimeLabel = nullptr;
    anchorLabel = nullptr;
    anchorMaxLabel = nullptr;
    anchorMinLabel = nullptr;
    price1MinMaxLabel = nullptr;
    price1MinDiffLabel = nullptr;
    price1MinMinLabel = nullptr;
    price30MinMaxLabel = nullptr;
    price30MinDiffLabel = nullptr;
    price30MinMinLabel = nullptr;
    price2HMaxLabel = nullptr;
    price2HDiffLabel = nullptr;
    price2HMinLabel = nullptr;
#if defined(PLATFORM_ESP32S3_JC3248W535)
    price5mMaxLabel = nullptr;
    price5mDiffLabel = nullptr;
    price5mMinLabel = nullptr;
    price1dMaxLabel = nullptr;
    price1dDiffLabel = nullptr;
    price1dMinLabel = nullptr;
    price7dMaxLabel = nullptr;
    price7dDiffLabel = nullptr;
    price7dMinLabel = nullptr;
#endif
    ipLabel = nullptr;
    chartVersionLabel = nullptr;
    lblFooterLine1 = nullptr;
    ramLabel = nullptr;
    lblFooterLine2 = nullptr;

    ::chart = nullptr;
    ::dataSeries = nullptr;
    ::trendLabel = nullptr;
    ::warmStartStatusLabel = nullptr;
    ::volumeConfirmLabel = nullptr;
    ::volatilityLabel = nullptr;
    ::mediumTrendLabel = nullptr;
    ::longTermTrendLabel = nullptr;
    ::chartTitle = nullptr;
    ::chartDateLabel = nullptr;
    ::chartBeginLettersLabel = nullptr;
    ::chartTimeLabel = nullptr;
    ::anchorLabel = nullptr;
    ::anchorMaxLabel = nullptr;
    ::anchorMinLabel = nullptr;
    ::price1MinMaxLabel = nullptr;
    ::price1MinDiffLabel = nullptr;
    ::price1MinMinLabel = nullptr;
    ::price30MinMaxLabel = nullptr;
    ::price30MinDiffLabel = nullptr;
    ::price30MinMinLabel = nullptr;
    ::price2HMaxLabel = nullptr;
    ::price2HDiffLabel = nullptr;
    ::price2HMinLabel = nullptr;
#if defined(PLATFORM_ESP32S3_JC3248W535)
    ::price5mMaxLabel = nullptr;
    ::price5mDiffLabel = nullptr;
    ::price5mMinLabel = nullptr;
    ::price1dMaxLabel = nullptr;
    ::price1dDiffLabel = nullptr;
    ::price1dMinLabel = nullptr;
#endif
    ::ipLabel = nullptr;
    ::chartVersionLabel = nullptr;
    ::lblFooterLine1 = nullptr;
    ::ramLabel = nullptr;
    ::lblFooterLine2 = nullptr;

    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i) {
        priceBox[i] = nullptr;
        priceTitle[i] = nullptr;
        priceLbl[i] = nullptr;
        ::priceBox[i] = nullptr;
        ::priceTitle[i] = nullptr;
        ::priceLbl[i] = nullptr;
    }
}

// Fase 8.4.1: buildUI() verplaatst naar UIController module (parallel implementatie)
void UIController::buildUI() {
    static bool s_loggedFirstUiBuild = false;
    lv_obj_t* screen = lv_scr_act();
    if (screen == nullptr) {
        Serial.println(F("[UI] WARN: lv_scr_act is null, skip buildUI"));
        return;
    }
    resetUiPointers();
    lv_obj_clean(screen);
    disableScroll(lv_scr_act());
    
    createChart();
    createHeaderLabels();
    createPriceBoxes();
    createFooter();
    applyChartHeaderFooterColors(getQuoteAccentColor());
    applyBtcEurBoxColors(getQuoteAccentColor());

    if (!s_loggedFirstUiBuild) {
        Serial.println("[UI] First UI build completed");
        s_loggedFirstUiBuild = true;
    }
}

// Alleen displaytekst; enum-waarden REGIME_* blijven in RegimeEngine.
static const char* regimeDisplayLabelText(RegimeKind k)
{
    switch (k) {
        case REGIME_SLAP:
            return getText("RUSTIG", "CALM");
        case REGIME_ENERGIEK:
            return getText("ENERGIEK", "ENERGETIC");
        case REGIME_GELADEN:
        default:
            return getText("GELADEN", "LOADED");
    }
}

// Alleen kleurmapping voor regime-weergave rechtsonder.
static lv_color_t regimeDisplayLabelColor(RegimeKind k)
{
    switch (k) {
        case REGIME_SLAP:
            return lv_palette_main(LV_PALETTE_GREEN);
        case REGIME_ENERGIEK:
            return lv_palette_main(LV_PALETTE_RED);
        case REGIME_GELADEN:
        default:
            return lv_palette_main(LV_PALETTE_ORANGE);  // Amber-achtig, consistent met DATAXX%-accenten.
    }
}

// Fase 8.5.2: updateTrendLabel() naar Module
// Helper functie om trend label bij te werken
void UIController::updateTrendLabel()
{
    // Fase 8.5.2: Gebruik globale pointer (synchroniseert met module pointer)
    if (::trendLabel == nullptr) return;
    
    // Toon trend alleen als beide availability flags true zijn
    if (hasRet2h && hasRet30m)
    {
        const char* trendText = "";
        lv_color_t trendColor = lv_palette_main(LV_PALETTE_GREY);
        
        // Bepaal of data uit warm-start of live komt
        bool isFromWarmStart = (hasRet2hWarm && hasRet30mWarm) && !(hasRet2hLive && hasRet30mLive);
        bool isFromLive = (hasRet2hLive && hasRet30mLive);
        
        // Fase 5.3.14: Gebruik TrendDetector module getter i.p.v. globale variabele
        TrendState currentTrend = trendDetector.getTrendState();
        switch (currentTrend) {
            case TREND_UP:
                trendText = getText("2h//", "2h//");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_GREEN); // Groen voor live UP
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
            case TREND_DOWN:
                trendText = getText("2h\\\\", "2h\\\\");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_RED); // Rood voor live DOWN
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
            case TREND_SIDEWAYS:
            default:
                trendText = getText("2h=", "2h=");
                if (isFromWarmStart) {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs voor warm-start
                } else if (isFromLive) {
                    trendColor = lv_palette_main(LV_PALETTE_BLUE); // Blauw voor live SIDEWAYS
                } else {
                    trendColor = lv_palette_main(LV_PALETTE_GREY); // Grijs als fallback
                }
                break;
        }
        
        // Geen "-warm" tekst meer - kleur geeft status aan
        lv_label_set_text(::trendLabel, trendText);
        lv_obj_set_style_text_color(::trendLabel, trendColor, 0);
    }
    else
    {
        // Toon specifiek wat ontbreekt: 30m of 2h
        uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
        char waitText[24];
        
        if (!hasRet30m) {
            // Warm-up 30m: toon status alleen als warm-start NIET succesvol was
            // Als warm-start succesvol was maar hasRet30m nog false, toon dan warm-start status
            if (hasRet30mWarm) {
                // Warm-start heeft 30m data, maar hasRet30m is nog false (mogelijk bug, toon "--")
                lv_label_set_text(::trendLabel, "--");
                lv_obj_set_style_text_color(::trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
            
            // Warm-start was niet succesvol: bereken minuten nodig voor 30m window met ≥80% live
            uint8_t livePct30 = calcLivePctMinuteAverages(30);
            
            if (availableMinutes < 30) {
                // Nog niet genoeg data: toon minuten tot 30
                // Geoptimaliseerd: language check geëlimineerd (beide branches identiek)
                uint8_t minutesNeeded = 30 - availableMinutes;
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %um", minutesNeeded);
            } else if (livePct30 < 80) {
                // Genoeg data maar niet genoeg live: toon percentage live
                // Geoptimaliseerd: language check geëlimineerd (beide branches identiek)
                    snprintf(waitText, sizeof(waitText), "Warm-up 30m %u%%", livePct30);
            } else {
                // Zou niet moeten voorkomen (livePct30 >= 80 maar hasRet30m is false)
                lv_label_set_text(::trendLabel, "--");
                lv_obj_set_style_text_color(::trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
        } else if (!hasRet2h) {
            // Warm-up 2h: bereken minuten nodig voor 120m window met ≥80% live
            uint8_t livePct120 = calcLivePctMinuteAverages(120);
            
            if (availableMinutes < 120) {
                // Nog niet genoeg data: toon minuten tot 120
                // Geoptimaliseerd: language check geëlimineerd (beide branches identiek)
                uint8_t minutesNeeded = 120 - availableMinutes;
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %um", minutesNeeded);
            } else if (livePct120 < 80) {
                // Genoeg data maar niet genoeg live: toon percentage live
                // Geoptimaliseerd: language check geëlimineerd (beide branches identiek)
                    snprintf(waitText, sizeof(waitText), "Warm-up 2h %u%%", livePct120);
            } else {
                // Zou niet moeten voorkomen (livePct120 >= 80 maar hasRet2h is false)
                lv_label_set_text(::trendLabel, "--");
                lv_obj_set_style_text_color(::trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
                return;
            }
        } else {
            // Beide ontbreken (zou niet moeten voorkomen, maar fallback)
            lv_label_set_text(::trendLabel, "--");
            lv_obj_set_style_text_color(::trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            return;
        }
        
        lv_label_set_text(::trendLabel, waitText);
        lv_obj_set_style_text_color(::trendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    }
}

// Fase 8.5.3: updateVolatilityLabel() naar Module
// Helper functie om volatiliteit label bij te werken
void UIController::updateVolatilityLabel()
{
    // Fase 8.5.3: Gebruik globale pointer (synchroniseert met module pointer)
    if (::volatilityLabel == nullptr) return;

    // Regime op dezelfde plek als volatility (rechtsonder chart); bij uit: bestaande VLAK/GOLVEND/GRILLIG
    if (regimeEngineEnabled) {
        const RegimeSnapshot& rs = regimeEngineGetSnapshot();
        lv_label_set_text(::volatilityLabel, regimeDisplayLabelText(rs.committedRegime));
        lv_obj_set_style_text_color(::volatilityLabel, regimeDisplayLabelColor(rs.committedRegime), 0);
        return;
    }
    
    const char* volText = "";
    lv_color_t volColor = lv_palette_main(LV_PALETTE_GREY);
    
    // Fase 5.3.14: Gebruik VolatilityTracker module getter i.p.v. globale variabele
    VolatilityState currentVol = volatilityTracker.getVolatilityState();
    switch (currentVol) {
        case VOLATILITY_LOW:
            volText = getText("VLAK", "FLAT");
            volColor = lv_palette_main(LV_PALETTE_GREEN);
            break;
        case VOLATILITY_MEDIUM:
            volText = getText("GOLVEND", "WAVES");
            volColor = lv_palette_main(LV_PALETTE_ORANGE);
            break;
        case VOLATILITY_HIGH:
            volText = getText("GRILLIG", "VOLATILE");
            volColor = lv_palette_main(LV_PALETTE_RED);
            break;
    }
    
    lv_label_set_text(::volatilityLabel, volText);
    lv_obj_set_style_text_color(::volatilityLabel, volColor, 0);
}

// Fase 8.5.3: updateVolumeConfirmLabel() naar Module
// Helper functie om volume confirm label bij te werken
void UIController::updateVolumeConfirmLabel()
{
    if (::volumeConfirmLabel == nullptr) return;

    const char* volumeText = "--";
    lv_color_t volumeColor = lv_palette_main(LV_PALETTE_GREY);
    unsigned long nowMs = millis();

    if (lastVolumeRange1m.valid) {
        if (fabsf(lastVolumeRange1m.volumeDeltaPct) >= VOLUME_BADGE_THRESHOLD_PCT) {
            volumeText = (lastVolumeRange1m.volumeDeltaPct >= 0.0f) ? "VOLUME//" : "VOLUME\\\\";
            volumeColor = (lastVolumeRange1m.volumeDeltaPct >= 0.0f)
                              ? lv_palette_main(LV_PALETTE_GREEN)
                              : lv_palette_main(LV_PALETTE_RED);
        } else {
            volumeText = "VOLUME=";
            volumeColor = lv_palette_main(LV_PALETTE_GREY);
        }
    } else if (lastVolumeRange1m.rangePct > 0.0f) {
        // Candle is geldig maar volume-EMA nog niet valide
        volumeText = "VOLUME=";
        volumeColor = lv_palette_main(LV_PALETTE_GREY);
    }
    
    lv_label_set_text(::volumeConfirmLabel, volumeText);
    lv_obj_set_style_text_color(::volumeConfirmLabel, volumeColor, 0);
}

// Fase 8.5.4: updateMediumTrendLabel() naar Module
// Helper functie om medium trend label bij te werken
void UIController::updateMediumTrendLabel()
{
    // Fase 8.5.4: Gebruik globale pointer (synchroniseert met module pointer)
    if (::mediumTrendLabel == nullptr) return;
    
    extern bool hasRet1d;
    if (hasRet1d)
    {
        extern float ret_1d;
        extern TrendDetector trendDetector;
        
        // Gebruik threshold van 2.0% voor 1d trend
        const float mediumThreshold = 2.0f;
        TrendState mediumTrend = trendDetector.determineMediumTrendState(0.0f, ret_1d, mediumThreshold);
        
        const char* trendText = "";
        lv_color_t trendColor = lv_palette_main(LV_PALETTE_GREY);
        
        switch (mediumTrend) {
            case TREND_UP:
                trendText = getText("1d//", "1d//");
                trendColor = lv_palette_main(LV_PALETTE_GREEN);
                break;
            case TREND_DOWN:
                trendText = getText("1d\\\\", "1d\\\\");
                trendColor = lv_palette_main(LV_PALETTE_RED);
                break;
            case TREND_SIDEWAYS:
            default:
                trendText = getText("1d=", "1d=");
                trendColor = lv_palette_main(LV_PALETTE_BLUE);
                break;
        }
        
        lv_label_set_text(::mediumTrendLabel, trendText);
        lv_obj_set_style_text_color(::mediumTrendLabel, trendColor, 0);
        
        #if DEBUG_CALCULATIONS
        static float lastLoggedRet1d = -999.0f;
        static const char* lastLoggedText = nullptr;
        if (fabsf(ret_1d - lastLoggedRet1d) > 0.01f || lastLoggedText != trendText) {
            Serial.printf(F("[UI][1d] hasRet1d=%d, ret_1d=%.4f%%, trend=%s, text=%s\n"),
                         hasRet1d ? 1 : 0, ret_1d,
                         (mediumTrend == TREND_UP) ? "UP" : (mediumTrend == TREND_DOWN) ? "DOWN" : "SIDEWAYS",
                         trendText);
            lastLoggedRet1d = ret_1d;
            lastLoggedText = trendText;
        }
        #endif
    }
    else
    {
        lv_label_set_text(::mediumTrendLabel, "--");
        lv_obj_set_style_text_color(::mediumTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
        
        #if DEBUG_CALCULATIONS
        static bool lastLoggedHasRet1d = true;
        if (lastLoggedHasRet1d != hasRet1d) {
            Serial.printf(F("[UI][1d] hasRet1d=%d, label set to '--'\n"), hasRet1d ? 1 : 0);
            lastLoggedHasRet1d = hasRet1d;
        }
        #endif
    }
}

// Fase 8.5.5: updateLongTermTrendLabel() naar Module
// Helper functie om lange termijn trend label bij te werken
void UIController::updateLongTermTrendLabel()
{
    // Fase 8.5.5: Gebruik globale pointer (synchroniseert met module pointer)
    if (::longTermTrendLabel == nullptr) return;
    
    // Toon lange termijn trend alleen als 7d beschikbaar is
    extern bool hasRet7d;
    if (hasRet7d)
    {
        extern float ret_7d;
        extern TrendDetector trendDetector;
        
        // Gebruik threshold van 1.0% voor lange termijn trend
        const float longTermThreshold = 1.0f;
        TrendState longTermTrend = trendDetector.determineLongTermTrendState(ret_7d, longTermThreshold);
        
        const char* trendText = "";
        lv_color_t trendColor = lv_palette_main(LV_PALETTE_GREY);
        
        switch (longTermTrend) {
            case TREND_UP:
                trendText = getText("7d//", "7d//");
                trendColor = lv_palette_main(LV_PALETTE_GREEN);
                break;
            case TREND_DOWN:
                trendText = getText("7d\\\\", "7d\\\\");
                trendColor = lv_palette_main(LV_PALETTE_RED);
                break;
            case TREND_SIDEWAYS:
            default:
                trendText = getText("7d=", "7d=");
                trendColor = lv_palette_main(LV_PALETTE_BLUE);
                break;
        }
        
        lv_label_set_text(::longTermTrendLabel, trendText);
        lv_obj_set_style_text_color(::longTermTrendLabel, trendColor, 0);
        
        #if DEBUG_CALCULATIONS
        static float lastLoggedRet7d = -999.0f;
        static const char* lastLoggedText7d = nullptr;
        if (fabsf(ret_7d - lastLoggedRet7d) > 0.01f || lastLoggedText7d != trendText) {
            Serial.printf(F("[UI][7d] hasRet7d=%d, ret_7d=%.4f%%, trend=%s, text=%s\n"),
                         hasRet7d ? 1 : 0, ret_7d,
                         (longTermTrend == TREND_UP) ? "UP" : (longTermTrend == TREND_DOWN) ? "DOWN" : "SIDEWAYS",
                         trendText);
            lastLoggedRet7d = ret_7d;
            lastLoggedText7d = trendText;
        }
        #endif
    }
    else
    {
        lv_label_set_text(::longTermTrendLabel, "--");
        lv_obj_set_style_text_color(::longTermTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
        
        #if DEBUG_CALCULATIONS
        static bool lastLoggedHasRet7d = true;
        if (lastLoggedHasRet7d != hasRet7d) {
            Serial.printf(F("[UI][7d] hasRet7d=%d, label set to '--'\n"), hasRet7d ? 1 : 0);
            lastLoggedHasRet7d = hasRet7d;
        }
        #endif
    }
}

// Fase 8.6.1: updateBTCEURCard() naar Module
// Helper functie om BTCEUR card bij te werken
void UIController::updateBTCEURCard(bool hasNewData)
{
    // Fase 8.6.1: Gebruik globale pointers (synchroniseert met module pointers)
    if (::priceTitle[0] != nullptr) {
        // Gebruik dynamisch symbol i.p.v. hardcoded "BTCEUR"
        lv_label_set_text(::priceTitle[0], ::symbols[0]);
        setBtcTitleLabel();
    }
    
    float displayPrice = prices[0];
    if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "UI BTCEUR snapshot")) {
        float lk = latestKnownPrice;
        float px = prices[0];
        safeMutexGive(dataMutex, "UI BTCEUR snapshot");
        displayPrice = (lk > 0.0f) ? lk : px;
    }
    
    // Update price label alleen als geformatteerde string verandert (gedeeld EUR-format)
    {
        static char s_lastMainPriceFmt[PRICE_LBL_BUFFER_SIZE] = "";
        formatQuotePriceEur(priceLblBuffer, PRICE_LBL_BUFFER_SIZE, displayPrice);
        if (::priceLbl[0] != nullptr &&
            (strcmp(priceLblBuffer, s_lastMainPriceFmt) != 0 || s_lastMainPriceFmt[0] == '\0')) {
            strncpy(s_lastMainPriceFmt, priceLblBuffer, sizeof(s_lastMainPriceFmt));
            s_lastMainPriceFmt[sizeof(s_lastMainPriceFmt) - 1] = '\0';
            lv_label_set_text(::priceLbl[0], priceLblBuffer);
            lastPriceLblValue = displayPrice;
        }
    }
    
    // Bitcoin waarde linksonderin volgt quote kleur (EUR blauw, USDC groen)
    if (::priceLbl[0] != nullptr) {
            lv_obj_set_style_text_color(::priceLbl[0], getQuoteAccentColor(), 0);
    }
    float activeAnchorPrice = AlertEngine::getActiveAnchorPrice(anchorPrice);
    bool anchorDisplayActive = activeAnchorPrice > 0.0f;

    // Bereken dynamische anchor-waarden op basis van trend voor UI weergave
    AnchorConfigEffective effAnchorUI;
    if (anchorDisplayActive) {
        // Fase 5.3.14: Gebruik TrendDetector module getter i.p.v. globale variabele
        TrendState currentTrend = trendDetector.getTrendState();
        // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
        effAnchorUI = anchorSystem.calcEffectiveAnchor(anchorMaxLoss, anchorTakeProfit, currentTrend);
    }
    
    #if defined(PLATFORM_ESP32S3_GEEK)
    if (::anchorMaxLabel != nullptr) {
        if (anchorDisplayActive) {
            // Gebruik dynamische take profit waarde
            float takeProfitPrice = activeAnchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                formatQuotePriceEur(anchorMaxLabelBuffer, ANCHOR_MAX_LABEL_BUFFER_SIZE, takeProfitPrice);
                lv_label_set_text(::anchorMaxLabel, anchorMaxLabelBuffer);
                lastAnchorMaxValue = takeProfitPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMaxLabelBuffer) > 0) {
                anchorMaxLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorMaxLabel, "");
                lastAnchorMaxValue = -1.0f;
            }
        }
    }
    
    if (::anchorLabel != nullptr) {
        if (anchorDisplayActive) {
            if (lastAnchorValue != activeAnchorPrice || lastAnchorValue < 0.0f) {
                formatQuotePriceEur(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, activeAnchorPrice);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = activeAnchorPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorLabelBuffer) > 0) {
                anchorLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorLabel, "");
                lastAnchorValue = -1.0f;
            }
        }
    }
    
    if (::anchorMinLabel != nullptr) {
        if (anchorDisplayActive) {
            // Gebruik dynamische max loss waarde
            float stopLossPrice = activeAnchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                formatQuotePriceEur(anchorMinLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, stopLossPrice);
                lv_label_set_text(::anchorMinLabel, anchorMinLabelBuffer);
                lastAnchorMinValue = stopLossPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMinLabelBuffer) > 0) {
                anchorMinLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorMinLabel, "");
                lastAnchorMinValue = -1.0f;
            }
        }
    }
    #else
    if (::anchorMaxLabel != nullptr) {
        if (anchorDisplayActive) {
            // Toon dynamische take profit waarde (effectief percentage)
            float takeProfitPrice = activeAnchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                char tpStr[32];
                formatQuotePriceEur(tpStr, sizeof(tpStr), takeProfitPrice);
                snprintf(anchorMaxLabelBuffer, ANCHOR_MAX_LABEL_BUFFER_SIZE, "+%.2f%% %s",
                         effAnchorUI.takeProfitPct, tpStr);
                lv_label_set_text(::anchorMaxLabel, anchorMaxLabelBuffer);
                lastAnchorMaxValue = takeProfitPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMaxLabelBuffer) > 0) {
                anchorMaxLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorMaxLabel, "");
                lastAnchorMaxValue = -1.0f;
            }
        }
    }
    
    if (::anchorLabel != nullptr) {
        if (anchorDisplayActive && displayPrice > 0.0f) {
            float anchorPct = ((displayPrice - activeAnchorPrice) / activeAnchorPrice) * 100.0f;
            char adStr[32];
            formatQuotePriceEur(adStr, sizeof(adStr), activeAnchorPrice);
            static float s_lastAncPctLine = 1.0e10f;
            if (lastAnchorValue != activeAnchorPrice || fabsf(s_lastAncPctLine - anchorPct) > 0.0001f ||
                lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%c%.2f%% %s",
                         anchorPct >= 0 ? '+' : '-', fabsf(anchorPct), adStr);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = activeAnchorPrice;
                s_lastAncPctLine = anchorPct;
            }
        } else if (anchorDisplayActive) {
            if (lastAnchorValue != activeAnchorPrice || lastAnchorValue < 0.0f) {
                formatQuotePriceEur(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, activeAnchorPrice);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = activeAnchorPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorLabelBuffer) > 0) {
                anchorLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorLabel, "");
                lastAnchorValue = -1.0f;
            }
        }
    }
    
    if (::anchorMinLabel != nullptr) {
        if (anchorDisplayActive) {
            // Toon dynamische max loss waarde (effectief percentage)
            float stopLossPrice = activeAnchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                char slStr[32];
                formatQuotePriceEur(slStr, sizeof(slStr), stopLossPrice);
                snprintf(anchorMinLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f%% %s",
                         effAnchorUI.maxLossPct, slStr);
                lv_label_set_text(::anchorMinLabel, anchorMinLabelBuffer);
                lastAnchorMinValue = stopLossPrice;
            }
        } else {
            // Update alleen als label niet leeg is
            if (strlen(anchorMinLabelBuffer) > 0) {
                anchorMinLabelBuffer[0] = '\0';
                lv_label_set_text(::anchorMinLabel, "");
                lastAnchorMinValue = -1.0f;
            }
        }
    }
    #endif
    
    // Zorg dat border altijd zichtbaar is voor BTCEUR blok na update
    if (::priceBox[0] != nullptr) {
        lv_obj_set_style_border_width(::priceBox[0], 1, 0);
        lv_obj_set_style_border_color(::priceBox[0], lv_palette_main(LV_PALETTE_GREY), 0);
    }
}

// Fase 8.5.4: updateWarmStartStatusLabel() naar Module
// Grafiekbadge: 30m-live-status (hasRet30mLive / minuutbuffer), niet de algemene warmStartStatus-enum
void UIController::updateWarmStartStatusLabel()
{
    // Fase 8.5.4: Gebruik globale pointer (synchroniseert met module pointer)
    if (::warmStartStatusLabel == nullptr) return;
    
    char warmStartText[16];
    lv_color_t statusColor;
    if (hasRet30mLive) {
        snprintf(warmStartText, sizeof(warmStartText), "LIVE");
        // LCDwiki: LIVE geel tot 2h live; daarna groen. JC3248: geel tot 7d live; daarna groen. Overige boards: groen.
#if defined(PLATFORM_ESP32S3_LCDWIKI_28)
        if (hasRet2hLive) {
            statusColor = lv_palette_main(LV_PALETTE_GREEN);
        } else {
            statusColor = lv_palette_main(LV_PALETTE_YELLOW);
        }
#elif defined(PLATFORM_ESP32S3_JC3248W535)
        if (hasRet7dLive) {
            statusColor = lv_palette_main(LV_PALETTE_GREEN);
        } else {
            statusColor = lv_palette_main(LV_PALETTE_YELLOW);
        }
#else
        statusColor = lv_palette_main(LV_PALETTE_GREEN);
#endif
    } else {
        uint8_t availableMinutes = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
        if (availableMinutes < 30) {
            unsigned int progress = (unsigned int)availableMinutes * 100U / 30U;
            if (progress > 99U) {
                progress = 99U;
            }
            snprintf(warmStartText, sizeof(warmStartText), "DATA%u%%", progress);
            statusColor = lv_palette_main(LV_PALETTE_ORANGE);
        } else {
            uint8_t livePct30 = calcLivePctMinuteAverages(30);
            snprintf(warmStartText, sizeof(warmStartText), "DATA%u%%", (unsigned)livePct30);
            statusColor = lv_palette_main(LV_PALETTE_ORANGE);
        }
    }
    lv_label_set_text(::warmStartStatusLabel, warmStartText);
    lv_obj_set_style_text_color(::warmStartStatusLabel, statusColor, 0);
}

#if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
// Min/max live-merge UI: invariant-warnings (throttled; onafhankelijk van DEBUG_UI_TIMEFRAME_MINMAX)
static constexpr uint32_t kUiTfInvWarnMs = 30000;
static uint32_t s_uiTfLastInvWarnMs[7] = {0};

static void uiTfMaybeWarnInvariant(uint8_t cardIdx, const char* tfTag,
    float rawMin, float rawMax, float curPrice, bool dataOk) {
    if (cardIdx >= 7) return;
    uint32_t now = millis();
    if (now - s_uiTfLastInvWarnMs[cardIdx] < kUiTfInvWarnMs) return;
    if (!dataOk) {
        s_uiTfLastInvWarnMs[cardIdx] = now;
        Serial.printf("[UI][minmax] WARN %s data invalid/empty\n", tfTag);
        return;
    }
    if (curPrice > 0.0f && rawMin > 0.0f && rawMax > 0.0f && rawMax >= rawMin) {
        if (curPrice < rawMin || curPrice > rawMax) {
            s_uiTfLastInvWarnMs[cardIdx] = now;
            Serial.printf("[UI][minmax] WARN %s cur=%.2f outside raw [%.2f,%.2f] (before live-merge)\n",
                          tfTag, curPrice, rawMin, rawMax);
        }
    }
}

#if DEBUG_UI_TIMEFRAME_MINMAX
static constexpr uint32_t kUiTfDbgLogMs = 3000;
static uint32_t s_uiTfLastDbgMs[7] = {0};

static void uiTfDebugLog(uint8_t cardIdx, const char* tfTag, const char* src, const char* readiness,
    float rawMin, float rawMax, float curPrice, float finMin, float finMax, int sampleCount,
    unsigned long liveAgeMs, uint8_t lkpSrc, int mergeSkippedStale) {
    if (cardIdx >= 7) return;
    uint32_t now = millis();
    if (now - s_uiTfLastDbgMs[cardIdx] < kUiTfDbgLogMs) return;
    s_uiTfLastDbgMs[cardIdx] = now;
    Serial.printf("[UI][tfmin] %s src=%s rd=%s raw=%.0f/%.0f cur=%.0f fin=%.0f/%.0f n=%d ageMs=%lu lkp=%u skip=%d\n",
                  tfTag, src, readiness, rawMin, rawMax, curPrice, finMin, finMax, sampleCount,
                  (unsigned long)liveAgeMs, (unsigned)lkpSrc, mergeSkippedStale);
}
#endif
#endif

#if UI_HAS_TF_MINMAX_STATUS_UI
enum : uint8_t {
    UI_TF_SRC_NONE = 0,
    UI_TF_SRC_LIVE = 1,
    UI_TF_SRC_WARM = 2,
    UI_TF_SRC_MIX = 3
};
#if defined(PLATFORM_ESP32S3_JC3248W535)
static const uint8_t kNestedTfChain[] = {1, 4, 2, 3, 5, 6};
#elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
static const uint8_t kNestedTfChain[] = {1, 2, 3};
#elif defined(PLATFORM_ESP32S3_GEEK)
static const uint8_t kNestedTfChain[] = {1, 2};
#endif

static uint8_t uiClassifyLivePct(uint8_t lp)
{
    if (lp >= 80U) {
        return UI_TF_SRC_LIVE;
    }
    if (lp == 0U) {
        return UI_TF_SRC_WARM;
    }
    return UI_TF_SRC_MIX;
}
#if defined(PLATFORM_ESP32S3_JC3248W535)
// 1d/7d: bij min/max uit hourly buffer — % SOURCE_LIVE in laatste 24/168 uur (hourlyAveragesSource); warmStart-only → WARM
static uint8_t uiClassify1dFromHourly(void)
{
    if (g_uiLastMinMaxSource1d == 1U) {
        return uiClassifyLivePct(calcLivePctHourlyLastN(24));
    }
    if (g_uiLastMinMaxSource1d == 2U) {
        return UI_TF_SRC_WARM;
    }
    return UI_TF_SRC_NONE;
}
static uint8_t uiClassify7dFromHourly(void)
{
    if (g_uiLastMinMaxSource7d == 1U) {
        return uiClassifyLivePct(calcLivePctHourlyLastN(168));
    }
    if (g_uiLastMinMaxSource7d == 2U) {
        return UI_TF_SRC_WARM;
    }
    return UI_TF_SRC_NONE;
}
#endif
// 30m/2h: zelfde bron als 1m/5m — % SOURCE_LIVE in minuutbuffer (niet hasRet*Warm, blijft true na warmstart)
static uint8_t uiClassify30mFromMinuteBuffer()
{
    return uiClassifyLivePct(calcLivePctMinuteAverages(30));
}
static uint8_t uiClassify2hFromMinuteBuffer()
{
    return uiClassifyLivePct(calcLivePctMinuteAverages(120));
}

static const char* uiTfSrcAbbr(uint8_t s)
{
    switch (s) {
        case UI_TF_SRC_LIVE:
            return "LIVE";
        case UI_TF_SRC_WARM:
            return "WARM";
        case UI_TF_SRC_MIX:
            return "MIX";
        default:
            return "--";
    }
}

// TF-kaarten: kleur van titelregel (symbool + %) volgens min/max-bron: wit=LIVE oranje=WARM geel=MIX
static void uiApplyTfSrcTitleColor(uint8_t idx)
{
    if (idx >= SYMBOL_COUNT || ::priceTitle[idx] == nullptr) {
        return;
    }
    lv_color_t c;
    switch (g_uiTfMinMaxSrc[idx]) {
        case UI_TF_SRC_LIVE:
            c = lv_color_white();
            break;
        case UI_TF_SRC_WARM:
            c = lv_palette_main(LV_PALETTE_ORANGE);
            break;
        case UI_TF_SRC_MIX:
            c = lv_palette_main(LV_PALETTE_YELLOW);
            break;
        default:
            c = lv_palette_main(LV_PALETTE_GREY);
            break;
    }
    lv_obj_set_style_text_color(::priceTitle[idx], c, 0);
}

#if defined(DEBUG_CALCULATIONS) || (DEBUG_UI_TIMEFRAME_MINMAX)
static uint32_t s_uiTfsrcLogMs = 0;
#endif

void uiFinalizeNestedTfMinMax(UIController* self)
{
    float cumMin = 0.0f;
    float cumMax = 0.0f;
    bool have = false;
    const size_t n = sizeof(kNestedTfChain) / sizeof(kNestedTfChain[0]);
    for (size_t k = 0; k < n; k++) {
        uint8_t idx = kNestedTfChain[k];
        if (!g_uiTfRawValid[idx]) {
            continue;
        }
        float rmin = g_uiTfRawMin[idx];
        float rmax = g_uiTfRawMax[idx];
        if (!have) {
            cumMin = rmin;
            cumMax = rmax;
            have = true;
        } else {
            if (rmin < cumMin) {
                cumMin = rmin;
            }
            if (rmax > cumMax) {
                cumMax = rmax;
            }
        }
        const float finMin = cumMin;
        const float finMax = cumMax;
        const float eps = 0.5f;
        // Nested chain: fin* is cumulatief — breder dan raw voor deze TF is bedoeld; geen productie-WARN.
#if DEBUG_UI_TIMEFRAME_MINMAX
        if ((finMin < rmin - eps || finMax > rmax + eps) && finMin > 0.0f && finMax > 0.0f) {
            static uint32_t s_uiNestDbgMs = 0;
            uint32_t now = millis();
            if (now - s_uiNestDbgMs >= 30000UL) {
                s_uiNestDbgMs = now;
                Serial.printf(
                    F("[MINMAX][DBG] nested expand TF%u: raw [%.0f,%.0f] -> display [%.0f,%.0f] (cumulative chain)\n"),
                    (unsigned)idx, (double)rmin, (double)rmax, (double)finMin, (double)finMax);
            }
        }
#endif
        float diff = (finMin > 0.0f && finMax > 0.0f) ? (finMax - finMin) : 0.0f;
        switch (idx) {
            case 1:
                self->updateMinMaxDiffLabels(::price1MinMaxLabel, ::price1MinMinLabel, ::price1MinDiffLabel,
                                             price1MinMaxLabelBuffer, price1MinMinLabelBuffer, price1MinDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice1MinMaxValue, lastPrice1MinMinValue, lastPrice1MinDiffValue);
                break;
            case 2:
                self->updateMinMaxDiffLabels(::price30MinMaxLabel, ::price30MinMinLabel, ::price30MinDiffLabel,
                                             price30MinMaxLabelBuffer, price30MinMinLabelBuffer, price30MinDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice30MinMaxValue, lastPrice30MinMinValue, lastPrice30MinDiffValue);
                break;
#if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
            case 3:
                self->updateMinMaxDiffLabels(::price2HMaxLabel, ::price2HMinLabel, ::price2HDiffLabel,
                                             price2HMaxLabelBuffer, price2HMinLabelBuffer, price2HDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice2HMaxValue, lastPrice2HMinValue, lastPrice2HDiffValue);
                break;
#endif
#if defined(PLATFORM_ESP32S3_JC3248W535)
            case 4:
                self->updateMinMaxDiffLabels(::price5mMaxLabel, ::price5mMinLabel, ::price5mDiffLabel,
                                             price5mMaxLabelBuffer, price5mMinLabelBuffer, price5mDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice5mMaxValue, lastPrice5mMinValue, lastPrice5mDiffValue);
                break;
            case 5:
                self->updateMinMaxDiffLabels(::price1dMaxLabel, ::price1dMinLabel, ::price1dDiffLabel,
                                             price1dMaxLabelBuffer, price1dMinLabelBuffer, price1dDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice1dMaxValue, lastPrice1dMinValue, lastPrice1dDiffValue);
                break;
            case 6:
                self->updateMinMaxDiffLabels(::price7dMaxLabel, ::price7dMinLabel, ::price7dDiffLabel,
                                             price7dMaxLabelBuffer, price7dMinLabelBuffer, price7dDiffLabelBuffer,
                                             finMax, finMin, diff,
                                             lastPrice7dMaxValue, lastPrice7dMinValue, lastPrice7dDiffValue);
                break;
#endif
            default:
                break;
        }
    }
#if defined(DEBUG_CALCULATIONS) || (DEBUG_UI_TIMEFRAME_MINMAX)
    {
        uint32_t now = millis();
        if (now - s_uiTfsrcLogMs >= 30000UL) {
            s_uiTfsrcLogMs = now;
#if defined(PLATFORM_ESP32S3_JC3248W535)
            Serial.printf(
                F("[TFSRC] 1m=%s 5m=%s 30m=%s 2h=%s 1d=%s 7d=%s\n"),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[1]), uiTfSrcAbbr(g_uiTfMinMaxSrc[4]),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[2]), uiTfSrcAbbr(g_uiTfMinMaxSrc[3]),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[5]), uiTfSrcAbbr(g_uiTfMinMaxSrc[6]));
#elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
            Serial.printf(
                F("[TFSRC] 1m=%s 30m=%s 2h=%s\n"),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[1]), uiTfSrcAbbr(g_uiTfMinMaxSrc[2]),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[3]));
#elif defined(PLATFORM_ESP32S3_GEEK)
            Serial.printf(
                F("[TFSRC] 1m=%s 30m=%s\n"),
                uiTfSrcAbbr(g_uiTfMinMaxSrc[1]), uiTfSrcAbbr(g_uiTfMinMaxSrc[2]));
#endif
        }
    }
#endif
    for (uint8_t di = 1; di < SYMBOL_COUNT; di++) {
        uiApplyTfSrcTitleColor(di);
    }
}
#endif

// Fase 8.6.2: updateAveragePriceCard() naar Module
// Helper functie om average price cards (1min/30min) bij te werken
void UIController::updateAveragePriceCard(uint8_t index)
{
    // Fase 8.6.2: Gebruik globale pointers (synchroniseert met module pointers)
    float pct = prices[index];
    float currentPrice = (lastFetchedPrice > 0.0f) ? lastFetchedPrice : prices[0];
    // Versheid live koers: lastFetchedPrice wordt samen met latestKnownPriceMs gezet; fallback prices[0] ~ lastApiMs
    const unsigned long uiLiveRefMs = (lastFetchedPrice > 0.0f)
        ? latestKnownPriceMs
        : (unsigned long)lastApiMs;
    const unsigned long uiLiveAgeMs = (uiLiveRefMs > 0UL)
        ? (millis() - uiLiveRefMs)
        : 0UL;
    const bool uiLiveStale = (UI_APPLY_LIVE_MINMAX_MAX_STALE_MS > 0UL) &&
        (uiLiveRefMs == 0UL || uiLiveAgeMs > UI_APPLY_LIVE_MINMAX_MAX_STALE_MS);
#if DEBUG_UI_TIMEFRAME_MINMAX
    static uint32_t s_uiTfLiveMetaLogMs = 0;
    {
        uint32_t nowMeta = millis();
        if (nowMeta - s_uiTfLiveMetaLogMs >= 3000) {
            s_uiTfLiveMetaLogMs = nowMeta;
            Serial.printf("[UI][tfmin] live meta ageMs=%lu refMs=%lu LKP=%u stale=%d maxStaleMs=%lu\n",
                          (unsigned long)uiLiveAgeMs, (unsigned long)uiLiveRefMs,
                          (unsigned)latestKnownPriceSource, uiLiveStale ? 1 : 0,
                          (unsigned long)UI_APPLY_LIVE_MINMAX_MAX_STALE_MS);
        }
    }
#endif
    auto applyLiveMinMax = [&](float &minVal, float &maxVal) {
        if (uiLiveStale) {
            static uint32_t s_lastStaleMergeWarnMs = 0;
            uint32_t now = millis();
            if (now - s_lastStaleMergeWarnMs >= 30000UL) {
                s_lastStaleMergeWarnMs = now;
                Serial.printf("[UI][minmax] WARN merge skipped: stale live price ageMs=%lu (maxStaleMs=%lu refMs=%lu)\n",
                              (unsigned long)uiLiveAgeMs,
                              (unsigned long)UI_APPLY_LIVE_MINMAX_MAX_STALE_MS,
                              (unsigned long)uiLiveRefMs);
            }
            return;
        }
        if (currentPrice > 0.0f) {
            if (minVal <= 0.0f || currentPrice < minVal) minVal = currentPrice;
            if (maxVal <= 0.0f || currentPrice > maxVal) maxVal = currentPrice;
        }
    };
    // 1m heeft 30 samples nodig bij 2000ms interval
    bool hasData1m = (index == 1) ? (secondArrayFilled || secondIndex >= 30) : true;
    // Voor 30m box: gebruik hasRet30m (inclusief warm-start) OF 30+ minuten live data
    bool hasData30m = (index == 2) ? (hasRet30m || (minuteArrayFilled || minuteIndex >= 30)) : true;
    #if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
    // Voor 2h box: gebruik warm-start data OF live data (minuteIndex >= 2 voor minimal, >= 120 voor volledig)
    bool hasData2h = (index == 3) ? (hasRet2h || (minuteArrayFilled || minuteIndex >= 120)) : true;
    bool hasData2hMinimal = (index == 3) ? (hasRet2h || (minuteArrayFilled || minuteIndex >= 2)) : true;  // Warm-start OF minimaal 2 minuten live data
#if defined(PLATFORM_ESP32S3_JC3248W535)
    bool hasData = (index == 1) ? hasData1m :
                   (index == 2) ? hasData30m :
                   (index == 3) ? hasData2hMinimal :
                   (index == 4) ? uiFiveMinuteHasMinimalData() :
                   (index == 5) ? hasRet1d :
                   (index == 6) ? hasRet7d : true;
#else
    bool hasData = (index == 1) ? hasData1m :
                   (index == 2) ? hasData30m :
                   (index == 3) ? hasData2hMinimal :
                   true;
#endif
    
    // Debug voor 2h box: alleen loggen wanneer waarde verandert
    if (index == 3) {
        #if !DEBUG_BUTTON_ONLY
        static float lastLoggedPct2h = -999.0f;
        static bool lastLoggedHasData2h = false;
        bool shouldShowPct = (index == 3) ? (hasData2hMinimal) : (hasData && pct != 0.0f);
        // Log alleen als waarde of hasData status verandert
        if (fabsf(pct - lastLoggedPct2h) > 0.001f || hasData2hMinimal != lastLoggedHasData2h) {
            Serial.printf("[UI] 2h box: hasData=%d, hasData2hMinimal=%d, pct=%.4f, prices[3]=%.4f, shouldShowPct=%d\n", 
                          hasData, hasData2hMinimal, pct, prices[3], shouldShowPct);
            lastLoggedPct2h = pct;
            lastLoggedHasData2h = hasData2hMinimal;
        }
        #endif
    }
    #else
    bool hasData = (index == 1) ? hasData1m : ((index == 2) ? hasData30m : true);
    #endif
    
    if (!hasData) {
        pct = 0.0f;
    }

    
    if (::priceTitle[index] != nullptr) {
        #if defined(PLATFORM_ESP32S3_JC3248W535)
        bool shouldShowPct = (index == 4) ? uiFiveMinuteHasMinimalData() :
                             (index == 5) ? hasRet1d :
                             (index == 6) ? hasRet7d :
                             (index == 3) ? (hasData2hMinimal) :
                             (index == 2) ? (hasData30m) :
                             (hasData1m);
        if (shouldShowPct) {
        #elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
        // 4-symbol boards met 2h-kaart
        bool shouldShowPct = (index == 3) ? (hasData2hMinimal) :
                             (index == 2) ? (hasData30m) :
                             (hasData1m);
        if (shouldShowPct) {
        #else
        // 3-symbol: alleen 1m/30m
        bool shouldShowPct = (index == 2) ? (hasData30m) : (hasData1m);
        if (shouldShowPct) {
        #endif
            // Format nieuwe tekst
            char newText[32];  // Verkleind van 48 naar 32 bytes (max: "30 min  +12.34%" = ~20 chars)
            const char* label = symbols[index];
#if defined(PLATFORM_ESP32S3_JC3248W535)
            if (pct == 0.0f && (index == 3 || index == 2 || index == 4 || index == 5 || index == 6)) {
#else
            if (pct == 0.0f && (index == 3 || index == 2)) {
#endif
                // Voor 2h/30m/1d box: toon 0.00% als de return 0 is
                snprintf(newText, sizeof(newText), "%s  0.00%%", label);
            } else {
                snprintf(newText, sizeof(newText), "%s  %c%.2f%%", label, pct >= 0 ? '+' : '-', fabsf(pct));
            }
            // Update alleen als tekst veranderd is
            if (strcmp(lastPriceTitleText[index], newText) != 0) {
                strncpy(priceTitleBuffer[index], newText, sizeof(priceTitleBuffer[index]) - 1);
                priceTitleBuffer[index][sizeof(priceTitleBuffer[index]) - 1] = '\0';
                strncpy(lastPriceTitleText[index], newText, sizeof(lastPriceTitleText[index]) - 1);
                lastPriceTitleText[index][sizeof(lastPriceTitleText[index]) - 1] = '\0';
                lv_label_set_text(::priceTitle[index], priceTitleBuffer[index]);
            }
        } else {
            // Update alleen als tekst veranderd is
            if (strcmp(lastPriceTitleText[index], symbols[index]) != 0) {
                strncpy(priceTitleBuffer[index], symbols[index], sizeof(priceTitleBuffer[index]) - 1);
                priceTitleBuffer[index][sizeof(priceTitleBuffer[index]) - 1] = '\0';
                strncpy(lastPriceTitleText[index], symbols[index], sizeof(lastPriceTitleText[index]) - 1);
                lastPriceTitleText[index][sizeof(lastPriceTitleText[index]) - 1] = '\0';
                lv_label_set_text(::priceTitle[index], priceTitleBuffer[index]);
            }
        }
    }
    
    if (index == 1 && ::price1MinMaxLabel != nullptr && ::price1MinMinLabel != nullptr && ::price1MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInSecondPrices(minVal, maxVal);
        applyLiveMinMax(minVal, maxVal);
        
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
        updateMinMaxDiffLabels(::price1MinMaxLabel, ::price1MinMinLabel, ::price1MinDiffLabel,
                              price1MinMaxLabelBuffer, price1MinMinLabelBuffer, price1MinDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice1MinMaxValue, lastPrice1MinMinValue, lastPrice1MinDiffValue);
#if UI_HAS_TF_MINMAX_STATUS_UI
        g_uiTfRawMin[1] = minVal;
        g_uiTfRawMax[1] = maxVal;
        g_uiTfRawValid[1] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
        g_uiTfMinMaxSrc[1] = uiClassifyLivePct(calcLivePctSecondWindow());
#endif
    }
    
    if (index == 2 && ::price30MinMaxLabel != nullptr && ::price30MinMinLabel != nullptr && ::price30MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInLast30Minutes(minVal, maxVal);
        applyLiveMinMax(minVal, maxVal);
        
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
        updateMinMaxDiffLabels(::price30MinMaxLabel, ::price30MinMinLabel, ::price30MinDiffLabel,
                              price30MinMaxLabelBuffer, price30MinMinLabelBuffer, price30MinDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice30MinMaxValue, lastPrice30MinMinValue, lastPrice30MinDiffValue);
#if UI_HAS_TF_MINMAX_STATUS_UI
        g_uiTfRawMin[2] = minVal;
        g_uiTfRawMax[2] = maxVal;
        g_uiTfRawValid[2] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
        g_uiTfMinMaxSrc[2] = uiClassify30mFromMinuteBuffer();
#endif
    }
    
    #if defined(PLATFORM_ESP32S3_LCDWIKI_28) || defined(PLATFORM_ESP32S3_JC3248W535)
    if (index == 3 && ::price2HMaxLabel != nullptr && ::price2HMinLabel != nullptr && ::price2HDiffLabel != nullptr)
    {
        TwoHMetrics m = computeTwoHMetrics();
        bool ok = m.valid;
        if (!ok) {
            if (hasData2hMinimal) {
                uiTfMaybeWarnInvariant(3, "2h", 0.0f, 0.0f, currentPrice, false);
            }
        } else {
            averagePrices[3] = m.avg2h;
            float minVal = m.low2h;
            float maxVal = m.high2h;
            uiTfMaybeWarnInvariant(3, "2h", minVal, maxVal, currentPrice, true);
            #if DEBUG_UI_TIMEFRAME_MINMAX
            float rawMin2h = minVal;
            float rawMax2h = maxVal;
            const char* src2h = "minute";
            const char* rd2h = hasRet2hLive ? (hasRet2hWarm ? "mixed" : "live")
                                           : (hasRet2hWarm ? "warm" : "--");
            uint16_t win2h = minuteArrayFilled ? MINUTES_FOR_30MIN_CALC : minuteIndex;
            if (win2h > 120) {
                win2h = 120;
            }
            applyLiveMinMax(minVal, maxVal);
            uiTfDebugLog(3, "2h", src2h, rd2h, rawMin2h, rawMax2h, currentPrice, minVal, maxVal, (int)win2h,
                          uiLiveAgeMs, latestKnownPriceSource, uiLiveStale ? 1 : 0);
            #else
            applyLiveMinMax(minVal, maxVal);
            #endif
            float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
            updateMinMaxDiffLabels(::price2HMaxLabel, ::price2HMinLabel, ::price2HDiffLabel,
                                  price2HMaxLabelBuffer, price2HMinLabelBuffer, price2HDiffLabelBuffer,
                                  maxVal, minVal, diff,
                                  lastPrice2HMaxValue, lastPrice2HMinValue, lastPrice2HDiffValue);
#if UI_HAS_TF_MINMAX_STATUS_UI
            g_uiTfRawMin[3] = minVal;
            g_uiTfRawMax[3] = maxVal;
            g_uiTfRawValid[3] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
            g_uiTfMinMaxSrc[3] = uiClassify2hFromMinuteBuffer();
#endif
        }
    }
    #endif
    #if defined(PLATFORM_ESP32S3_JC3248W535)
    if (index == 4 && ::price5mMaxLabel != nullptr && ::price5mMinLabel != nullptr && ::price5mDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInFiveMinutePrices(minVal, maxVal);
        applyLiveMinMax(minVal, maxVal);
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        updateMinMaxDiffLabels(::price5mMaxLabel, ::price5mMinLabel, ::price5mDiffLabel,
                              price5mMaxLabelBuffer, price5mMinLabelBuffer, price5mDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice5mMaxValue, lastPrice5mMinValue, lastPrice5mDiffValue);
        g_uiTfRawMin[4] = minVal;
        g_uiTfRawMax[4] = maxVal;
        g_uiTfRawValid[4] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
        g_uiTfMinMaxSrc[4] = uiClassifyLivePct(livePct5m);
    }
    if (index == 5 && ::price1dMaxLabel != nullptr && ::price1dMinLabel != nullptr && ::price1dDiffLabel != nullptr)
    {
        if (hasRet1d) {
            float minVal, maxVal;
            findMinMaxInLast24Hours(minVal, maxVal);
            bool dataOk1d = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
            uiTfMaybeWarnInvariant(5, "1d", minVal, maxVal, currentPrice, dataOk1d);
            #if DEBUG_UI_TIMEFRAME_MINMAX
            float rawMin1d = minVal;
            float rawMax1d = maxVal;
            const char* src1d = (g_uiLastMinMaxSource1d == 1) ? "hourly"
                                : (g_uiLastMinMaxSource1d == 2) ? "warmStart" : "?";
            const char* rd1d = (g_uiLastMinMaxSource1d == 1) ? (hasRet1dWarm ? "mixed" : "live")
                                : (g_uiLastMinMaxSource1d == 2) ? "warm" : "?";
            int sc1d = (g_uiLastMinMaxSource1d == 1) ? 24 : (g_uiLastMinMaxSource1d == 2) ? 0 : -1;
            applyLiveMinMax(minVal, maxVal);
            uiTfDebugLog(5, "1d", src1d, rd1d, rawMin1d, rawMax1d, currentPrice, minVal, maxVal, sc1d,
                          uiLiveAgeMs, latestKnownPriceSource, uiLiveStale ? 1 : 0);
            #else
            applyLiveMinMax(minVal, maxVal);
            #endif
            float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
            updateMinMaxDiffLabels(::price1dMaxLabel, ::price1dMinLabel, ::price1dDiffLabel,
                                  price1dMaxLabelBuffer, price1dMinLabelBuffer, price1dDiffLabelBuffer,
                                  maxVal, minVal, diff,
                                  lastPrice1dMaxValue, lastPrice1dMinValue, lastPrice1dDiffValue);
            g_uiTfRawMin[5] = minVal;
            g_uiTfRawMax[5] = maxVal;
            g_uiTfRawValid[5] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
            g_uiTfMinMaxSrc[5] = uiClassify1dFromHourly();
        } else {
            lastPrice1dMaxValue = -1.0f;
            lastPrice1dMinValue = -1.0f;
            lastPrice1dDiffValue = -1.0f;
            strcpy(price1dMaxLabelBuffer, "--");
            strcpy(price1dMinLabelBuffer, "--");
            strcpy(price1dDiffLabelBuffer, "--");
            lv_label_set_text(::price1dMaxLabel, "--");
            lv_label_set_text(::price1dMinLabel, "--");
            lv_label_set_text(::price1dDiffLabel, "--");
        }
    }
    if (index == 6 && ::price7dMaxLabel != nullptr && ::price7dMinLabel != nullptr && ::price7dDiffLabel != nullptr)
    {
        if (hasRet7d) {
            float minVal, maxVal;
            findMinMaxInLast7Days(minVal, maxVal);
            bool dataOk7d = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
            uiTfMaybeWarnInvariant(6, "7d", minVal, maxVal, currentPrice, dataOk7d);
            #if DEBUG_UI_TIMEFRAME_MINMAX
            float rawMin7d = minVal;
            float rawMax7d = maxVal;
            const char* src7d = (g_uiLastMinMaxSource7d == 1) ? "hourly"
                                : (g_uiLastMinMaxSource7d == 2) ? "warmStart" : "?";
            const char* rd7d = (g_uiLastMinMaxSource7d == 1) ? (hasRet7dWarm ? "mixed" : "live")
                                : (g_uiLastMinMaxSource7d == 2) ? "warm" : "?";
            int sc7d = (g_uiLastMinMaxSource7d == 1) ? 168 : (g_uiLastMinMaxSource7d == 2) ? 0 : -1;
            applyLiveMinMax(minVal, maxVal);
            uiTfDebugLog(6, "7d", src7d, rd7d, rawMin7d, rawMax7d, currentPrice, minVal, maxVal, sc7d,
                          uiLiveAgeMs, latestKnownPriceSource, uiLiveStale ? 1 : 0);
            #else
            applyLiveMinMax(minVal, maxVal);
            #endif
            float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
            updateMinMaxDiffLabels(::price7dMaxLabel, ::price7dMinLabel, ::price7dDiffLabel,
                                  price7dMaxLabelBuffer, price7dMinLabelBuffer, price7dDiffLabelBuffer,
                                  maxVal, minVal, diff,
                                  lastPrice7dMaxValue, lastPrice7dMinValue, lastPrice7dDiffValue);
            g_uiTfRawMin[6] = minVal;
            g_uiTfRawMax[6] = maxVal;
            g_uiTfRawValid[6] = (minVal > 0.0f && maxVal > 0.0f && maxVal >= minVal);
            g_uiTfMinMaxSrc[6] = uiClassify7dFromHourly();
        } else {
            lastPrice7dMaxValue = -1.0f;
            lastPrice7dMinValue = -1.0f;
            lastPrice7dDiffValue = -1.0f;
            strcpy(price7dMaxLabelBuffer, "--");
            strcpy(price7dMinLabelBuffer, "--");
            strcpy(price7dDiffLabelBuffer, "--");
            lv_label_set_text(::price7dMaxLabel, "--");
            lv_label_set_text(::price7dMinLabel, "--");
            lv_label_set_text(::price7dDiffLabel, "--");
        }
    }
    #endif
    
    if (!hasData)
    {
        // Update alleen als label niet al "--" is
        if (lastPriceLblValueArray[index] >= 0.0f || strcmp(priceLblBufferArray[index], "--") != 0) {
            lv_label_set_text(::priceLbl[index], "--");
            strcpy(priceLblBufferArray[index], "--");
            lastPriceLblValueArray[index] = -1.0f;
            
            // FASE 7.2: UI Average label update verificatie logging
            #if DEBUG_CALCULATIONS
#if defined(PLATFORM_ESP32S3_JC3248W535)
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : ((index == 4) ? "5m" : ((index == 5) ? "1d" : ((index == 6) ? "7d" : "?")))));
#else
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : "?"));
#endif
            Serial.printf(F("[UI][Average] %s label set to '--' (no data)\n"), timeframe);
            #endif
        }
    }
    else if (averagePrices[index] > 0.0f)
    {
        char avgFmt[32];
        formatQuotePriceEur(avgFmt, sizeof(avgFmt), averagePrices[index]);
        if (strcmp(avgFmt, priceLblBufferArray[index]) != 0 || lastPriceLblValueArray[index] < 0.0f) {
            strncpy(priceLblBufferArray[index], avgFmt, sizeof(priceLblBufferArray[index]));
            priceLblBufferArray[index][sizeof(priceLblBufferArray[index]) - 1] = '\0';
            lv_label_set_text(::priceLbl[index], priceLblBufferArray[index]);
            lastPriceLblValueArray[index] = averagePrices[index];
            
            // FASE 7.2: UI Average label update verificatie logging
            #if DEBUG_CALCULATIONS
#if defined(PLATFORM_ESP32S3_JC3248W535)
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : ((index == 4) ? "5m" : ((index == 5) ? "1d" : ((index == 6) ? "7d" : "?")))));
#else
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : "?"));
#endif
            Serial.printf(F("[UI][Average] %s label updated: %s\n"), timeframe, priceLblBufferArray[index]);
            #endif
        }
    }
    else
    {
        // averagePrices[index] is 0.0f of niet gezet
        // Update alleen als label niet al "--" is
        if (lastPriceLblValueArray[index] >= 0.0f || strcmp(priceLblBufferArray[index], "--") != 0) {
            lv_label_set_text(::priceLbl[index], "--");
            strcpy(priceLblBufferArray[index], "--");
            lastPriceLblValueArray[index] = -1.0f;
            
            // FASE 7.2: UI Average label update verificatie logging
            #if DEBUG_CALCULATIONS
#if defined(PLATFORM_ESP32S3_JC3248W535)
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : ((index == 4) ? "5m" : ((index == 5) ? "1d" : ((index == 6) ? "7d" : "?")))));
#else
            const char* timeframe = (index == 1) ? "1m" : ((index == 2) ? "30m" : ((index == 3) ? "2h" : "?"));
#endif
            Serial.printf(F("[UI][Average] %s label set to '--' (no data)\n"), timeframe);
            #endif
        }
    }
}

// Fase 8.6.3: updatePriceCardColor() naar Module
// Helper functie om price card kleuren bij te werken
void UIController::updatePriceCardColor(uint8_t index, float pct)
{
    // BTCEUR (index 0) heeft altijd blauwe tekstkleur, skip deze functie
    if (index == 0) {
        return;
    }
    
    // Fase 8.6.3: Gebruik globale pointers (synchroniseert met module pointers)
#if defined(PLATFORM_ESP32S3_JC3248W535)
    bool hasDataForColor = (index == 1) ? secondArrayFilled :
                           (index == 2) ? (minuteArrayFilled || minuteIndex >= 30) :
                           (index == 3) ? (hasRet2h || (minuteArrayFilled || minuteIndex >= 2)) :
                           (index == 4) ? uiFiveMinuteHasMinimalData() :
                           (index == 5) ? hasRet1d :
                           (index == 6) ? hasRet7d :
                           false;
    bool shouldShowColor = (index == 3 || index == 4 || index == 5 || index == 6) ? (hasDataForColor) : (hasDataForColor && pct != 0.0f);
#elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
    bool hasDataForColor = (index == 1) ? secondArrayFilled :
                           (index == 2) ? (minuteArrayFilled || minuteIndex >= 30) :
                           (index == 3) ? (hasRet2h || (minuteArrayFilled || minuteIndex >= 2)) :
                           false;
    bool shouldShowColor = (index == 3) ? (hasDataForColor) : (hasDataForColor && pct != 0.0f);
#else
    bool hasDataForColor = (index == 1) ? secondArrayFilled : (minuteArrayFilled || minuteIndex >= 30);
    bool shouldShowColor = hasDataForColor && pct != 0.0f;
#endif
    
    if (shouldShowColor)
    {
        // ~0.00% return: data wel geldig, maar visueel neutraal (geen groen bij exacte nul)
        static const float kFlatReturnPctEps = 0.005f;
        const bool isFlatReturn = (fabsf(pct) < kFlatReturnPctEps);
        if (isFlatReturn) {
            lv_obj_set_style_text_color(::priceLbl[index], lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_bg_color(::priceBox[index], lv_color_black(), 0);
        } else if (pct > 0.0f) {
            lv_obj_set_style_text_color(::priceLbl[index], lv_palette_lighten(LV_PALETTE_GREEN, 4), 0);
            lv_color_t bg = lv_color_mix(lv_palette_main(LV_PALETTE_GREEN), lv_color_black(), 127);
            lv_obj_set_style_bg_color(::priceBox[index], bg, 0);
        } else {
            lv_obj_set_style_text_color(::priceLbl[index], lv_palette_lighten(LV_PALETTE_RED, 3), 0);
            lv_color_t bg = lv_color_mix(lv_palette_main(LV_PALETTE_RED), lv_color_black(), 127);
            lv_obj_set_style_bg_color(::priceBox[index], bg, 0);
        }
    }
    else
    {
        lv_obj_set_style_text_color(::priceLbl[index], lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_bg_color(::priceBox[index], lv_color_black(), 0);
    }
    
    lv_obj_set_height(::priceBox[index], LV_SIZE_CONTENT);
}

// Fase 8.7.1: updateChartSection() naar Module
// Helper functie om chart section bij te werken
void UIController::updateChartSection(int32_t currentPrice, bool hasNewPriceData, float refPriceEur)
{
    // Chartserie: effectieve kleur bij symbool- of chart-instellingwijziging; quote-accent alleen bij symboolwijziging
    static char lastBitvavoSymbol[24] = {0};
    static char lastChartMode[8] = {0};
    static char lastChartManual[16] = {0};
    const bool symbolChanged = (strncmp(bitvavoSymbol, lastBitvavoSymbol, sizeof(lastBitvavoSymbol)) != 0);
    const bool chartModeChanged = (strncmp(chartColorMode, lastChartMode, sizeof(lastChartMode)) != 0);
    const bool chartManualChanged = (strncmp(chartColorManual, lastChartManual, sizeof(lastChartManual)) != 0);
    if (symbolChanged || chartModeChanged || chartManualChanged) {
        lv_chart_set_series_color(::chart, ::dataSeries, getChartSeriesLineColor());
        lv_obj_invalidate(::chart);
    }
    if (symbolChanged) {
        applyChartHeaderFooterColors(getQuoteAccentColor());
        applyBtcEurBoxColors(getQuoteAccentColor());
        setBtcTitleLabel();
    }
    if (symbolChanged) {
        safeStrncpy(lastBitvavoSymbol, bitvavoSymbol, sizeof(lastBitvavoSymbol));
    }
    if (chartModeChanged) {
        safeStrncpy(lastChartMode, chartColorMode, sizeof(lastChartMode));
    }
    if (chartManualChanged) {
        safeStrncpy(lastChartManual, chartColorManual, sizeof(lastChartManual));
    }

    // Fase 8.7.1: Gebruik globale pointers (synchroniseert met module pointers)
    // Voeg een punt toe aan de grafiek als er geldige data is (currentPrice volgt live snapshot in updateUI)
    if (currentPrice > 0) {
        // Track laatste chart waarde om conditional invalidate te doen
        static int32_t lastChartValue = 0;
        bool valueChanged = (currentPrice != lastChartValue);
        
        lv_chart_set_next_value(::chart, ::dataSeries, currentPrice);
        
        // Conditional invalidate: alleen als waarde is veranderd of er nieuwe data is
        if (valueChanged || hasNewPriceData || newPriceDataAvailable) {
            lv_obj_invalidate(::chart);
            lastChartValue = currentPrice;
        }
        
        // Reset flag na gebruik
        newPriceDataAvailable = false;
    }
    
    // Update chart range (refPriceEur bepaalt schaal / halfRange, los van Y-waarde)
    this->updateChartRange(currentPrice, refPriceEur);
    
    // Update chart title (device-id in titel waar van toepassing)
    if (::chartTitle != nullptr) {
        char deviceIdBuffer[16] = {0};
        const char* alertPos = strstr(ntfyTopic, "-alert");
        if (alertPos != nullptr) {
            size_t len = alertPos - ntfyTopic;
            if (len > 0 && len < sizeof(deviceIdBuffer)) {
                safeStrncpy(deviceIdBuffer, ntfyTopic, len + 1);
            } else {
                safeStrncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer));
            }
        } else {
            safeStrncpy(deviceIdBuffer, ntfyTopic, sizeof(deviceIdBuffer));
        }
        lv_label_set_text(::chartTitle, deviceIdBuffer);
    }
    
    // Update chart begin letters label (compacte header layouts)
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    if (::chartBeginLettersLabel != nullptr) {
        char deviceIdBuffer[16];
        getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
        lv_label_set_text(::chartBeginLettersLabel, deviceIdBuffer);
    }
    #endif
}

// Fase 8.8.1: updateUI() naar Module
// Main UI update functie
void UIController::updateUI()
{
    // Fase 8.8.1: Gebruik globale pointers (synchroniseert met module pointers)
    // Veiligheid: controleer of chart en dataSeries bestaan
    if (::chart == nullptr || ::dataSeries == nullptr) {
        // Serial_println is een macro, gebruik Serial.println direct
        #if !DEBUG_BUTTON_ONLY
        Serial.println(F("[UI] WARN: Chart of dataSeries is null, skip update"));
        #endif
        return;
    }
    
    // Bovenste grafiek: live latestKnownPrice onder mutex; fallback prices[symbolIndexToChart] (zelfde cadans als priceRepeatTask)
    float chartPriceFloat = prices[symbolIndexToChart];
    if (dataMutex != nullptr && safeMutexTake(dataMutex, pdMS_TO_TICKS(100), "UI chart snapshot")) {
        float lk = latestKnownPrice;
        float px = prices[symbolIndexToChart];
        safeMutexGive(dataMutex, "UI chart snapshot");
        chartPriceFloat = (lk > 0.0f) ? lk : px;
    }
    int32_t p = chartPriceEurToY(chartPriceFloat);
    
    // Bepaal of er nieuwe data is op basis van timestamp
    // Bij 2000ms interval + retries kan call tot ~3000ms duren, dus marge van 3000ms
    unsigned long currentTime = millis();
    bool hasNewPriceData = false;
    if (lastApiMs > 0) {
        unsigned long timeSinceLastApi = (currentTime >= lastApiMs) ? (currentTime - lastApiMs) : (ULONG_MAX - lastApiMs + currentTime);
        hasNewPriceData = (timeSinceLastApi < 3000);  // 2000ms interval + 1000ms marge voor retries
    }
    
    // Update UI sections (gebruik module versies)
    updateChartSection(p, hasNewPriceData, chartPriceFloat);
    updateHeaderSection();
    updatePriceCardsSection(hasNewPriceData);
    updateFooter();
}

// Fase 8.9.1: checkButton() naar Module
// Physical button check function (boards met HAS_PHYSICAL_BUTTON)
void UIController::checkButton()
{
    unsigned long now = millis();
    
    // Read button state (LOW = pressed, HIGH = not pressed due to INPUT_PULLUP)
    int buttonState = digitalRead(BUTTON_PIN);
    
    // Edge detection: detect HIGH -> LOW transition (button pressed)
    // Dit zorgt ervoor dat we alleen triggeren bij het indrukken, niet tijdens het ingedrukt houden
    if (buttonState == LOW && lastButtonState == HIGH && (now - lastButtonPress >= BUTTON_DEBOUNCE_MS)) {
        lastButtonPress = now;
        lastButtonState = buttonState; // Update state
        Serial.println("[Button] Physical reset button pressed - setting anchor price");
        
        // Execute reset and set anchor (thread-safe, same as MQTT callback)
        float currentPrice = 0.0f;
        
        // Als prices[0] nog 0 is, probeer eerst een prijs op te halen (alleen als WiFi verbonden is)
        if (WiFi.status() == WL_CONNECTED) {
            // Check of we al een prijs hebben, zo niet, haal er een op
            if (safeMutexTake(dataMutex, pdMS_TO_TICKS(500), "checkButton price check")) {
                if (prices[0] <= 0.0f) {
                    Serial.println("[Button] Prijs nog niet beschikbaar, haal prijs op...");
                    safeMutexGive(dataMutex, "checkButton price check");
                    // Haal prijs op (buiten mutex om deadlock te voorkomen)
                    fetchPrice();
                    // Wacht even zodat de prijs kan worden opgeslagen
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else {
                    safeMutexGive(dataMutex, "checkButton price available");
                }
            }
        }
        
        // Phase 1: Anchor set in apiTask context (queue; geen HTTPS in UI task)
        if (queueAnchorSetting(0.0f, true)) {
            this->updateUI();
        } else {
            Serial.println("[Button] WARN: Kon anchor niet in queue zetten");
        }
    } else {
        // Update lastButtonState voor edge detection
        lastButtonState = buttonState;
    }
}

// Fase 8.10.1: setupLVGL() naar Module
// LVGL display initialisatie
void UIController::setupLVGL()
{
    Serial.println("[LVGL] Active init path: UIController::setupLVGL()");
    // init LVGL
    lv_init();

    // Set a tick source so that LVGL will know how much time elapsed
    lv_tick_set_cb(millis_cb);

    // register print function for debugging
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    uint32_t screenWidth = g_displayBackend ? g_displayBackend->width() : 0;
    uint32_t screenHeight = g_displayBackend ? g_displayBackend->height() : 0;
    
    // Detecteer PSRAM beschikbaarheid
    bool psramAvailable = hasPSRAM();

    // JC3248W535 (QSPI + DMA): LVGL-buffer in SPIRAM geeft op ESP32-S3 vaak cache-coherentie-artefacten
    // (kleine gekleurde blokjes). Forceer INTERNAL+DMA voor de draw buffer.
#if defined(PLATFORM_ESP32S3_JC3248W535)
    const bool lvglDrawBufForceInternal = false;
#else
    const bool lvglDrawBufForceInternal = false;
#endif
    
    // Bepaal useDoubleBuffer: board-aware
    bool useDoubleBuffer;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
        // JC3248W535 testmodus: expliciet single buffer.
        useDoubleBuffer = false;
    #else
        // Fallback: double buffer alleen met PSRAM
        useDoubleBuffer = psramAvailable;
    #endif
    
    // Bepaal buffer lines per board
    uint32_t bufLines;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3 met PSRAM: 30 regels (of fallback kleiner als geen PSRAM)
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;  // ESP32-S3 zonder PSRAM: 2 regels
        }
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: 30 regels met PSRAM, 2 zonder
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;
        }
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
        // Forceer full-frame buffer (320x480) voor veilige AXS15231B/QSPI testmodus.
        bufLines = screenHeight;
    #else
        // Fallback
        bufLines = psramAvailable ? 30 : 2;
    #endif
    
    // ESP32-S3 met PSRAM (2MB): gebruik volledige frame-buffer als het scherm klein genoeg is
    #if defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
    if (psramAvailable) {
        const size_t fullFrameBytes = (size_t)screenWidth * screenHeight * sizeof(lv_color_t) * 2;  // double buffer
        if (fullFrameBytes <= 400000u) {
            bufLines = screenHeight;
        }
    }
    #endif

#if defined(PLATFORM_ESP32S3_JC3248W535)
    const size_t bytesPerPixel = sizeof(uint16_t); // hard force RGB565 pad voor deze boardtest
#else
    const size_t bytesPerPixel = sizeof(lv_color_t);
#endif

    uint32_t bufSize = screenWidth * bufLines;
    uint8_t numBuffers = useDoubleBuffer ? 2 : 1;  // 1 of 2 buffers afhankelijk van useDoubleBuffer
    size_t bufSizeBytes = (size_t)bufSize * bytesPerPixel * numBuffers;
    bool useFullFrame = (bufLines >= screenHeight);
    
    const char* bufferLocation;
    uint32_t freeHeapBefore = ESP.getFreeHeap();
    size_t largestFreeBlockBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Bepaal board naam voor logging
    const char* boardName;
    #if defined(PLATFORM_ESP32S3_SUPERMINI)
        boardName = "ESP32-S3";
    #elif defined(PLATFORM_ESP32S3_GEEK)
        boardName = "ESP32-S3 GEEK";
    #elif defined(PLATFORM_ESP32S3_LCDWIKI_28)
        boardName = "LCDWIKI28";
    #elif defined(PLATFORM_ESP32S3_JC3248W535)
        boardName = "JC3248W535";
    #elif defined(PLATFORM_ESP32S3_AMOLED_206)
        boardName = "AMOLED206";
    #else
        boardName = "Unknown";
    #endif
    
    // Alloceer buffer één keer bij init (niet herhaald)
    if (disp_draw_buf == nullptr) {
        if (psramAvailable && !lvglDrawBufForceInternal) {
            // Met PSRAM: probeer eerst SPIRAM allocatie
            disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (disp_draw_buf) {
                bufferLocation = "SPIRAM";
            } else {
                // Fallback naar INTERNAL+DMA als SPIRAM alloc faalt
                Serial.println("[LVGL] SPIRAM allocatie gefaald, valt terug op INTERNAL+DMA");
                bufferLocation = "INTERNAL+DMA (fallback)";
                disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
            }
        } else {
            // Zonder PSRAM of geforceerd intern (QSPI/DMA): INTERNAL+DMA
            bufferLocation = lvglDrawBufForceInternal ? "INTERNAL+DMA (QSPI/DMA-safe)" : "INTERNAL+DMA";
            disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSizeBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        
        if (!disp_draw_buf) {
            Serial.printf("[LVGL] FATAL: Draw buffer allocatie gefaald! Vereist: %u bytes\n", bufSizeBytes);
            Serial.printf("[LVGL] Free heap: %u bytes, Largest free block: %u bytes\n", 
                         ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            while (true) {
                /* no need to continue */
            }
        }
        
        disp_draw_buf_size = bufSizeBytes;
        
        // Uitgebreide logging bij boot
        uint32_t freeHeapAfter = ESP.getFreeHeap();
        size_t largestFreeBlockAfter = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        Serial.printf("[LVGL] Board: %s, Display: %ux%u pixels\n", boardName, screenWidth, screenHeight);
        Serial.printf("[LVGL] PSRAM: %s, useDoubleBuffer: %s, fullFrame: %s\n", 
                     psramAvailable ? "yes" : "no", useDoubleBuffer ? "true" : "false", useFullFrame ? "yes" : "no");
        Serial.printf("[LVGL] Draw buffer: %u lines, %u pixels, %u bytes (%u buffer%s)\n", 
                     (unsigned)bufLines, (unsigned)bufSize, (unsigned)bufSizeBytes, numBuffers, numBuffers == 1 ? "" : "s");
        Serial.printf("[LVGL] Draw buffer per buffer bytes: %u\n", (unsigned)((size_t)bufSize * bytesPerPixel));
        Serial.printf("[LVGL] Buffer bytes/pixel (configured): %u\n", (unsigned)bytesPerPixel);
        Serial.printf("[LVGL] Buffer locatie: %s\n", bufferLocation);
        Serial.printf("[LVGL] Heap: %u -> %u bytes free, Largest block: %u -> %u bytes\n",
                     freeHeapBefore, freeHeapAfter, largestFreeBlockBefore, largestFreeBlockAfter);
    } else {
        Serial.println(F("[LVGL] WARNING: Draw buffer al gealloceerd! (herhaalde allocatie voorkomen)"));
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    // LVGL buffer setup: single of double buffering, full of partial frame
    size_t bufSizePixels = bufSize;
    size_t bufSizeBytesPerBuffer = bufSizePixels * bytesPerPixel;
    // AXS15231B QSPI panels are sensitive to partial updates.
    // Force full refresh on JC3248W535CIY to avoid random block artifacts.
#if defined(PLATFORM_ESP32S3_JC3248W535)
    lv_display_render_mode_t renderMode = LV_DISPLAY_RENDER_MODE_FULL;
#else
    lv_display_render_mode_t renderMode = useFullFrame ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL;
#endif

#if defined(PLATFORM_ESP32S3_JC3248W535) && defined(LV_COLOR_FORMAT_RGB565)
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
#endif
    
    void *buf2 = nullptr;
    if (useDoubleBuffer) {
        buf2 = (uint8_t *)disp_draw_buf + bufSizeBytesPerBuffer;
    }
    lv_display_set_buffers(disp, disp_draw_buf, buf2, bufSizeBytesPerBuffer, renderMode);

    const char *renderModeStr = (renderMode == LV_DISPLAY_RENDER_MODE_FULL) ? "FULL" :
                                (renderMode == LV_DISPLAY_RENDER_MODE_PARTIAL) ? "PARTIAL" : "DIRECT";
    lv_color_format_t activeColorFormat = lv_display_get_color_format(disp);
    uint8_t activeBpp = lv_color_format_get_size(activeColorFormat);
    Serial.printf("[LVGL] Active render mode: %s\n", renderModeStr);
    Serial.printf("[LVGL] Active color format: %d, bytes/pixel: %u\n", (int)activeColorFormat, (unsigned)activeBpp);
    Serial.printf("[LVGL] Active bufLines: %u\n", (unsigned)bufLines);

#if defined(PLATFORM_ESP32S3_JC3248W535)
    if (activeColorFormat != LV_COLOR_FORMAT_RGB565 || activeBpp != 2) {
        Serial.printf("[LVGL][ERROR] JC3248 requires RGB565/2BPP but got fmt=%d bpp=%u\n",
                      (int)activeColorFormat, (unsigned)activeBpp);
    }
    if (renderMode != LV_DISPLAY_RENDER_MODE_FULL || !useFullFrame || bufLines != screenHeight) {
        Serial.printf("[LVGL][ERROR] JC3248 requires full-frame. mode=%s fullFrame=%s bufLines=%u screenHeight=%u\n",
                      renderModeStr, useFullFrame ? "yes" : "no", (unsigned)bufLines, (unsigned)screenHeight);
    }
#endif
}

// Helper: Update min/max/diff labels (geoptimaliseerd: elimineert code duplicatie)
void UIController::updateMinMaxDiffLabels(lv_obj_t* maxLabel, lv_obj_t* minLabel, lv_obj_t* diffLabel,
                                          char* maxBuffer, char* minBuffer, char* diffBuffer,
                                          float maxVal, float minVal, float diff,
                                          float& lastMaxValue, float& lastMinValue, float& lastDiffValue)
{
    if (minVal > 0.0f && maxVal > 0.0f) {
        if (lastMaxValue != maxVal || lastMaxValue < 0.0f) {
            formatQuotePriceEur(maxBuffer, 24, maxVal);
            lv_label_set_text(maxLabel, maxBuffer);
            lastMaxValue = maxVal;
            
            // FASE 7.1: UI Min/Max label update verificatie logging
            #if DEBUG_CALCULATIONS
            Serial.printf(F("[UI][MinMax] max label updated: %s\n"), maxBuffer);
            #endif
        }
        if (lastDiffValue != diff || lastDiffValue < 0.0f) {
            // Zelfde printf-tier als top/dal: referentie = maxVal (zelfde prijsorde als het bereik)
            formatQuotePriceEurAtReferenceTier(diffBuffer, 24, diff, maxVal);
            lv_label_set_text(diffLabel, diffBuffer);
            lastDiffValue = diff;
        }
        if (lastMinValue != minVal || lastMinValue < 0.0f) {
            formatQuotePriceEur(minBuffer, 24, minVal);
            lv_label_set_text(minLabel, minBuffer);
            lastMinValue = minVal;
            
            // FASE 7.1: UI Min/Max label update verificatie logging
            #if DEBUG_CALCULATIONS
            Serial.printf(F("[UI][MinMax] min label updated: %s\n"), minBuffer);
            #endif
        }
    } else {
        // Update alleen als labels niet "--" zijn
        if (strcmp(maxBuffer, "--") != 0) {
            strcpy(maxBuffer, "--");
            lv_label_set_text(maxLabel, "--");
            lastMaxValue = -1.0f;
            
            // FASE 7.1: UI Min/Max label update verificatie logging
            #if DEBUG_CALCULATIONS
            Serial.printf(F("[UI][MinMax] max label set to '--' (no data)\n"));
            #endif
        }
        if (strcmp(diffBuffer, "--") != 0) {
            strcpy(diffBuffer, "--");
            lv_label_set_text(diffLabel, "--");
            lastDiffValue = -1.0f;
        }
        if (strcmp(minBuffer, "--") != 0) {
            strcpy(minBuffer, "--");
            lv_label_set_text(minLabel, "--");
            lastMinValue = -1.0f;
            
            // FASE 7.1: UI Min/Max label update verificatie logging
            #if DEBUG_CALCULATIONS
            Serial.printf(F("[UI][MinMax] min label set to '--' (no data)\n"));
            #endif
        }
    }
}

// Fase 8.7.1: updateHeaderSection() naar Module
// Helper functie om header section bij te werken
void UIController::updateHeaderSection()
{
    // Fase 8.7.1: Gebruik module versies van update functies
    updateDateTimeLabels();
    updateTrendLabel();
    updateVolatilityLabel();
    updateVolumeConfirmLabel();
    updateMediumTrendLabel();
    updateLongTermTrendLabel();
    updateWarmStartStatusLabel();
}

// Fase 8.7.3: updatePriceCardsSection() naar Module
// Helper functie om price cards section bij te werken
void UIController::updatePriceCardsSection(bool hasNewPriceData)
{
#if UI_HAS_TF_MINMAX_STATUS_UI
    uiResetTfMinMaxSnapshot();
#endif
    // Fase 8.7.3: Gebruik module versies van update functies
    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i) {
        float pct = 0.0f;
        
        if (i == 0) {
            // BTCEUR card
            updateBTCEURCard(hasNewPriceData);
            pct = 0.0f; // BTCEUR heeft geen percentage voor kleur
        } else {
            // 1min/30min cards
            pct = prices[i];
            updateAveragePriceCard(i);
        }
        
        // Update kleuren
        updatePriceCardColor(i, pct);
    }
#if UI_HAS_TF_MINMAX_STATUS_UI
    uiFinalizeNestedTfMinMax(this);
#endif
}

// Fase 8.11.3: updateChartRange() verplaatst vanuit .ino naar UIController module
// Helper functie om chart range te berekenen en bij te werken
void UIController::updateChartRange(int32_t currentPrice, float refPriceEur)
{
    #ifndef POINTS_TO_CHART
    #define POINTS_TO_CHART 60      // Number of points on the chart (60 points = 2 minutes at 2000ms API interval)
    #endif

    const int32_t halfRange = chartHalfRangeY(refPriceEur, getChartPriceScale(refPriceEur));
    
    int32_t chartMin = INT32_MAX;
    int32_t chartMax = INT32_MIN;
    int32_t sum = 0;
    uint16_t count = 0;
    
    // Gebruik member pointers i.p.v. globale pointers
    int32_t *yArray = lv_chart_get_series_y_array(this->chart, this->dataSeries);
    
    for (uint16_t i = 0; i < POINTS_TO_CHART; i++)
    {
        int32_t val = yArray[i];
        if (val != LV_CHART_POINT_NONE)
        {
            if (val < chartMin) chartMin = val;
            if (val > chartMax) chartMax = val;
            sum += val;
            count++;
        }
    }
    
    int32_t chartAverage = 0;
    if (count > 0 && chartMin != INT32_MAX && chartMax != INT32_MIN)
    {
        chartAverage = sum / count;
        
        if (chartMin == INT32_MAX || chartMax == INT32_MIN || chartMin > chartMax)
        {
            chartMin = chartAverage - halfRange;
            chartMax = chartAverage + halfRange;
        }
        
        if (chartMin == chartMax)
        {
            int32_t minMargin = chartAverage / 100;
            if (minMargin < 10) minMargin = 10;
            chartMin = chartMin - minMargin;
            chartMax = chartMax + minMargin;
        }
        
        int32_t range = chartMax - chartMin;
        int32_t margin = range / 20;
        if (margin < 10) margin = 10;
        
        // Update globale minRange en maxRange (extern gedeclareerd)
        minRange = chartMin - margin;
        maxRange = chartMax + margin;
        
        if (currentPrice < minRange) minRange = currentPrice - margin;
        if (currentPrice > maxRange) maxRange = currentPrice + margin;
        
        if (minRange < 0) minRange = 0;
        if (maxRange < 0) maxRange = 0;
        if (minRange >= maxRange)
        {
            int32_t fallbackMargin = halfRange / 20;
            if (fallbackMargin < 10) fallbackMargin = 10;
            minRange = chartAverage - halfRange - fallbackMargin;
            maxRange = chartAverage + halfRange + fallbackMargin;
            if (minRange < 0) minRange = 0;
        }
    }
    else
    {
        chartAverage = currentPrice;
        int32_t margin = halfRange / 20;
        if (margin < 10) margin = 10;
        minRange = currentPrice - halfRange - margin;
        maxRange = currentPrice + halfRange + margin;
    }
    
    // Gebruik member pointer i.p.v. globale pointer
    lv_chart_set_range(this->chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);
}
