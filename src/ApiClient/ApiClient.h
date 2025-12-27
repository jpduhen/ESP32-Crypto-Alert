#ifndef APICLIENT_H
#define APICLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// S0: FreeRTOS headers voor SemaphoreHandle_t
#include <freertos/semphr.h>

// S1: ArduinoJson support voor streaming parsing (optioneel)
#if __has_include(<ArduinoJson.h>)
    #include <ArduinoJson.h>
    #define USE_ARDUINOJSON_STREAMING 1
#else
    #define USE_ARDUINOJSON_STREAMING 0
#endif

// API Configuration constants
// T1: Verhoogde connect/read timeouts voor betere stabiliteit
#define HTTP_CONNECT_TIMEOUT_MS_DEFAULT 4000  // Connect timeout (4000ms)
#define HTTP_READ_TIMEOUT_MS_DEFAULT 4000     // Read timeout (4000ms)
#define HTTP_TIMEOUT_MS_DEFAULT HTTP_READ_TIMEOUT_MS_DEFAULT  // Backward compatibility: totale timeout = read timeout

// M2: Extern declaratie voor globale response buffer (gedefinieerd in .ino)
extern char gApiResp[384];  // Verkleind van 512 naar 384 bytes

// S0: Extern declaratie voor netwerk mutex (gedefinieerd in .ino)
extern SemaphoreHandle_t gNetMutex;

// C2: Extern declaraties voor netwerk mutex helpers met debug logging (gedefinieerd in .ino)
extern void netMutexLock(const char* taskName);
extern void netMutexUnlock(const char* taskName);

// ApiClient class - voor HTTP API calls naar Binance
// Fase 4.1.3: httpGET() toegevoegd als private method, test methode toegevoegd
// N2: Persistent HTTPClient en WiFiClient voor keep-alive connecties
class ApiClient {
public:
    ApiClient();
    
    // Initialisatie (lege implementatie voor nu)
    void begin();
    
    // Public HTTP GET method
    // Fase 4.1.4: Eerste integratie - gebruikt in fetchPrice()
    bool httpGET(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs = HTTP_TIMEOUT_MS_DEFAULT);
    
    // Parse Binance price from JSON response
    // Fase 4.1.6: Verplaatst naar ApiClient
    static bool parseBinancePrice(const char *body, float &out);
    
    // High-level method: Fetch Binance price for a symbol
    // Fase 4.1.7: Combineert httpGET + parseBinancePrice
    // S1: Gebruikt nu streaming JSON parsing (geen buffer allocatie)
    // symbol: Binance symbol (bijv. "BTCEUR")
    // out: output parameter voor prijs
    // Returns: true bij succes, false bij fout
    bool fetchBinancePrice(const char* symbol, float& out);
    
    // S1: Streaming JSON parsing helper (gebruikt ArduinoJson als beschikbaar)
    // Parse price direct van WiFiClient stream zonder body buffer
    // Returns: true bij succes, false bij fout
    bool parseBinancePriceFromStream(WiFiClient* stream, float& out);
    
    // Helper functions (static, kunnen ook buiten class gebruikt worden)
    static bool isValidPrice(float price);
    static bool safeAtof(const char* str, float& out);
    
private:
    // N2: Persistent HTTPClient en WiFiClient voor keep-alive connecties
    WiFiClient wifiClient;
    HTTPClient httpClient;
    
    // Private HTTP GET implementation
    bool httpGETInternal(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs);
};

#endif // APICLIENT_H



