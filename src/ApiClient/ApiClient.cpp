#include "ApiClient.h"
#include "../Memory/HeapMon.h"
#include "../Net/HttpFetch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Constructor - initialiseer persistent clients
ApiClient::ApiClient() {
    // N2: Persistent clients worden automatisch geïnitialiseerd
}

// Begin - configureer persistent clients voor keep-alive
void ApiClient::begin() {
    // N2: Configureer HTTPClient voor keep-alive (connection reuse)
    // setReuse(true) wordt per call gedaan in httpGETInternal en fetchBitvavoPrice
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
    
    const uint8_t MAX_RETRIES = 0; // Geen retries voor normale calls (snellere failure, voorkomt langzame calls)
    // T1: Backoff retry delays: alleen voor speciale gevallen (niet gebruikt met MAX_RETRIES=0)
    const uint32_t RETRY_DELAYS[] = {100, 200}; // Backoff delays in ms (verlaagd)
    
    bool result = false;
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        // N2: Gebruik persistent HTTPClient voor keep-alive
        HTTPClient& http = httpClient;
        
        // T1: Expliciete connect/read timeout settings (geoptimaliseerd: 2000ms connect, 2500ms read)
        // Gebruik timeoutMs als read timeout, connect timeout is altijd 2000ms
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
            // N2: Optimaliseer keep-alive: alleen reset als verbinding niet meer geldig is
            // Check of verbinding nog actief is voordat we resetten (bespaart tijd)
            bool connectionActive = http.connected();
            if (connectionActive) {
                // Verbinding is nog actief, probeer te hergebruiken (keep-alive optimalisatie)
                // Alleen resetten als vorige request gefaald is (retry)
                if (attempt > 0) {
                    http.end();  // Reset bij retry
                    connectionActive = false;
                }
            }
            
            // N2: Gebruik persistent WiFiClient voor keep-alive
            // Alleen http.begin() aanroepen als verbinding niet actief is
            if (!connectionActive) {
                if (!http.begin(wifiClient, url)) {
                    if (attempt == MAX_RETRIES) {
                        // S2: Log zonder String concatenatie
                        Serial.printf(F("[HTTP] http.begin() gefaald na %d pogingen voor URL: %s\n"), attempt + 1, url);
                    }
                    shouldRetry = (attempt < MAX_RETRIES);  // Retry bij begin failure
                    break;
                }
            }
            
            // N2: Voeg User-Agent header toe om Cloudflare blocking te voorkomen
            // Headers toevoegen voor elke request (ook bij keep-alive reuse)
            http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
            http.addHeader(F("Accept"), F("application/json"));
            
            // M1: Heap telemetry vóór HTTP GET (intern)
            logHeap("HTTP_GET_PRE");
        
        int code = http.GET();
        unsigned long requestTime = millis() - requestStart;
            lastCode = code;  // S2: Bewaar voor retry logica
            
            // M1: Heap telemetry na HTTP GET (intern)
            logHeap("HTTP_GET_POST");
            
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
            logHeap("HTTP_BODY_READ_PRE");
            
            // Read response into buffer via streaming
            WiFiClient *stream = http.getStreamPtr();
            if (stream == nullptr) {
                Serial.println(F("[HTTP] Stream pointer is null"));
                break;
            }
            
                size_t bytesRead = 0;
            const size_t CHUNK_SIZE = 256;  // Lees in chunks
            unsigned long readStart = millis();
            const unsigned long READ_TIMEOUT_MS = timeoutMs > 0 ? timeoutMs : HTTP_READ_TIMEOUT_MS_DEFAULT;
            
            // Read in chunks: continue zolang stream connected/available en binnen timeout
            while (http.connected() && bytesRead < (bufferSize - 1) && (millis() - readStart) < READ_TIMEOUT_MS) {
                size_t remaining = bufferSize - 1 - bytesRead;
                size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
                
                size_t n = stream->readBytes((uint8_t*)(buffer + bytesRead), chunkSize);
                if (n == 0) {
                    if (!stream->available()) {
                        // Geen data beschikbaar, check timeout
                        if ((millis() - readStart) >= READ_TIMEOUT_MS) {
                            break;  // Timeout bereikt
                        }
                        delay(5);  // Kortere delay voor snellere response (was 10ms)
                        continue;
                    }
                }
                bytesRead += n;
                }
                buffer[bytesRead] = '\0';
                
                // Performance monitoring: log langzame calls
                if (requestTime > 1000) {
                Serial.printf(F("[HTTP] Langzame response: %lu ms\n"), requestTime);
            }
            
            // M1: Heap telemetry na body read
            logHeap("HTTP_BODY_READ_POST");
            
            attemptOk = true;
            result = true;
        } while(0);
        
        // N2: Keep-alive optimalisatie: alleen cleanup bij fouten of als verbinding niet meer actief
        // Bij succes: laat verbinding open voor keep-alive (snellere volgende requests)
        if (attemptOk) {
            // Succes: alleen cleanup als verbinding niet meer actief (voor keep-alive reuse)
            if (!http.connected()) {
                http.end();  // Alleen cleanup als verbinding al gesloten is
            }
            // Anders: laat verbinding open voor keep-alive (http.end() wordt later aangeroepen bij volgende request of bij fout)
            
            if (attempt > 0) {
                Serial.printf(F("[HTTP] Succes na retry (poging %d/%d)\n"), attempt + 1, MAX_RETRIES + 1);
            }
            break;
        } else {
            // Fout: altijd cleanup om verbinding te resetten
            http.end();
            WiFiClient* stream = http.getStreamPtr();
            if (stream != nullptr) {
                stream->stop();
            }
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
// FASE 1.1: Debug logging toegevoegd voor verificatie
bool ApiClient::parseBitvavoPrice(const char *body, float &out)
{
    // Bitvavo response format: [{"market":"BTC-EUR","price":"34243"}]
    // Geconsolideerde validatie: check alle voorwaarden in één keer
    if (body == nullptr || strlen(body) == 0) {
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPrice: body is null or empty"));
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPrice: raw JSON body (first 100 chars): %.100s\n"), body);
    #endif
    
    // Bitvavo retourneert een array met objecten, zoek naar "price":" in het eerste object
    const char *priceStart = strstr(body, "\"price\":\"");
    if (priceStart == nullptr) {
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPrice: 'price' field not found in JSON"));
        #endif
        return false;
    }
    
    priceStart += 9; // skip to first digit after "price":""
    
    const char *priceEnd = strchr(priceStart, '"');
    if (priceEnd == nullptr) {
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPrice: price field not properly terminated"));
        #endif
        return false;
    }
    
    // Valideer lengte (geconsolideerd: check alles in één keer)
    size_t priceLen = priceEnd - priceStart;
    if (priceLen == 0 || priceLen > 20 || priceLen >= 32) {  // Max 20 karakters voor prijs, buffer is 32
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPrice: invalid price length: %u\n"), priceLen);
        #endif
        return false;
    }
    
    // Extract price string (gebruik stack buffer, geen heap allocatie)
    char priceStr[32];
    strncpy(priceStr, priceStart, priceLen);
    priceStr[priceLen] = '\0';
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPrice: extracted price string: '%s' (len=%u)\n"), priceStr, priceLen);
    #endif
    
    // Convert to float en valideer in één keer
    float val;
    if (!safeAtof(priceStr, val)) {
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPrice: safeAtof failed for '%s'\n"), priceStr);
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPrice: converted to float: %.8f\n"), val);
    #endif
    
    if (!isValidPrice(val)) {
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPrice: isValidPrice failed for %.8f\n"), val);
        #endif
        return false;
    }
    
    out = val;
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPrice: SUCCESS - final price: %.2f\n"), out);
    #endif
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
// Geoptimaliseerd: elimineert code duplicatie tussen httpGETInternal en fetchBitvavoPrice
void ApiClient::logHttpError(int code, const char* phase, unsigned long requestTime, 
                            uint8_t attempt, uint8_t maxAttempts, const char* prefix)
{
    if (code < 0) {
        // T2: Gebruik errorToString voor leesbare error messages (deterministisch, geen mojibake)
        String localErr = HTTPClient().errorToString(code);  // Temporary object voor errorToString
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
bool ApiClient::parseBitvavoPriceFromStream(WiFiClient* stream, float& out)
{
    // Bitvavo response format: [{"market":"BTC-EUR","price":"34243"}]
    if (stream == nullptr) {
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: stream is null"));
        #endif
        return false;
    }
    
    bool ok = false;
    
    #if USE_ARDUINOJSON_STREAMING
    // Gebruik ArduinoJson voor streaming parsing (geen heap allocaties)
    StaticJsonDocument<256> doc;  // 256 bytes is ruim voldoende voor Bitvavo price response (~100 bytes)
    DeserializationError error = deserializeJson(doc, *stream);
    
    if (error) {
        Serial.printf(F("[HTTP] JSON parse error: %s\n"), error.c_str());
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: ArduinoJson deserializeJson failed: %s\n"), error.c_str());
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: ArduinoJson parsing successful"));
    #endif
    
    // Bitvavo retourneert een array, pak het eerste element
    if (!doc.is<JsonArray>() || doc.size() == 0) {
        Serial.println(F("[HTTP] JSON is not an array or array is empty"));
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: JSON is not an array or array is empty"));
        #endif
        return false;
    }
    
    JsonObject firstItem = doc[0];
    if (!firstItem.containsKey("price")) {
        Serial.println(F("[HTTP] JSON missing 'price' field"));
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: JSON missing 'price' field"));
        #endif
        return false;
    }
    
    const char* priceStr = firstItem["price"];
    if (priceStr == nullptr) {
        Serial.println(F("[HTTP] JSON 'price' field is null"));
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: JSON 'price' field is null"));
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: extracted price string: '%s'\n"), priceStr);
    #endif
    
    // Convert to float
    float val;
    if (!safeAtof(priceStr, val)) {
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: safeAtof failed for '%s'\n"), priceStr);
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: converted to float: %.8f\n"), val);
    #endif
    
    // Validate price
    if (!isValidPrice(val)) {
        #if DEBUG_CALCULATIONS
        Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: isValidPrice failed for %.8f\n"), val);
        #endif
        return false;
    }
    
    out = val;
    ok = true;
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: SUCCESS - final price: %.2f\n"), out);
    #endif
    #else
    // Fallback: handmatige parsing (als ArduinoJson niet beschikbaar is)
    // Lees response in kleine chunks en parse handmatig
    char buffer[128];  // Stack buffer voor kleine responses
    size_t bytesRead = 0;
    const size_t MAX_READ = sizeof(buffer) - 1;
    
    #if DEBUG_CALCULATIONS
    Serial.println(F("[API][DEBUG] parseBitvavoPriceFromStream: using manual parsing (ArduinoJson not available)"));
    #endif
    
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
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] parseBitvavoPriceFromStream: read %u bytes from stream\n"), bytesRead);
    #endif
    
    // Parse met bestaande handmatige parser
    ok = parseBitvavoPrice(buffer, out);
    #endif
    
    return ok;
}

// High-level method: Fetch Bitvavo price for a market
// Fase 4.1.7: Combineert httpGET + parseBitvavoPrice
// S1: Gebruikt nu streaming JSON parsing (geen buffer allocatie)
// S2: Gebruikt do-while(0) patroon voor consistente cleanup
bool ApiClient::fetchBitvavoPrice(const char* symbol, float& out)
{
    if (symbol == nullptr || strlen(symbol) == 0) {
        #if DEBUG_CALCULATIONS
        Serial.println(F("[API][DEBUG] fetchBitvavoPrice: symbol is null or empty"));
        #endif
        return false;
    }
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] fetchBitvavoPrice: starting fetch for market: %s\n"), symbol);
    #endif
    
    // M1: Heap telemetry vóór URL build
    logHeap("API_URL_BUILD");
    
    // Build Bitvavo API URL
    char url[128];
    snprintf(url, sizeof(url), "https://api.bitvavo.com/v2/ticker/price?market=%s", symbol);
    
    #if DEBUG_CALCULATIONS
    Serial_printf(F("[API][DEBUG] fetchBitvavoPrice: URL: %s\n"), url);
    #endif
    
    // M1: Heap telemetry vóór HTTP GET
    logHeap("API_GET_PRE");
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("ApiClient::fetchBitvavoPrice");
    
    const uint8_t MAX_RETRIES = 0; // Geen retries voor normale price fetches (snellere failure)
    const uint32_t RETRY_DELAYS[] = {100, 200}; // Backoff delays in ms (verlaagd, niet gebruikt met MAX_RETRIES=0)
    bool ok = false;
    
    for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        bool attemptOk = false;
        bool shouldRetry = false;
        int lastCode = 0;

        // N2: Gebruik persistent HTTPClient/WiFiClient voor keep-alive
        HTTPClient& http = httpClient;

        // S2: do-while(0) patroon voor consistente cleanup per attempt
        do {
            // T1: Expliciete connect/read timeout settings (geoptimaliseerd: 2000ms connect, 2500ms read)
            http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS_DEFAULT);
            http.setTimeout(HTTP_READ_TIMEOUT_MS_DEFAULT);
            // N2: Enable keep-alive voor snellere opvolgende requests
            http.setReuse(true);

            unsigned long requestStart = millis();

            // N2: Optimaliseer keep-alive: alleen reset bij retry of ongeldige verbinding
            bool connectionActive = http.connected();
            if (connectionActive && attempt > 0) {
                http.end();
                connectionActive = false;
            }

            // Alleen http.begin() als er geen actieve verbinding is
            if (!connectionActive) {
                #if !DEBUG_BUTTON_ONLY
                Serial.printf(F("[API] Fetching price from: %s\n"), url);
                #endif
                if (!http.begin(wifiClient, url)) {
                    #if !DEBUG_BUTTON_ONLY
                    if (attempt == MAX_RETRIES) {
                        Serial.printf(F("[API] http.begin() gefaald voor URL: %s\n"), url);
                    }
                    #endif
                    shouldRetry = (attempt < MAX_RETRIES);
                    break;
                }
            } else {
                #if !DEBUG_BUTTON_ONLY
                Serial.printf(F("[API] Fetching price from (keep-alive): %s\n"), url);
                #endif
            }

            // N2: Voeg headers toe voor elke request (ook bij keep-alive reuse)
            http.addHeader(F("User-Agent"), F("ESP32-CryptoMonitor/1.0"));
            http.addHeader(F("Accept"), F("application/json"));

            int code = http.GET();
            unsigned long requestTime = millis() - requestStart;
            lastCode = code;
            
            // M1: Heap telemetry na HTTP GET
            logHeap("API_GET_POST");
            
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
                
                // N2: Log ook response body bij HTTP 400 voor debugging
                if (code == 400) {
                    WiFiClient* errorStream = http.getStreamPtr();
                    if (errorStream != nullptr && errorStream->available()) {
                        char errorBuf[256];
                        size_t errorLen = errorStream->readBytes((uint8_t*)errorBuf, sizeof(errorBuf) - 1);
                        errorBuf[errorLen] = '\0';
                        Serial.printf(F("[API] HTTP 400 response body: %s\n"), errorBuf);
                    }
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
            logHeap("API_PARSE_PRE");
            
            // Parse price from stream
            if (!parseBitvavoPriceFromStream(stream, out)) {
                Serial.println(F("[API] JSON parse failed"));
                #if DEBUG_CALCULATIONS
                Serial.println(F("[API][DEBUG] fetchBitvavoPrice: parseBitvavoPriceFromStream failed"));
                #endif
                break;
            }
            
            // M1: Heap telemetry na JSON parse
            logHeap("API_PARSE_POST");
            
            #if DEBUG_CALCULATIONS
            Serial_printf(F("[API][DEBUG] fetchBitvavoPrice: SUCCESS - parsed price: %.2f for market: %s\n"), out, symbol);
            #endif
            
            attemptOk = true;
            ok = true;
        } while(0);
        
        // N2: Cleanup alleen bij fouten (keep-alive bij succes)
        if (attemptOk) {
            // Succes: alleen cleanup als verbinding niet meer actief is
            if (!http.connected()) {
                http.end();
            }
            if (attempt > 0) {
                Serial.printf(F("[API] Succes na retry (poging %d/%d)\n"), attempt + 1, MAX_RETRIES + 1);
            }
            break;
        } else {
            // Fout: cleanup
            http.end();
            WiFiClient* stream = http.getStreamPtr();
            if (stream != nullptr) {
                stream->stop();
            }
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
    netMutexUnlock("ApiClient::fetchBitvavoPrice");
    
    return ok;
}


