#include "OtaWebUpdater.h"

#include "OtaWebUpdaterConfig.h"

#include <ESP.h>      // ESP.restart()
#include <Update.h>   // Update.begin / write / end / abort

// OTA_ENABLED is defined in the project platform config.
// platform_config.h include PINS files alleen in de main .ino.
// Voor modules (zoals deze OTA updater) definiëren we MODULE_INCLUDE om
// bus/gfx multiple definition fouten te voorkomen.
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

#ifndef Serial_printf
#define Serial_printf Serial.printf
#endif

OtaWebUpdater::OtaWebUpdater()
    : server(nullptr),
      otaWritten(0),
      otaTotal(0),
      otaStarted(false) {}

void OtaWebUpdater::begin(WebServer* srv) {
    server = srv;
}

bool OtaWebUpdater::parseOtaTotalFromBody(const String& body, size_t& outTotal) {
    // Very small JSON parse for {"total":number}
    int i = body.indexOf(F("\"total\""));
    if (i < 0) return false;
    i = body.indexOf(':', i);
    if (i < 0) return false;

    const char* p = body.c_str() + i + 1;
    while (*p == ' ' || *p == '\t') p++;

    unsigned long val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }

    if (val == 0 || val > OTA_WEBUPDATER_MAX_SIZE) return false;
    outTotal = (size_t)val;
    return true;
}

void OtaWebUpdater::registerRoutes(WebServer* srv) {
    server = srv;
#if OTA_ENABLED
    if (server == nullptr) return;

    // Upload page
    server->on("/update", HTTP_GET, [this]() { this->handleUpdateGet(); });

    // Start OTA (JSON body: {"total":N} or query arg: ?total=N)
    server->on("/update/start", HTTP_POST, [this]() { this->handleUpdateStart(); });

    // Chunked upload:
    //  - POST /update/chunk (multipart form-data, includes field name 'c')
    //  - server's upload callback is called for each received chunk
    server->on("/update/chunk", HTTP_POST,
                [this]() { this->handleUpdateChunkPost(); },
                [this]() { this->handleUpdateChunkUpload(); });

    // Finish OTA
    server->on("/update/end", HTTP_POST, [this]() { this->handleUpdateEnd(); });
#else
    (void)server;
#endif
}

void OtaWebUpdater::handleUpdateGet() {
#if OTA_ENABLED
    if (server == nullptr) return;
    if (!isClientConnected(server)) return;

    const char* html =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Firmware update</title><style>"
        "body{font-family:sans-serif;background:#1a1a1a;color:#eee;padding:20px;max-width:500px;margin:0 auto;}"
        "h1{font-size:1.2em;} input[type=file]{margin:10px 0;}"
        "button{background:#2196F3;color:#fff;border:none;padding:12px 24px;border-radius:4px;cursor:pointer;font-size:16px;}"
        "button:disabled{opacity:0.5;cursor:not-allowed;}"
        "#msg{margin-top:15px;padding:10px;border-radius:4px;}"
        ".ok{background:#2e7d32;}.err{background:#c62828;}"
        ".progress-wrap{margin-top:12px;display:none;}"
        ".progress-bar{height:24px;background:#333;border-radius:4px;overflow:hidden;}"
        ".progress-fill{height:100%;background:#2196F3;width:0%;transition:width 0.15s;}"
        "#progressPct{margin-top:6px;font-size:14px;color:#00BCD4;}"
        "</style></head><body>"
        "<h1>Firmware update (OTA)</h1>"
        "<p>Kies een .bin bestand.</p>"
        "<p style='font-size:0.9em;color:#aaa;'>In Arduino IDE compileer je via Menu &rarr; Sketch &rarr; Export compiled Binary. "
        "Het bestand &hellip;.ino.bin staat in de build-map.</p>"
        "<p><input type='file' id='file' accept='.bin'><button type='button' id='btn' onclick='doOtaUpload()'>Upload</button></p>"
        "<div class='progress-wrap' id='progressWrap'><div class='progress-bar'>"
        "<div class='progress-fill' id='progressFill'></div></div><div id='progressPct'>0%</div></div>"
        "<div id='msg'></div>"
        "<script>"
        "function doOtaUpload(){"
        "var f=document.getElementById('file').files[0],btn=document.getElementById('btn');"
        "var wrap=document.getElementById('progressWrap'),fill=document.getElementById('progressFill');"
        "var pct=document.getElementById('progressPct'),msg=document.getElementById('msg');"
        "if(!f){msg.innerHTML='<span class=err>Kies een bestand.</span>';return;}"
        "btn.disabled=true;msg.textContent='Uploaden...';wrap.style.display='block';fill.style.width='0%';pct.textContent='0%';"
        "function setPct(w,t){var v=(t>0)?Math.min(100,Math.round(100*w/t)):0;fill.style.width=v+'%';pct.textContent=v+'%';}"
        "function fail(e){msg.innerHTML='<span class=err>'+e.message+'</span>';btn.disabled=false;wrap.style.display='none';}"
        "var CHUNK=32768;"
        "fetch('/update/start?total='+f.size,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({total:f.size})})"
        ".then(function(r){if(!r.ok)throw new Error(r.status);return r.json();}).then(function(j){if(!j.ok)throw new Error(j.error||'Start');"
        "var off=0;"
        "function next(){"
        "if(off>=f.size){return fetch('/update/end',{method:'POST'}).then(function(r){if(!r.ok)throw new Error(r.status);return r.json();})"
        ".then(function(){fill.style.width='100%';pct.textContent='100%';msg.innerHTML='<span class=ok>OK. Herstart...</span>';setTimeout(function(){location.href='/';},3000);})"
        ".catch(function(e){msg.innerHTML='<span class=err>'+e.message+'</span>';btn.disabled=false;});}"
        "var end=Math.min(off+CHUNK,f.size),fd=new FormData();fd.append('c',f.slice(off,end));"
        "return fetch('/update/chunk',{method:'POST',body:fd}).then(function(r){if(!r.ok)throw new Error(r.status);return r.json();})"
        ".then(function(j){off=end;setPct(j.written,j.total);return next();});"
        "}"
        "return next();"
        "}).catch(fail);"
        "}"
        "</script>"
        "</body></html>";

    server->send(200, "text/html", html);
#endif
}

void OtaWebUpdater::handleUpdateStart() {
#if OTA_ENABLED
    if (server == nullptr) return;

    size_t total = 0;

    // Total out URL parameter (trusted) or out JSON body
    if (server->hasArg("total")) {
        long t = server->arg("total").toInt();
        if (t > 0 && (size_t)t <= OTA_WEBUPDATER_MAX_SIZE) total = (size_t)t;
    }

    if (total == 0) {
        // Try JSON body (server->arg("plain"))
        parseOtaTotalFromBody(server->arg("plain"), total);
    }

    if (total == 0) {
        server->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid total\"}");
        return;
    }

    if (total > OTA_WEBUPDATER_MAX_SIZE) {
        server->send(400, "application/json", "{\"ok\":false,\"error\":\"File too large\"}");
        return;
    }

    if (otaStarted) {
        Update.abort();
        otaStarted = false;
    }

    if (!Update.begin(total, U_FLASH)) {
        Update.printError(Serial);
        server->send(500, "application/json", "{\"ok\":false,\"error\":\"Update.begin failed\"}");
        return;
    }

    otaWritten = 0;
    otaTotal = total;
    otaStarted = true;
    Serial_printf(F("[OTA] Start: %u bytes\n"), (unsigned)total);

    char buf[80];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"total\":%u}", (unsigned)total);
    server->send(200, "application/json", buf);
#endif
}

void OtaWebUpdater::handleUpdateChunkUpload() {
#if OTA_ENABLED
    if (server == nullptr) return;

    HTTPUpload& upload = server->upload();
    static unsigned long s_lastLogKb = 0;

    if (upload.status == UPLOAD_FILE_WRITE) {
        if (otaWritten + upload.currentSize > OTA_WEBUPDATER_MAX_SIZE) {
            Update.abort();
            otaStarted = false;
            return;
        }

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            return;
        }

        otaWritten += upload.currentSize;
        unsigned long kb = (unsigned long)(otaWritten / 1024u);
        if (kb >= s_lastLogKb + 100u || (s_lastLogKb == 0 && kb >= 100u)) {
            Serial_printf(F("[OTA] %lu KB\n"), kb);
            s_lastLogKb = kb;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        s_lastLogKb = 0;
    }
#endif
}

void OtaWebUpdater::handleUpdateChunkPost() {
#if OTA_ENABLED
    if (server == nullptr) return;

    if (Update.hasError()) {
        Update.printError(Serial);
        server->send(500, "application/json", "{\"written\":0,\"total\":0,\"error\":\"write failed\"}");
        return;
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "{\"written\":%u,\"total\":%u}", (unsigned)otaWritten, (unsigned)otaTotal);
    server->send(200, "application/json", buf);
#endif
}

void OtaWebUpdater::handleUpdateEnd() {
#if OTA_ENABLED
    if (server == nullptr) return;

    otaStarted = false;

    if (!Update.end(true)) {
        Update.printError(Serial);
        server->send(500, "application/json", "{\"done\":false,\"error\":\"Update.end failed\"}");
        return;
    }

    Serial_printf(F("[OTA] Klaar: %u bytes, herstart\n"), (unsigned)otaWritten);
    server->send(200, "application/json", "{\"done\":true}");
    delay(1500);
    Serial.flush();
    ESP.restart();
#endif
}

