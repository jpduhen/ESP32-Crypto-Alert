#pragma once

#include <stddef.h>
#include <stdint.h>

// Meetpatch: seriële audit vóór sendNotification (implementatie in ESP32-Crypto-Alert.ino).
void ntfyBuildSequenceId(const char* title, const char* body, char* outSeq, size_t outSeqSize);
void alertAuditPriceSnapshot(float* outPrice, const char** outSrcTag, uint32_t* outAgeMs);
void alertAuditLog(const char* rule, const char* seqNullable, float price, const char* price_src,
                   uint32_t price_age_ms, const char* metric, float threshold,
                   const char* context1, const char* context2);
