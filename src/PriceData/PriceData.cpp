#include "PriceData.h"
#include "../ApiClient/ApiClient.h"  // Voor ApiClient::isValidPrice()
// DEBUG_CALCULATIONS wordt gedefinieerd in PriceData.h (forward declaration)
// Als platform_config.h al geïncludeerd is in .ino, zou DEBUG_CALCULATIONS al gedefinieerd moeten zijn
// Als het niet gedefinieerd is, betekent dit dat PriceData.h het op 0 heeft gezet
// We kunnen het hier niet opnieuw definiëren omdat PriceData.h al geïncludeerd is

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
extern bool isValidPrice(float price);  // Voor price validatie
// Forward declaration updated: calculateAverage heeft nu currentIndex parameter
extern float calculateAverage(float *array, uint8_t size, bool filled, uint8_t currentIndex);

// Forward declaration voor macro (moet in .ino blijven)
#ifndef VALUES_FOR_1MIN_RETURN
#define VALUES_FOR_1MIN_RETURN 30  // Fallback: 60000 / 2000
#endif

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

float PriceData::calculateReturn1Minute(float* averagePrices) {
    // 1m trend: lineaire regressie over de laatste 1 minuut (VALUES_FOR_1MIN_RETURN samples)
    const float* priceArray = this->getSecondPrices();
    uint16_t arraySize = SECONDS_PER_MINUTE;
    uint16_t currentIndex = this->getSecondIndex();
    bool arrayFilled = this->getSecondArrayFilled();
    uint16_t windowSamples = VALUES_FOR_1MIN_RETURN;
    const char* logPrefix = "[Ret1m]";
    uint32_t logIntervalMs = 10000;
    uint8_t averagePriceIndex = 1;
    
    auto updateAveragePrice = [&](float value) {
        if (averagePrices != nullptr && averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = value;
        }
    };
    
    uint16_t availableSamples = arrayFilled ? arraySize : currentIndex;
    if (availableSamples < windowSamples) {
        updateAveragePrice(0.0f);
        if (logIntervalMs > 0) {
            static uint32_t lastLogTime = 0;
            uint32_t now = millis();
            if (now - lastLogTime > logIntervalMs) {
                Serial_printf("%s Wachten op data: index=%u (nodig: %u)\n", logPrefix, currentIndex, windowSamples);
                lastLogTime = now;
            }
        }
        return 0.0f;
    }
    
    uint16_t lastWrittenIdx;
    if (arrayFilled) {
        lastWrittenIdx = getLastWrittenIndex(currentIndex, arraySize);
    } else {
        if (currentIndex == 0) {
            return 0.0f;
        }
        lastWrittenIdx = currentIndex - 1;
    }
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint16_t validPoints = 0;
    float avgSum = 0.0f;
    uint16_t avgCount = 0;
    
    for (uint16_t i = 0; i < windowSamples; i++) {
        uint16_t idx;
        if (!arrayFilled) {
            if (i >= currentIndex) break;
            idx = currentIndex - 1 - i;  // newest -> oldest
        } else {
            int32_t idx_temp = getRingBufferIndexAgo(lastWrittenIdx, i, arraySize);
            if (idx_temp < 0) break;
            idx = (uint16_t)idx_temp;
        }
        
        float price = priceArray[idx];
        if (isValidPrice(price)) {
            // i=0 is newest, so reverse x to make 0 = oldest
            float x = (float)(windowSamples - 1 - i);
            float y = price;
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
            avgSum += price;
            avgCount++;
            validPoints++;
        }
    }
    
    if (validPoints < 2 || avgCount == 0) {
        updateAveragePrice(0.0f);
        return 0.0f;
    }
    
    float avgPrice = avgSum / avgCount;
    updateAveragePrice(avgPrice);
    
    float n = (float)validPoints;
    float denominator = (n * sumX2) - (sumX * sumX);
    if (fabsf(denominator) < 0.0001f || avgPrice <= 0.0f) {
        return 0.0f;
    }
    
    float slope = ((n * sumXY) - (sumX * sumY)) / denominator;  // price per second
    float pctPerMinute = (slope * 60.0f / avgPrice) * 100.0f;
    return pctPerMinute;
}



