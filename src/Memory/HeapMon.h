#ifndef HEAPMON_H
#define HEAPMON_H

#include <Arduino.h>

/**
 * HeapMon: Heap telemetry voor geheugenfragmentatie audit
 * 
 * Biedt heap snapshots en rate-limited logging voor geheugenanalyse.
 */
struct HeapSnap {
    uint32_t freeHeap;      // Vrije heap in bytes
    uint32_t largestBlock;  // Grootste vrije block in bytes
    uint32_t minFreeHeap;   // Minimum vrije heap sinds boot in bytes
};

/**
 * Neem een heap snapshot
 */
HeapSnap snapHeap();

/**
 * Log heap telemetry met rate limiting
 * @param tag Unieke tag voor deze tracepoint (max 1 log per tag per 5 seconden)
 */
void logHeap(const char* tag);

/**
 * Reset rate limit voor een specifieke tag (voor test doeleinden)
 */
void resetRateLimit(const char* tag);

#endif // HEAPMON_H


