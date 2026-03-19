#ifndef OTA_WEB_UPDATER_H
#define OTA_WEB_UPDATER_H

#include <Arduino.h>
#include <WebServer.h>

// Web-based chunked OTA updater for ESP32 (no ArduinoOTA).
// It registers dedicated routes:
//   - GET  /update
//   - POST /update/start
//   - POST /update/chunk
//   - POST /update/end
//
// Minimal integration:
//   OtaWebUpdater ota;
//   ota.begin(&server);
//   ota.registerRoutes(&server);
class OtaWebUpdater {
public:
    OtaWebUpdater();

    // Store the WebServer pointer to use inside route handlers.
    void begin(WebServer* srv);

    // Register the OTA routes on the given WebServer.
    void registerRoutes(WebServer* srv);

private:
    WebServer* server;
    size_t otaWritten;
    size_t otaTotal;
    bool otaStarted;

    // Helper: check client connection before sending large HTML responses
    static inline bool isClientConnected(WebServer* srv) {
        return (srv != nullptr) && srv->client().connected();
    }

    // JSON parse for {"total": N} from server->arg("plain")
    static bool parseOtaTotalFromBody(const String& body, size_t& outTotal);

    // Route handlers (copied/adapted from WebServer.cpp OTA implementation)
    void handleUpdateGet();
    void handleUpdateStart();
    void handleUpdateChunkPost();
    void handleUpdateChunkUpload();
    void handleUpdateEnd();
};

#endif // OTA_WEB_UPDATER_H

