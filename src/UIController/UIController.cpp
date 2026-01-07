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

#include "UIController.h"
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
// Fase 8.5.2: updateTrendLabel() dependencies
#include "../TrendDetector/TrendDetector.h"  // Voor TrendState enum
// Fase 8.5.3: updateVolatilityLabel() dependencies
#include "../VolatilityTracker/VolatilityTracker.h"  // Voor VolatilityState enum
// Fase 8.6.x: volume/range UI indicators dependencies
#include "../AlertEngine/AlertEngine.h"  // Voor VolumeRangeStatus
// Fase 8.6.1: updateBTCEURCard() dependencies
#include "../AnchorSystem/AnchorSystem.h"  // Voor AnchorConfigEffective struct

// Platform-specifieke constants (gedefinieerd in platform_config.h, maar we includen niet om dubbele definitie van gfx/bus te voorkomen)
// Fase 8.3: createChart() dependencies - constants worden extern gebruikt
// Note: CHART_WIDTH, CHART_HEIGHT, CHART_ALIGN_Y, FONT_SIZE_* zijn gedefinieerd in platform_config.h
// We gebruiken ze via extern of via de .ino file die ze al heeft geïncludeerd
// Als fallback definiëren we default waarden
#ifndef CHART_WIDTH
#define CHART_WIDTH 240  // Default voor CYD (wordt overschreven door platform_config.h als het later wordt gedefinieerd)
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
#ifndef PRICE_RANGE
#define PRICE_RANGE 200  // Default value (wordt overschreven door .ino als het later wordt gedefinieerd)
#endif

// Forward declarations voor globale variabelen (worden gebruikt door callbacks)
extern void Serial_println(const char*);
extern void Serial_println(const __FlashStringHelper*);
extern Arduino_GFX* gfx;
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
extern uint32_t maxRange;
extern uint32_t minRange;
extern char ntfyTopic[];
extern void getDeviceIdFromTopic(const char* topic, char* buffer, size_t bufferSize);
// Fase 8.11.1: createFooter() dependencies (CYD platforms)
extern lv_obj_t *lblFooterLine1;
extern lv_obj_t *lblFooterLine2;
extern lv_obj_t *ramLabel;
extern void disableScroll(lv_obj_t *obj);
extern const char* symbols[];
// VERSION_STRING wordt gedefinieerd in platform_config.h (beschikbaar voor alle modules)
// Geen fallback nodig omdat platform_config.h altijd wordt geïncludeerd
extern void formatIPAddress(IPAddress ip, char* buffer, size_t bufferSize);
// Fase 8.11.1: createFooter() dependencies (CYD platforms)
extern lv_obj_t *lblFooterLine1;
extern lv_obj_t *lblFooterLine2;
extern lv_obj_t *ramLabel;

// Fase 8.5.2: updateTrendLabel() dependencies
extern TrendDetector trendDetector;
extern bool hasRet2h;
extern bool hasRet30m;
extern bool hasRet2hWarm;
extern bool hasRet30mWarm;
extern bool hasRet2hLive;
extern bool hasRet30mLive;
extern uint8_t language;
extern bool minuteArrayFilled;
extern uint8_t minuteIndex;
extern uint8_t calcLivePctMinuteAverages(uint16_t windowMinutes);
extern const char* getText(const char* nlText, const char* enText);
// MINUTES_FOR_30MIN_CALC is een #define, niet een variabele
#ifndef MINUTES_FOR_30MIN_CALC
#define MINUTES_FOR_30MIN_CALC 120  // Default (wordt overschreven door .ino)
#endif
// Fase 8.5.3: updateVolatilityLabel() dependencies
extern VolatilityTracker volatilityTracker;
// Fase 8.5.4: updateWarmStartStatusLabel() dependencies
#include "../WarmStart/WarmStart.h"  // Voor WarmStartStatus enum en WarmStartStats struct
extern WarmStartStatus warmStartStatus;
extern WarmStartStats warmStartStats;
extern lv_obj_t *warmStartStatusLabel;
// Fase 8.6.1: updateBTCEURCard() dependencies
#include "../AnchorSystem/AnchorSystem.h"  // Voor AnchorConfigEffective struct
extern TrendDetector trendDetector;
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
// Buffer sizes (gedefinieerd in .ino)
#define PRICE_LBL_BUFFER_SIZE 24
#define ANCHOR_LABEL_BUFFER_SIZE 24
extern char priceLblBuffer[PRICE_LBL_BUFFER_SIZE];
extern char anchorMaxLabelBuffer[ANCHOR_LABEL_BUFFER_SIZE];
extern char anchorLabelBuffer[ANCHOR_LABEL_BUFFER_SIZE];
extern char anchorMinLabelBuffer[ANCHOR_LABEL_BUFFER_SIZE];
// Fase 8.6.2: updateAveragePriceCard() dependencies
extern bool secondArrayFilled;
extern float averagePrices[];
extern void findMinMaxInSecondPrices(float &minVal, float &maxVal);
extern void findMinMaxInLast30Minutes(float &minVal, float &maxVal);
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
extern void findMinMaxInLast2Hours(float &minVal, float &maxVal);
#endif
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
extern char price1MinMaxLabelBuffer[20];
extern char price1MinMinLabelBuffer[20];
extern char price1MinDiffLabelBuffer[20];
extern char price30MinMaxLabelBuffer[20];
extern char price2HMaxLabelBuffer[20];
extern char price2HMinLabelBuffer[20];
extern char price2HDiffLabelBuffer[20];
extern char price30MinMinLabelBuffer[20];
extern char price30MinDiffLabelBuffer[32];
extern float lastPrice1MinMaxValue;
extern float lastPrice1MinMinValue;
extern float lastPrice1MinDiffValue;
extern float lastPrice30MinMaxValue;
extern float lastPrice30MinMinValue;
extern float lastPrice30MinDiffValue;
extern float lastPrice2HMaxValue;
extern float lastPrice2HMinValue;
extern float lastPrice2HDiffValue;
extern char lastPriceTitleText[SYMBOL_COUNT][32];  // Verkleind van 48 naar 32 bytes
extern char priceLblBufferArray[SYMBOL_COUNT][24];
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
extern lv_obj_t *lblFooterLine1;
extern lv_obj_t *lblFooterLine2;
extern lv_obj_t *ramLabel;
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
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    
    if (gfx != nullptr) {
        gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    }
    
    lv_disp_flush_ready(disp);
}

// Fase 8.3.1: createChart() verplaatst naar UIController module (parallel implementatie)
void UIController::createChart() {
    // Chart - gebruik platform-specifieke afmetingen
    chart = lv_chart_create(lv_scr_act());
    ::chart = chart;  // Fase 8.4.3: Synchroniseer met globale pointer voor backward compatibility
    lv_chart_set_point_count(chart, POINTS_TO_CHART);
    lv_obj_set_size(chart, CHART_WIDTH, CHART_HEIGHT);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, CHART_ALIGN_Y);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    disableScroll(chart);
    
    int32_t p = (int32_t)lroundf(openPrices[symbolIndexToChart] * 100.0f);
    maxRange = p + PRICE_RANGE;
    minRange = p - PRICE_RANGE;
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);

    // Maak één blauwe serie aan voor alle punten
    dataSeries = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
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
    #if !defined(PLATFORM_TTGO) && !defined(PLATFORM_ESP32S3_SUPERMINI) && !defined(PLATFORM_ESP32S3_GEEK)
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
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
    // TTGO/GEEK: Compacte layout met datum op regel 1, beginletters/versie/tijd op regel 2
    chartDateLabel = lv_label_create(lv_scr_act());
    ::chartDateLabel = chartDateLabel;  // Fase 8.4.3: Synchroniseer met globale pointer
    lv_obj_set_style_text_font(chartDateLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chartDateLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartDateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartDateLabel, "-- -- --");
    lv_obj_set_width(chartDateLabel, CHART_WIDTH);
    lv_obj_set_pos(chartDateLabel, 0, 0); // TTGO: originele positie (geen aanpassing nodig)
    
    chartBeginLettersLabel = lv_label_create(lv_scr_act());
    ::chartBeginLettersLabel = chartBeginLettersLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartBeginLettersLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chartBeginLettersLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(chartBeginLettersLabel, LV_TEXT_ALIGN_LEFT, 0);
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartBeginLettersLabel, deviceIdBuffer);
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
    // ESP32-S3: Ruimere layout met datum/tijd zoals CYD, maar met device ID links
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
    char deviceIdBuffer[16];
    getDeviceIdFromTopic(ntfyTopic, deviceIdBuffer, sizeof(deviceIdBuffer));
    lv_label_set_text(chartBeginLettersLabel, deviceIdBuffer);
    lv_obj_set_pos(chartBeginLettersLabel, 0, 2);
    
    chartTimeLabel = lv_label_create(lv_scr_act());
    ::chartTimeLabel = chartTimeLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(chartTimeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chartTimeLabel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(chartTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(chartTimeLabel, "--:--:--");
    lv_obj_set_width(chartTimeLabel, 240);
    lv_obj_set_pos(chartTimeLabel, 0, 4);
    #else
    // CYD: Ruimere layout met datum/tijd op verschillende posities
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
    for (uint8_t i = 0; i < SYMBOL_COUNT; ++i)
    {
        priceBox[i] = lv_obj_create(lv_scr_act());
        ::priceBox[i] = priceBox[i];  // Fase 8.4.3: Synchroniseer met globale pointer
        lv_obj_set_size(priceBox[i], LV_PCT(100), LV_SIZE_CONTENT);

        if (i == 0) {
            lv_obj_align(priceBox[i], LV_ALIGN_TOP_LEFT, 0, PRICE_BOX_Y_START);
        }
        else {
            lv_obj_align_to(priceBox[i], priceBox[i - 1], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
        }

    lv_obj_set_style_radius(priceBox[i], 6, 0);
    lv_obj_set_style_pad_all(priceBox[i], 4, 0);
    lv_obj_set_style_border_width(priceBox[i], 1, 0);
    lv_obj_set_style_border_color(priceBox[i], lv_palette_main(LV_PALETTE_GREY), 0);
    disableScroll(priceBox[i]);

        // Symbol caption
        priceTitle[i] = lv_label_create(priceBox[i]);
        ::priceTitle[i] = priceTitle[i];  // Fase 8.4.3: Synchroniseer
        if (i == 0) {
            lv_obj_set_style_text_font(priceTitle[i], FONT_SIZE_TITLE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceTitle[i], FONT_SIZE_TITLE_OTHER, 0);
        }
        lv_obj_set_style_text_color(priceTitle[i], lv_color_white(), 0);
        lv_label_set_text(priceTitle[i], symbols[i]);
        lv_obj_align(priceTitle[i], LV_ALIGN_TOP_LEFT, 0, 0);
        
        // Live price - platform-specifieke layout
        priceLbl[i] = lv_label_create(priceBox[i]);
        ::priceLbl[i] = priceLbl[i];  // Fase 8.4.3: Synchroniseer
        if (i == 0) {
            lv_obj_set_style_text_font(priceLbl[i], FONT_SIZE_PRICE_BTCEUR, 0);
        } else {
            lv_obj_set_style_text_font(priceLbl[i], FONT_SIZE_PRICE_OTHER, 0);
        }
        
        #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
        if (i == 0) {
            lv_obj_set_style_text_align(priceLbl[i], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (i == 0) - TTGO layout
        if (i == 0) {
            anchorMaxLabel = lv_label_create(priceBox[i]);
            ::anchorMaxLabel = anchorMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorLabel = lv_label_create(priceBox[i]);
            ::anchorLabel = anchorLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[i]);
            ::anchorMinLabel = anchorMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
        }
        #else
        if (i == 0) {
            lv_obj_set_style_text_align(priceLbl[i], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_style_text_color(priceLbl[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        } else {
            lv_obj_align_to(priceLbl[i], priceTitle[i], LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);
        }
        
        // Anchor labels alleen voor BTCEUR (i == 0) - CYD/ESP32-S3 layout (met percentages)
        if (i == 0) {
            anchorLabel = lv_label_create(priceBox[i]);
            ::anchorLabel = anchorLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(anchorLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(anchorLabel, "");
            
            anchorMaxLabel = lv_label_create(priceBox[i]);
            ::anchorMaxLabel = anchorMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(anchorMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            lv_label_set_text(anchorMaxLabel, "");
            
            anchorMinLabel = lv_label_create(priceBox[i]);
            ::anchorMinLabel = anchorMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(anchorMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(anchorMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(anchorMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(anchorMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
            lv_label_set_text(anchorMinLabel, "");
        }
        #endif
        
        lv_label_set_text(priceLbl[i], "--");
        
        // Min/Max/Diff labels voor 1 min blok
        if (i == 1)
        {
            price1MinMaxLabel = lv_label_create(priceBox[i]);
            ::price1MinMaxLabel = price1MinMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price1MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMaxLabel, "--");
            lv_obj_align(price1MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price1MinDiffLabel = lv_label_create(priceBox[i]);
            ::price1MinDiffLabel = price1MinDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price1MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinDiffLabel, "--");
            lv_obj_align(price1MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price1MinMinLabel = lv_label_create(priceBox[i]);
            ::price1MinMinLabel = price1MinMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price1MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price1MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price1MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price1MinMinLabel, "--");
            lv_obj_align(price1MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        
        // Min/Max/Diff labels voor 30 min blok (index 2)
        if (i == 2)
        {
            price30MinMaxLabel = lv_label_create(priceBox[i]);
            ::price30MinMaxLabel = price30MinMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price30MinMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMaxLabel, "--");
            lv_obj_align(price30MinMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price30MinDiffLabel = lv_label_create(priceBox[i]);
            ::price30MinDiffLabel = price30MinDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price30MinDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinDiffLabel, "--");
            lv_obj_align(price30MinDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price30MinMinLabel = lv_label_create(priceBox[i]);
            ::price30MinMinLabel = price30MinMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price30MinMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price30MinMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price30MinMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price30MinMinLabel, "--");
            lv_obj_align(price30MinMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
        }
        
        // Min/Max/Diff labels voor 2h blok (index 3) - alleen voor CYD platforms
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        if (i == 3)
        {
            // Initialiseer buffers
            strcpy(price2HMaxLabelBuffer, "--");
            strcpy(price2HDiffLabelBuffer, "--");
            strcpy(price2HMinLabelBuffer, "--");
            
            price2HMaxLabel = lv_label_create(priceBox[i]);
            ::price2HMaxLabel = price2HMaxLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HMaxLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HMaxLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_text_align(price2HMaxLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HMaxLabel, price2HMaxLabelBuffer);
            lv_obj_align(price2HMaxLabel, LV_ALIGN_RIGHT_MID, 0, -14);
            
            price2HDiffLabel = lv_label_create(priceBox[i]);
            ::price2HDiffLabel = price2HDiffLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HDiffLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HDiffLabel, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_obj_set_style_text_align(price2HDiffLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HDiffLabel, price2HDiffLabelBuffer);
            lv_obj_align(price2HDiffLabel, LV_ALIGN_RIGHT_MID, 0, 0);
            
            price2HMinLabel = lv_label_create(priceBox[i]);
            ::price2HMinLabel = price2HMinLabel;  // Fase 8.4.3: Synchroniseer
            lv_obj_set_style_text_font(price2HMinLabel, FONT_SIZE_PRICE_MIN_MAX_DIFF, 0);
            lv_obj_set_style_text_color(price2HMinLabel, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_text_align(price2HMinLabel, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(price2HMinLabel, price2HMinLabelBuffer);
            lv_obj_align(price2HMinLabel, LV_ALIGN_RIGHT_MID, 0, 14);
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
            #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
            // TTGO/GEEK: compact formaat dd-mm-yy voor lagere resolutie
            char dateStr[9]; // dd-mm-yy + null terminator = 9 karakters
            strftime(dateStr, sizeof(dateStr), "%d-%m-%y", &timeinfo);
            #else
            // CYD/ESP32-S3: volledig formaat dd-mm-yyyy voor hogere resolutie
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
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
    // TTGO/GEEK: IP-adres links, versienummer rechts
    ipLabel = lv_label_create(lv_scr_act());
    ::ipLabel = ipLabel;  // Fase 8.4.3: Synchroniseer
    lv_obj_set_style_text_font(ipLabel, FONT_SIZE_FOOTER, 0);
    lv_obj_set_style_text_color(ipLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ipLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    
    chartVersionLabel = lv_label_create(lv_scr_act());
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
    // CYD: Footer met 2 regels
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

    const char* volumeText = "";
    lv_color_t volumeColor = lv_palette_main(LV_PALETTE_GREY);

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
    }
    else
    {
        lv_label_set_text(::mediumTrendLabel, "--");
        lv_obj_set_style_text_color(::mediumTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
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
    }
    else
    {
        lv_label_set_text(::longTermTrendLabel, "--");
        lv_obj_set_style_text_color(::longTermTrendLabel, lv_palette_main(LV_PALETTE_GREY), 0);
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
    }
    
    // Update price label alleen als waarde veranderd is (cache check)
    if (::priceLbl[0] != nullptr && (lastPriceLblValue != prices[0] || lastPriceLblValue < 0.0f)) {
        snprintf(priceLblBuffer, PRICE_LBL_BUFFER_SIZE, "%.2f", prices[0]);
        lv_label_set_text(::priceLbl[0], priceLblBuffer);
        lastPriceLblValue = prices[0];
    }
    
    // Bitcoin waarde linksonderin altijd blauw
    if (::priceLbl[0] != nullptr) {
            lv_obj_set_style_text_color(::priceLbl[0], lv_palette_main(LV_PALETTE_BLUE), 0);
    }
    // Bereken dynamische anchor-waarden op basis van trend voor UI weergave
    AnchorConfigEffective effAnchorUI;
    if (anchorActive && anchorPrice > 0.0f) {
        // Fase 5.3.14: Gebruik TrendDetector module getter i.p.v. globale variabele
        TrendState currentTrend = trendDetector.getTrendState();
        // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
        effAnchorUI = anchorSystem.calcEffectiveAnchor(anchorMaxLoss, anchorTakeProfit, currentTrend);
    }
    
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_GEEK)
    if (::anchorMaxLabel != nullptr) {
        if (anchorActive && anchorPrice > 0.0f) {
            // Gebruik dynamische take profit waarde
            float takeProfitPrice = anchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                snprintf(anchorMaxLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f", takeProfitPrice);
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
        if (anchorActive && anchorPrice > 0.0f) {
            // Update alleen als waarde veranderd is
            if (lastAnchorValue != anchorPrice || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f", anchorPrice);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = anchorPrice;
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
        if (anchorActive && anchorPrice > 0.0f) {
            // Gebruik dynamische max loss waarde
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                snprintf(anchorMinLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f", stopLossPrice);
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
        if (anchorActive && anchorPrice > 0.0f) {
            // Toon dynamische take profit waarde (effectief percentage)
            float takeProfitPrice = anchorPrice * (1.0f + effAnchorUI.takeProfitPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMaxValue != takeProfitPrice || lastAnchorMaxValue < 0.0f) {
                snprintf(anchorMaxLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "+%.2f%% %.2f", effAnchorUI.takeProfitPct, takeProfitPrice);
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
        if (anchorActive && anchorPrice > 0.0f && prices[0] > 0.0f) {
            float anchorPct = ((prices[0] - anchorPrice) / anchorPrice) * 100.0f;
            // Update alleen als waarde veranderd is (check zowel anchorPrice als anchorPct)
            float currentValue = anchorPrice + anchorPct;  // Combinatie voor cache check
            if (lastAnchorValue != currentValue || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%c%.2f%% %.2f",
                         anchorPct >= 0 ? '+' : '-', fabsf(anchorPct), anchorPrice);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = currentValue;
            }
        } else if (anchorActive && anchorPrice > 0.0f) {
            // Update alleen als waarde veranderd is
            if (lastAnchorValue != anchorPrice || lastAnchorValue < 0.0f) {
                snprintf(anchorLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f", anchorPrice);
                lv_label_set_text(::anchorLabel, anchorLabelBuffer);
                lastAnchorValue = anchorPrice;
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
        if (anchorActive && anchorPrice > 0.0f) {
            // Toon dynamische max loss waarde (effectief percentage)
            float stopLossPrice = anchorPrice * (1.0f + effAnchorUI.maxLossPct / 100.0f);
            // Update alleen als waarde veranderd is
            if (lastAnchorMinValue != stopLossPrice || lastAnchorMinValue < 0.0f) {
                snprintf(anchorMinLabelBuffer, ANCHOR_LABEL_BUFFER_SIZE, "%.2f%% %.2f", effAnchorUI.maxLossPct, stopLossPrice);
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
// Helper functie om warm-start status label bij te werken
void UIController::updateWarmStartStatusLabel()
{
    // Fase 8.5.4: Gebruik globale pointer (synchroniseert met module pointer)
    if (::warmStartStatusLabel == nullptr) return;
    
    char warmStartText[16];
    if (warmStartStatus == WARMING_UP) {
        snprintf(warmStartText, sizeof(warmStartText), "DATA%u%%", warmStartStats.warmUpProgress);
    } else if (warmStartStatus == LIVE_COLD) {
        snprintf(warmStartText, sizeof(warmStartText), "COLD");
    } else {
        snprintf(warmStartText, sizeof(warmStartText), "LIVE");
    }
    lv_label_set_text(::warmStartStatusLabel, warmStartText);
    lv_color_t statusColor = (warmStartStatus == WARMING_UP) ? lv_palette_main(LV_PALETTE_ORANGE) :
                              (warmStartStatus == LIVE_COLD) ? lv_palette_main(LV_PALETTE_BLUE) :
                              lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_text_color(::warmStartStatusLabel, statusColor, 0);
}

// Fase 8.6.2: updateAveragePriceCard() naar Module
// Helper functie om average price cards (1min/30min) bij te werken
void UIController::updateAveragePriceCard(uint8_t index)
{
    // Fase 8.6.2: Gebruik globale pointers (synchroniseert met module pointers)
    float pct = prices[index];
    bool hasData1m = (index == 1) ? secondArrayFilled : true;
    bool hasData30m = (index == 2) ? (minuteArrayFilled || minuteIndex >= 30) : true;
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    bool hasData2h = (index == 3) ? (minuteArrayFilled || minuteIndex >= 120) : true;
    bool hasData2hMinimal = (index == 3) ? (minuteArrayFilled || minuteIndex >= 2) : true;  // Minimaal 2 minuten data nodig voor return berekening (of buffer vol)
    bool hasData = (index == 1) ? hasData1m : ((index == 2) ? hasData30m : ((index == 3) ? hasData2hMinimal : true));
    
    // Debug voor 2h box
    if (index == 3) {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        #if !DEBUG_BUTTON_ONLY
        bool shouldShowPct = (index == 3) ? (hasData2hMinimal) : (hasData && pct != 0.0f);
        Serial.printf("[UI] 2h box: hasData=%d, hasData2hMinimal=%d, pct=%.4f, prices[3]=%.4f, shouldShowPct=%d\n", 
                      hasData, hasData2hMinimal, pct, prices[3], shouldShowPct);
        #endif
        #endif
    }
    #else
    bool hasData = (index == 1) ? hasData1m : ((index == 2) ? hasData30m : true);
    #endif
    
    if (!hasData) {
        pct = 0.0f;
    }
    
    if (::priceTitle[index] != nullptr) {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // Voor 2h box: toon percentage als er minimaal 2 minuten data zijn (hasData2hMinimal)
        // Voor andere boxes: toon alleen als pct != 0.0f
        bool shouldShowPct = (index == 3) ? (hasData2hMinimal) : (hasData && pct != 0.0f);
        if (shouldShowPct) {  // Voor 2h box: toon percentage altijd als er data is (ook als 0.00%)
        #else
        if (hasData && pct != 0.0f) {
        #endif
            // Format nieuwe tekst
            char newText[32];  // Verkleind van 48 naar 32 bytes (max: "30 min  +12.34%" = ~20 chars)
            if (pct == 0.0f && index == 3) {
                // Voor 2h box: toon 0.00% als de return 0 is
                snprintf(newText, sizeof(newText), "%s  0.00%%", symbols[index]);
            } else {
                snprintf(newText, sizeof(newText), "%s  %c%.2f%%", symbols[index], pct >= 0 ? '+' : '-', fabsf(pct));
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
        
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
        updateMinMaxDiffLabels(::price1MinMaxLabel, ::price1MinMinLabel, ::price1MinDiffLabel,
                              price1MinMaxLabelBuffer, price1MinMinLabelBuffer, price1MinDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice1MinMaxValue, lastPrice1MinMinValue, lastPrice1MinDiffValue);
    }
    
    if (index == 2 && ::price30MinMaxLabel != nullptr && ::price30MinMinLabel != nullptr && ::price30MinDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInLast30Minutes(minVal, maxVal);
        
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
        updateMinMaxDiffLabels(::price30MinMaxLabel, ::price30MinMinLabel, ::price30MinDiffLabel,
                              price30MinMaxLabelBuffer, price30MinMinLabelBuffer, price30MinDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice30MinMaxValue, lastPrice30MinMinValue, lastPrice30MinDiffValue);
    }
    
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    if (index == 3 && ::price2HMaxLabel != nullptr && ::price2HMinLabel != nullptr && ::price2HDiffLabel != nullptr)
    {
        float minVal, maxVal;
        findMinMaxInLast2Hours(minVal, maxVal);
        
        float diff = (minVal > 0.0f && maxVal > 0.0f) ? (maxVal - minVal) : 0.0f;
        // Geoptimaliseerd: gebruik helper functie i.p.v. gedupliceerde code
        updateMinMaxDiffLabels(::price2HMaxLabel, ::price2HMinLabel, ::price2HDiffLabel,
                              price2HMaxLabelBuffer, price2HMinLabelBuffer, price2HDiffLabelBuffer,
                              maxVal, minVal, diff,
                              lastPrice2HMaxValue, lastPrice2HMinValue, lastPrice2HDiffValue);
    }
    #endif
    
    if (!hasData)
    {
        lv_label_set_text(::priceLbl[index], "--");
    }
    else if (averagePrices[index] > 0.0f)
    {
        // Update alleen als waarde veranderd is
        if (lastPriceLblValueArray[index] != averagePrices[index] || lastPriceLblValueArray[index] < 0.0f) {
            snprintf(priceLblBufferArray[index], sizeof(priceLblBufferArray[index]), "%.2f", averagePrices[index]);
            lv_label_set_text(::priceLbl[index], priceLblBufferArray[index]);
            lastPriceLblValueArray[index] = averagePrices[index];
        }
    }
    else
    {
        // averagePrices[index] is 0.0f of niet gezet
        lv_label_set_text(::priceLbl[index], "--");
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
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    // Voor 2h box: gebruik minimaal 2 minuten data (net als voor return berekening) of buffer vol
    bool hasDataForColor = (index == 1) ? secondArrayFilled : ((index == 2) ? (minuteArrayFilled || minuteIndex >= 30) : (minuteArrayFilled || minuteIndex >= 2));
    #else
    bool hasDataForColor = (index == 1) ? secondArrayFilled : (minuteArrayFilled || minuteIndex >= 30);
    #endif
    
    // Voor 2h box: toon kleur ook als pct 0.0f is maar er wel data is
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
    bool shouldShowColor = (index == 3) ? (hasDataForColor) : (hasDataForColor && pct != 0.0f);
    #else
    bool shouldShowColor = hasDataForColor && pct != 0.0f;
    #endif
    
    if (shouldShowColor)
    {
        lv_obj_set_style_text_color(::priceLbl[index],
                                    pct >= 0 ? lv_palette_lighten(LV_PALETTE_GREEN, 4)
                                             : lv_palette_lighten(LV_PALETTE_RED, 3),
                                    0);
        
        lv_color_t bg = pct >= 0
                            ? lv_color_mix(lv_palette_main(LV_PALETTE_GREEN), lv_color_black(), 127)
                            : lv_color_mix(lv_palette_main(LV_PALETTE_RED), lv_color_black(), 127);
        lv_obj_set_style_bg_color(::priceBox[index], bg, 0);
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
void UIController::updateChartSection(int32_t currentPrice, bool hasNewPriceData)
{
    // Fase 8.7.1: Gebruik globale pointers (synchroniseert met module pointers)
    // Voeg een punt toe aan de grafiek als er geldige data is
    if (prices[symbolIndexToChart] > 0.0f) {
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
    
    // Update chart range
    this->updateChartRange(currentPrice);
    
    // Update chart title (CYD displays)
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
    
    // Update chart begin letters label (TTGO displays)
    #if defined(PLATFORM_TTGO) || defined(PLATFORM_ESP32S3_SUPERMINI) || defined(PLATFORM_ESP32S3_GEEK)
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
    
    // Data wordt al beschermd door mutex in uiTask
    int32_t p = (int32_t)lroundf(prices[symbolIndexToChart] * 100.0f);
    
    // Bepaal of er nieuwe data is op basis van timestamp
    // Bij 2000ms interval + retries kan call tot ~3000ms duren, dus marge van 3000ms
    unsigned long currentTime = millis();
    bool hasNewPriceData = false;
    if (lastApiMs > 0) {
        unsigned long timeSinceLastApi = (currentTime >= lastApiMs) ? (currentTime - lastApiMs) : (ULONG_MAX - lastApiMs + currentTime);
        hasNewPriceData = (timeSinceLastApi < 3000);  // 2000ms interval + 1000ms marge voor retries
    }
    
    // Update UI sections (gebruik module versies)
    updateChartSection(p, hasNewPriceData);
    updateHeaderSection();
    updatePriceCardsSection(hasNewPriceData);
    updateFooter();
}

// Fase 8.9.1: checkButton() naar Module
// Physical button check function (voor TTGO en CYD platforms)
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
        
        // Gebruik helper functie om anchor in te stellen (gebruikt huidige prijs als default)
        // Fase 6.2.7: Gebruik AnchorSystem module i.p.v. globale functie
        if (anchorSystem.setAnchorPrice(0.0f)) {
            // Update UI (this will also take the mutex internally)
            // Fase 8.8.1: Gebruik module versie (binnen class, gebruik this->)
            this->updateUI();
        } else {
            Serial.println("[Button] WARN: Kon anchor niet instellen");
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
    // init LVGL
    lv_init();

    // Set a tick source so that LVGL will know how much time elapsed
    lv_tick_set_cb(millis_cb);

    // register print function for debugging
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    uint32_t screenWidth = gfx->width();
    uint32_t screenHeight = gfx->height();
    
    // Detecteer PSRAM beschikbaarheid
    bool psramAvailable = hasPSRAM();
    
    // Bepaal useDoubleBuffer: board-aware
    bool useDoubleBuffer;
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // CYD zonder PSRAM: force single buffer (geen double buffering)
        useDoubleBuffer = false;  // Altijd false voor CYD zonder PSRAM
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        // ESP32-S3: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_ESP32S3_GEEK)
        // ESP32-S3 GEEK: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #elif defined(PLATFORM_TTGO)
        // TTGO: double buffer alleen als PSRAM beschikbaar is
        useDoubleBuffer = psramAvailable;
    #else
        // Fallback: double buffer alleen met PSRAM
        useDoubleBuffer = psramAvailable;
    #endif
    
    // Bepaal buffer lines per board (compile-time instelbaar voor CYD)
    uint8_t bufLines;
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28)
        // CYD zonder PSRAM: compile-time instelbaar (default 4, kan 1/2/4 zijn voor testen)
        // Na geheugenoptimalisaties kunnen we meer buffer gebruiken voor betere performance
        #ifndef CYD_BUF_LINES_NO_PSRAM
        #define CYD_BUF_LINES_NO_PSRAM 2  // Default: 2 regels (was 1->2->4, verlaagd voor extra DRAM ruimte op CYD)
        #endif
        if (psramAvailable) {
            bufLines = 40;  // CYD met PSRAM: 40 regels
        } else {
            bufLines = CYD_BUF_LINES_NO_PSRAM;  // CYD zonder PSRAM: compile-time instelbaar
        }
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
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
    #elif defined(PLATFORM_TTGO)
        // TTGO: 30 regels met PSRAM, 2 zonder
        if (psramAvailable) {
            bufLines = 30;
        } else {
            bufLines = 2;
        }
    #else
        // Fallback
        bufLines = psramAvailable ? 30 : 2;
    #endif
    
    uint32_t bufSize = screenWidth * bufLines;
    uint8_t numBuffers = useDoubleBuffer ? 2 : 1;  // 1 of 2 buffers afhankelijk van useDoubleBuffer
    size_t bufSizeBytes = bufSize * sizeof(lv_color_t) * numBuffers;
    
    const char* bufferLocation;
    uint32_t freeHeapBefore = ESP.getFreeHeap();
    size_t largestFreeBlockBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Bepaal board naam voor logging
    const char* boardName;
    #if defined(PLATFORM_CYD24)
        boardName = "CYD24";
    #elif defined(PLATFORM_CYD28)
        boardName = "CYD28";
    #elif defined(PLATFORM_ESP32S3_SUPERMINI)
        boardName = "ESP32-S3";
    #elif defined(PLATFORM_TTGO)
        boardName = "TTGO";
    #else
        boardName = "Unknown";
    #endif
    
    // Alloceer buffer één keer bij init (niet herhaald)
    if (disp_draw_buf == nullptr) {
        if (psramAvailable) {
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
            // Zonder PSRAM: gebruik INTERNAL+DMA geheugen (geen DEFAULT)
            bufferLocation = "INTERNAL+DMA";
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
        Serial.printf("[LVGL] PSRAM: %s, useDoubleBuffer: %s\n", 
                     psramAvailable ? "yes" : "no", useDoubleBuffer ? "true" : "false");
        Serial.printf("[LVGL] Draw buffer: %u lines, %u pixels, %u bytes (%u buffer%s)\n", 
                     bufLines, bufSize, bufSizeBytes, numBuffers, numBuffers == 1 ? "" : "s");
        Serial.printf("[LVGL] Buffer locatie: %s\n", bufferLocation);
        Serial.printf("[LVGL] Heap: %u -> %u bytes free, Largest block: %u -> %u bytes\n",
                     freeHeapBefore, freeHeapAfter, largestFreeBlockBefore, largestFreeBlockAfter);
    } else {
        Serial.println(F("[LVGL] WARNING: Draw buffer al gealloceerd! (herhaalde allocatie voorkomen)"));
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    // LVGL buffer setup: single of double buffering
    // LVGL 9.0+ verwacht buffer size in BYTES, niet pixels
    size_t bufSizePixels = bufSize;  // Aantal pixels in buffer
    size_t bufSizeBytesPerBuffer = bufSizePixels * sizeof(lv_color_t);  // Bytes per buffer
    
    if (useDoubleBuffer) {
        // Double buffering: beide buffers in dezelfde allocatie
        // bufSizeBytes is al berekend als bufSize * sizeof(lv_color_t) * 2
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSizeBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        // Single buffering: alleen eerste buffer gebruiken (size in bytes)
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSizeBytesPerBuffer, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }
}

// Helper: Update min/max/diff labels (geoptimaliseerd: elimineert code duplicatie)
void UIController::updateMinMaxDiffLabels(lv_obj_t* maxLabel, lv_obj_t* minLabel, lv_obj_t* diffLabel,
                                          char* maxBuffer, char* minBuffer, char* diffBuffer,
                                          float maxVal, float minVal, float diff,
                                          float& lastMaxValue, float& lastMinValue, float& lastDiffValue)
{
    if (minVal > 0.0f && maxVal > 0.0f) {
        // Update alleen als waarden veranderd zijn
        if (lastMaxValue != maxVal || lastMaxValue < 0.0f) {
            snprintf(maxBuffer, 20, "%.2f", maxVal);
            lv_label_set_text(maxLabel, maxBuffer);
            lastMaxValue = maxVal;
        }
        if (lastDiffValue != diff || lastDiffValue < 0.0f) {
            snprintf(diffBuffer, 20, "%.2f", diff);
            lv_label_set_text(diffLabel, diffBuffer);
            lastDiffValue = diff;
        }
        if (lastMinValue != minVal || lastMinValue < 0.0f) {
            snprintf(minBuffer, 20, "%.2f", minVal);
            lv_label_set_text(minLabel, minBuffer);
            lastMinValue = minVal;
        }
    } else {
        // Update alleen als labels niet "--" zijn
        if (strcmp(maxBuffer, "--") != 0) {
            strcpy(maxBuffer, "--");
            lv_label_set_text(maxLabel, "--");
            lastMaxValue = -1.0f;
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
}

// Fase 8.11.3: updateChartRange() verplaatst vanuit .ino naar UIController module
// Helper functie om chart range te berekenen en bij te werken
void UIController::updateChartRange(int32_t currentPrice)
{
    // Constants (gedefinieerd in .ino)
    #ifndef PRICE_RANGE
    #define PRICE_RANGE 200         // The range of price for the chart, adjust as needed
    #endif
    #ifndef POINTS_TO_CHART
    #define POINTS_TO_CHART 60      // Number of points on the chart (60 points = 2 minutes at 2000ms API interval)
    #endif
    
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
            chartMin = chartAverage - PRICE_RANGE;
            chartMax = chartAverage + PRICE_RANGE;
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
            int32_t fallbackMargin = PRICE_RANGE / 20;
            if (fallbackMargin < 10) fallbackMargin = 10;
            minRange = chartAverage - PRICE_RANGE - fallbackMargin;
            maxRange = chartAverage + PRICE_RANGE + fallbackMargin;
            if (minRange < 0) minRange = 0;
        }
    }
    else
    {
        chartAverage = currentPrice;
        int32_t margin = PRICE_RANGE / 20;
        if (margin < 10) margin = 10;
        minRange = currentPrice - PRICE_RANGE - margin;
        maxRange = currentPrice + PRICE_RANGE + margin;
    }
    
    // Gebruik member pointer i.p.v. globale pointer
    lv_chart_set_range(this->chart, LV_CHART_AXIS_PRIMARY_Y, minRange, maxRange);
}
