#include "HeapMon.h"
#include <esp_heap_caps.h>

// Rate limiting: max 1 log per tag per 5 seconden
static const unsigned long RATE_LIMIT_MS = 5000;
static const int MAX_TAGS = 32;  // Maximum aantal unieke tags

struct TagLogEntry {
    const char* tag;
    unsigned long lastLogTime;
};

static TagLogEntry tagLogs[MAX_TAGS];
static int tagCount = 0;

HeapSnap snapHeap() {
    HeapSnap snap;
    snap.freeHeap = ESP.getFreeHeap();
    snap.largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snap.minFreeHeap = ESP.getMinFreeHeap();
    return snap;
}

void logHeap(const char* tag) {
    if (tag == nullptr) {
        return;
    }
    
    // Rate limiting: check of deze tag recent is gelogd
    unsigned long now = millis();
    bool shouldLog = true;
    int tagIndex = -1;
    
    // Zoek bestaande tag entry
    for (int i = 0; i < tagCount; i++) {
        if (tagLogs[i].tag == tag || (tagLogs[i].tag != nullptr && strcmp(tagLogs[i].tag, tag) == 0)) {
            tagIndex = i;
            // Check rate limit
            if ((now - tagLogs[i].lastLogTime) < RATE_LIMIT_MS) {
                shouldLog = false;
            } else {
                tagLogs[i].lastLogTime = now;
            }
            break;
        }
    }
    
    // Nieuwe tag toevoegen als er ruimte is
    if (tagIndex == -1 && tagCount < MAX_TAGS) {
        tagIndex = tagCount;
        tagLogs[tagIndex].tag = tag;
        tagLogs[tagIndex].lastLogTime = now;
        tagCount++;
        shouldLog = true;
    }
    
    // Log alleen als rate limit niet is bereikt
    if (shouldLog) {
        HeapSnap snap = snapHeap();
        Serial.printf("[Heap] %s: free=%u largest=%u minFree=%u\n", 
                     tag, snap.freeHeap, snap.largestBlock, snap.minFreeHeap);
    }
}

void resetRateLimit(const char* tag) {
    if (tag == nullptr) {
        return;
    }
    
    for (int i = 0; i < tagCount; i++) {
        if (tagLogs[i].tag == tag || (tagLogs[i].tag != nullptr && strcmp(tagLogs[i].tag, tag) == 0)) {
            tagLogs[i].lastLogTime = 0;
            break;
        }
    }
}


