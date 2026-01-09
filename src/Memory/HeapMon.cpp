#include "HeapMon.h"
#include <esp_heap_caps.h>

// Rate limiting: max 1 log per tag per 5 seconden
constexpr unsigned long RATE_LIMIT_MS = 5000;
constexpr uint8_t MAX_TAGS = 32;  // Maximum aantal unieke tags

struct TagLogEntry {
    const char* tag;
    unsigned long lastLogTime;
};

static TagLogEntry tagLogs[MAX_TAGS];
static uint8_t tagCount = 0;

// Helper: Find tag index in tagLogs array (geoptimaliseerd: elimineert code duplicatie)
// Geoptimaliseerd: pointer vergelijking eerst (sneller), dan string vergelijking alleen als nodig
static int findTagIndex(const char* tag) {
    if (tag == nullptr) {
        return -1;
    }
    
    for (int i = 0; i < tagCount; i++) {
        // Geoptimaliseerd: pointer vergelijking eerst (sneller), dan string vergelijking
        if (tagLogs[i].tag == tag ||
            (!HEAPMON_POINTER_ONLY_MATCH &&
             tagLogs[i].tag != nullptr && strcmp(tagLogs[i].tag, tag) == 0)) {
            return i;
        }
    }
    return -1;
}

static int findOldestTagIndex() {
    if (tagCount == 0) {
        return -1;
    }

    int oldestIndex = 0;
    unsigned long oldestTime = tagLogs[0].lastLogTime;
    for (int i = 1; i < tagCount; i++) {
        if (tagLogs[i].lastLogTime < oldestTime) {
            oldestTime = tagLogs[i].lastLogTime;
            oldestIndex = i;
        }
    }
    return oldestIndex;
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
        } else {
            // Evict oudste tag om logging niet stilletjes te verliezen
            tagIndex = findOldestTagIndex();
            if (tagIndex >= 0) {
                tagLogs[tagIndex].tag = tag;
                tagLogs[tagIndex].lastLogTime = now;
                shouldLog = true;
            }
        }
    }
    
    // Log alleen als rate limit niet is bereikt
    #if !DEBUG_BUTTON_ONLY
    if (shouldLog) {
        HeapSnap snap = snapHeap();
        if (Serial) {
            Serial.printf("[Heap] %s: free=%u largest=%u minFree=%u\n",
                         tag, snap.freeHeap, snap.largestBlock, snap.minFreeHeap);
        }
    }
    #endif
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




