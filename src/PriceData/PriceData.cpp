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
// Forward declaration updated: calculateAverage heeft nu currentIndex parameter
extern float calculateAverage(float *array, uint8_t size, bool filled, uint8_t currentIndex);
extern uint32_t getLastWrittenIndex(uint32_t currentIndex, uint32_t arraySize);

// Forward declaration voor macro (moet in .ino blijven)
#ifndef VALUES_FOR_1MIN_RETURN
#define VALUES_FOR_1MIN_RETURN 30  // Fallback: 60000 / 2000
#endif

// Forward declaration voor Serial_printf macro
#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

float PriceData::calculateReturn1Minute(float* averagePrices) {
    const float* priceArray = this->getSecondPrices();
    uint16_t arraySize = SECONDS_PER_MINUTE;
    uint16_t currentIndex = this->getSecondIndex();
    bool arrayFilled = this->getSecondArrayFilled();
    uint8_t averagePriceIndex = 1;
    
    uint16_t count = arrayFilled ? arraySize : currentIndex;
    if (count < 2) {
        if (averagePrices != nullptr && averagePriceIndex < 3) {
            averagePrices[averagePriceIndex] = 0.0f;
        }
        return 0.0f;
    }
    
    // Bereken gemiddelde prijs voor weergave
    if (averagePrices != nullptr && averagePriceIndex < 3) {
        averagePrices[averagePriceIndex] = calculateAverage(this->getSecondPrices(), arraySize, arrayFilled, currentIndex);
    }
    
    // Lineaire regressie over de laatste minuut
    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    float sumX2 = 0.0f;
    uint16_t validPoints = 0;
    
    float stepSeconds = 60.0f / (float)SECONDS_PER_MINUTE;
    uint16_t lastWrittenIdx = arrayFilled ? getLastWrittenIndex(currentIndex, arraySize) : (currentIndex > 0 ? (currentIndex - 1) : 0);
    uint16_t startIdx = arrayFilled ? ((lastWrittenIdx + arraySize - (count - 1)) % arraySize) : 0;
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = arrayFilled ? ((startIdx + i) % arraySize) : i;
        float price = priceArray[idx];
        if (price <= 0.0f) {
            continue;
        }
        float x = (float)i * stepSeconds; // tijd in seconden
        sumX += x;
        sumY += price;
        sumXY += x * price;
        sumX2 += x * x;
        validPoints++;
    }
    
    if (validPoints < 2) {
        return 0.0f;
    }
    
    float denom = ((float)validPoints * sumX2) - (sumX * sumX);
    if (fabsf(denom) < 1e-6f) {
        return 0.0f;
    }
    float slope = (((float)validPoints * sumXY) - (sumX * sumY)) / denom; // prijs per seconde
    float avgPrice = sumY / (float)validPoints;
    if (avgPrice <= 0.0f) {
        return 0.0f;
    }
    float slopePerMinute = slope * 60.0f;
    float pctPerMinute = (slopePerMinute / avgPrice) * 100.0f;
    return pctPerMinute;
}



