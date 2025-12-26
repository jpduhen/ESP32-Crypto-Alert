#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include <Arduino.h>
#include <lvgl.h>

// Note: LVGL types (lv_display_t, lv_obj_t, lv_area_t, lv_log_level_t) zijn gedefinieerd in lvgl.h

// Forward declarations voor dependencies
class PriceData;
class TrendDetector;
class VolatilityTracker;
class AnchorSystem;

// SYMBOL_COUNT wordt gedefinieerd in platform_config.h (per platform)
// Hier alleen een fallback als het nog niet gedefinieerd is
#ifndef SYMBOL_COUNT
#define SYMBOL_COUNT 3  // Fallback default
#endif

// UIController class - beheert LVGL UI initialisatie, building en updates
// Fase 8: UI Module refactoring
class UIController {
public:
    UIController();
    void begin();
    
    // LVGL callback functies (moeten extern blijven voor LVGL)
    // Fase 8.1.3: Verplaatst naar UIController module
    // Note: Deze functies zijn static omdat LVGL ze direct aanroept
    static void my_print(lv_log_level_t level, const char *buf);
    static uint32_t millis_cb(void);
    static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    
    // Fase 8.2.1: UI object pointers (parallel implementatie - globaal blijft bestaan)
    // Getters voor externe access (indien nodig voor backward compatibility)
    lv_obj_t* getChart() const { return chart; }
    lv_chart_series_t* getDataSeries() const { return dataSeries; }
    lv_obj_t* getPriceBox(uint8_t index) const { return (index < SYMBOL_COUNT) ? priceBox[index] : nullptr; }
    lv_obj_t* getPriceTitle(uint8_t index) const { return (index < SYMBOL_COUNT) ? priceTitle[index] : nullptr; }
    lv_obj_t* getPriceLbl(uint8_t index) const { return (index < SYMBOL_COUNT) ? priceLbl[index] : nullptr; }
    lv_obj_t* getChartTitle() const { return chartTitle; }
    lv_obj_t* getChartVersionLabel() const { return chartVersionLabel; }
    lv_obj_t* getChartDateLabel() const { return chartDateLabel; }
    lv_obj_t* getChartTimeLabel() const { return chartTimeLabel; }
    lv_obj_t* getIpLabel() const { return ipLabel; }
    lv_obj_t* getAnchorLabel() const { return anchorLabel; }
    lv_obj_t* getAnchorMaxLabel() const { return anchorMaxLabel; }
    
    // Fase 8.4: buildUI() naar Module
    void buildUI();
    
    // Fase 8.5: update*Label() functies naar Module
    void updateDateTimeLabels();
    void updateTrendLabel();
    void updateVolatilityLabel();
    void updateWarmStartStatusLabel();
    
    // Fase 8.6: update*Card() functies naar Module
    void updateBTCEURCard(bool hasNewData);
    void updateAveragePriceCard(uint8_t index);
    void updatePriceCardColor(uint8_t index, float pct);
    
    // Fase 8.7: update*Section() functies naar Module
    void updateChartSection(int32_t currentPrice, bool hasNewPriceData);
    void updateHeaderSection();
    void updatePriceCardsSection(bool hasNewPriceData);
    
    // Fase 8.8: updateUI() naar Module
    void updateUI();
    
    // Fase 8.9: checkButton() naar Module
    void checkButton();
    
    // Fase 8.10: LVGL Initialisatie naar Module
    void setupLVGL();
    
private:
    // Fase 8.2.1: UI object pointers (parallel - globaal blijft bestaan voor backward compatibility)
    // Note: SYMBOL_COUNT is gedefinieerd in .ino als #define SYMBOL_COUNT 3
    
    // Chart widgets
    lv_obj_t *chart;
    lv_chart_series_t *dataSeries;
    
    // Chart labels (trend, volatility, warm-start status)
    lv_obj_t *trendLabel;
    lv_obj_t *volatilityLabel;
    lv_obj_t *warmStartStatusLabel;
    
    // Footer labels
    lv_obj_t *lblFooterLine1;
    lv_obj_t *lblFooterLine2;
    lv_obj_t *ramLabel;
    lv_obj_t *chartVersionLabel;
    
    // Price cards
    lv_obj_t *priceBox[SYMBOL_COUNT];
    lv_obj_t *priceTitle[SYMBOL_COUNT];
    lv_obj_t *priceLbl[SYMBOL_COUNT];
    
    // Header labels
    lv_obj_t *chartTitle;
    lv_obj_t *chartDateLabel;
    lv_obj_t *chartTimeLabel;
    lv_obj_t *chartBeginLettersLabel;
    lv_obj_t *ipLabel;
    
    // Price info labels
    lv_obj_t *price1MinMaxLabel;
    lv_obj_t *price1MinMinLabel;
    lv_obj_t *price1MinDiffLabel;
    lv_obj_t *price30MinMaxLabel;
    lv_obj_t *price30MinMinLabel;
    lv_obj_t *price30MinDiffLabel;
    
    // Anchor labels
    lv_obj_t *anchorLabel;
    lv_obj_t *anchorMaxLabel;
    lv_obj_t *anchorMinLabel;
    
    // Fase 8.3: create*() functies
    void createChart();
    void createHeaderLabels();
    void createPriceBoxes();
    void createFooter();
    
    // Fase 8.11.3: Helper functies (verplaatst vanuit .ino)
    void updateChartRange(int32_t currentPrice);
    
    // Forward declarations voor interne helpers (worden later geïmplementeerd)
    // Fase 8.3: createHeaderLabels(), createPriceBoxes(), createFooter()
    // Fase 8.5: update*Label() functies
    // Fase 8.6: update*Card() functies
    // Fase 8.7: update*Section() functies
    // Fase 8.8: updateUI()
    // Fase 8.9: checkButton()
    // Fase 8.10: LVGL initialisatie
};

// Global instance (wordt aangemaakt in .ino)
extern UIController uiController;

#endif // UICONTROLLER_H

