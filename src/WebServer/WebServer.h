#ifndef WEBSERVERMODULE_H
#define WEBSERVERMODULE_H

#include <Arduino.h>
#include <WebServer.h>
#include "../TrendDetector/TrendDetector.h"
#include "../VolatilityTracker/VolatilityTracker.h"

// Forward declarations voor dependencies
class AnchorSystem;

// WebServerModule class - beheert web interface voor instellingen
// Fase 9: Web Interface Module refactoring
// Note: WebServer is al een ESP32 library class, dus we gebruiken WebServerModule als module naam
class WebServerModule {
public:
    WebServerModule();
    void begin();
    
    // Fase 9.1.2: Web server setup
    void setupWebServer();
    
    // Fase 9.1.3: HTML generatie
    void renderSettingsHTML();
    
    // Fase 9.1.4: Web handlers
    void handleRoot();
    void handleSave();
    void handleNotFound();
    void handleAnchorSet();
    void handleNtfyReset();
    void handleStatus();  // WEB-PERF-3: JSON status endpoint
    
    // Handler voor webTask (blijft in .ino, maar kan module method aanroepen)
    void handleClient();
    
private:
    // Fase 9.1.3: HTML helper functies
    void sendHtmlHeader(const char* platformName, const char* ntfyTopic);
    void sendHtmlFooter();
    void sendInputRow(const char* label, const char* name, const char* type, const char* value, 
                     const char* placeholder, float minVal = 0, float maxVal = 0, float step = 0);
    void sendCheckboxRow(const char* label, const char* name, bool checked);
    void sendStatusRow(const char* label, const char* value);
    void sendSectionHeader(const char* title, const char* sectionId, bool expanded = false);
    void sendSectionFooter();
    void sendSectionDesc(const char* desc);
    
    // Helper functies voor code duplicatie eliminatie
    static const char* getTrendText(TrendState trend);
    static const char* getVolatilityText(VolatilityState vol);
    bool parseFloatArg(const char* argName, float& result, float minVal, float maxVal);
    bool parseIntArg(const char* argName, int& result, int minVal, int maxVal);
    bool parseStringArg(const char* argName, char* dest, size_t destSize);
    
    // WEB-PERF-3: HTML caching voor performance
    static bool sPageCacheValid;
    static String sPageCache;
    void invalidatePageCache();
    String getOrBuildSettingsPage();
    
    // WebServer instance (wordt extern gedeclareerd in .ino)
    // Note: WebServer is een externe library class, we gebruiken een referentie
    WebServer* server;
};

// Global instance (wordt aangemaakt in .ino)
extern WebServerModule webServerModule;

#endif // WEBSERVERMODULE_H

