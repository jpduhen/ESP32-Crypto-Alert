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

// Helper: Find tag index in tagLogs array (geoptimaliseerd: elimineert code duplicatie)
// Geoptimaliseerd: pointer vergelijking eerst (sneller), dan string vergelijking alleen als nodig
static int findTagIndex(const char* tag) {
    if (tag == nullptr) {
        return -1;
    }
    
    for (int i = 0; i < tagCount; i++) {
        // Geoptimaliseerd: pointer vergelijking eerst (sneller), dan string vergelijking
        if (tagLogs[i].tag == tag || 
            (tagLogs[i].tag != nullptr && strcmp(tagLogs[i].tag, tag) == 0)) {
            return i;
        }
    }
    return -1;
}

HeapSnap snapHeap() {
    HeapSnap snap;
    snap.freeHeap = ESP.getFreeHeap();
    snap.largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snap.minFreeHeap = ESP.getMinFreeHeap();
    return snap;
}

void logHeap(const char* tag) {
    // Geconsolideerde validatie: early return
    if (tag == nullptr) {
        return;
    }
    
    // Rate limiting: check of deze tag recent is gelogd
    unsigned long now = millis();
    int tagIndex = findTagIndex(tag);
    bool shouldLog = false;
    
    if (tagIndex >= 0) {
        // Bestaande tag: check rate limit
        if ((now - tagLogs[tagIndex].lastLogTime) >= RATE_LIMIT_MS) {
            tagLogs[tagIndex].lastLogTime = now;
            shouldLog = true;
        }
    } else {
        // Nieuwe tag toevoegen als er ruimte is
        if (tagCount < MAX_TAGS) {
            tagIndex = tagCount;
            tagLogs[tagIndex].tag = tag;
            tagLogs[tagIndex].lastLogTime = now;
            tagCount++;
            shouldLog = true;
        }
    }
    
    // Log alleen als rate limit niet is bereikt
    if (shouldLog) {
        HeapSnap snap = snapHeap();
        Serial.printf("[Heap] %s: free=%u largest=%u minFree=%u\n", 
                     tag, snap.freeHeap, snap.largestBlock, snap.minFreeHeap);
    }
}

void resetRateLimit(const char* tag) {
    // Geconsolideerde validatie: early return
    if (tag == nullptr) {
        return;
    }
    
    // Geoptimaliseerd: gebruik helper functie i.p.v. duplicatie
    int tagIndex = findTagIndex(tag);
    if (tagIndex >= 0) {
        tagLogs[tagIndex].lastLogTime = 0;
    }
}





