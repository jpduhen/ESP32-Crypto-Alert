// Minimal configuration for the web-based chunked OTA updater.
// Copying this folder into another ESP32 project should be enough.

#ifndef OTA_WEB_UPDATER_CONFIG_H
#define OTA_WEB_UPDATER_CONFIG_H

// Keep in sync with the OTA maximum size used in the current WebUI.
// Value taken from the original WebServer.cpp implementation.
#ifndef OTA_WEBUPDATER_MAX_SIZE
#define OTA_WEBUPDATER_MAX_SIZE 0x1E0000u
#endif

// Used in the JS chunking logic (frontend).
#ifndef OTA_WEBUPDATER_JS_CHUNK_SIZE
#define OTA_WEBUPDATER_JS_CHUNK_SIZE 32768
#endif

#endif // OTA_WEB_UPDATER_CONFIG_H

