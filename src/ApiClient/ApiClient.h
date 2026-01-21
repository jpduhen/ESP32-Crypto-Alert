#ifndef APICLIENT_H
#define APICLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Include alleen DEBUG flags, niet de hele platform_config.h (voorkomt PINS includes)
// Deze worden gedefinieerd in platform_config.h, hier alleen guards voor backward compatibility
// Als platform_config.h al geïncludeerd is, worden deze guards niet uitgevoerd
#ifndef DEBUG_BUTTON_ONLY
#define DEBUG_BUTTON_ONLY 1
#endif
// DEBUG_CALCULATIONS wordt gedefinieerd in platform_config.h
// Als het daar niet gedefinieerd is, gebruik dan default 0 (geen debug logging)
#ifndef DEBUG_CALCULATIONS
#define DEBUG_CALCULATIONS 0
#endif

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
// T1: Geoptimaliseerde timeouts voor snellere API calls (verlaagd voor betere responsiviteit)
// Connect timeout: 2000ms (genoeg voor meeste netwerken, maar sneller dan 4000ms)
// Read timeout: 2500ms (genoeg voor kleine responses, maar sneller dan 4000ms)
#define HTTP_CONNECT_TIMEOUT_MS_DEFAULT 2000  // Connect timeout (2000ms - geoptimaliseerd)
#define HTTP_READ_TIMEOUT_MS_DEFAULT 2500     // Read timeout (2500ms - geoptimaliseerd voor kleine responses)
#define HTTP_TIMEOUT_MS_DEFAULT HTTP_READ_TIMEOUT_MS_DEFAULT  // Backward compatibility: totale timeout = read timeout

// M2: Extern declaratie voor globale response buffer (gedefinieerd in .ino)
extern char gApiResp[304];  // Verkleind van 320 naar 304 bytes (bespaart 16 bytes DRAM)

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
    
    // Parse Bitvavo price from JSON response
    // Bitvavo response format: [{"market":"BTC-EUR","price":"34243"}]
    static bool parseBitvavoPrice(const char *body, float &out);
    
    // High-level method: Fetch Bitvavo price for a market
    // Fase 4.1.7: Combineert httpGET + parseBitvavoPrice
    // S1: Gebruikt nu streaming JSON parsing (geen buffer allocatie)
    // symbol: Bitvavo market (bijv. "BTC-EUR")
    // out: output parameter voor prijs
    // Returns: true bij succes, false bij fout
    bool fetchBitvavoPrice(const char* symbol, float& out);
    
    // S1: Streaming JSON parsing helper (gebruikt ArduinoJson als beschikbaar)
    // Parse price direct van WiFiClient stream zonder body buffer
    // Bitvavo response format: [{"market":"BTC-EUR","price":"34243"}]
    // Returns: true bij succes, false bij fout
    bool parseBitvavoPriceFromStream(WiFiClient* stream, float& out);
    
    // Helper functions (static, kunnen ook buiten class gebruikt worden)
    static bool isValidPrice(float price);
    static bool safeAtof(const char* str, float& out);
    
    // Helper: Log HTTP error (consolideert error logging logica)
    static void logHttpError(int code, const char* phase, unsigned long requestTime, 
                            uint8_t attempt = 0, uint8_t maxAttempts = 1, 
                            const char* prefix = "[HTTP]");
    
    // Helper: Detect HTTP error phase (consolideert fase detectie)
    static const char* detectHttpErrorPhase(int code);
    
private:
    // N2: Persistent HTTPClient en WiFiClient voor keep-alive connecties
    WiFiClient wifiClient;
    HTTPClient httpClient;
    
    // Private HTTP GET implementation
    bool httpGETInternal(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs);
};

#endif // APICLIENT_H



