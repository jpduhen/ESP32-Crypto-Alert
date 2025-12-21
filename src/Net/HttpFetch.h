#ifndef HTTPFETCH_H
#define HTTPFETCH_H

#include <Arduino.h>
#include <freertos/semphr.h>

// C2: Extern declaraties voor netwerk mutex en helpers (gedefinieerd in .ino)
extern SemaphoreHandle_t gNetMutex;
extern void netMutexLock(const char* taskName);
extern void netMutexUnlock(const char* taskName);

/**
 * HttpFetch: Streaming HTTP body read naar herbruikbare buffer
 * 
 * Vervangt http.getString() om geheugenfragmentatie te voorkomen.
 * Leest HTTP response body direct in een char buffer via streaming.
 */
bool httpGetToBuffer(const char* url, char* buf, size_t bufCap, size_t* outLen, int timeoutMs);

#endif // HTTPFETCH_H


