// Minimal configuration for the web-based chunked OTA updater.
// Copying this folder into another ESP32 project should be enough.

#ifndef OTA_WEB_UPDATER_CONFIG_H
#define OTA_WEB_UPDATER_CONFIG_H

// Bovengrens in firmware; de echte limiet is min(dit, ESP.getFreeSketchSpace()) = OTA-slot uit de
// partitietabel in flash. Arduino “Minimal SPIFFS” heeft vaak ~0x1E0000 per app; custom partitions.csv kan groter zijn.
#ifndef OTA_WEBUPDATER_MAX_SIZE
#define OTA_WEBUPDATER_MAX_SIZE 0x2F0000u
#endif

// Used in the JS chunking logic (frontend).
#ifndef OTA_WEBUPDATER_JS_CHUNK_SIZE
#define OTA_WEBUPDATER_JS_CHUNK_SIZE 32768
#endif

#endif // OTA_WEB_UPDATER_CONFIG_H

