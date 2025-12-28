#include "HttpFetch.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "../Memory/HeapMon.h"

bool httpGetToBuffer(const char* url, char* buf, size_t bufCap, size_t* outLen, int timeoutMs)
{
    // Geconsolideerde validatie: check alle parameters in één keer
    if (url == nullptr || buf == nullptr || bufCap == 0 || outLen == nullptr) {
        return false;
    }
    
    *outLen = 0;
    buf[0] = '\0';
    
    // C2: Neem netwerk mutex voor alle HTTP operaties (met debug logging)
    netMutexLock("httpGetToBuffer");
    
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(1000);  // 1 seconde connect timeout
    http.setReuse(false);
    
    bool result = false;
    
    // C2: do-while(0) patroon voor consistente cleanup
    do {
        if (!http.begin(url)) {
            break;
        }
        
        // M1: Heap telemetry vóór HTTP GET
        logHeap("HTTP_BODY_GET_PRE");
        
        int code = http.GET();
        
        // Geconsolideerde error check: log error en break
        if (code != 200) {
            #if !DEBUG_BUTTON_ONLY
            Serial.printf(F("[HttpFetch] HTTP error: code=%d\n"), code);
            #endif
            break;
        }
        
        // Get response size (kan -1 zijn voor chunked encoding)
        int contentLength = http.getSize();
        // Geconsolideerde size check: check alles in één keer
        if (contentLength > 0 && (size_t)contentLength >= bufCap) {
            Serial.printf(F("[HttpFetch] Response te groot: %d bytes (buffer: %zu bytes)\n"), contentLength, bufCap);
            break;
        }
        
        // M1: Heap telemetry vóór body read
        logHeap("HTTP_BODY_READ_PRE");
        
        // Read body via streaming
        WiFiClient* stream = http.getStreamPtr();
        if (stream == nullptr) {
            break;
        }
        
        size_t totalRead = 0;
        const size_t CHUNK_SIZE = 256;  // Lees in chunks van 256 bytes
        
        // Geconsolideerde loop conditie: check alles in één keer
        // Read in chunks: continue zolang stream connected/available
        while (http.connected() && (contentLength < 0 || totalRead < (size_t)contentLength)) {
            // Geconsolideerde buffer check: check buffer ruimte in één keer
            if (totalRead >= (bufCap - 1)) {
                // Buffer vol, truncate
                Serial.printf(F("[HttpFetch] WARN: Response truncated (buffer vol: %zu bytes)\n"), bufCap);
                break;
            }
            
            size_t remaining = bufCap - 1 - totalRead;
            size_t chunkSize = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
            
            size_t bytesRead = stream->readBytes((uint8_t*)(buf + totalRead), chunkSize);
            if (bytesRead == 0) {
                // Geconsolideerde check: geen data meer beschikbaar
                if (!stream->available()) {
                    break;
                }
                // Wacht kort op meer data
                delay(10);
                continue;
            }
            
            totalRead += bytesRead;
        }
        
        // NUL terminator
        buf[totalRead] = '\0';
        *outLen = totalRead;
        
        // M1: Heap telemetry na body read
        logHeap("HTTP_BODY_READ_POST");
        
        result = true;
    } while(0);
    
    // C2: ALTIJD cleanup (ook bij code<0, code!=200, parse error)
    // Hard close: http.end() + client.stop() voor volledige cleanup
    http.end();
    WiFiClient* stream = http.getStreamPtr();
    if (stream != nullptr) {
        stream->stop();
    }
    
    // C2: Geef netwerk mutex vrij (met debug logging)
    netMutexUnlock("httpGetToBuffer");
    
    return result;
}


