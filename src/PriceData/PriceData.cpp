#include "PriceData.h"
#include "../ApiClient/ApiClient.h"  // Voor ApiClient::isValidPrice()

// Note: In stap 4.2.3 gebruiken we nog de globale variabelen uit .ino bestand
// Deze zijn static, maar omdat .ino en .cpp in dezelfde compilation unit zitten,
// kunnen we ze direct gebruiken. In stap 4.2.5 worden state variabelen verplaatst.

// Constructor - initialiseer state variabelen (Fase 4.2.5)
PriceData::PriceData() {
    // Fase 4.2.5: Initialiseer state variabelen
    secondIndex = 0;
    secondArrayFilled = false;
    fiveMinuteIndex = 0;
    fiveMinuteArrayFilled = false;
}

// Begin - synchroniseer state met globale variabelen (Fase 4.2.5)
void PriceData::begin() {
    // Fase 4.2.5: Synchroniseer PriceData state met globale variabelen
    syncStateFromGlobals();
}

// Fase 4.2.5: Synchroniseer PriceData state met globale variabelen
void PriceData::syncStateFromGlobals() {
    // Forward declarations voor globale variabelen
    extern uint8_t secondIndex;
    extern bool secondArrayFilled;
    extern uint16_t fiveMinuteIndex;
    extern bool fiveMinuteArrayFilled;
    
    // Kopieer waarden van globale variabelen naar PriceData state
    this->secondIndex = secondIndex;
    this->secondArrayFilled = secondArrayFilled;
    this->fiveMinuteIndex = fiveMinuteIndex;
    this->fiveMinuteArrayFilled = fiveMinuteArrayFilled;
}

// Fase 4.2.9: Getters voor minuteIndex en minuteArrayFilled (nog globaal)
uint8_t PriceData::getMinuteIndex() const {
    extern uint8_t minuteIndex;
    return minuteIndex;
}

bool PriceData::getMinuteArrayFilled() const {
    extern bool minuteArrayFilled;
    return minuteArrayFilled;
}

// Fase 4.2.3: addPriceToSecondArray() is inline geïmplementeerd in header
// omdat globale variabelen direct beschikbaar moeten zijn

// Fase 4.2.8: calculateReturn1Minute() verplaatst naar PriceData
// Forward declarations voor helper functies uit .ino
extern int32_t getRingBufferIndexAgo(uint32_t currentIndex, uint32_t positionsAgo, uint32_t bufferSize);
extern uint32_t getLastWrittenIndex(uint32_t currentIndex, uint32_t bufferSize);
extern bool areValidPrices(float price1, float price2);
extern float calculateAverage(float *array, uint8_t size, bool filled);

// Forward declaration voor macro (moet in .ino blijven)
#ifndef VALUES_FOR_1MIN_RETURN
#define VALUES_FOR_1MIN_RETURN 30  // Fallback: 60000 / 2000
#endif

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

float PriceData::calculateReturn1Minute(float* averagePrices) {
    // Fase 4.2.8: Implementeer calculateReturn1Minute() logica direct in PriceData
    // Gebaseerd op calculateReturnGeneric() maar specifiek voor 1-minuut return
    // Geoptimaliseerd: geconsolideerde checks, helper voor averagePrices updates
    
    const float* priceArray = this->getSecondPrices();
    uint16_t arraySize = SECONDS_PER_MINUTE;
    uint16_t currentIndex = this->getSecondIndex();
    bool arrayFilled = this->getSecondArrayFilled();
    uint16_t positionsAgo = VALUES_FOR_1MIN_RETURN;
    const char* logPrefix = "[Ret1m]";
    uint32_t logIntervalMs = 10000;
    uint8_t averagePriceIndex = 1;
    
    // Helper: Update averagePrices array (geconsolideerd om duplicatie te elimineren)
    auto updateAveragePrice = [&](float value) {
        if (averagePrices != nullptr && averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = value;
        }
    };
    
    // Geconsolideerde check: check if we have enough data
    if (!arrayFilled && currentIndex < positionsAgo) {
        updateAveragePrice(0.0f);
        if (logIntervalMs > 0) {
            static uint32_t lastLogTime = 0;
            uint32_t now = millis();
            if (now - lastLogTime > logIntervalMs) {
                Serial_printf("%s Wachten op data: index=%u (nodig: %u)\n", logPrefix, currentIndex, positionsAgo);
                lastLogTime = now;
            }
        }
        return 0.0f;
    }
    
    // Get current price (geconsolideerde logica)
    float priceNow;
    if (arrayFilled) {
        uint16_t lastWrittenIdx = getLastWrittenIndex(currentIndex, arraySize);
        priceNow = priceArray[lastWrittenIdx];
    } else {
        // Geconsolideerde check: early return als currentIndex == 0
        if (currentIndex == 0) {
            return 0.0f;
        }
        priceNow = priceArray[currentIndex - 1];
    }
    
    // Get price X positions ago (geconsolideerde logica)
    float priceXAgo;
    if (arrayFilled) {
        int32_t idxXAgo = getRingBufferIndexAgo(currentIndex, positionsAgo, arraySize);
        if (idxXAgo < 0) {
            Serial_printf("%s FATAL: idxXAgo invalid, currentIndex=%u\n", logPrefix, currentIndex);
            return 0.0f;
        }
        priceXAgo = priceArray[idxXAgo];
    } else {
        // Geconsolideerde check: early return als niet genoeg data
        if (currentIndex < positionsAgo) {
            return 0.0f;
        }
        priceXAgo = priceArray[currentIndex - positionsAgo];
    }
    
    // Validate prices
    if (!areValidPrices(priceNow, priceXAgo)) {
        updateAveragePrice(0.0f);
        Serial_printf("%s ERROR: priceNow=%.2f, priceXAgo=%.2f - invalid!\n", logPrefix, priceNow, priceXAgo);
        return 0.0f;
    }
    
    // Calculate average for display (if requested)
    if (averagePrices != nullptr && averagePriceIndex < 3 && averagePriceIndex == 1) {
        // For 1m: use calculateAverage helper
        averagePrices[1] = calculateAverage(this->getSecondPrices(), SECONDS_PER_MINUTE, this->getSecondArrayFilled());
    }
    
    // Return percentage: (now - X ago) / X ago * 100
    return ((priceNow - priceXAgo) / priceXAgo) * 100.0f;
}



