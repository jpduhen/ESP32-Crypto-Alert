#ifndef PRICEDATA_H
#define PRICEDATA_H

#include <Arduino.h>
#include "../ApiClient/ApiClient.h"  // Voor ApiClient::isValidPrice()

// Platform config moet eerst voor PLATFORM_* defines
// Note: Als PriceData.h wordt geïncludeerd, moet platform_config.h al geïncludeerd zijn
// Constants (moeten overeenkomen met .ino bestand)
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_5MINUTES 300
#define MINUTES_FOR_30MIN_CALC 120

// DataSource enum - gebruikt voor tracking waar data vandaan komt
enum DataSource {
    SOURCE_BINANCE,  // Data van Binance historische klines
    SOURCE_LIVE      // Data van live API calls
};

// Forward declarations voor globale arrays (parallel implementatie - stap 4.2.5)
// State variabelen zijn nu in PriceData, arrays blijven globaal tot stap 4.2.6+
extern float secondPrices[];
extern DataSource secondPricesSource[];
#if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
extern float *fiveMinutePrices;
extern DataSource *fiveMinutePricesSource;
extern float *minuteAverages;
extern DataSource *minuteAveragesSource;
#else
extern float fiveMinutePrices[];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
extern DataSource fiveMinutePricesSource[];
extern float minuteAverages[];
extern DataSource minuteAveragesSource[];
#endif
void updateWarmStartStatus();

// PriceData class - beheert alle prijs data arrays en berekeningen
class PriceData {
public:
    PriceData();
    void begin();
    
    // Fase 4.2.5: Synchroniseer state met globale variabelen (parallel implementatie)
    void syncStateFromGlobals();
    
    // Fase 4.2.6: Getters voor arrays (parallel, arrays blijven globaal)
    // Return pointer naar globale array (wordt later verplaatst naar PriceData members)
    float* getSecondPrices() { 
        extern float secondPrices[];
        return secondPrices;
    }
    DataSource* getSecondPricesSource() {
        extern DataSource secondPricesSource[];
        return secondPricesSource;
    }
    uint8_t getSecondIndex() const { return secondIndex; }
    bool getSecondArrayFilled() const { return secondArrayFilled; }
    
    // Fase 4.2.9: Getters voor fiveMinutePrices arrays (parallel, arrays blijven globaal)
    float* getFiveMinutePrices() {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
        extern float *fiveMinutePrices;
        #else
        extern float fiveMinutePrices[];
        #endif
        return fiveMinutePrices;
    }
    DataSource* getFiveMinutePricesSource() {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
        extern DataSource *fiveMinutePricesSource;
        #else
        extern DataSource fiveMinutePricesSource[];
        #endif
        return fiveMinutePricesSource;
    }
    uint16_t getFiveMinuteIndex() const { return fiveMinuteIndex; }
    bool getFiveMinuteArrayFilled() const { return fiveMinuteArrayFilled; }
    
    // Fase 4.2.9: Getters voor minuteAverages arrays (parallel, arrays blijven globaal)
    float* getMinuteAverages() {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
        extern float *minuteAverages;
        #else
        extern float minuteAverages[];
        #endif
        return minuteAverages;
    }
    DataSource* getMinuteAveragesSource() {
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
        extern DataSource *minuteAveragesSource;
        #else
        extern DataSource minuteAveragesSource[];
        #endif
        return minuteAveragesSource;
    }
    // Note: minuteIndex en minuteArrayFilled zijn nog globaal, worden later verplaatst
    // Voor nu gebruiken we forward declarations
    uint8_t getMinuteIndex() const;
    bool getMinuteArrayFilled() const;
    
    // Fase 4.2.3: addPriceToSecondArray() toegevoegd
    // Fase 4.2.5: Gebruikt nu PriceData state variabelen (parallel, globale arrays blijven bestaan)
    void addPriceToSecondArray(float price) {
        // Validate input
        if (!ApiClient::isValidPrice(price))
        {
            Serial.printf("[Array] WARN: Ongeldige prijs in addPriceToSecondArray: %.2f\n", price);
            return;
        }
        
        // Bounds check voor secondPrices array
        // Fase 4.2.5: Gebruikt PriceData state (this->secondIndex)
        // Note: Als secondIndex == SECONDS_PER_MINUTE, betekent dit dat buffer vol is, reset naar 0
        if (this->secondIndex >= SECONDS_PER_MINUTE)
        {
            // Dit kan gebeuren na warm-start als copyCount == SECONDS_PER_MINUTE
            // In dat geval is de buffer vol, dus volgende write moet naar index 0
            this->secondIndex = 0;
            this->secondArrayFilled = true;
        }
        
        // Gebruik nog globale arrays (worden later verplaatst in stap 4.2.6+)
        extern float secondPrices[];
        extern DataSource secondPricesSource[];
        secondPrices[this->secondIndex] = price;
        secondPricesSource[this->secondIndex] = SOURCE_LIVE;  // Mark as live data
        // Geconsolideerde index update: check en update in één keer
        this->secondIndex = (this->secondIndex + 1) % SECONDS_PER_MINUTE;
        if (this->secondIndex == 0) {
            this->secondArrayFilled = true;
        }
        
        // Ook toevoegen aan 5-minuten buffer met bounds checking
        // Note: Als fiveMinuteIndex == SECONDS_PER_5MINUTES, betekent dit dat buffer vol is, reset naar 0
        if (this->fiveMinuteIndex >= SECONDS_PER_5MINUTES)
        {
            // Dit kan gebeuren na warm-start als fiveMinuteIndex == SECONDS_PER_5MINUTES
            // In dat geval is de buffer vol, dus volgende write moet naar index 0
            this->fiveMinuteIndex = 0;
            this->fiveMinuteArrayFilled = true;
        }
        
        // Null pointer check voor dynamische arrays (CYD/TTGO platforms)
        #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
        extern float *fiveMinutePrices;
        extern DataSource *fiveMinutePricesSource;
        if (fiveMinutePrices == nullptr || fiveMinutePricesSource == nullptr) {
            Serial.printf("[Array] ERROR: fiveMinutePrices arrays niet gealloceerd!\n");
            return; // Skip als arrays niet gealloceerd zijn
        }
        #else
        extern float fiveMinutePrices[];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
        extern DataSource fiveMinutePricesSource[];
        #endif
        
        fiveMinutePrices[this->fiveMinuteIndex] = price;
        fiveMinutePricesSource[this->fiveMinuteIndex] = SOURCE_LIVE;  // Mark as live data
        // Geconsolideerde index update: check en update in één keer
        this->fiveMinuteIndex = (this->fiveMinuteIndex + 1) % SECONDS_PER_5MINUTES;
        if (this->fiveMinuteIndex == 0) {
            this->fiveMinuteArrayFilled = true;
        }
        
        // Update warm-start status periodiek (elke 10 seconden)
        static unsigned long lastStatusUpdate = 0;
        unsigned long now = millis();
        if (now - lastStatusUpdate > 10000) {  // Elke 10 seconden
            updateWarmStartStatus();
            lastStatusUpdate = now;
        }
    }
    
    // Fase 4.2.8: calculateReturn1Minute() verplaatst naar PriceData
    // Bereken 1-minuut return: prijs nu vs 60 seconden geleden
    // averagePrices pointer is optioneel (kan nullptr zijn)
    float calculateReturn1Minute(float* averagePrices = nullptr);
    
private:
    // Fase 4.2.5: State variabelen toegevoegd (parallel, gebruikt in addPriceToSecondArray)
    uint8_t secondIndex;
    bool secondArrayFilled;
    uint16_t fiveMinuteIndex;
    bool fiveMinuteArrayFilled;
    
    // Fase 4.2.2: Arrays toegevoegd (parallel, nog niet gebruikt)
    // Note: Tijdelijk uitgecommentarieerd om naamconflicten te voorkomen met globale variabelen
    // Deze worden later gebruikt in stap 4.2.6+ wanneer arrays verplaatst worden
    /*
    // Array van 60 posities voor laatste 60 seconden (1 minuut)
    float secondPrices[SECONDS_PER_MINUTE];
    DataSource secondPricesSource[SECONDS_PER_MINUTE];  // Source tracking per sample
    
    // Array van 300 posities voor laatste 300 seconden (5 minuten)
    // Voor CYD/TTGO zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
    // ESP32-S3 SuperMini en GEEK hebben PSRAM, gebruiken statische arrays
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
    float *fiveMinutePrices;  // Dynamisch gealloceerd voor CYD/TTGO zonder PSRAM
    DataSource *fiveMinutePricesSource;  // Dynamisch gealloceerd
    #else
    float fiveMinutePrices[SECONDS_PER_5MINUTES];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
    DataSource fiveMinutePricesSource[SECONDS_PER_5MINUTES];  // Source tracking per sample
    #endif
    
    // Array van 120 posities voor laatste 120 minuten (2 uur)
    // Voor CYD/TTGO zonder PSRAM: dynamisch alloceren om DRAM overflow te voorkomen
    // ESP32-S3 SuperMini en GEEK hebben PSRAM, gebruiken statische arrays
    #if defined(PLATFORM_CYD24) || defined(PLATFORM_CYD28) || defined(PLATFORM_TTGO)
    float *minuteAverages;  // Dynamisch gealloceerd voor CYD/TTGO zonder PSRAM
    DataSource *minuteAveragesSource;  // Dynamisch gealloceerd
    #else
    float minuteAverages[MINUTES_FOR_30MIN_CALC];  // Statische arrays voor platforms met PSRAM (ESP32-S3 SuperMini, GEEK)
    DataSource minuteAveragesSource[MINUTES_FOR_30MIN_CALC];  // Source tracking per sample
    #endif
    */
};

#endif // PRICEDATA_H



