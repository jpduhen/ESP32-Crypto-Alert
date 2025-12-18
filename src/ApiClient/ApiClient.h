#ifndef APICLIENT_H
#define APICLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// API Configuration constants
#define HTTP_TIMEOUT_MS_DEFAULT 1200  // HTTP timeout (1200ms = 80% van API interval 1500ms)
#define HTTP_CONNECT_TIMEOUT_MS_DEFAULT 1000  // Connect timeout (1000ms)

// ApiClient class - voor HTTP API calls naar Binance
// Fase 4.1.3: httpGET() toegevoegd als private method, test methode toegevoegd
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
    // symbol: Binance symbol (bijv. "BTCEUR")
    // out: output parameter voor prijs
    // Returns: true bij succes, false bij fout
    bool fetchBinancePrice(const char* symbol, float& out);
    
    // Helper functions (static, kunnen ook buiten class gebruikt worden)
    static bool isValidPrice(float price);
    static bool safeAtof(const char* str, float& out);
    
private:
    // Private HTTP GET implementation
    bool httpGETInternal(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs);
};

#endif // APICLIENT_H
