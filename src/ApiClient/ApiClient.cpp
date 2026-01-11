#include "ApiClient.h"
#include "../Memory/HeapMon.h"
#include "../Net/HttpFetch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Fallback: als macro niet beschikbaar is in de header (bijv. in oudere branches)
#ifndef APICLIENT_PRICE_KEEPALIVE
#define APICLIENT_PRICE_KEEPALIVE 0
#endif

// Constructor - initialiseer persistent clients
ApiClient::ApiClient() {
    // N2: Persistent clients worden automatisch geïnitialiseerd
}

// Begin - configureer persistent clients voor keep-alive
void ApiClient::begin() {
    // N2: Configureer HTTPClient voor keep-alive (connection reuse)
    // setReuse(true) wordt per call gedaan in httpGETInternal en fetchBinancePrice
}

// Public HTTP GET method
// Fase 4.1.4: Eerste integratie - gebruikt in fetchPrice()
bool ApiClient::httpGET(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs) {
    return httpGETInternal(url, buffer, bufferSize, timeoutMs);
}

// Private HTTP GET implementation
// Fase 4.1.2: Kopie van oude httpGET() functie voor verificatie
// S0: Wrapped met gNetMutex voor thread-safe HTTP operaties
bool ApiClient::httpGETInternal(const char *url, char *buffer, size_t bufferSize, uint32_t timeoutMs)
{
    if (buffer == nullptr || bufferSize == 0) {
        Serial.printf(F("[HTTP] Invalid buffer parameters\n"));
        return false;
    }
    
    buffer[0] = '\0'; // Initialize buffer
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("ApiClient::httpGETInternal");
    
    const uint8_t MAX_RETRIES = 1; // Max 1 retry (2 pogingen totaal) - verminderd voor snellere failure
    // T1: Backoff retry delays: 250ms voor eerste retry, 500ms voor tweede retry
    const uint32_t RETRY_DELAYS[] = {250, 500}; // Backoff delays in ms
    
    bool result = false;
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        // N2: Gebruik persistent HTTPClient voor keep-alive
        HTTPClient& http = httpClient;
        
        // T1: Expliciete connect/read timeout settings (verhoogd naar 4000ms)
        // Gebruik timeoutMs als read timeout, connect timeout is altijd 4000ms
        http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS_DEFAULT);
        http.setTimeout(timeoutMs > 0 ? timeoutMs : HTTP_READ_TIMEOUT_MS_DEFAULT);
        // N2: Enable connection reuse voor keep-alive
        http.setReuse(true);
        
        unsigned long requestStart = millis();
        bool attemptOk = false;
        bool shouldRetry = false;  // S2: Flag voor retry logica (buiten do-while)
        int lastCode = 0;  // S2: Bewaar code voor retry logica
        
        // S2: do-while(0) patroon voor consistente cleanup per attempt
        do {
            // N2: Reset client state voor nieuwe request (belangrijk voor keep-alive)
            // Als vorige request gefaald is of verbinding niet meer geldig, reset client
            if (http.connected()) {
                http.end();  // Sluit vorige verbinding als die nog open is
            }
            
            // N2: Gebruik persistent WiFiClient voor keep-alive
            if (!http.begin(wifiClient, url)) {
            if (attempt == MAX_RETRIES) {
                    // S2: Log zonder String concatenatie
                    Serial.printf(F("[HTTP] http.begin() gefaald na %d pogingen voor URL: %s\n"), attempt + 1, url);
                }
                shouldRetry = (attempt < MAX_RETRIES);  // Retry bij begin failure
                break;
            }
            
            // N2: Voeg User-Agent header toe om Cloudflare blocking te voorkomen
            http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
            http.addHeader(F("Accept"), F("application/json"));
            
            // M1: Heap telemetry vóór HTTP GET (intern)
            #if APICLIENT_HEAP_LOG
            logHeap("HTTP_GET_PRE");
            #endif
        
        int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
            lastCode = code;  // S2: Bewaar voor retry logica
            
            // M1: Heap telemetry na HTTP GET (intern)
            #if APICLIENT_HEAP_LOG
            logHeap("HTTP_GET_POST");
            #endif
            
            if (code != 200) {
                // Geconsolideerde retry check: check alle retry-waardige fouten in één keer
                shouldRetry = (code == HTTPC_ERROR_CONNECTION_REFUSED || 
                              code == HTTPC_ERROR_CONNECTION_LOST || 
                              code == HTTPC_ERROR_READ_TIMEOUT ||
                              code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                              code == HTTPC_ERROR_SEND_PAYLOAD_FAILED);
                
                // Geoptimaliseerd: gebruik helper functie voor error logging
                const char* phase = detectHttpErrorPhase(code);
                logHttpError(code, phase, requestTime, attempt, MAX_RETRIES + 1, "[HTTP]");
                
                // S2: Break uit do-while voor cleanup, retry logica gebeurt hieronder
                break;
            }
            
            // Get response size first
            int contentLength = http.getSize();
            if (contentLength > 0 && (size_t)contentLength >= bufferSize) {
                Serial.printf(F("[HTTP] Response te groot: %d bytes (buffer: %zu bytes)\n"), contentLength, bufferSize);
                break;
            }
            
            // M2: Read response body via streaming (vervangt getString() fallback)
            // M1: Heap telemetry vóór body read
            #if APICLIENT_HEAP_LOG
            logHeap("HTTP_BODY_READ_PRE");
            #endif
            
            // Read response into buffer via streaming
            WiFiClient *stream = http.getStreamPtr();
            if (stream == nullptr) {
                Serial.println(F("[HTTP] Stream pointer is null"));
                break;
            }
            
            size_t bytesRead = 0;
            const size_t CHUNK_SIZE = 256;  // Lees in chunks
            const uint32_t readTimeoutMs = (timeoutMs > 0) ? timeoutMs : HTTP_READ_TIMEOUT_MS_DEFAULT;
            unsigned long readStart = millis();
            
            // Read in chunks: continue zolang stream connected/available
            while (http.connected() && bytesRead < (bufferSize - 1)) {
                size_t remaining = bufferSize - 1 - bytesRead;
                size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
                
                size_t n = stream->readBytes((uint8_t*)(buffer + bytesRead), chunkSize);
                if (n == 0) {
                    if (!stream->available()) {
                        break;
                    }
                    if ((millis() - readStart) > readTimeoutMs) {
                        break;
                    }
                    delay(10);  // Wacht kort op meer data
                    continue;
                }
                bytesRead += n;
                readStart = millis();  // reset timeout bij ontvangst data
                }
                buffer[bytesRead] = '\0';
                
                // Performance monitoring: log langzame calls
                if (requestTime > 1000) {
                Serial.printf(F("[HTTP] Langzame response: %lu ms\n"), requestTime);
            }
            
            // M1: Heap telemetry na body read
            #if APICLIENT_HEAP_LOG
            logHeap("HTTP_BODY_READ_POST");
            #endif
            
            attemptOk = true;
            result = true;
        } while(0);
        
        // C2: ALTIJD cleanup (ook bij succes) - HTTPClient op ESP32 vereist dit voor correcte reset
        // Hard close: http.end() + client.stop() voor volledige cleanup
        WiFiClient* stream = http.getStreamPtr();
        if (stream != nullptr) {
            stream->stop();
        }
        http.end();
        
        // S2: Succes - stop retries
        if (attemptOk) {
            if (attempt > 0) {
                Serial.printf(F("[HTTP] Succes na retry (poging %d/%d)\n"), attempt + 1, MAX_RETRIES + 1);
            }
            break;
        }
        
        // T1: Retry logica met backoff delays (alleen bij retry-waardige fouten en als attempt < MAX_RETRIES)
            if (shouldRetry && attempt < MAX_RETRIES) {
            uint32_t backoffDelay = (attempt < sizeof(RETRY_DELAYS)/sizeof(RETRY_DELAYS[0])) ? RETRY_DELAYS[attempt] : 500;
            Serial.printf(F("[HTTP] Retry %d/%d na %lu ms backoff\n"), attempt + 1, MAX_RETRIES, backoffDelay);
            delay(backoffDelay);  // T1: Backoff delay tussen retries
                continue;
            }
            
            // Geen retry meer mogelijk of niet-retry-waardige fout
        result = false;
        break;
        }
    
    // C2: Geef netwerk mutex vrij (met debug logging)
    netMutexUnlock("ApiClient::httpGETInternal");
    
    return result;
}

// Parse Binance price from JSON response
// Fase 4.1.6: Verplaatst naar ApiClient (parallel, nog niet gebruikt)
// Handmatige JSON parsing (geen heap allocaties, geen ArduinoJson dependency)
// Geoptimaliseerd: geconsolideerde validatie checks
bool ApiClient::parseBinancePrice(const char *body, float &out)
{
    // Geconsolideerde validatie: check alle voorwaarden in één keer
    if (body == nullptr || strlen(body) == 0) {
        return false;
    }
    
    const char *priceStart = strstr(body, "\"price\":\"");
    if (priceStart == nullptr) {
        return false;
    }
    
    priceStart += 9; // skip to first digit after "price":""
    
    const char *priceEnd = strchr(priceStart, '"');
    if (priceEnd == nullptr) {
        return false;
    }
    
    // Valideer lengte (geconsolideerd: check alles in één keer)
    size_t priceLen = priceEnd - priceStart;
    if (priceLen == 0 || priceLen > 20 || priceLen >= 32) {  // Max 20 karakters voor prijs, buffer is 32
        return false;
    }
    
    // Extract price string (gebruik stack buffer, geen heap allocatie)
    char priceStr[32];
    strncpy(priceStr, priceStart, priceLen);
    priceStr[priceLen] = '\0';
    
    // Convert to float en valideer in één keer
    float val;
    if (!safeAtof(priceStr, val) || !isValidPrice(val)) {
        return false;
    }
    
    out = val;
    return true;
}

// Helper: Detect HTTP error phase (consolideert fase detectie)
// Geoptimaliseerd: elimineert code duplicatie
const char* ApiClient::detectHttpErrorPhase(int code)
{
    if (code == HTTPC_ERROR_CONNECTION_REFUSED || code == HTTPC_ERROR_CONNECTION_LOST) {
        return "connect";
    } else if (code == HTTPC_ERROR_READ_TIMEOUT) {
        return "read";
    } else if (code == HTTPC_ERROR_SEND_HEADER_FAILED || code == HTTPC_ERROR_SEND_PAYLOAD_FAILED) {
        return "send";
    }
    return "unknown";
}

// Helper: Log HTTP error (consolideert error logging logica)
// Geoptimaliseerd: elimineert code duplicatie tussen httpGETInternal en fetchBinancePrice
void ApiClient::logHttpError(int code, const char* phase, unsigned long requestTime, 
                            uint8_t attempt, uint8_t maxAttempts, const char* prefix)
{
    if (code < 0) {
        // T2: Gebruik errorToString voor leesbare error messages (deterministisch, geen mojibake)
        static HTTPClient errorHttp;
        String localErr = errorHttp.errorToString(code);
        if (maxAttempts > 1) {
            Serial.printf(F("%s HTTP error (code=%d, fase=%s, tijd=%lu ms, poging %d/%d, error=%s)\n"), 
                         prefix, code, phase, requestTime, attempt + 1, maxAttempts, localErr.c_str());
        } else {
            Serial.printf(F("%s HTTP error (code=%d, fase=%s, tijd=%lu ms, error=%s)\n"), 
                         prefix, code, phase, requestTime, localErr.c_str());
        }
    } else {
        if (maxAttempts > 1) {
            Serial.printf(F("%s Status code=%d, tijd=%lu ms, poging %d/%d\n"), 
                         prefix, code, requestTime, attempt + 1, maxAttempts);
        } else {
            Serial.printf(F("%s HTTP status code=%d, tijd=%lu ms\n"), prefix, code, requestTime);
        }
    }
}

// Helper: Validate price value
bool ApiClient::isValidPrice(float price)
{
    // Geconsolideerde validatie: check alle voorwaarden in één keer
    if (isnan(price) || isinf(price) || price <= 0.0f || price > 1000000.0f) {
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

// S1: Streaming JSON parsing helper (gebruikt ArduinoJson als beschikbaar)
// Parse price direct van WiFiClient stream zonder body buffer
// S2: Gebruikt do-while(0) patroon voor consistente cleanup
bool ApiClient::parseBinancePriceFromStream(WiFiClient* stream, float& out)
{
    if (stream == nullptr) {
        return false;
    }
    
    bool ok = false;
    
    #if USE_ARDUINOJSON_STREAMING
    // Gebruik ArduinoJson voor streaming parsing (geen heap allocaties)
    StaticJsonDocument<256> doc;  // 256 bytes is ruim voldoende voor Binance price response (~100 bytes)
    DeserializationError error = deserializeJson(doc, *stream);
    
    if (error) {
        Serial.printf(F("[HTTP] JSON parse error: %s\n"), error.c_str());
        return false;
    }
    
    // Extract price field
    if (!doc.containsKey("price")) {
        Serial.println(F("[HTTP] JSON missing 'price' field"));
        return false;
    }
    
    const char* priceStr = doc["price"];
    if (priceStr == nullptr) {
        Serial.println(F("[HTTP] JSON 'price' field is null"));
        return false;
    }
    
    // Convert to float
    float val;
    if (!safeAtof(priceStr, val)) {
        return false;
    }
    
    // Validate price
    if (!isValidPrice(val)) {
        return false;
    }
    
    out = val;
    ok = true;
    #else
    // Fallback: handmatige parsing (als ArduinoJson niet beschikbaar is)
    // Lees response in kleine chunks en parse handmatig
    char buffer[128];  // Stack buffer voor kleine responses
    size_t bytesRead = 0;
    const size_t MAX_READ = sizeof(buffer) - 1;
    
    // Lees response body in chunks met timeout
    unsigned long readStart = millis();
    while (bytesRead < MAX_READ) {
        if (stream->available()) {
            size_t n = stream->readBytes((uint8_t*)(buffer + bytesRead), MAX_READ - bytesRead);
            if (n == 0) {
                delay(10);
                if ((millis() - readStart) > HTTP_READ_TIMEOUT_MS_DEFAULT) {
                    break;
                }
                continue;
            }
            bytesRead += n;
            readStart = millis();  // reset timeout bij ontvangst data
            continue;
        }
        if ((millis() - readStart) > HTTP_READ_TIMEOUT_MS_DEFAULT) {
            break;
        }
        delay(10);
    }
    buffer[bytesRead] = '\0';
    
    // Parse met bestaande handmatige parser
    ok = parseBinancePrice(buffer, out);
    #endif
    
    return ok;
}

// High-level method: Fetch Binance price for a symbol
// Fase 4.1.7: Combineert httpGET + parseBinancePrice
// S1: Gebruikt nu streaming JSON parsing (geen buffer allocatie)
// S2: Gebruikt do-while(0) patroon voor consistente cleanup
bool ApiClient::fetchBinancePrice(const char* symbol, float& out)
{
    if (symbol == nullptr || strlen(symbol) == 0) {
        return false;
    }
    
    // M1: Heap telemetry vóór URL build
    #if APICLIENT_HEAP_LOG
    logHeap("API_URL_BUILD");
    #endif
    
    // Build Binance API URL
    char url[128];
    snprintf(url, sizeof(url), "https://api.binance.com/api/v3/ticker/price?symbol=%s", symbol);
    
    // M1: Heap telemetry vóór HTTP GET
    #if APICLIENT_HEAP_LOG
    logHeap("API_GET_PRE");
    #endif
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("ApiClient::fetchBinancePrice");
    
    const uint8_t MAX_RETRIES = 1; // Max 1 retry (2 pogingen totaal)
    const uint32_t RETRY_DELAYS[] = {250, 750}; // Backoff delays in ms (extra delay voor rate limiting)
    bool ok = false;
    
    bool usePersistent = (APICLIENT_PRICE_KEEPALIVE != 0);
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        bool attemptOk = false;
        bool shouldRetry = false;
        int lastCode = 0;
        
        // Gebruik persistent client als keep-alive aan staat, anders lokaal object
        HTTPClient localHttp;
        HTTPClient& http = usePersistent ? httpClient : localHttp;
        
        // S2: do-while(0) patroon voor consistente cleanup per attempt
        do {
            // T1: Expliciete connect/read timeout settings (verhoogd naar 4000ms)
            http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS_DEFAULT);
            http.setTimeout(HTTP_READ_TIMEOUT_MS_DEFAULT);
            // Keep-alive alleen bij persistent client
            http.setReuse(usePersistent);
            
            // N2: Voeg User-Agent header toe VOOR http.begin() om Cloudflare blocking te voorkomen
            // Headers moeten worden toegevoegd voordat de verbinding wordt geopend
            http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
            http.addHeader(F("Accept"), F("application/json"));
            
            unsigned long requestStart = millis();
            
            // Normale URL flow (zoals voorheen, zonder DNS cache)
            #if !DEBUG_BUTTON_ONLY
            Serial.printf(F("[API] Fetching price from: %s\n"), url);
            #endif
            if (usePersistent) {
                if (http.connected()) {
                    http.end();
                }
                if (!http.begin(wifiClient, url)) {
                    #if !DEBUG_BUTTON_ONLY
                    if (attempt == MAX_RETRIES) {
                        Serial.printf(F("[API] http.begin() gefaald voor URL: %s\n"), url);
                    }
                    #endif
                    shouldRetry = (attempt < MAX_RETRIES);
                    break;
                }
            } else if (!http.begin(url)) {
                #if !DEBUG_BUTTON_ONLY
                if (attempt == MAX_RETRIES) {
                    Serial.printf(F("[API] http.begin() gefaald voor URL: %s\n"), url);
                }
                #endif
                shouldRetry = (attempt < MAX_RETRIES);
                break;
            }
            
            int code = http.GET();
            unsigned long requestTime = millis() - requestStart;
            lastCode = code;
            
            // M1: Heap telemetry na HTTP GET
            #if APICLIENT_HEAP_LOG
            logHeap("API_GET_POST");
            #endif
            
            if (code != 200) {
                // Geoptimaliseerd: gebruik helper functie voor error logging
                const char* phase = detectHttpErrorPhase(code);
                logHttpError(code, phase, requestTime, attempt, MAX_RETRIES + 1, "[API]");
                
                // Retry-waardige fouten (netwerk, rate-limit, server errors)
                shouldRetry = (code == HTTPC_ERROR_CONNECTION_REFUSED ||
                              code == HTTPC_ERROR_CONNECTION_LOST ||
                              code == HTTPC_ERROR_READ_TIMEOUT ||
                              code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                              code == HTTPC_ERROR_SEND_PAYLOAD_FAILED ||
                              code == 429 ||
                              (code >= 500 && code < 600));
                
                if (code == 400 && usePersistent) {
                    usePersistent = false;
                    shouldRetry = (attempt < MAX_RETRIES);
                }
                break;
            }
            
            // S1: Parse direct van stream (geen body buffer)
            WiFiClient* stream = http.getStreamPtr();
            if (stream == nullptr) {
                Serial.println(F("[API] Stream pointer is null"));
                break;
            }
            
            // M1: Heap telemetry vóór JSON parse
            #if APICLIENT_HEAP_LOG
            logHeap("API_PARSE_PRE");
            #endif
            
            // Parse price from stream
            if (!parseBinancePriceFromStream(stream, out)) {
                Serial.println(F("[API] JSON parse failed"));
                break;
            }
            
            // M1: Heap telemetry na JSON parse
            #if APICLIENT_HEAP_LOG
            logHeap("API_PARSE_POST");
            #endif
            
            attemptOk = true;
            ok = true;
        } while(0);
        
        // C2: ALTIJD cleanup (ook bij succes) - HTTPClient op ESP32 vereist dit voor correcte reset
        // Hard close: http.end() + client.stop() voor volledige cleanup
        WiFiClient* stream = http.getStreamPtr();
        if (!usePersistent && stream != nullptr) {
            stream->stop();
        }
        http.end();
        
        if (attemptOk) {
            if (attempt > 0) {
                Serial.printf(F("[API] Succes na retry (poging %d/%d)\n"), attempt + 1, MAX_RETRIES + 1);
            }
            break;
        }
        
        if (shouldRetry && attempt < MAX_RETRIES) {
            uint32_t backoffDelay = (attempt < sizeof(RETRY_DELAYS)/sizeof(RETRY_DELAYS[0])) ? RETRY_DELAYS[attempt] : 500;
            if (lastCode == 429 && backoffDelay < 1000) {
                backoffDelay = 1000;
            }
            Serial.printf(F("[API] Retry %d/%d na %lu ms backoff\n"), attempt + 1, MAX_RETRIES, backoffDelay);
            delay(backoffDelay);
            continue;
        }
    }
    
    // C2: Geef netwerk mutex vrij (met debug logging)
    netMutexUnlock("ApiClient::fetchBinancePrice");
    
    return ok;
}
