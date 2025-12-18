#include "ApiClient.h"

// Constructor - leeg voor nu
ApiClient::ApiClient() {
    // Leeg - structuur alleen
}

// Begin - leeg voor nu
void ApiClient::begin() {
    // Leeg - structuur alleen
}

// Public HTTP GET method
// Fase 4.1.4: Eerste integratie - gebruikt in fetchPrice()
bool ApiClient::httpGET(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs) {
    return httpGETInternal(url, buffer, bufferSize, timeoutMs);
}

// Private HTTP GET implementation
// Fase 4.1.2: Kopie van oude httpGET() functie voor verificatie
bool ApiClient::httpGETInternal(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs)
{
    if (buffer == nullptr || bufferSize == 0) {
        Serial.printf(F("[HTTP] Invalid buffer parameters\n"));
        return false;
    }
    
    buffer[0] = '\0'; // Initialize buffer
    
    const uint8_t MAX_RETRIES = 1; // Max 1 retry (2 pogingen totaal) - verminderd voor snellere failure
    const uint32_t RETRY_DELAY_MS = 100; // 100ms delay tussen retries - verlaagd voor betere timing
    
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        HTTPClient http;
        http.setTimeout(timeoutMs);
        http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS_DEFAULT); // Sneller falen bij connect problemen
        // Connection reuse uitgeschakeld - veroorzaakte eerder problemen
        // Bij retries altijd false voor betrouwbaarheid
        http.setReuse(false);
        
        unsigned long requestStart = millis();
        
        if (!http.begin(url))
        {
            http.end();
            if (attempt == MAX_RETRIES) {
                Serial.printf(F("[HTTP] http.begin() gefaald na %d pogingen voor: %s\n"), attempt + 1, url);
                return false;
            }
            // Retry bij begin failure
            delay(RETRY_DELAY_MS);
            continue;
        }
        
        int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
        
        if (code == 200)
        {
            // Get response size first
            int contentLength = http.getSize();
            if (contentLength > 0 && (size_t)contentLength >= bufferSize) {
                Serial.printf(F("[HTTP] Response te groot: %d bytes (buffer: %zu bytes)\n"), contentLength, bufferSize);
                http.end();
                return false;
            }
            
            // Read response into buffer
            WiFiClient *stream = http.getStreamPtr();
            if (stream != nullptr) {
                size_t bytesRead = 0;
                while (stream->available() && bytesRead < bufferSize - 1) {
                    int c = stream->read();
                    if (c < 0) break;
                    buffer[bytesRead++] = (char)c;
                }
                buffer[bytesRead] = '\0';
                
                // Performance monitoring: log langzame calls
                // Normale calls zijn < 500ms, langzaam is > 1000ms
                if (requestTime > 1000) {
                    Serial.printf(F("[HTTP] Langzame response: %lu ms (poging %d)\n"), requestTime, attempt + 1);
                }
            } else {
                // Fallback: stream niet beschikbaar, gebruik getString() maar kopieer direct naar buffer
                // Dit voorkomt String fragmentatie door direct naar buffer te kopiëren
                String payload = http.getString();
                size_t len = payload.length();
                if (len >= bufferSize) {
                    Serial.printf(F("[HTTP] Response te groot: %zu bytes (buffer: %zu bytes)\n"), len, bufferSize);
                    http.end();
                    return false;
                }
                strncpy(buffer, payload.c_str(), bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
            }
            
            http.end();
            if (attempt > 0) {
                Serial.printf(F("[HTTP] Succes na retry (poging %d/%d)\n"), attempt + 1, MAX_RETRIES + 1);
            }
            // Heap telemetry na HTTP fetch (optioneel, alleen bij belangrijke requests)
            // logHeapTelemetry("http");  // Uitgecommentarieerd om spam te voorkomen
            return true;
        }
        else
        {
            // Check of dit een retry-waardige fout is
            bool shouldRetry = false;
            if (code == HTTPC_ERROR_CONNECTION_REFUSED || code == HTTPC_ERROR_CONNECTION_LOST) {
                shouldRetry = true;
                Serial.printf(F("[HTTP] Connectie probleem: code=%d, tijd=%lu ms (poging %d/%d)\n"), code, requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code == HTTPC_ERROR_READ_TIMEOUT) {
                shouldRetry = true;
                Serial.printf(F("[HTTP] Read timeout na %lu ms (poging %d/%d)\n"), requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code == HTTPC_ERROR_SEND_HEADER_FAILED || code == HTTPC_ERROR_SEND_PAYLOAD_FAILED) {
                // Send failures kunnen tijdelijk zijn - retry waardig
                shouldRetry = true;
                Serial.printf(F("[HTTP] Send failure: code=%d, tijd=%lu ms (poging %d/%d)\n"), code, requestTime, attempt + 1, MAX_RETRIES + 1);
            } else if (code < 0) {
                // Andere HTTPClient error codes - retry alleen bij network errors
                shouldRetry = (code == HTTPC_ERROR_CONNECTION_REFUSED || 
                              code == HTTPC_ERROR_CONNECTION_LOST || 
                              code == HTTPC_ERROR_READ_TIMEOUT ||
                              code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                              code == HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                if (!shouldRetry) {
                    Serial.printf(F("[HTTP] Error code=%d, tijd=%lu ms (geen retry)\n"), code, requestTime);
                } else {
                    Serial.printf(F("[HTTP] Error code=%d, tijd=%lu ms (poging %d/%d)\n"), code, requestTime, attempt + 1, MAX_RETRIES + 1);
                }
            } else {
                // HTTP error codes (4xx, 5xx) - geen retry
                Serial.printf(F("[HTTP] GET gefaald: code=%d, tijd=%lu ms (geen retry)\n"), code, requestTime);
            }
            
            http.end();
            
            // Retry bij tijdelijke fouten
            if (shouldRetry && attempt < MAX_RETRIES) {
                delay(RETRY_DELAY_MS);
                continue;
            }
            
            // Geen retry meer mogelijk of niet-retry-waardige fout
            return false;
        }
    }
    
    return false;
}

// Parse Binance price from JSON response
// Fase 4.1.6: Verplaatst naar ApiClient (parallel, nog niet gebruikt)
// Handmatige JSON parsing (geen heap allocaties, geen ArduinoJson dependency)
bool ApiClient::parseBinancePrice(const char *body, float &out)
{
    // Check of body niet leeg is
    if (body == nullptr || strlen(body) == 0)
        return false;
    
    const char *priceStart = strstr(body, "\"price\":\"");
    if (priceStart == nullptr)
        return false;
    
    priceStart += 9; // skip to first digit after "price":""
    
    const char *priceEnd = strchr(priceStart, '"');
    if (priceEnd == nullptr)
        return false;
    
    // Valideer lengte
    size_t priceLen = priceEnd - priceStart;
    if (priceLen == 0 || priceLen > 20) // Max 20 karakters voor prijs (veiligheidscheck)
        return false;
    
    // Extract price string (gebruik stack buffer, geen heap allocatie)
    char priceStr[32];
    if (priceLen >= sizeof(priceStr)) {
        return false;
    }
    strncpy(priceStr, priceStart, priceLen);
    priceStr[priceLen] = '\0';
    
    // Convert to float
    float val;
    if (!safeAtof(priceStr, val)) {
        return false;
    }
    
    // Validate that we got a valid price
    if (!isValidPrice(val))
        return false;
    
    out = val;
    return true;
}

// Helper: Validate price value
bool ApiClient::isValidPrice(float price)
{
    // Check for NaN or Inf
    if (isnan(price) || isinf(price)) {
        return false;
    }
    
    // Check for reasonable price range (0.0001 to 1000000)
    if (price <= 0.0f || price > 1000000.0f) {
        return false;
    }
    
    return true;
}

// Helper: Safe string to float conversion
bool ApiClient::safeAtof(const char* str, float& out)
{
    if (str == nullptr || strlen(str) == 0) {
        return false;
    }
    
    float val = atof(str);
    
    // Check for NaN or Inf
    if (isnan(val) || isinf(val)) {
        return false;
    }
    
    out = val;
    return true;
}

// High-level method: Fetch Binance price for a symbol
// Fase 4.1.7: Combineert httpGET + parseBinancePrice
bool ApiClient::fetchBinancePrice(const char* symbol, float& out)
{
    if (symbol == nullptr || strlen(symbol) == 0) {
        return false;
    }
    
    // Build Binance API URL
    char url[128];
    snprintf(url, sizeof(url), "https://api.binance.com/api/v3/ticker/price?symbol=%s", symbol);
    
    // Fetch response
    char responseBuffer[512]; // Buffer voor API response (Binance responses zijn klein, ~100 bytes)
    bool httpSuccess = httpGET(url, responseBuffer, sizeof(responseBuffer), HTTP_TIMEOUT_MS_DEFAULT);
    
    if (!httpSuccess || strlen(responseBuffer) == 0) {
        return false;
    }
    
    // Parse price from response
    return parseBinancePrice(responseBuffer, out);
}
